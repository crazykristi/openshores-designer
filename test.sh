#!/usr/bin/env bash
#
# Runs the generator over a matrix of templates and hulls and validates every
# blueprint it produces with the engine's own report. Also starts the graphical
# tool once to make sure it comes up.
#
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC="${TOOLCHAIN:-$HOME/.local/hazeron-toolchain}"
export LD_LIBRARY_PATH="$TC/usr/lib64:${LD_LIBRARY_PATH:-}"
SHIPGEN="$HERE/build/bin/shipgen"
SHIPGEN_GUI="$HERE/build/bin/shipgen-gui"
SHIPGEN_MESH="$HERE/build/bin/shipgen-mesh"
# macOS has neither GNU timeout nor an X11 desktop. Use Cocoa for GUI tests.
ON_MAC=0
if [ "$(uname -s)" = Darwin ]; then
	ON_MAC=1
	timeout() {
		python3 - "$@" <<'PYTHON'
import os
import subprocess
import sys

env = os.environ.copy()
env.pop("QT_QPA_PLATFORM", None)
try:
    result = subprocess.run(sys.argv[2:], env=env, timeout=float(sys.argv[1]))
    sys.exit(result.returncode if result.returncode >= 0 else 128 - result.returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
PYTHON
	}
fi

WORK="$HERE/work/test"
DESIGNS="${DESIGNS:-$HOME/Documents/designs}"
EXAMPLE_BUILDINGS="${EXAMPLE_BUILDINGS:-$HOME/Documents/designs/examplebuildings}"

if [ ! -x "$SHIPGEN" ]; then
	echo "shipgen has not been built. Run ./build.sh first."
	exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK"

# The hulls to try.
HULLS=()
for f in "$HERE/testdata/testhull.stl" "$HERE/testdata/testhull.3ds"; do
	[ -e "$f" ] && HULLS+=("--model|$f")
done
for f in "$DESIGNS"/*.soh "$DESIGNS"/*.SoH; do
	[ -e "$f" ] && HULLS+=("--hull|$f")
done

TEMPLATES=(military_frigate cargo_hauler science_lab colony_transport patrol_cutter shuttle)
BUILDING_TEMPLATES=(office_block residential barracks command_post warehouse factory biodome)

pass=0
fail=0
validated=0

check() {
	local label="$1" design="$2"
	local errors
	errors="$("$SHIPGEN" validate "$design" 2>&1 | sed -n '/^Errors/,/^$/p' | grep -c "Error:")"
	if [ "$errors" = "0" ]; then
		pass=$((pass+1))
		validated=$((validated+1))
		printf "  ok    %s\n" "$label"
	else
		fail=$((fail+1))
		printf "  FAIL  %s\n" "$label"
		"$SHIPGEN" validate "$design" 2>&1 | sed -n '/^Errors/,/^$/p' | head -5
	fi
}

# The engine's report does not look at everything that matters for a ship the
# player can walk around in: a room may poke out of the hull, two rooms may
# share space, or a door may have no room behind it. The inspect command says
# all of that, so the test reads it.
check_interior() {
	local label="$1" design="$2"
	local report="$WORK/interior.txt"
	if ! "$SHIPGEN" inspect "$design" > "$report" 2>&1; then
		fail=$((fail+1))
		printf "  FAIL  %s (could not be inspected)\n" "$label"
		return
	fi

	local problems=""
	if grep -q 'OUTSIDE' "$report"; then
		problems="$problems; a room is outside the hull"
	fi
	if ! grep -q '(no room parts)' "$report"; then
		if ! grep -q '^Room overlaps: none$' "$report"; then
			problems="$problems; two rooms share space"
		fi
		if ! grep -q 'states: ' "$report"; then
			problems="$problems; there are rooms but no doors"
		fi
		if grep -q 'open state room voids: NONE' "$report"; then
			problems="$problems; a door has no room void"
		fi
		if grep -q 'open state hull voids: NONE' "$report"; then
			problems="$problems; a hull door has no hull void"
		fi
		if grep -q '^Rooms without a door: [1-9]' "$report"; then
			problems="$problems; a room has no door"
		fi
		if grep -q '^Rooms that are only there while a door is open' "$report"; then
			problems="$problems; a room's only void is tied to a door"
		fi
	fi

	if [ -z "$problems" ]; then
		pass=$((pass+1))
		printf "  ok    %s interior\n" "$label"
	else
		fail=$((fail+1))
		printf "  FAIL  %s interior%s\n" "$label" "$problems"
	fi
}

# The exact check. It builds a closed triangle mesh for every hull part and
# tests each room against the triangles, so it catches a room that is inside
# the silhouette projections but in the open air of a concavity. A hull part
# that is not closed cannot answer the question at all, so that fails too:
# a room there is not certified, it is merely unchecked.
check_audit() {
	local label="$1" design="$2"
	local report="$WORK/audit.txt"
	if "$SHIPGEN" audit "$design" > "$report" 2>&1; then
		pass=$((pass+1))
		printf "  ok    %s exact containment\n" "$label"
	else
		fail=$((fail+1))
		printf "  FAIL  %s exact containment\n" "$label"
		sed -n '/OUTSIDE/p;/CANNOT BE CHECKED/p;/NOT CLOSED/p;/^Note:/p' "$report" | head -5
	fi
}

echo "== Spacecraft =="
for t in "${TEMPLATES[@]}"; do
	"$SHIPGEN" brief "$t" "$WORK/$t.json" > /dev/null || exit 1
done

for entry in "${HULLS[@]}"; do
	kind="${entry%%|*}"
	file="${entry#*|}"
	name="$(basename "$file")"
	for t in "${TEMPLATES[@]}"; do
		out="$WORK/out/${t}_${name}"
		mkdir -p "$out"
		"$SHIPGEN" generate "$kind" "$file" --brief "$WORK/$t.json" --out "$out" > "$WORK/log.txt" 2>&1
		if [ $? -ne 0 ]; then
			if grep -q 'not a spacecraft design' "$WORK/log.txt"; then
				pass=$((pass+1))
				echo "  ok    $t / $name skipped: not a spacecraft design"
			elif grep -q 'the design has no hull part' "$WORK/log.txt"; then
				pass=$((pass+1))
				echo "  ok    $t / $name skipped: the source design has no hull"
			elif grep -q 'not a closed solid' "$WORK/log.txt"; then
				pass=$((pass+1))
				echo "  ok    $t / $name refused: the hull is not a closed solid"
			else
				fail=$((fail+1))
				echo "  FAIL  $t / $name could not be generated"
				head -3 "$WORK/log.txt"
			fi
			continue
		fi
		# The model must not have more faces than the game can carry.
		faces=$(grep -o "Faces: [0-9]*" "$WORK/log.txt" | head -1 | grep -o "[0-9]*")
		if [ -n "$faces" ] && [ "$faces" -gt 15000 ]; then
			fail=$((fail+1))
			echo "  FAIL  $t / $name has $faces faces, which is over the limit of 15000"
		fi
		for design in "$out"/*.SoH; do
			[ -e "$design" ] || continue
			check "$t / $name / $(basename "$design")" "$design"
			check_interior "$t / $name / $(basename "$design")" "$design"
			check_audit "$t / $name / $(basename "$design")" "$design"
		done
	done
done

# What the game wants, on a generated ship: hulls wound with a negative
# signed volume, rooms positive, and one engineer running the FTL drive.
frigate=$(ls "$WORK/out/military_frigate_testhull.stl"/*decks*.SoH 2>/dev/null | head -1)
if [ -n "$frigate" ]; then
	"$SHIPGEN" parts "$frigate" > "$WORK/frigate-parts.txt" 2>&1
	if grep -E '  Hull  .*signed volume -' "$WORK/frigate-parts.txt" > /dev/null &&
			! grep -E '  Room  .*signed volume -' "$WORK/frigate-parts.txt" > /dev/null &&
			[ "$(grep -c 'Engineer Console' "$WORK/frigate-parts.txt")" = "1" ]; then
		pass=$((pass+1))
		echo "  ok    the generated frigate is wound the way the game wants, with one engineer"
	else
		fail=$((fail+1))
		echo "  FAIL  the generated frigate is wound wrongly or has the wrong engineers"
		grep -E '  (Hull|Room)  |Engineer' "$WORK/frigate-parts.txt" | head -6
	fi
	# The bridge is at the bow (+Y, the fore half of the ship) and looks out
	# through windows cut in the hull.
	if awk '/  Room  \[Bridge\]/{ for(i=1;i<=NF;i++) if($i=="y"){ split($(i+1),b,"\\.\\."); if(b[1]+b[2] > 0) fore=1 } } END{ exit fore?0:1 }' "$WORK/frigate-parts.txt" &&
			grep -q '  Window  \[Bridge\]' "$WORK/frigate-parts.txt"; then
		pass=$((pass+1))
		echo "  ok    the generated frigate's bridge is at the bow, with windows"
	else
		fail=$((fail+1))
		echo "  FAIL  the generated frigate's bridge is not at the bow, or has no windows"
		grep -E '  Room  \[Bridge\]|Window' "$WORK/frigate-parts.txt" | head -4
	fi
fi

echo
echo "== Buildings =="
for t in "${BUILDING_TEMPLATES[@]}"; do
	"$SHIPGEN" building-brief "$t" "$WORK/$t.json" > /dev/null || exit 1
	out="$WORK/bout/$t"
	mkdir -p "$out"
	"$SHIPGEN" generate-building --brief "$WORK/$t.json" --out "$out" > "$WORK/log.txt" 2>&1
	faces=$(grep -o "faces           : [0-9]*" "$WORK/log.txt" | head -1 | grep -o "[0-9]*$")
	if [ -n "$faces" ] && [ "$faces" -gt 15000 ]; then
		fail=$((fail+1))
		echo "  FAIL  $t has $faces faces, which is over the limit of 15000"
	fi
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "$t / $(basename "$design")" "$design"
		check_interior "$t / $(basename "$design")" "$design"
		check_audit "$t / $(basename "$design")" "$design"
	done
done

# A primitive base is a closed solid on its own, so it has to make a valid
# blueprint without any model at all.
# Textures have to reach the faces, not just the palette. A palette entry on
# its own shows nothing in the game, so the test checks that every hull vertex
# has texture coordinates as well.
echo
echo "== Painting =="
out="$WORK/paint"
mkdir -p "$out"
"$SHIPGEN" generate --model "$HERE/testdata/testhull.stl" --brief "$WORK/shuttle.json" \
	--out "$out" --texture '#3a6ea5' --texture '#b5651d' --paint by-face --paint-scale 6 \
	> "$WORK/log.txt" 2>&1
paint_design=$(ls "$out"/*.SoH 2>/dev/null | head -1)
if [ -z "$paint_design" ]; then
	fail=$((fail+1))
	echo "  FAIL  painting made no blueprint"
	head -5 "$WORK/log.txt"
else
	if grep -q 'painted 1 hull part' "$WORK/log.txt"; then
		pass=$((pass+1))
		echo "  ok    the hull was painted"
	else
		fail=$((fail+1))
		echo "  FAIL  the hull was not painted"
		grep -i 'paint' "$WORK/log.txt" | head -3
	fi
	# One hull part cannot use two textures. The tool has to say so rather
	# than silently dropping one.
	if grep -q 'Each hull part takes one texture' "$WORK/log.txt"; then
		pass=$((pass+1))
		echo "  ok    the unused texture was reported"
	else
		fail=$((fail+1))
		echo "  FAIL  the unused texture was not reported"
	fi
	"$SHIPGEN" inspect "$paint_design" > "$WORK/paint.txt" 2>&1
	slots=$(grep -c '^  slot ' "$WORK/paint.txt")
	if [ "$slots" -ge 2 ]; then
		pass=$((pass+1))
		echo "  ok    the palette holds $slots textures"
	else
		fail=$((fail+1))
		echo "  FAIL  the palette holds $slots textures, not 2"
	fi
	if grep -Eq 'UVs on [1-9][0-9]* of' "$WORK/paint.txt"; then
		pass=$((pass+1))
		echo "  ok    the hull faces have texture coordinates"
	else
		fail=$((fail+1))
		echo "  FAIL  the hull faces have no texture coordinates"
	fi
	check "painted ship / $(basename "$paint_design")" "$paint_design"
fi

echo
echo "== Primitive base shapes =="
for shape in rectangle circle hexagon; do
	out="$WORK/base/$shape"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--base "$shape" --base-size 160x120 --base-height 24 \
		--brief "$WORK/office_block.json" --out "$out" --layout grid > "$WORK/log.txt" 2>&1
	faces=$(grep -o "faces           : [0-9]*" "$WORK/log.txt" | head -1 | grep -o "[0-9]*$")
	if [ -z "$faces" ] || [ "$faces" -gt 15000 ]; then
		fail=$((fail+1))
		echo "  FAIL  the $shape base has '$faces' faces"
	else
		pass=$((pass+1))
		echo "  ok    the $shape base has $faces faces"
	fi
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "$shape base / $(basename "$design")" "$design"
	done
done

echo
echo "== Example building blueprints =="
if [ -d "$EXAMPLE_BUILDINGS" ]; then
	for design in "$EXAMPLE_BUILDINGS"/*.SoH; do
		[ -e "$design" ] || continue
		if "$SHIPGEN" inspect "$design" > "$WORK/log.txt" 2>&1; then
			pass=$((pass+1))
			echo "  ok    read $(basename "$design")"
		else
			fail=$((fail+1))
			echo "  FAIL  could not read $(basename "$design")"
			head -3 "$WORK/log.txt"
		fi
	done
else
	echo "  (no building examples at $EXAMPLE_BUILDINGS)"
fi

echo
echo "== The real models =="
MODELS="$HERE/testdata/models"
if [ -d "$MODELS" ] && [ -e "$MODELS/aetherite_top.stl" ]; then
	# The Aetherite is a Capitol. It is in three parts, so it needs three
	# model arguments.
	"$SHIPGEN" building-brief capitol "$WORK/capitol.json" > /dev/null
	out="$WORK/models/capitol"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--model "$MODELS/aetherite_bottom.stl" \
		--model "$MODELS/aetherite_ring.stl" \
		--model "$MODELS/aetherite_top.stl" \
		--fit 120 --brief "$WORK/capitol.json" --out "$out" > "$WORK/log.txt" 2>&1
	faces=$(grep -o "faces           : [0-9]*" "$WORK/log.txt" | head -1 | grep -o "[0-9]*$")
	if [ -n "$faces" ] && [ "$faces" -gt 15000 ]; then
		fail=$((fail+1))
		echo "  FAIL  the Capitol has $faces faces, which is over the limit of 15000"
	else
		pass=$((pass+1))
		echo "  ok    the Capitol has $faces faces"
	fi
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "capitol from the Aetherite / $(basename "$design")" "$design"
	done

	# The Capitol has to come out like the reference blueprint.
	if [ -e "$HERE/testdata/aetherite_capitol_reference.SoH" ]; then
		ref="$HERE/testdata/aetherite_capitol_reference.SoH"
		mine="$WORK/models/capitol/Capitol_grid.SoH"
		if [ -e "$mine" ]; then
			"$SHIPGEN" inspect "$ref" > "$WORK/ref.txt" 2>&1
			"$SHIPGEN" inspect "$mine" > "$WORK/mine.txt" 2>&1
			same=1
			for what in "Industry" "Hull:" "Ground Vehicle Parking Spot" "Space Vehicle Parking Spot" "Site"; do
				r=$(grep -c "$what" "$WORK/ref.txt")
				m=$(grep -c "$what" "$WORK/mine.txt")
				if [ "$r" != "$m" ]; then
					same=0
					echo "    the '$what' count differs: reference $r, ours $m"
				fi
			done
			if [ "$same" = "1" ]; then
				pass=$((pass+1))
				echo "  ok    the Capitol matches the reference blueprint"
			else
				fail=$((fail+1))
				echo "  FAIL  the Capitol does not match the reference blueprint"
			fi
		fi
	fi

	# A model in three parts gets three textures, one per hull part.
	out="$WORK/models/paint-parts"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--model "$MODELS/aetherite_bottom.stl" \
		--model "$MODELS/aetherite_ring.stl" \
		--model "$MODELS/aetherite_top.stl" \
		--fit 120 --brief "$WORK/capitol.json" --out "$out" \
		--texture '#a33' --texture '#3a3' --texture '#33a' > "$WORK/log.txt" 2>&1
	paint_design=$(ls "$out"/*.SoH 2>/dev/null | head -1)
	if [ -z "$paint_design" ]; then
		fail=$((fail+1))
		echo "  FAIL  painting a model in parts made no blueprint"
	else
		"$SHIPGEN" inspect "$paint_design" > "$WORK/paint-parts.txt" 2>&1
		painted=$(grep -c 'UVs on [1-9][0-9]* of' "$WORK/paint-parts.txt")
		if [ "$painted" -ge 3 ]; then
			pass=$((pass+1))
			echo "  ok    $painted model parts were painted separately"
		else
			fail=$((fail+1))
			echo "  FAIL  only $painted model parts were painted, not 3"
		fi
		check "painted model in parts / $(basename "$paint_design")" "$paint_design"
	fi

	# Every shape on offer has to give a blueprint that the game accepts.
	for preset in as-is spire obelisk column flared spiral spiral-cluster cathedral ring tower rough wide; do
		out="$WORK/models/preset-$preset"
		mkdir -p "$out"
		"$SHIPGEN" generate-building \
			--model "$MODELS/aetherite_bottom.stl" \
			--model "$MODELS/aetherite_ring.stl" \
			--model "$MODELS/aetherite_top.stl" \
			--fit 397 --preset "$preset" --brief "$WORK/capitol.json" --out "$out" > "$WORK/log.txt" 2>&1
		if ! grep -q "saved as" "$WORK/log.txt"; then
			fail=$((fail+1))
			echo "  FAIL  the shape '$preset' did not make a blueprint"
			continue
		fi
		faces=$(grep -o "faces           : [0-9]*" "$WORK/log.txt" | head -1 | grep -o "[0-9]*$")
		if [ -n "$faces" ] && [ "$faces" -gt 15000 ]; then
			fail=$((fail+1))
			echo "  FAIL  the shape '$preset' has $faces faces, which is over the limit of 15000"
			continue
		fi
		for design in "$out"/*.SoH; do
			[ -e "$design" ] || continue
			check "the shape $preset / $(basename "$design")" "$design"
		done
	done

	# A set of buildings from the same model.
	cat > "$WORK/set.json" << SETJSON
{
    "name": "Aetherite Buildings",
    "fit": 397,
    "faces": 5000,
    "model": [
        "$MODELS/aetherite_bottom.stl",
        "$MODELS/aetherite_ring.stl",
        "$MODELS/aetherite_top.stl"
    ],
    "buildings": [
        { "template": "capitol", "shape": "cathedral", "name": "Crystal Capitol" },
        { "template": "arena", "shape": "spiral", "name": "Spiral Arena" },
        { "template": "office_block", "shape": "tower", "name": "Crystal Tower", "variations": 2 }
    ]
}
SETJSON
	out="$WORK/models/set"
	mkdir -p "$out"
	"$SHIPGEN" generate-set --set "$WORK/set.json" --out "$out" > "$WORK/log.txt" 2>&1
	count=$(ls "$out"/*.SoH 2>/dev/null | wc -l)
	if [ "$count" != "4" ]; then
		fail=$((fail+1))
		echo "  FAIL  the set gave $count blueprints, not 4"
	else
		pass=$((pass+1))
		echo "  ok    the set gave 4 blueprints"
	fi
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "a building from the set / $(basename "$design")" "$design"
	done

	# A set of variations of the same look.
	out="$WORK/models/variations"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--model "$MODELS/aetherite_bottom.stl" \
		--model "$MODELS/aetherite_ring.stl" \
		--model "$MODELS/aetherite_top.stl" \
		--fit 397 --preset cathedral --vary 3 --brief "$WORK/capitol.json" --out "$out" > "$WORK/log.txt" 2>&1
	count=$(ls "$out"/*.SoH 2>/dev/null | wc -l)
	if [ "$count" != "6" ]; then
		fail=$((fail+1))
		echo "  FAIL  three variations of two layouts gave $count blueprints, not 6"
	else
		pass=$((pass+1))
		echo "  ok    three variations of two layouts gave 6 blueprints"
	fi
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "a variation / $(basename "$design")" "$design"
	done

	# The Moogle Warrior is an arena.
	"$SHIPGEN" building-brief arena "$WORK/arena.json" > /dev/null
	out="$WORK/models/arena"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--model "$MODELS/moogle_warrior.stl" \
		--fit 200 --brief "$WORK/arena.json" --out "$out" > "$WORK/log.txt" 2>&1
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "arena from the Moogle Warrior / $(basename "$design")" "$design"
	done

	# Two models on a primitive base: each file is its own object, put in a
	# grid that follows the base.
	out="$WORK/models/base_grid"
	mkdir -p "$out"
	"$SHIPGEN" generate-building \
		--base rectangle --base-size 260x180 --base-height 6 \
		--model "$MODELS/graha.stl" --model "$MODELS/mithra.stl" \
		--fit 90 --faces 4000 \
		--brief "$WORK/office_block.json" --out "$out" --layout grid > "$WORK/log.txt" 2>&1
	if [ -e "$out"/*.SoH ]; then
		for design in "$out"/*.SoH; do
			[ -e "$design" ] || continue
			check "two models on a base / $(basename "$design")" "$design"
		done
	else
		fail=$((fail+1))
		echo "  FAIL  two models on a base made no blueprint"
	fi

	# A miniature used as a ship. At 80 feet the hull is only 13 feet wide
	# inside, which is too small for a room and a door, so the miniature is
	# scaled to 160 feet. That still exercises the same path.
	"$SHIPGEN" brief shuttle "$WORK/shuttle.json" > /dev/null
	out="$WORK/models/ship"
	mkdir -p "$out"
	"$SHIPGEN" generate \
		--model "$MODELS/mithra.stl" --fit 160 \
		--brief "$WORK/shuttle.json" --out "$out" > "$WORK/log.txt" 2>&1
	for design in "$out"/*.SoH; do
		[ -e "$design" ] || continue
		check "ship from the Mithra / $(basename "$design")" "$design"
	done
else
	echo "  (the models have not been taken out of the zip files."
	echo "   Run testdata/extract_models.sh to get them.)"
fi

echo
echo "== The mesh tool =="
if [ -x "$SHIPGEN_MESH" ]; then
	# The tool takes a model down to a number of faces and keeps the shape.
	if [ -e "$MODELS/aetherite_top.stl" ]; then
		out="$WORK/mesh"
		mkdir -p "$out"
		"$SHIPGEN" reduce --model "$MODELS/aetherite_top.stl" --faces 2000 \
			--out "$out/reduced.stl" > "$WORK/log.txt" 2>&1
		if [ -e "$out/reduced.stl" ]; then
			pass=$((pass+1))
			echo "  ok    the model was reduced to 2000 faces"
			# The reduced model has to be readable again.
			faces=$("$SHIPGEN" reduce --model "$out/reduced.stl" --faces 100000 \
				--out "$out/again.stl" 2>&1 | grep -o "kept [0-9]*" | grep -o "[0-9]*")
			if [ -n "$faces" ] && [ "$faces" -ge 1900 ] && [ "$faces" -le 2100 ]; then
				pass=$((pass+1))
				echo "  ok    the reduced model reads back with $faces faces"
			else
				fail=$((fail+1))
				echo "  FAIL  the reduced model reads back with $faces faces, not about 2000"
			fi
		else
			fail=$((fail+1))
			echo "  FAIL  the model was not reduced"
			head -5 "$WORK/log.txt"
		fi
	fi
	# The tool has to be able to say what is wrong with a model.
	if [ -e "$MODELS/aetherite_top.stl" ]; then
		if "$SHIPGEN" mesh-info --model "$MODELS/aetherite_top.stl" > "$WORK/info.txt" 2>&1; then
			if grep -q "triangles" "$WORK/info.txt" && grep -q "windings" "$WORK/info.txt"; then
				pass=$((pass+1))
				echo "  ok    the model report works"
			else
				fail=$((fail+1))
				echo "  FAIL  the model report did not say much"
				head -5 "$WORK/info.txt"
			fi
		else
			fail=$((fail+1))
			echo "  FAIL  the model report did not run"
			head -5 "$WORK/info.txt"
		fi
	fi
	# A small window has to work, which means the panel has to scroll.
	if QT_QPA_PLATFORM=offscreen timeout 30 "$SHIPGEN_MESH" --size 760x480 \
			--screenshot "$WORK/mesh.png" > "$WORK/mesh.log" 2>&1; then
		pass=$((pass+1))
		echo "  ok    the mesh tool starts"
	else
		fail=$((fail+1))
		echo "  FAIL  the mesh tool did not start"
		head -5 "$WORK/mesh.log"
	fi
else
	echo "  (the mesh tool was not built)"
fi

echo "== Files from a newer game build =="
# The White Star was saved by a game build that writes a part type (90) the
# engine source does not know. The tolerant loader (src/sohfile.h) must read
# every other part and write the unknown one back unchanged.
WHITESTAR="$DESIGNS/whitestar-notele.SoH"
if [ -e "$WHITESTAR" ]; then
	"$SHIPGEN" inspect "$WHITESTAR" > "$WORK/whitestar.txt" 2>&1
	if grep -q '^Parts        : 88$' "$WORK/whitestar.txt" && grep -q 'type 90' "$WORK/whitestar.txt"; then
		pass=$((pass+1))
		echo "  ok    the White Star reads in full (88 parts, one of a newer type kept unread)"
	else
		fail=$((fail+1))
		echo "  FAIL  the White Star did not read in full"
		head -8 "$WORK/whitestar.txt"
	fi
	if "$SHIPGEN" resave "$WHITESTAR" "$WORK/whitestar-resaved.SoH" > /dev/null 2>&1 &&
			"$SHIPGEN" inspect "$WORK/whitestar-resaved.SoH" 2>&1 | grep -q '^Parts        : 88$'; then
		pass=$((pass+1))
		echo "  ok    the White Star survives being saved again"
	else
		fail=$((fail+1))
		echo "  FAIL  the White Star lost parts when it was saved again"
	fi
else
	echo "  (no $WHITESTAR)"
fi

echo
echo "== The triangle designer =="
# A plan drawn with triangles (src/triplan.h) becomes a blueprint
# (src/planbuild.h). The example has a hull over two decks, four rooms,
# interior doors, a hull door, a ladder and paint.
mkdir -p "$WORK/plan"
if "$SHIPGEN" plan-example "$WORK/plan/example.json" > /dev/null 2>&1 &&
		"$SHIPGEN" build-plan "$WORK/plan/example.json" --out "$WORK/plan/example.SoH" > "$WORK/plan/build.txt" 2>&1; then
	pass=$((pass+1))
	echo "  ok    the example plan builds"
	check "the example plan" "$WORK/plan/example.SoH"
	check_interior "the example plan" "$WORK/plan/example.SoH"
	check_audit "the example plan" "$WORK/plan/example.SoH"
	# The same plan builds to the same file, every time, from the command line
	# or the designer. A window or door in the plan makes the engine's cuts
	# matter, which is where the order of its parts shows.
	"$SHIPGEN" build-plan "$WORK/plan/example.json" --out "$WORK/plan/again.SoH" > /dev/null 2>&1
	designerSame=yes
	if [ -x "$(dirname "$SHIPGEN")/shipgen-designer" ]; then
		QT_QPA_PLATFORM=offscreen "$(dirname "$SHIPGEN")/shipgen-designer" --open "$WORK/plan/example.json" --export "$WORK/plan/again-designer.SoH" > /dev/null 2>&1
		cmp -s "$WORK/plan/example.SoH" "$WORK/plan/again-designer.SoH" || designerSame=no
	fi
	if cmp -s "$WORK/plan/example.SoH" "$WORK/plan/again.SoH" && [ "$designerSame" = yes ]; then
		pass=$((pass+1))
		echo "  ok    the same plan builds to the same file, from the command line and the designer"
	else
		fail=$((fail+1))
		echo "  FAIL  the same plan built to different files (designer the same: $designerSame)"
	fi
	if grep -q '^Ladders      : 1$' "$WORK/plan/build.txt" && grep -q '^Doors        : 3$' "$WORK/plan/build.txt"; then
		pass=$((pass+1))
		echo "  ok    the example plan has its doors and its ladder"
	else
		fail=$((fail+1))
		echo "  FAIL  the example plan lost doors or the ladder"
		sed -n 1,12p "$WORK/plan/build.txt"
	fi
	if grep -q '^Windows      : 29$' "$WORK/plan/build.txt"; then
		pass=$((pass+1))
		echo "  ok    the example plan has its 29 windows (8 at the sides of the bridge, 20 round the bow, 1 in the hall)"
	else
		fail=$((fail+1))
		echo "  FAIL  the example plan lost windows"
		grep '^Windows' "$WORK/plan/build.txt"
	fi
	# Turned around, the example keeps all it has; turned twice, it is the
	# same plan (TriPlan::Flip). Mirrored and upside down twice too.
	if "$SHIPGEN" plan-flip "$WORK/plan/example.json" "$WORK/plan/turned.json" --turn > /dev/null 2>&1 &&
			"$SHIPGEN" build-plan "$WORK/plan/turned.json" --out "$WORK/plan/turned.SoH" > "$WORK/plan/turned.txt" 2>&1 &&
			diff <(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/build.txt") \
				<(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/turned.txt") > /dev/null; then
		pass=$((pass+1))
		echo "  ok    the example plan turned around keeps its rooms, doors, windows, ladder and stations"
	else
		fail=$((fail+1))
		echo "  FAIL  the example plan turned around lost something"
		diff <(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/build.txt") \
			<(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/turned.txt")
	fi
	twice=ok
	for how in --turn --mirror --fore-aft --upside-down; do
		"$SHIPGEN" plan-flip "$WORK/plan/example.json" "$WORK/plan/once.json" $how > /dev/null 2>&1
		"$SHIPGEN" plan-flip "$WORK/plan/once.json" "$WORK/plan/twice.json" $how > /dev/null 2>&1
		cmp -s "$WORK/plan/example.json" "$WORK/plan/twice.json" || twice="not the same after $how twice"
	done
	# A zero-G shaft over both decks (written with its old name, "Gravity
	# Well"): the two rooms and the structure between them are zero-G, and
	# the floor between them is opened.
	python3 - "$WORK/plan/example.json" "$WORK/plan/zerog.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); W = d['width']
def dec(s):
    out = []
    for p in s.split(','):
        if p:
            n, v = p.split('*'); out += [int(v)] * int(n)
    return out
def enc(a):
    o = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        o.append("%d*%d" % (j - i, a[i])); i = j
    return ','.join(o)
d['rooms'].append({'colour': '#40e0d0', 'label': 'Drop Shaft 1', 'type': 'Gravity Well'})
ri = len(d['rooms']) - 1
for L in d['layers']:
    r = dec(L['room']); h = dec(L['hull'])
    for cy in range(6, 9):
        for cx in range(20, 23):
            for k in range(4):
                i = (cy * W + cx) * 4 + k
                if h[i]: r[i] = ri
    L['room'] = enc(r)
d['ladders'] = []
json.dump(d, open(sys.argv[2], 'w'))
PY
	if "$SHIPGEN" build-plan "$WORK/plan/zerog.json" --out "$WORK/plan/zerog.SoH" > "$WORK/plan/zerog.txt" 2>&1 &&
			grep -q '^Zero-G joins : 1$' "$WORK/plan/zerog.txt" &&
			[ "$("$SHIPGEN" parts "$WORK/plan/zerog.SoH" 2>&1 | grep -c 'Room Void .*Drop Shaft.*zero-G')" = 3 ]; then
		pass=$((pass+1))
		echo "  ok    zero-G shafts on two decks are one zero-G space"
		check "the zero-G shaft plan" "$WORK/plan/zerog.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  zero-G shafts on two decks were not joined"
		grep -E '^(Zero-G|Rooms)' "$WORK/plan/zerog.txt"
	fi

	# An L-shaped "Open to Deck Below" room over a room on the deck below: the
	# two are joined by a void with gravity. A second one has nothing under it
	# and is told so.
	python3 - "$WORK/plan/example.json" "$WORK/plan/open.json" "$WORK/plan/open-none.json" <<'PY'
import json, sys
def dec(s):
    out = []
    for p in s.split(','):
        if p:
            n, v = p.split('*'); out += [int(v)] * int(n)
    return out
def enc(a):
    o = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        o.append("%d*%d" % (j - i, a[i])); i = j
    return ','.join(o)
def make(out, lowerToo, x0):
    d = json.load(open(sys.argv[1])); W = d['width']
    d['rooms'].append({'colour': '#c080ff', 'label': 'Atrium', 'type': 'Open to Deck Below'})
    up = len(d['rooms']) - 1
    if lowerToo:
        d['rooms'].append({'colour': '#40c0a0', 'label': 'Atrium Floor', 'type': 'Lounge'})
    lo = len(d['rooms']) - 1
    cells = [(x, y) for y in range(6, 9) for x in range(x0, x0 + 4)] + [(x, y) for y in range(9, 11) for x in range(x0, x0 + 2)]
    for di, L in enumerate(d['layers']):
        if di == 0 and not lowerToo:
            continue
        r = dec(L['room']); h = dec(L['hull'])
        for cx, cy in cells:
            for k in range(4):
                i = (cy * W + cx) * 4 + k
                if h[i]: r[i] = lo if di == 0 else up
        L['room'] = enc(r)
    d['ladders'] = []
    json.dump(d, open(out, 'w'))
make(sys.argv[2], True, 20)
make(sys.argv[3], False, 26)
PY
	if "$SHIPGEN" build-plan "$WORK/plan/open.json" --out "$WORK/plan/open.SoH" > "$WORK/plan/open.txt" 2>&1 &&
			grep -q '^Open to below: 1$' "$WORK/plan/open.txt" &&
			[ "$("$SHIPGEN" parts "$WORK/plan/open.SoH" 2>&1 | grep -c 'Room Void .*Atrium Floor to Atrium')" = 1 ] &&
			[ "$("$SHIPGEN" parts "$WORK/plan/open.SoH" 2>&1 | grep -c 'Room Void .*Atrium.*zero-G')" = 0 ]; then
		pass=$((pass+1))
		echo "  ok    an open-to-below room joins the room under it, with gravity"
		check "the open-to-below plan" "$WORK/plan/open.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  an open-to-below room was not joined to the room under it"
		grep -E '^(Open|Rooms)' "$WORK/plan/open.txt"
	fi
	if "$SHIPGEN" build-plan "$WORK/plan/open-none.json" --out "$WORK/plan/open-none.SoH" > "$WORK/plan/open-none.txt" 2>&1 &&
			grep -q "has no room under it" "$WORK/plan/open-none.txt" &&
			grep -q '^Open to below: 0$' "$WORK/plan/open-none.txt"; then
		pass=$((pass+1))
		echo "  ok    an open-to-below room with nothing under it keeps its floor and says so"
	else
		fail=$((fail+1))
		echo "  FAIL  an open-to-below room with nothing under it was not reported"
		grep -E '^(Open|Warn)|under it' "$WORK/plan/open-none.txt"
	fi

	# A shaft's walls stay whole where it opens into the deck above, and the
	# crew can walk the whole ship.
	gapLine="$("$SHIPGEN" parts "$WORK/plan/zerog.SoH" 2>&1 | grep 'Room Void .*Drop Shaft.* to ' | head -1)"
	wallsOk="$(python3 - "$SHIPGEN" "$WORK/plan/zerog.SoH" "$gapLine" <<'PY'
import re, subprocess, sys
m = re.search(r'x (-?[\d.]+)\.\.(-?[\d.]+)\s+y (-?[\d.]+)\.\.(-?[\d.]+)\s+z (-?[\d.]+)\.\.(-?[\d.]+)', sys.argv[3])
x0, x1, y0, y1, z0, z1 = map(float, m.groups())
ok = True
for z in (z0 + .5, z1 - .5):
    for d, half in (((1, 0, 0), (x1 - x0) / 2), ((-1, 0, 0), (x1 - x0) / 2), ((0, 1, 0), (y1 - y0) / 2), ((0, -1, 0), (y1 - y0) / 2)):
        r = subprocess.run([sys.argv[1], 'sight', sys.argv[2], str((x0 + x1) / 2 + .013), str((y0 + y1) / 2 + .017), str(z)] + [str(v) for v in d],
                           capture_output=True, text=True).stdout.strip()
        if not r.endswith(']') or float(r.split()[0]) > half + .5:
            ok = False
print('yes' if ok else 'no')
PY
)"
	if [ "$wallsOk" = yes ] && ! grep -q 'cannot walk between' "$WORK/plan/open.txt" "$WORK/plan/build.txt"; then
		pass=$((pass+1))
		echo "  ok    a shaft's walls are whole where it joins the deck above; the crew can walk the ship"
	else
		fail=$((fail+1))
		echo "  FAIL  a hole in a shaft's wall ($wallsOk), or rooms the crew cannot walk to"
		echo "$gapLine"; grep -h 'cannot walk' "$WORK/plan/open.txt" "$WORK/plan/build.txt"
	fi

	# Door rows: named from either cell, neighbours still join; a row number
	# keeps them apart. Textures per room and door. A zigzag wall's windows are
	# one flat pane with a clear view, or one per tooth when asked.
	python3 - "$WORK/plan/example.json" "$WORK/plan" <<'PY'
import json, sys
def dec(s):
    out = []
    for p in s.split(','):
        if p:
            n, v = p.split('*'); out += [int(v)] * int(n)
    return out
def enc(a):
    out, i = [], 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]:
            j += 1
        out.append('%d*%d' % (j - i, a[i]))
        i = j
    return ','.join(out)
d = json.load(open(sys.argv[1])); W = d['width']; D = d['depth']; out = sys.argv[2]
rooms = [dec(L['room']) for L in d['layers']]
cell = lambda L, x, y: [rooms[L][(y * W + x) * 4 + k] for k in range(4)]
def find(n):
    for L in range(len(rooms)):
        for x in range(W - 1):
            for y in range(D - n):
                here = [cell(L, x, y + i) for i in range(n)]
                if len({r for c in here for r in c}) == 1 and here[0][0] >= 0 and all(max(cell(L, x + 1, y + i)) < 0 for i in range(n)):
                    return L, x, y
    raise SystemExit('no wall')
L, x, y = find(2)
base = list(d['doors'])
d['doors'] = base + [{'deck': L, 'x': x, 'y': y, 'side': 1}, {'deck': L, 'x': x + 1, 'y': y + 1, 'side': 3}]
json.dump(d, open(out + '/row.json', 'w'))
d['doors'] = base + [{'deck': L, 'x': x, 'y': y, 'side': 1, 'row': 1}, {'deck': L, 'x': x + 1, 'y': y + 1, 'side': 3, 'row': 2}]
json.dump(d, open(out + '/apart.json', 'w'))
d['doors'] = base + [{'deck': L, 'x': x, 'y': y, 'side': 1, 'texture': 111}]
d['rooms'][0].update({'wallTexture': 33, 'floorTexture': 58, 'ceilingTexture': 53})
json.dump(d, open(out + '/textures.json', 'w'))
d = json.load(open(sys.argv[1]))
# The port wall of the example's bridge, close to the hull, teeth cut in it.
L, y = 1, 13
for x in (21, 23):
    rooms[L][(y * W + x) * 4 + 2] = -1
d['layers'][L]['room'] = enc(rooms[L])
d['windows'] = [q for q in d['windows'] if not (q['deck'] == L and q['y'] == y and 20 <= q['x'] <= 23)]
for x in (20, 22):
    d['windows'] += [{'deck': L, 'x': x, 'y': y, 'side': 2}]
for x in (21, 23):
    d['windows'] += [{'deck': L, 'x': x, 'y': y, 'side': 1, 'edge': 1}, {'deck': L, 'x': x, 'y': y, 'side': 3, 'edge': 2}]
d['joinWindows'] = True
json.dump(d, open(out + '/zigzag.json', 'w'))
d['flatWindows'] = False
json.dump(d, open(out + '/zigzag-teeth.json', 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/example.json" --out "$WORK/plan/ex.SoH" > "$WORK/plan/ex.txt" 2>&1
	baseDoors="$(sed -n 's/^Doors *: //p' "$WORK/plan/ex.txt")"
	for name in row apart textures zigzag zigzag-teeth; do
		"$SHIPGEN" build-plan "$WORK/plan/$name.json" --out "$WORK/plan/$name.SoH" > "$WORK/plan/$name.txt" 2>&1
	done
	if [ "$(sed -n 's/^Doors *: //p' "$WORK/plan/row.txt")" = "$((baseDoors + 1))" ] &&
			[ "$(sed -n 's/^Doors *: //p' "$WORK/plan/apart.txt")" = "$((baseDoors + 2))" ]; then
		pass=$((pass+1))
		echo "  ok    neighbouring doors are one door from whichever cell they are named; a row number keeps them apart"
	else
		fail=$((fail+1))
		echo "  FAIL  door rows: $baseDoors, then $(grep '^Doors' "$WORK/plan/row.txt"), $(grep '^Doors' "$WORK/plan/apart.txt")"
	fi
	texFaces="$("$SHIPGEN" faces "$WORK/plan/textures.SoH" 2>&1)"
	if echo "$texFaces" | grep -q 'library texture 33' && echo "$texFaces" | grep -q 'library texture 58' &&
			echo "$texFaces" | grep -q 'library texture 111'; then
		pass=$((pass+1))
		echo "  ok    rooms and doors can have textures of their own from the game's library"
	else
		fail=$((fail+1))
		echo "  FAIL  room and door textures"
		echo "$texFaces" | grep 'library texture' | head -20
	fi
	zigParts="$("$SHIPGEN" parts "$WORK/plan/zigzag.SoH" 2>&1 | grep ' Window ' | grep ' 4 tris')"
	flatCount="$(sed -n 's/^Windows *: //p' "$WORK/plan/zigzag.txt")"
	teethParts="$(sed -n 's/^Windows *: //p' "$WORK/plan/zigzag-teeth.txt")"
	zigView="$(python3 - "$SHIPGEN" "$WORK/plan/zigzag.SoH" "$zigParts" <<'PY'
import re, subprocess, sys
m = re.search(r'x (-?[\d.]+)\.\.(-?[\d.]+)\s+y (-?[\d.]+)\.\.(-?[\d.]+)\s+z (-?[\d.]+)\.\.(-?[\d.]+)', sys.argv[3])
x0, x1, y0, y1, z0, z1 = map(float, m.groups())
# The pane is thin one way, wide the other. From the room's side, at both ends
# and the middle, the pane is the first thing seen: no frame in the way.
ok = True
for f in (.2, .5, .8):
    hits = []
    for s in (1, -1):
        if x1 - x0 < y1 - y0:
            eye = [(x0 + x1) / 2 + s * 4, y0 + (y1 - y0) * f, (z0 + z1) / 2]; d = [-s, 0, 0]
        else:
            eye = [x0 + (x1 - x0) * f, (y0 + y1) / 2 + s * 4, (z0 + z1) / 2]; d = [0, -s, 0]
        hits.append(subprocess.run([sys.argv[1], 'sight', sys.argv[2]] + [str(v + .013) for v in eye] + [str(v) for v in d], capture_output=True, text=True).stdout)
    if not any('Window' in h for h in hits):
        ok = False
print('yes' if ok else 'no')
PY
)"
	if [ "$(echo "$zigParts" | wc -l)" = 1 ] && [ "$teethParts" -gt "$flatCount" ] && [ "$zigView" = yes ] &&
			! grep -q 'could not be cut' "$WORK/plan/zigzag.txt"; then
		pass=$((pass+1))
		echo "  ok    windows along a zigzag wall are one flat pane with a clear view, or one per tooth when asked"
		check "the zigzag window plan" "$WORK/plan/zigzag.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  the flat window: panes '$zigParts', $flatCount windows, per tooth $teethParts, view $zigView"
		grep -E 'indow' "$WORK/plan/zigzag.txt" | head -5
	fi

	# People can get about: routes followed the way the game's crew follow them,
	# doorways keep the rooms' gravity, screens read the right way round. And the
	# space out to a flat pane can be walked into.
	crewReport="$("$SHIPGEN" crew-check "$WORK/plan/ex.SoH" 2>&1)"
	crewRoutes="$(echo "$crewReport" | sed -n 's/^Crew routes *: \([0-9]*\) of \([0-9]*\).*/\1 \2/p')"
	set -- $crewRoutes
	if [ "${2:-0}" -gt 0 ] && [ "$1" = "$2" ] && echo "$crewReport" | grep -q '^Natural doorways: 0$' &&
			echo "$crewReport" | grep -q '^Mirrored screens: 0$'; then
		pass=$((pass+1))
		echo "  ok    the crew can follow every route; doorways keep the rooms' gravity; screens are not mirrored"
	else
		fail=$((fail+1))
		echo "  FAIL  crew check"
		echo "$crewReport" | head -12
	fi
	zigVoids="$("$SHIPGEN" parts "$WORK/plan/zigzag.SoH" 2>&1 | grep -c 'Room Void')"
	teethVoids="$("$SHIPGEN" parts "$WORK/plan/zigzag-teeth.SoH" 2>&1 | grep -c 'Room Void')"
	if [ "$zigVoids" = "$((teethVoids + 1))" ]; then
		pass=$((pass+1))
		echo "  ok    the space out to a flat window is open, not a wall nobody can see"
	else
		fail=$((fail+1))
		echo "  FAIL  voids with a flat window: $zigVoids, per tooth: $teethVoids"
	fi

	# Ceiling windows: two neighbouring cells of the top deck's room, which join
	# into one pane when asked; a third under a room is dropped on opening.
	# Desks: the same plan with integrated stations.
	python3 - "$WORK/plan/example.json" "$WORK/plan" <<'PY'
import json, sys
def dec(s):
    out = []
    for p in s.split(','):
        if p:
            n, v = p.split('*'); out += [int(v)] * int(n)
    return out
d = json.load(open(sys.argv[1])); W = d['width']; out = sys.argv[2]
decks = [dec(L['room']) for L in d['layers']]
whole = lambda deck, x, y: all(decks[deck][(y * W + x) * 4 + k] >= 0 for k in range(4))
pair = next((x, y) for y in range(len(decks[-1]) // W // 4) for x in range(W - 1) if whole(-1, x, y) and whole(-1, x + 1, y))
below = next((x, y) for y in range(len(decks[0]) // W // 4) for x in range(W) if whole(0, x, y) and whole(1, x, y))
top = len(decks) - 1
sky = [{'deck': top, 'x': pair[0], 'y': pair[1]}, {'deck': top, 'x': pair[0] + 1, 'y': pair[1]}]
d['skylights'] = sky
json.dump(d, open(out + '/sky.json', 'w'))
d['joinWindows'] = True
json.dump(d, open(out + '/sky-joined.json', 'w'))
d['joinWindows'] = False
d['skylights'] = sky + [{'deck': 0, 'x': below[0], 'y': below[1]}]
json.dump(d, open(out + '/sky-under.json', 'w'))
del d['skylights']
d['bridgeStyle'] = 'desks'
json.dump(d, open(out + '/desks.json', 'w'))
d['deskHeight'] = 3.5
json.dump(d, open(out + '/desks-high.json', 'w'))
del d['deskHeight']
d['deskStanding'] = True
json.dump(d, open(out + '/desks-standing.json', 'w'))
del d['deskStanding']
d['leaveOut'] = [k for k in d.get('leaveOut', []) if k != 'firecontrol']
d['fireControlModule'] = 261
json.dump(d, open(out + '/fire-ballistic.json', 'w'))
d['fireControlModule'] = 1
json.dump(d, open(out + '/fire-bad.json', 'w'))
del d['fireControlModule']
json.dump(d, open(out + '/fire-default.json', 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/sky.json" --out "$WORK/plan/sky.SoH" > "$WORK/plan/sky.txt" 2>&1
	"$SHIPGEN" build-plan "$WORK/plan/sky-joined.json" --out "$WORK/plan/sky-joined.SoH" > "$WORK/plan/sky-joined.txt" 2>&1
	"$SHIPGEN" build-plan "$WORK/plan/sky-under.json" --out "$WORK/plan/sky-under.SoH" > "$WORK/plan/sky-under.txt" 2>&1
	if grep -q '^Ceiling wind.: 2$' "$WORK/plan/sky.txt" &&
			grep -q '^Ceiling wind.: 1$' "$WORK/plan/sky-joined.txt" &&
			grep -q '^Ceiling wind.: 2$' "$WORK/plan/sky-under.txt"; then
		pass=$((pass+1))
		echo "  ok    ceiling windows are cut, join, and are refused under a room"
		check "the ceiling window plan" "$WORK/plan/sky.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  ceiling windows"
		grep -E 'Ceiling|eiling window' "$WORK/plan/sky.txt" "$WORK/plan/sky-joined.txt" "$WORK/plan/sky-under.txt"
	fi
	"$SHIPGEN" plan-flip "$WORK/plan/sky.json" "$WORK/plan/once.json" --upside-down > /dev/null 2>&1
	"$SHIPGEN" plan-flip "$WORK/plan/once.json" "$WORK/plan/twice.json" --upside-down > /dev/null 2>&1
	if grep -q '"floor"' "$WORK/plan/once.json" && ! grep -q '"floor"' "$WORK/plan/twice.json" && grep -q '"skylights"' "$WORK/plan/twice.json"; then
		pass=$((pass+1))
		echo "  ok    a ceiling window turned upside down is a floor window, and back"
	else
		fail=$((fail+1))
		echo "  FAIL  a ceiling window did not survive being turned upside down"
	fi
	"$SHIPGEN" build-plan "$WORK/plan/desks.json" --out "$WORK/plan/desks.SoH" > "$WORK/plan/desks.txt" 2>&1
	# The fire control station runs the module chosen; anything else is refused.
	for name in fire-ballistic fire-bad fire-default; do
		"$SHIPGEN" build-plan "$WORK/plan/$name.json" --out "$WORK/plan/$name.SoH" > "$WORK/plan/$name.txt" 2>&1
	done
	fireModule() { "$SHIPGEN" parts "$1" 2>&1 | grep 'Fire Control' | sed -n 's/.*module \([^ ]*\).*/\1/p' | head -1; }
	if [ "$(fireModule "$WORK/plan/fire-ballistic.SoH")" = "Unknown:261" ] && [ "$(fireModule "$WORK/plan/fire-default.SoH")" = "Unknown:252" ] &&
			[ "$(fireModule "$WORK/plan/fire-bad.SoH")" = "Unknown:252" ] && grep -q 'Fire control module 1 is not' "$WORK/plan/fire-bad.txt"; then
		pass=$((pass+1))
		echo "  ok    the fire control station runs the module chosen"
		check "the ballistic fire control plan" "$WORK/plan/fire-ballistic.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  fire control modules: $(fireModule "$WORK/plan/fire-ballistic.SoH"), $(fireModule "$WORK/plan/fire-default.SoH"), $(fireModule "$WORK/plan/fire-bad.SoH")"
		grep -E 'Fire control|fire control' "$WORK/plan/fire-bad.txt" "$WORK/plan/fire-ballistic.txt"
	fi

	# Desks at another height, and for standing: the screen goes with the desk, a
	# standing desk has no stool.
	"$SHIPGEN" build-plan "$WORK/plan/desks-high.json" --out "$WORK/plan/desks-high.SoH" > "$WORK/plan/desks-high.txt" 2>&1
	"$SHIPGEN" build-plan "$WORK/plan/desks-standing.json" --out "$WORK/plan/desks-standing.SoH" > "$WORK/plan/desks-standing.txt" 2>&1
	helmTop() { "$SHIPGEN" parts "$1" 2>&1 | sed -n 's/.*Helm Station .* z [0-9.-]*\.\.\([0-9.-]*\).*/\1/p' | head -1; }
	stoolFaces() { "$SHIPGEN" parts "$1" 2>&1 | grep -E 'Mesh  \[' | sed -E 's/.*\] +([0-9]+) tris.*/\1/' | awk '{ n += $1 } END { print n + 0 }'; }
	topSit="$(helmTop "$WORK/plan/desks.SoH")"; topHigh="$(helmTop "$WORK/plan/desks-high.SoH")"; topStand="$(helmTop "$WORK/plan/desks-standing.SoH")"
	if [ -n "$topSit" ] && python3 -c "import sys; a,b,c=map(float,sys.argv[1:]); sys.exit(0 if abs(b-a-.5)<.11 and abs(c-a-1.2)<.11 else 1)" "$topSit" "$topHigh" "$topStand" &&
			[ "$(stoolFaces "$WORK/plan/desks-standing.SoH")" -lt "$(stoolFaces "$WORK/plan/desks.SoH")" ] &&
			"$SHIPGEN" crew-check "$WORK/plan/desks-standing.SoH" | grep -q '^Mirrored screens: 0$'; then
		pass=$((pass+1))
		echo "  ok    desks can be higher, or for standing with no stool; the screen goes with the desk"
		check "the standing desk plan" "$WORK/plan/desks-standing.SoH"
		check "the high desk plan" "$WORK/plan/desks-high.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  desk height and standing: tops $topSit, $topHigh, $topStand; mesh faces $(stoolFaces "$WORK/plan/desks.SoH"), $(stoolFaces "$WORK/plan/desks-standing.SoH")"
	fi
	deskParts="$("$SHIPGEN" parts "$WORK/plan/desks.SoH" 2>&1)"
	chairParts="$("$SHIPGEN" parts "$WORK/plan/example.SoH" 2>&1)"
	if [ "$(echo "$deskParts" | grep -c 'Command Station')" = 1 ] &&
			[ "$(echo "$deskParts" | grep -c 'Helm Station')" = 1 ] &&
			[ "$(echo "$deskParts" | grep -c 'Helm Console')" = 0 ] &&
			[ "$(echo "$chairParts" | grep -c ' Station ')" = 0 ] &&
			[ "$(echo "$chairParts" | grep -c 'Helm Console')" = 1 ]; then
		pass=$((pass+1))
		echo "  ok    the desk style makes integrated stations; the default stays swivel chairs"
		check "the desk bridge plan" "$WORK/plan/desks.SoH"
	else
		fail=$((fail+1))
		echo "  FAIL  the desk style"
		grep -E 'Stations|desk' "$WORK/plan/desks.txt"
	fi

	if [ "$twice" = ok ]; then
		pass=$((pass+1))
		echo "  ok    a plan turned, mirrored or turned upside down twice is the same plan"
	else
		fail=$((fail+1))
		echo "  FAIL  $twice"
	fi
	# An empty deck under the ship and a diagonal corner on the top deck
	# (testdata/hull_edge_cases.py): the design is moved so its origin is in
	# the hull (else "CG outside design extents", and the game hung on it),
	# the corner is filled in, and the engine takes it.
	if command -v python3 > /dev/null 2>&1; then
		if python3 "$HERE/testdata/hull_edge_cases.py" "$WORK/plan/example.json" "$WORK/plan/edges.json" > /dev/null 2>&1 &&
				"$SHIPGEN" build-plan "$WORK/plan/edges.json" --out "$WORK/plan/edges.SoH" > "$WORK/plan/edges.txt" 2>&1 &&
				grep -q 'was moved .* origin is inside the hull' "$WORK/plan/edges.txt" &&
				grep -q 'triangle(s) were added to the hull' "$WORK/plan/edges.txt" &&
				grep -q '^Errors (0)' "$WORK/plan/edges.txt"; then
			pass=$((pass+1))
			echo "  ok    a plan with an empty bottom deck and a diagonal corner is moved, mended and accepted"
		else
			fail=$((fail+1))
			echo "  FAIL  a plan with an empty bottom deck and a diagonal corner"
			grep -E 'moved|added|Error' "$WORK/plan/edges.txt" | head -5
		fi
	fi
	# What the game wants (learned from blueprints that work in it): hulls
	# wound with a negative signed volume, rooms positive, fore along +Y,
	# one engineer running the FTL drive.
	"$SHIPGEN" parts "$WORK/plan/example.SoH" > "$WORK/plan/parts.txt" 2>&1
	if grep -E '  Hull  .*signed volume -' "$WORK/plan/parts.txt" > /dev/null &&
			! grep -E '  Room  .*signed volume -' "$WORK/plan/parts.txt" > /dev/null &&
			! grep -E '  Hull  .*signed volume [0-9]' "$WORK/plan/parts.txt" > /dev/null; then
		pass=$((pass+1))
		echo "  ok    the hull and rooms are wound the way the game wants"
	else
		fail=$((fail+1))
		echo "  FAIL  the hull or a room is wound the wrong way"
		grep -E '  (Hull|Room)  ' "$WORK/plan/parts.txt" | head -5
	fi
	if awk '/  Hull  /{ for(i=1;i<=NF;i++){ if($i=="x"){split($(i+1),a,"\\.\\.")} if($i=="y"){split($(i+1),b,"\\.\\.")} } if(b[2]-b[1] > a[2]-a[1]) ok=1 } END{ exit ok?0:1 }' "$WORK/plan/parts.txt"; then
		pass=$((pass+1))
		echo "  ok    the ship is longest fore to aft (+Y)"
	else
		fail=$((fail+1))
		echo "  FAIL  the ship lies sideways"
	fi
	if [ "$(grep -c 'Engineer Console' "$WORK/plan/parts.txt")" = "1" ] && grep -q '^FTL drive: part' "$WORK/plan/parts.txt"; then
		pass=$((pass+1))
		echo "  ok    one engineer station runs the FTL drive"
	else
		fail=$((fail+1))
		echo "  FAIL  the engineer stations are wrong"
		grep -E 'Engineer|FTL' "$WORK/plan/parts.txt"
	fi
	# Consoles are the game's swivel chairs: flat on the floor (one z), like
	# the working designs. Walk paths run at the height of someone standing on
	# the floor (3.25 ft up), and the hull door,
	# 3.4 ft above the ground, has a ramp down to below the hull (a Barrier
	# to walk on and a walk path down it, as at the user's aircraft factory),
	# with only the ladder between the decks (and the level piece from its top to
	# over the floor beside the hole); with --no-ramps it has a
	# ladder instead.
	if awk '/ Console  /{ for(i=1;i<=NF;i++) if($i=="z"){ split($(i+1),c,"\\.\\."); if(c[1]!=c[2]) bad=1 } n++ } END{ exit (n>0 && !bad)?0:1 }' "$WORK/plan/parts.txt"; then
		pass=$((pass+1))
		echo "  ok    the console chairs stand on the floor"
	else
		fail=$((fail+1))
		echo "  FAIL  a console chair is not flat on the floor"
		grep ' Console  ' "$WORK/plan/parts.txt" | head -3
	fi
	if [ "$(grep -c 'Ladder Path' "$WORK/plan/parts.txt")" = "2" ] && grep -q '^Ramps        : 1$' "$WORK/plan/build.txt" &&
			grep -q 'Barrier  \[Hull Door\]' "$WORK/plan/parts.txt" &&
			[ "$(grep -E 'Walk Path .* z [0-9.-]+\.\.' "$WORK/plan/parts.txt" | grep -vcE ' z (3\.8\.\.3\.8|13\.8\.\.13\.8)$')" = "1" ] &&
			grep -E 'Walk Path .* z -2\.4\.\.3\.8$' "$WORK/plan/parts.txt" > /dev/null; then
		pass=$((pass+1))
		echo "  ok    the walk paths run at standing height over the floors and a ramp leads down from the hull door"
	else
		fail=$((fail+1))
		echo "  FAIL  the walk paths are off the floors, or the hull door has no ramp"
		grep -E 'Ladder Path|Walk Path|Barrier' "$WORK/plan/parts.txt" | grep -vE ' z (3\.8\.\.3\.8|13\.8\.\.13\.8)$' | head -5
	fi
	"$SHIPGEN" build-plan "$WORK/plan/example.json" --no-ramps --out "$WORK/plan/noramp.SoH" > "$WORK/plan/noramp.txt" 2>&1
	if grep -q '^Ramps        : 0$' "$WORK/plan/noramp.txt" &&
			[ "$("$SHIPGEN" parts "$WORK/plan/noramp.SoH" | grep -c 'Ladder Path')" = "3" ]; then
		pass=$((pass+1))
		echo "  ok    without ramps, a ladder leads down from the hull door"
	else
		fail=$((fail+1))
		echo "  FAIL  --no-ramps does not give the hull door a ladder"
	fi
	faces=$(sed -n 's/^Shaped hull  : \([0-9]*\) faces.*/\1/p' "$WORK/plan/build.txt")
	if [ -n "$faces" ] && [ "$faces" -le 5000 ]; then
		pass=$((pass+1))
		echo "  ok    the drawn hull keeps to the face budget ($faces faces)"
	else
		fail=$((fail+1))
		echo "  FAIL  the drawn hull is over the face budget ($faces faces)"
	fi
	# The same plan always gives the same hull.
	"$SHIPGEN" build-plan "$WORK/plan/example.json" --out "$WORK/plan/again.SoH" > "$WORK/plan/again.txt" 2>&1
	if [ "$(sed -n 2p "$WORK/plan/build.txt")" = "$(sed -n 2p "$WORK/plan/again.txt")" ]; then
		pass=$((pass+1))
		echo "  ok    the drawn hull comes out the same every time"
	else
		fail=$((fail+1))
		echo "  FAIL  the drawn hull differs between runs"
	fi
	# A plan whose hull is filled in from a model: the test box, 120 ft long
	# (30 x 100 x 25 units, so 36 ft wide and 30 ft high): three decks of a
	# 24 x 8 cell block, nearly every triangle hull.
	if "$SHIPGEN" plan-from-model "$HERE/testdata/testhull.stl" "$WORK/plan/frommodel.json" --length 120 > "$WORK/plan/frommodel.txt" 2>&1 &&
			grep -q '3 decks' "$WORK/plan/frommodel.txt" &&
			hulltris=$(sed -n 's/.*; \([0-9]*\) hull triangles.*/\1/p' "$WORK/plan/frommodel.txt") &&
			[ "$hulltris" -ge 2000 ] && [ "$hulltris" -le 2304 ]; then
		pass=$((pass+1))
		echo "  ok    a plan's hull is filled in from a model ($hulltris triangles)"
	else
		fail=$((fail+1))
		echo "  FAIL  the plan from a model is wrong"
		cat "$WORK/plan/frommodel.txt" | head -5
	fi
	# The importer says how much of the model came through and recommends a
	# length that keeps more: the test box 60 ft long is 15 ft high, a deck
	# and a half, and keeps about 63%; at 90 ft it fills three decks.
	"$SHIPGEN" plan-from-model "$HERE/testdata/testhull.stl" "$WORK/plan/short.json" --length 60 > "$WORK/plan/short.txt" 2>&1
	if grep -q 'holding about 6[0-9]% of the model' "$WORK/plan/short.txt" &&
			grep -q '^Recommended: about 90 ft long holds about' "$WORK/plan/short.txt"; then
		pass=$((pass+1))
		echo "  ok    the importer recommends a length that keeps more of the model"
	else
		fail=$((fail+1))
		echo "  FAIL  the importer's recommendation is wrong"
		grep -E 'holding|Recommended' "$WORK/plan/short.txt"
	fi
	# Slicing: the test box made the way models from the internet are, five
	# separate panels with gaps between them and no top, comes out as full
	# as the closed box; the closed-pieces method keeps next to nothing.
	"$SHIPGEN" plan-from-model "$HERE/testdata/openpanels.obj" "$WORK/plan/open.json" --length 120 > "$WORK/plan/open.txt" 2>&1
	"$SHIPGEN" plan-from-model "$HERE/testdata/openpanels.obj" "$WORK/plan/open-solid.json" --length 120 --method solid > "$WORK/plan/open-solid.txt" 2>&1
	opentris=$(sed -n 's/.*; \([0-9]*\) hull triangles.*/\1/p' "$WORK/plan/open.txt")
	solidtris=$(sed -n 's/.*; \([0-9]*\) hull triangles.*/\1/p' "$WORK/plan/open-solid.txt")
	if [ -n "$opentris" ] && [ "$opentris" -ge 2000 ] && [ "$opentris" -le 2304 ] && [ "${solidtris:-0}" -lt 500 ]; then
		pass=$((pass+1))
		echo "  ok    an open model is filled in slice by slice ($opentris triangles; ${solidtris:-0} from closed pieces only)"
	else
		fail=$((fail+1))
		echo "  FAIL  the open model is not filled in by slicing"
		grep -E 'hull triangles' "$WORK/plan/open.txt" "$WORK/plan/open-solid.txt"
	fi
	# The model as the hull: the open test box's plan remembers the model,
	# and the hull is built from the model's sliced solid, closed.
	"$SHIPGEN" build-plan "$WORK/plan/open.json" --out "$WORK/plan/open-model.SoH" --no-furnish > "$WORK/plan/open-model.txt" 2>&1
	if grep -q '"asHull": true' "$WORK/plan/open.json" && grep -q 'The hull is the model openpanels.obj' "$WORK/plan/open-model.txt" &&
			"$SHIPGEN" inspect "$WORK/plan/open-model.SoH" 2>&1 | grep -q 'closed solid'; then
		pass=$((pass+1))
		echo "  ok    the hull of a plan from a model is the model itself, closed"
	else
		fail=$((fail+1))
		echo "  FAIL  the model is not the hull"
		grep -E '^Hull|model' "$WORK/plan/open-model.txt" | head -5
	fi
	# The model hull's shape is the user's choice: new plans are sharp (the
	# model's own edges and faces), and a plan set to smooth builds too; both
	# closed.
	sed 's/"sharp": true/"sharp": false/' "$WORK/plan/open.json" > "$WORK/plan/open-smooth.json"
	"$SHIPGEN" build-plan "$WORK/plan/open-smooth.json" --out "$WORK/plan/open-smooth.SoH" --no-furnish > "$WORK/plan/open-smooth.txt" 2>&1
	if grep -q '"sharp": true' "$WORK/plan/open.json" && ! grep -q '"sharp": true' "$WORK/plan/open-smooth.json" &&
			"$SHIPGEN" inspect "$WORK/plan/open-smooth.SoH" 2>&1 | grep -q 'closed solid' &&
			! cmp -s "$WORK/plan/open-model.SoH" "$WORK/plan/open-smooth.SoH"; then
		pass=$((pass+1))
		echo "  ok    the model hull can be sharp or smooth, as chosen"
	else
		fail=$((fail+1))
		echo "  FAIL  the model hull's shape is not the chosen one"
		grep -E '^Hull' "$WORK/plan/open-smooth.txt" | head -3
	fi
	# The number of decks (slices) asked for sets the length: the box is a
	# quarter as high as it is long, so 6 decks make it 240 ft long.
	if "$SHIPGEN" plan-from-model "$HERE/testdata/testhull.stl" "$WORK/plan/decks.json" --decks 6 > "$WORK/plan/decks.txt" 2>&1 &&
			grep -q ', 6 decks;' "$WORK/plan/decks.txt" && grep -q 'the model is 240 ft long' "$WORK/plan/decks.txt"; then
		pass=$((pass+1))
		echo "  ok    the decks asked for set the length"
	else
		fail=$((fail+1))
		echo "  FAIL  the decks asked for do not set the length"
		grep -E 'decks' "$WORK/plan/decks.txt"
	fi
	# The hull simplifier: an ellipsoid 300 ft long has 45,528 faces after the
	# flat areas are merged; it comes down to the budget of 5,000, closed,
	# and rooms filling two decks right up to the hull stay inside it.
	if command -v python3 >/dev/null 2>&1; then
		"$SHIPGEN" plan-from-model "$HERE/testdata/ellipsoid.obj" "$WORK/plan/ellipsoid.json" --length 300 > /dev/null 2>&1
		python3 "$HERE/testdata/add_rooms.py" "$WORK/plan/ellipsoid.json" "$WORK/plan/ellipsoid-rooms.json" > /dev/null
		"$SHIPGEN" build-plan "$WORK/plan/ellipsoid-rooms.json" --out "$WORK/plan/ellipsoid.SoH" --no-furnish > "$WORK/plan/ellipsoid.txt" 2>&1
		faces=$(sed -n 's/^Hull *: \([0-9]*\) faces.*/\1/p' "$WORK/plan/ellipsoid.txt")
		if grep -q 'simplified to' "$WORK/plan/ellipsoid.txt" && [ -n "$faces" ] && [ "$faces" -le 5000 ] &&
				"$SHIPGEN" inspect "$WORK/plan/ellipsoid.SoH" 2>&1 | grep -q 'closed solid' &&
				"$SHIPGEN" audit "$WORK/plan/ellipsoid.SoH" > "$WORK/plan/ellipsoid-audit.txt" 2>&1 &&
				grep -q 'Rooms outside the hull: none' "$WORK/plan/ellipsoid-audit.txt"; then
			pass=$((pass+1))
			echo "  ok    a hull over the budget is simplified to $faces faces, closed, with its rooms inside"
		else
			fail=$((fail+1))
			echo "  FAIL  the hull simplifier"
			grep -E '^Hull|simplified|still' "$WORK/plan/ellipsoid.txt"
			grep -E 'outside' "$WORK/plan/ellipsoid-audit.txt" | head -5
		fi
	else
		echo "  skip  the hull simplifier (needs python3)"
	fi
	# A vehicle bay: five door edges side by side on the engine room's
	# starboard wall make one wide bay door, and a ground vehicle parking
	# spot goes on its floor.
	sed -e 's|^    "doors": \[$|    "doors": [ {"deck": 0, "side": 0, "x": 2, "y": 2}, {"deck": 0, "side": 0, "x": 3, "y": 2}, {"deck": 0, "side": 0, "x": 4, "y": 2}, {"deck": 0, "side": 0, "x": 5, "y": 2}, {"deck": 0, "side": 0, "x": 6, "y": 2},|' \
		-e 's|^    "parking": \[$|    "parking": [ {"deck": 0, "x": 4, "y": 8, "kind": "ground", "across": true}|' \
		"$WORK/plan/example.json" > "$WORK/plan/bay.json"
	"$SHIPGEN" build-plan "$WORK/plan/bay.json" --out "$WORK/plan/bay.SoH" > "$WORK/plan/bay.txt" 2>&1
	"$SHIPGEN" parts "$WORK/plan/bay.SoH" > "$WORK/plan/bay-parts.txt" 2>&1
	if grep -q '^Doors        : 4$' "$WORK/plan/bay.txt" && grep -q '^Parking      : 1$' "$WORK/plan/bay.txt" &&
			grep -q 'Errors (0)' "$WORK/plan/bay.txt" &&
			awk '/Hull Void  \[Hull Door\]  tied/{ for(i=1;i<=NF;i++) if($i=="y"){ split($(i+1),b,"\\.\\."); if(b[2]-b[1] > 20) wide=1 } } END{ exit wide?0:1 }' "$WORK/plan/bay-parts.txt"; then
		pass=$((pass+1))
		echo "  ok    neighbouring door edges make one wide bay door, with a vehicle parking spot"
	else
		fail=$((fail+1))
		echo "  FAIL  the bay door or the parking spot is wrong"
		grep -E '^(Doors|Parking)|Errors|Warning' "$WORK/plan/bay.txt" | head -5
	fi
	# Stairs from the hall up into the bridge, three cells long: a sloped
	# Barrier to walk on and steps to see (as in the user's aircraft
	# factory), a walk path up the slope (floor to floor), a rail round the
	# opening above, and no walk path across the opening.
	sed -e 's|^    "stairs": \[$|    "stairs": [ {"deck": 0, "x": 18, "y": 8, "side": 1}|' \
		"$WORK/plan/example.json" > "$WORK/plan/stairs.json"
	"$SHIPGEN" build-plan "$WORK/plan/stairs.json" --out "$WORK/plan/stairs.SoH" > "$WORK/plan/stairs.txt" 2>&1
	"$SHIPGEN" parts "$WORK/plan/stairs.SoH" > "$WORK/plan/stairs-parts.txt" 2>&1
	if grep -q '^Stairs       : 1$' "$WORK/plan/stairs.txt" && grep -q 'Errors (0)' "$WORK/plan/stairs.txt" &&
			[ "$(grep -c 'Barrier  \[Ladder (stairs) Hall 1 to Bridge 1\]' "$WORK/plan/stairs-parts.txt")" = "4" ] &&
			grep -E 'Walk Path .* z 3\.8\.\.13\.8$' "$WORK/plan/stairs-parts.txt" > /dev/null &&
			! awk '/Walk Path .* z 13\.8\.\.13\.8$/{ for(i=1;i<=NF;i++){ if($i=="x") split($(i+1),a,"\\.\\."); if($i=="y") split($(i+1),b,"\\.\\.") } if(a[2] > -4.4 && a[1] < -0.6 && b[2] > 0.7 && b[1] < 14.8) bad=1 } END{ exit bad?0:1 }' "$WORK/plan/stairs-parts.txt"; then
		pass=$((pass+1))
		echo "  ok    stairs climb from the hall into the bridge, with a rail round the opening"
	else
		fail=$((fail+1))
		echo "  FAIL  the stairs are wrong"
		grep -E '^Stairs|Errors|Warning' "$WORK/plan/stairs.txt" | head -5
		grep -E 'stairs|z 0\.5\.\.10\.5' "$WORK/plan/stairs-parts.txt" | head -8
	fi
	check "the plan with stairs" "$WORK/plan/stairs.SoH"
	# Joined windows, lower: the four bridge windows on each side are one
	# long window each; in the bow, windows that line up join too,
	# from 2 ft above the bridge floor (10.5) to 7 ft.
	sed -e 's|^    "joinWindows": false|    "joinWindows": true|' -e 's|^    "windowSill": 3|    "windowSill": 2|' \
		-e 's|^    "windowTop": 7.5|    "windowTop": 7|' \
		"$WORK/plan/example.json" > "$WORK/plan/joined.json"
	"$SHIPGEN" build-plan "$WORK/plan/joined.json" --out "$WORK/plan/joined.SoH" > "$WORK/plan/joined.txt" 2>&1
	if grep -q '^Windows      : 22$' "$WORK/plan/joined.txt" && grep -q 'Errors (0)' "$WORK/plan/joined.txt" &&
			[ "$("$SHIPGEN" parts "$WORK/plan/joined.SoH" | grep -cE '  Window  \[Bridge 1\] .* y 10\.5\.\.29\.5  z 12\.5\.\.17\.5$')" = "2" ]; then
		pass=$((pass+1))
		echo "  ok    windows side by side join into one, at the sill and top asked for"
	else
		fail=$((fail+1))
		echo "  FAIL  joined windows are wrong"
		grep -E '^Windows|Errors' "$WORK/plan/joined.txt"
		"$SHIPGEN" parts "$WORK/plan/joined.SoH" | grep '  Window  ' | head -4
	fi
	check "the plan with joined windows" "$WORK/plan/joined.SoH"
	# The hull sloped between the decks: closed, more volume than stepped
	# (sloping only adds hull: the drawn triangles are the least it may be),
	# no room outside it.
	sed -e 's|^    "slopedDecks": false|    "slopedDecks": true|' "$WORK/plan/example.json" > "$WORK/plan/sloped.json"
	"$SHIPGEN" build-plan "$WORK/plan/sloped.json" --out "$WORK/plan/sloped.SoH" > "$WORK/plan/sloped.txt" 2>&1
	stepped=$(sed -n 's/^Hull         : .* about \([0-9]*\) cubic meters$/\1/p' "$WORK/plan/build.txt")
	sloped=$(sed -n 's/^Hull         : .* about \([0-9]*\) cubic meters$/\1/p' "$WORK/plan/sloped.txt")
	if [ -n "$sloped" ] && [ -n "$stepped" ] && [ "$sloped" -gt "$stepped" ] && grep -q 'Errors (0)' "$WORK/plan/sloped.txt" &&
			! grep -q 'could not be sloped' "$WORK/plan/sloped.txt" &&
			"$SHIPGEN" inspect "$WORK/plan/sloped.SoH" | grep -q 'Rooms outside the hull: 0'; then
		pass=$((pass+1))
		echo "  ok    the hull slopes between the decks ($stepped m3 stepped, $sloped m3 sloped), rooms inside"
	else
		fail=$((fail+1))
		echo "  FAIL  the sloped hull is wrong ($stepped m3 stepped, $sloped m3 sloped)"
		grep -E '^Hull|Errors|sloped' "$WORK/plan/sloped.txt" | head -4
	fi
	check "the plan with a sloped hull" "$WORK/plan/sloped.SoH"
	# Room surfaces have their normals pointing into the room (as in the
	# user's scout), and a hull door's switches are in reach: inside the room
	# and just outside the hull, 4 ft up, not in the middle of the door.
	if grep -qE 'Hull Door  \[Hull Door\]  switches \([-0-9.]+, [-0-9.]+, 4\.5\) \([-0-9.]+, [-0-9.]+, 4\.5\)' "$WORK/plan/parts.txt"; then
		pass=$((pass+1))
		echo "  ok    the hull door's switches are inside and outside, 4 ft up"
	else
		fail=$((fail+1))
		echo "  FAIL  the hull door's switches are not where they can be reached"
		grep 'Hull Door  \[' "$WORK/plan/parts.txt" | head -2
	fi
else
	fail=$((fail+1))
	echo "  FAIL  the example plan did not build"
	head -20 "$WORK/plan/build.txt"
fi

echo
echo "== The Aether kit =="
if "$SHIPGEN" kit all --out "$WORK/kit" > "$WORK/kit.txt" 2>&1; then
	pass=$((pass+1))
	echo "  ok    every blueprint of the Aether kit is accepted by the engine"
else
	fail=$((fail+1))
	echo "  FAIL  a blueprint of the Aether kit has errors"
	grep -E "^==|Error" "$WORK/kit.txt" | head -10
fi
for design in "$WORK/kit"/*.SoH; do
	[ -e "$design" ] || continue
	check "the kit / $(basename "$design")" "$design"
done

echo "== The graphical tool =="
if [ -x "$SHIPGEN_GUI" ]; then
	# The preview needs a real OpenGL context, which the offscreen platform
	# does not give it. Xvfb is used when it is available so that the 3D view
	# is actually drawn and can be checked; otherwise the tool is only
	# started, which still catches a crash on the way up.
	XVFB_PID=""
	GUI_DISPLAY=""
	if [ "$ON_MAC" = 0 ] && [ -x "$TC/x11/Xvfb" ]; then
		"$TC/x11/Xvfb" :99 -screen 0 1280x900x24 > "$WORK/xvfb.log" 2>&1 &
		XVFB_PID=$!
		sleep 1
		if kill -0 "$XVFB_PID" 2>/dev/null; then
			GUI_DISPLAY=:99
		else
			XVFB_PID=""
		fi
	fi

	GUI_DESIGN=$(ls "$WORK"/paint/*.SoH 2>/dev/null | head -1)
	GUI_ARGS=(--screenshot "$WORK/gui.png" --screenshot-preview "$WORK/preview.png")
	[ -n "$GUI_DESIGN" ] && GUI_ARGS+=(--open "$GUI_DESIGN")

	run_gui() {
		if [ -n "$GUI_DISPLAY" ]; then
			DISPLAY="$GUI_DISPLAY" LIBGL_ALWAYS_SOFTWARE=1 timeout 30 "$SHIPGEN_GUI" "$@"
		else
			QT_QPA_PLATFORM=offscreen timeout 30 "$SHIPGEN_GUI" "$@"
		fi
	}

	if run_gui "${GUI_ARGS[@]}" > "$WORK/gui.log" 2>&1; then
		pass=$((pass+1))
		echo "  ok    the graphical tool starts"
		if [ -n "$GUI_DISPLAY" ] || [ "$ON_MAC" = 1 ]; then
			if [ "$ON_MAC" = 1 ]; then
				preview_size=$(stat -f %z "$WORK/preview.png" 2>/dev/null || echo 0)
			else
				preview_size=$(stat -c %s "$WORK/preview.png" 2>/dev/null || echo 0)
			fi
			if [ "$preview_size" -gt 4000 ]; then
				pass=$((pass+1))
				echo "  ok    the 3D view draws the painted model ($preview_size bytes)"
			else
				fail=$((fail+1))
				echo "  FAIL  the 3D view is empty ($preview_size bytes)"
			fi
		fi
	else
		fail=$((fail+1))
		echo "  FAIL  the graphical tool did not start"
		head -5 "$WORK/gui.log"
	fi

	SHIPGEN_DESIGNER="$HERE/build/bin/shipgen-designer"
	if [ -x "$SHIPGEN_DESIGNER" ] && { [ -n "$GUI_DISPLAY" ] || [ "$ON_MAC" = 1 ]; }; then
		if DISPLAY="$GUI_DISPLAY" LIBGL_ALWAYS_SOFTWARE=1 timeout 60 "$SHIPGEN_DESIGNER" --open "$WORK/plan/example.json" \
				--step 5 --build --screenshot "$WORK/designer.png" > "$WORK/designer.log" 2>&1; then
			pass=$((pass+1))
			echo "  ok    the designer starts and builds the example plan"
		else
			fail=$((fail+1))
			echo "  FAIL  the designer did not start"
			head -5 "$WORK/designer.log"
		fi
	fi
	# A plan from a 3D model in the designer itself, in the background with
	# its progress window: the plan must be kept when the window closes
	# (closing it counted as Stop and threw every plan away in 0.3.0-alpha).
	if [ -x "$SHIPGEN_DESIGNER" ]; then
		QT_QPA_PLATFORM=offscreen timeout 120 "$SHIPGEN_DESIGNER" --from-model "$HERE/testdata/testhull.stl" --length 120 \
				--step 2 --screenshot "$WORK/designer-model.png" > "$WORK/designer-model.log" 2>&1
		if grep -q '^plan made from the model$' "$WORK/designer-model.log" && [ -s "$WORK/designer-model.png" ]; then
			pass=$((pass+1))
			echo "  ok    the designer makes a plan from a 3D model and keeps it"
		else
			fail=$((fail+1))
			echo "  FAIL  the designer did not keep the plan from the model"
			head -5 "$WORK/designer-model.log"
		fi
	fi

	if [ -n "$XVFB_PID" ]; then
		kill "$XVFB_PID" 2>/dev/null || true
		wait "$XVFB_PID" 2>/dev/null || true
	fi
else
	echo "  (the graphical tool was not built)"
fi

echo
echo "$pass checks passed, $validated blueprints validated cleanly, $fail with errors."
[ "$fail" = "0" ]
