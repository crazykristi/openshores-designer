#!/usr/bin/env bash
#
# Runs the generator over a matrix of templates and hulls and validates every
# blueprint it produces with the engine's own report. Also starts the graphical
# tool once to make sure it comes up.
#
set -uo pipefail
# A quiet grep that reads all its input. With pipefail, 'cmd | grep -q' fails
# at random: grep -q stops at the first match, and cmd is killed (SIGPIPE)
# if it has more to write.
qgrep() { grep "$@" > /dev/null; }

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
# A building with a teleporter room: its transporter stands in that room
# and has pads on the floor there, clear of it and of each other.
"$SHIPGEN" building-brief office_block "$WORK/teleporter.json" > /dev/null
python3 - "$WORK/teleporter.json" <<'PY'
import json, sys
b = json.load(open(sys.argv[1]))
b['volumes']['transporters'] = 1
b['rooms'] = [{"type": "Office", "size": [20, 16, 12]}, {"type": "Teleporter Room", "size": [24, 20, 12]}]
json.dump(b, open(sys.argv[1], 'w'))
PY
rm -rf "$WORK/bout/teleporter"
mkdir -p "$WORK/bout/teleporter"
"$SHIPGEN" generate-building --brief "$WORK/teleporter.json" --out "$WORK/bout/teleporter" --layout grid > /dev/null 2>&1
telParts="$("$SHIPGEN" parts "$WORK/bout/teleporter/"*.SoH 2>&1)"
if grep -q 'Transporter Console .*pad effect 1, pads ([^)]*) ([^)]*)' <<< "$telParts" &&
		[ "$(grep -c '  Mesh  \[Teleporter Room\]' <<< "$telParts")" -ge 2 ]; then
	pass=$((pass+1))
	echo "  ok    a building's transporter gets pads in its teleporter room"
else
	fail=$((fail+1))
	echo "  FAIL  pads for a building's transporter"
	grep -E 'Transporter|Teleporter' <<< "$telParts" | head
fi

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

# A binary STL that keeps a colour in each triangle's attribute bytes (as
# Materialise and VisCAM write them, "COLOR=" in the header): the engine's
# reader skipped that many bytes and lost its place, 151 triangles from 158,300.
python3 - "$WORK/colour.stl" <<'PY'
import struct, sys
tris = []
pts = [(0,0,0),(10,0,0),(0,10,0),(0,0,10)]
for a, b, c in [(0,2,1),(0,1,3),(0,3,2),(1,2,3)]:
    tris.append((pts[a], pts[b], pts[c]))
with open(sys.argv[1], 'wb') as f:
    f.write(b'STLB ATF 15.15.0.0 COLOR=' + b'\x80\x80\x80\xff' + b' ' * (80 - 29))
    f.write(struct.pack('<I', len(tris)))
    for t in tris:
        f.write(struct.pack('<3f', 0, 0, 0))
        for p in t:
            f.write(struct.pack('<3f', *p))
        f.write(struct.pack('<H', 0x7c1f))
PY
if "$SHIPGEN" mesh-info --model "$WORK/colour.stl" 2>&1 | grep -q 'triangles      : 4$' &&
		"$SHIPGEN" mesh-info --model "$WORK/colour.stl" 2>&1 | grep -q 'the model is closed'; then
	pass=$((pass+1))
	echo "  ok    a binary STL with colours in its attribute bytes reads whole"
else
	fail=$((fail+1))
	echo "  FAIL  a binary STL with colours in its attribute bytes"
	"$SHIPGEN" mesh-info --model "$WORK/colour.stl" 2>&1 | head -4
fi

# FBX, glTF and the other formats come through the Assimp library, when the
# build found it (build.sh says so). A 10 ft cube as glTF.
if "$SHIPGEN" mesh-info --model "$HERE/testdata/cube.gltf" 2>&1 | grep -q 'unsupported model format'; then
	echo "  (built without Assimp: glTF and FBX are not read)"
elif "$SHIPGEN" mesh-info --model "$HERE/testdata/cube.gltf" 2>&1 | grep -q 'triangles      : 12$' &&
		"$SHIPGEN" mesh-info --model "$HERE/testdata/cube.gltf" 2>&1 | grep -q 'the model is closed' &&
		"$SHIPGEN" plan-from-model "$HERE/testdata/cube.gltf" "$WORK/cube-plan.json" --length 50 > "$WORK/cube-plan.log" 2>&1 &&
		grep -q '"width": 1[0-2],' "$WORK/cube-plan.json"; then
	pass=$((pass+1))
	echo "  ok    a glTF model (through Assimp) reads closed and becomes a plan"
else
	fail=$((fail+1))
	echo "  FAIL  a glTF model through Assimp"
	"$SHIPGEN" mesh-info --model "$HERE/testdata/cube.gltf" 2>&1 | head -4
	tail -3 "$WORK/cube-plan.log"
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
			"$SHIPGEN" inspect "$WORK/whitestar-resaved.SoH" 2>&1 | qgrep '^Parts        : 88$'; then
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
	# A quarter turn (TriPlan::Rotate): clockwise then anticlockwise, and
	# four times clockwise, are the plan again; turned once it builds with
	# all it had, the grid's width and depth swapped.
	"$SHIPGEN" plan-flip "$WORK/plan/example.json" "$WORK/plan/once.json" --clockwise > /dev/null 2>&1
	"$SHIPGEN" plan-flip "$WORK/plan/once.json" "$WORK/plan/twice.json" --anticlockwise > /dev/null 2>&1
	cmp -s "$WORK/plan/example.json" "$WORK/plan/twice.json" || twice="not the same after a quarter turn back"
	cp "$WORK/plan/example.json" "$WORK/plan/four.json"
	for i in 1 2 3 4; do
		"$SHIPGEN" plan-flip "$WORK/plan/four.json" "$WORK/plan/four.json" --clockwise > /dev/null 2>&1
	done
	cmp -s "$WORK/plan/example.json" "$WORK/plan/four.json" || twice="not the same after four quarter turns"
	if "$SHIPGEN" build-plan "$WORK/plan/once.json" --out "$WORK/plan/quarter.SoH" > "$WORK/plan/quarter.txt" 2>&1 &&
			diff <(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/build.txt") \
				<(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/quarter.txt") > /dev/null &&
			python3 -c "import json,sys; a=json.load(open(sys.argv[1])); b=json.load(open(sys.argv[2])); sys.exit(0 if (a['width'],a['depth'])==(b['depth'],b['width']) else 1)" \
				"$WORK/plan/example.json" "$WORK/plan/once.json"; then
		pass=$((pass+1))
		echo "  ok    the example plan turned a quarter turn keeps its rooms, doors, windows, ladder and stations"
	else
		fail=$((fail+1))
		echo "  FAIL  the example plan turned a quarter turn lost something"
		diff <(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/build.txt") \
			<(grep -E '^(Rooms|Doors|Windows|Ladders|Stations|Errors)' "$WORK/plan/quarter.txt")
	fi
	# One deck a quarter turn: the grid grows when it would not fit, and the
	# deck keeps every triangle; turned back it is where it was.
	"$SHIPGEN" plan-flip "$WORK/plan/example.json" "$WORK/plan/deck1.json" --clockwise --deck 1 > "$WORK/plan/deck1.txt" 2>&1
	"$SHIPGEN" plan-flip "$WORK/plan/deck1.json" "$WORK/plan/deck1back.json" --anticlockwise --deck 1 > /dev/null 2>&1
	if python3 - "$WORK/plan/example.json" "$WORK/plan/deck1.json" "$WORK/plan/deck1back.json" <<'PY'
import json, sys
def dec(s):
    out = []
    for p in s.split(','):
        if p:
            n, v = p.split('*'); out += [int(v)] * int(n)
    return out
plans = [json.load(open(f)) for f in sys.argv[1:]]
counts = [[sum(1 for v in dec(L['hull']) if v) for L in p['layers']] for p in plans]
if not (counts[0] == counts[1] == counts[2]):
    sys.exit(1)
# Back where it was: each deck's drawn cells, from the middle of the grid.
def cells(p, d):
    W, D = p['width'], p['depth']
    h = dec(p['layers'][d]['hull'])
    return sorted((i // 4 % W - W / 2, i // 4 // W - D / 2, i % 4) for i, v in enumerate(h) if v)
sys.exit(0 if all(cells(plans[0], d) == cells(plans[2], d) for d in range(len(plans[0]['layers']))) else 1)
PY
	then
		pass=$((pass+1))
		echo "  ok    one deck turned a quarter turn keeps its hull, and turned back is where it was"
	else
		fail=$((fail+1))
		echo "  FAIL  one deck turned a quarter turn"
		cat "$WORK/plan/deck1.txt"
	fi
	# Fighter mode (experimental): a model at its real length becomes a
	# fighter plan (small cells, low decks, no FTL, no automatic berths); with
	# nothing drawn the build says what is missing; with a cockpit, a side
	# door and a helm placed by hand the engine takes it with no warnings.
	"$SHIPGEN" plan-from-model "$HERE/testdata/testhull.stl" "$WORK/plan/fighter.json" --length 45 --fighter > /dev/null 2>&1
	python3 - "$WORK/plan/fighter.json" "$WORK/plan/fighter-cockpit.json" <<'PY'
import json, sys
p = json.load(open(sys.argv[1])); W, D = p['width'], p['depth']
def dec(s):
    o = []
    for part in s.split(','):
        if part:
            n, v = part.split('*'); o += [int(v)] * int(n)
    return o
def enc(a):
    out = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        out.append(f"{j-i}*{a[i]}"); i = j
    return ','.join(out)
full = lambda h, cx, cy: 0 <= cx < W and 0 <= cy < D and all(h[(cy*W+cx)*4+k] for k in range(4))
best = None
for d, L in enumerate(p['layers']):
    h = dec(L['hull'])
    for cy in range(D):
        for cx in range(W):
            if best is None and all(full(h, cx+i, cy+j) for i in range(4) for j in range(2)):
                best = (d, cx, cy)
d, cx, cy = best
L = p['layers'][d]; h = dec(L['hull']); r = dec(L['room']) if L.get('room') else [-1] * len(h)
for i in range(4):
    for j in range(2):
        for k in range(4): r[((cy+j)*W+cx+i)*4+k] = 0
L['room'] = enc(r)
p['stations'] = [{'deck': d, 'x': cx + 2.6, 'y': cy + 1.0, 'yaw': 0, 'type': 'helm', 'seat': 'chair'}]
p['doors'] = [{'deck': d, 'x': cx + 1, 'y': cy, 'side': 0}]
json.dump(p, open(sys.argv[2], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/fighter.json" --out "$WORK/plan/fighter.SoH" > "$WORK/plan/fighter.txt" 2>&1
	"$SHIPGEN" build-plan "$WORK/plan/fighter-cockpit.json" --out "$WORK/plan/fighter-cockpit.SoH" > "$WORK/plan/fighter-cockpit.txt" 2>&1
	if python3 -c "import json,sys; p=json.load(open(sys.argv[1])); sys.exit(0 if p.get('mode')=='fighter' and p['template']=='fighter' and p['cellFeet']==2.5 and p['deckFeet']==4.5 and p.get('autoBerths') is False else 1)" "$WORK/plan/fighter.json" &&
			grep -q 'Fighter mode: there is no cockpit' "$WORK/plan/fighter.txt" &&
			grep -q 'Fighter mode: there is no way in' "$WORK/plan/fighter.txt" &&
			grep -q '^Stations     : 1 (1 placed by hand)$' "$WORK/plan/fighter-cockpit.txt" &&
			grep -q '^Berths       : 0$' "$WORK/plan/fighter-cockpit.txt" &&
			grep -q 'Fighter mode is experimental' "$WORK/plan/fighter-cockpit.txt" &&
			! grep -q 'Fighter mode: there is' "$WORK/plan/fighter-cockpit.txt" &&
			grep -q 'Errors (0)' "$WORK/plan/fighter-cockpit.txt" && grep -q 'Warnings (0)' "$WORK/plan/fighter-cockpit.txt"; then
		pass=$((pass+1))
		echo "  ok    fighter mode (experimental): a fighter from a model, with a cockpit, door and helm"
	else
		fail=$((fail+1))
		echo "  FAIL  fighter mode"
		grep -E 'Fighter|^Stations|^Berths|Error|Warning' "$WORK/plan/fighter.txt" "$WORK/plan/fighter-cockpit.txt"
	fi
	# A canopy over the cockpit and on over the hull ahead of it: the hull
	# over it is one window of the cockpit (this test hull has a few large
	# faces, so it is cut along the outline), nothing reaches out of the
	# hull, the helm sees out ahead and has glass overhead, and the plan
	# keeps the canopy when saved.
	python3 - "$WORK/plan/fighter-cockpit.json" "$WORK/plan/fighter-canopy.json" <<'PY'
import json, sys
p = json.load(open(sys.argv[1])); W, D = p['width'], p['depth']
def dec(s):
    o = []
    for part in s.split(','):
        if part:
            n, v = part.split('*'); o += [int(v)] * int(n)
    return o
st = p['stations'][0]; d = st['deck']; L = p['layers'][d]
r = dec(L['room']); h = dec(L['hull'])
cells = sorted({(i // 4) % W for i in range(len(r)) if r[i] == 0})
rows = sorted({(i // 4) // W for i in range(len(r)) if r[i] == 0})
sky = [{'deck': d, 'x': x, 'y': y, 'shape': 'canopy'} for x in cells for y in rows]
x = max(cells) + 1
if all(h[(y * W + x) * 4 + k] for y in rows for k in range(4)):
    sky += [{'deck': d, 'x': x, 'y': y, 'shape': 'canopy'} for y in rows]
p['skylights'] = sky
json.dump(p, open(sys.argv[2], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/fighter-canopy.json" --out "$WORK/plan/fighter-canopy.SoH" > "$WORK/plan/fighter-canopy.txt" 2>&1
	"$SHIPGEN" audit "$WORK/plan/fighter-canopy.SoH" > "$WORK/plan/fighter-canopy.audit" 2>&1
	"$SHIPGEN" view-check "$WORK/plan/fighter-canopy.SoH" > "$WORK/plan/fighter-canopy.view" 2>&1; viewOk=$?
	"$SHIPGEN" plan-flip "$WORK/plan/fighter-canopy.json" "$WORK/plan/fighter-canopy-saved.json" --clockwise > /dev/null 2>&1
	if grep -q '^Canopies     : 1$' "$WORK/plan/fighter-canopy.txt" && grep -q 'Errors (0)' "$WORK/plan/fighter-canopy.txt" &&
			grep -q 'sees out ahead 100 %' "$WORK/plan/fighter-canopy.txt" &&
			grep -q '^Parts sticking out of the hull: none$' "$WORK/plan/fighter-canopy.audit" && [ "$viewOk" -eq 0 ] &&
			grep -qE '^  up: .*\(glass [1-9]' "$WORK/plan/fighter-canopy.view" &&
			[ "$(grep -o '"shape": *"canopy"' "$WORK/plan/fighter-canopy-saved.json" | wc -l)" -eq "$(grep -o '"shape": *"canopy"' "$WORK/plan/fighter-canopy.json" | wc -l)" ]; then
		pass=$((pass+1))
		echo "  ok    a fighter's canopy: one window, nothing out of the hull, the helm sees out"
	else
		fail=$((fail+1))
		echo "  FAIL  a fighter's canopy"
		grep -E 'Canop|sees out|reaches|Error|Warning' "$WORK/plan/fighter-canopy.txt"
		sed -n '/Parts sticking/,$p' "$WORK/plan/fighter-canopy.audit"
		cat "$WORK/plan/fighter-canopy.view"
	fi
	# Stations, berths and paths placed by hand (Crew step) are built where
	# they are, and the build adds only the rest; what the Build page leaves
	# out (all windows, one door, one station marked skip) is not built but
	# stays in the plan. With the automatic ones off, only those by hand.
	python3 - "$WORK/plan/example.json" "$WORK/plan/hand.json" "$WORK/plan/handonly.json" <<'PY'
import json, sys
plan = json.load(open(sys.argv[1]))
plan['stations'] = [{'deck': 1, 'x': 18.5, 'y': 8.5, 'yaw': 90, 'type': 'helm', 'seat': 'chair'},
                    {'deck': 1, 'x': 21, 'y': 8, 'yaw': 0, 'type': 'sensor', 'skip': True}]
plan['berths'] = [{'deck': 0, 'x': 14.5, 'y': 3.5, 'yaw': 0, 'kind': 'crew'}]
plan['paths'] = [{'deck': 1, 'from': [19, 4, 0], 'to': [25, 8, 1]}]
plan['buildLeaveOut'] = ['windows']
plan['doors'][0]['skip'] = True
json.dump(plan, open(sys.argv[2], 'w'))
plan['autoStations'] = plan['autoBerths'] = plan['autoPaths'] = False
json.dump(plan, open(sys.argv[3], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/hand.json" --out "$WORK/plan/hand.SoH" > "$WORK/plan/hand.txt" 2>&1
	"$SHIPGEN" build-plan "$WORK/plan/handonly.json" --out "$WORK/plan/handonly.SoH" > "$WORK/plan/handonly.txt" 2>&1
	"$SHIPGEN" plan-flip "$WORK/plan/hand.json" "$WORK/plan/hand-turned.json" --clockwise > /dev/null 2>&1
	if grep -q '^Stations     : 6 (1 placed by hand)$' "$WORK/plan/hand.txt" &&
			grep -q '^Berths       : 60 (1 placed by hand)$' "$WORK/plan/hand.txt" &&
			grep -q '^Hand paths   : 1$' "$WORK/plan/hand.txt" && grep -q '^Windows      : 0$' "$WORK/plan/hand.txt" &&
			grep -q '^Doors        : 2$' "$WORK/plan/hand.txt" && grep -q 'Errors (0)' "$WORK/plan/hand.txt" &&
			grep -q '^Stations     : 1 (1 placed by hand)$' "$WORK/plan/handonly.txt" &&
			grep -q '^Berths       : 1 (1 placed by hand)$' "$WORK/plan/handonly.txt" &&
			grep -q 'Errors (0)' "$WORK/plan/handonly.txt" &&
			python3 -c "import json,sys; p=json.load(open(sys.argv[1])); sys.exit(0 if len(p['stations'])==2 and len(p['windows'])==29 and p['buildLeaveOut']==['windows'] and p['doors'][0].get('skip') else 1)" "$WORK/plan/hand-turned.json"; then
		pass=$((pass+1))
		echo "  ok    stations, berths and paths placed by hand, and what the Build page leaves out"
	else
		fail=$((fail+1))
		echo "  FAIL  items placed by hand or left out of the build"
		grep -E '^(Doors|Windows|Stations|Berths|Hand|Errors)' "$WORK/plan/hand.txt" "$WORK/plan/handonly.txt"
	fi
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
	if echo "$texFaces" | qgrep 'library texture 33' && echo "$texFaces" | qgrep 'library texture 58' &&
			echo "$texFaces" | qgrep 'library texture 111'; then
		pass=$((pass+1))
		echo "  ok    rooms and doors can have textures of their own from the game's library"
	else
		fail=$((fail+1))
		echo "  FAIL  room and door textures"
		echo "$texFaces" | grep 'library texture' | head -20
	fi
	# What a room is for, as the game reads it (its hold access), and the
	# stations a room asks for: a sick bay its medic, a transporter room its
	# transporter and pads, at least 0.5 m apart.
	python3 - "$WORK/plan/example.json" "$WORK/plan/uses.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
for r in d['rooms']:
    if r['type'] == 'Hall':
        r['type'], r['label'] = 'Transporter Room', 'Transporter 1'
    elif r['type'] == 'Crew Quarters':
        r['type'], r['label'] = 'Sick Bay', 'Sick Bay 1'
        r['access'] = ['officers', 'crew']
json.dump(d, open(sys.argv[2], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/uses.json" --out "$WORK/plan/uses.SoH" > "$WORK/plan/uses.txt" 2>&1
	"$SHIPGEN" parts "$WORK/plan/uses.SoH" > "$WORK/plan/uses.parts" 2>&1
	padsOk="$(python3 - "$WORK/plan/uses.parts" <<'PY'
import math, re, sys
text = open(sys.argv[1]).read()
m = re.search(r'Transporter Console .*pads((?: \([-\d.]+, [-\d.]+, [-\d.]+\))+)', text)
pads = [tuple(map(float, p)) for p in re.findall(r'\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)', m.group(1))] if m else []
apart = all(math.hypot(a[0] - b[0], a[1] - b[1]) >= 4.6 for i, a in enumerate(pads) for b in pads[i + 1:])
print('yes' if len(pads) >= 2 and apart and text.count('Mesh  [Transporter 1]') >= len(pads) else 'no')
PY
)"
	if grep -qE 'Room Void  \[Sick Bay 1\]  hold access: pharmacy  only officers, crew' "$WORK/plan/uses.parts" &&
			! grep -E 'Room Void  \[Engine Room 1\]' "$WORK/plan/uses.parts" | grep -q 'only' &&
			grep -qE 'Room Void  \[Engine Room 1\]  hold access: fuel coupling' "$WORK/plan/uses.parts" &&
			grep -qE 'Room Void  \[Bridge 1\]  hold access: hold' "$WORK/plan/uses.parts" &&
			grep -q 'No room is a hold, so the cargo' "$WORK/plan/uses.txt" &&
			grep -q 'Medic Console' "$WORK/plan/uses.parts" && [ "$padsOk" = yes ] &&
			grep -q '^Errors (0):' "$WORK/plan/uses.txt"; then
		pass=$((pass+1))
		echo "  ok    rooms are what their type says in the game (with no hold, the rooms of no use load the cargo; who may enter); a sick bay gets its medic, a transporter room its pads"
	else
		fail=$((fail+1))
		echo "  FAIL  room uses, the medic or the transporter pads ($padsOk)"
		grep -E 'hold access|Medic|Transporter Console' "$WORK/plan/uses.parts" | head -6
	fi
	# A weapon bay per weapon emitter: its own fire control station running
	# its own module, the emitter on it; kept through the designer.
	python3 - "$WORK/plan/example.json" "$WORK/plan/bays.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
d['leaveOut'] = [x for x in d.get('leaveOut', []) if x != 'firecontrol']
d['emitters'] = [e for e in d.get('emitters', []) if e.get('kind') != 'weapon'] + [
    {'deck': 0, 'x': 30, 'y': 8, 'side': 1, 'kind': 'weapon', 'module': 261},
    {'deck': 0, 'x': 30, 'y': 7, 'side': 1, 'kind': 'weapon', 'module': 254}]
json.dump(d, open(sys.argv[2], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/bays.json" --out "$WORK/plan/bays.SoH" > "$WORK/plan/bays.txt" 2>&1
	"$SHIPGEN" parts "$WORK/plan/bays.SoH" > "$WORK/plan/bays.parts" 2>&1
	if grep -q 'Fire Control .*module .*261' "$WORK/plan/bays.parts" && grep -q 'Fire Control .*module .*254' "$WORK/plan/bays.parts" &&
			grep -q '^Weapon emit  : 2$' "$WORK/plan/bays.txt" && grep -q '^Errors (0):' "$WORK/plan/bays.txt" &&
			QT_QPA_PLATFORM=offscreen "$(dirname "$SHIPGEN")/shipgen-designer" --open "$WORK/plan/bays.json" --export "$WORK/plan/bays-kept.SoH" > /dev/null 2>&1 &&
			cmp -s "$WORK/plan/bays-kept.SoH" "$WORK/plan/bays.SoH"; then
		pass=$((pass+1))
		echo "  ok    each weapon emitter is a weapon bay with its own fire control station and module"
	else
		fail=$((fail+1))
		echo "  FAIL  weapon bays per emitter"
		grep -E 'Fire Control' "$WORK/plan/bays.parts"; grep -E 'Weapon emit|Errors' "$WORK/plan/bays.txt"
	fi
	# Textures for the whole ship: every room and door without its own.
	python3 - "$WORK/plan/example.json" "$WORK/plan/shiptex.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
d['wallTexture'], d['floorTexture'], d['ceilingTexture'], d['doorTexture'] = 33, 58, 44, 111
for r in d['rooms']:
    for k in ('wallTexture', 'floorTexture', 'ceilingTexture'):
        r.pop(k, None)
for door in d['doors']:
    door.pop('texture', None)
json.dump(d, open(sys.argv[2], 'w'))
PY
	"$SHIPGEN" build-plan "$WORK/plan/shiptex.json" --out "$WORK/plan/shiptex.SoH" > "$WORK/plan/shiptex.txt" 2>&1
	shipTex="$("$SHIPGEN" faces "$WORK/plan/shiptex.SoH" 2>&1)"
	if qgrep 'library texture 33' <<< "$shipTex" && qgrep 'library texture 58' <<< "$shipTex" &&
			qgrep 'library texture 44' <<< "$shipTex" && qgrep 'library texture 111' <<< "$shipTex" &&
			QT_QPA_PLATFORM=offscreen "$(dirname "$SHIPGEN")/shipgen-designer" --open "$WORK/plan/shiptex.json" --export "$WORK/plan/shiptex-kept.SoH" > /dev/null 2>&1 &&
			cmp -s "$WORK/plan/shiptex-kept.SoH" "$WORK/plan/shiptex.SoH"; then
		pass=$((pass+1))
		echo "  ok    textures for the whole ship go on every room and door without its own"
	else
		fail=$((fail+1))
		echo "  FAIL  textures for the whole ship"
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
	if [ "${2:-0}" -gt 0 ] && [ "$1" = "$2" ] && echo "$crewReport" | qgrep '^Natural doorways: 0$' &&
			echo "$crewReport" | qgrep '^Mirrored screens: 0$'; then
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
			"$SHIPGEN" crew-check "$WORK/plan/desks-standing.SoH" | qgrep '^Mirrored screens: 0$'; then
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
	if [ "$(grep -c 'Ladder Path' "$WORK/plan/parts.txt")" = "1" ] && grep -q '^Ramps        : 1$' "$WORK/plan/build.txt" &&
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
			[ "$("$SHIPGEN" parts "$WORK/plan/noramp.SoH" | grep -c 'Ladder Path')" = "2" ]; then
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
	# A hull door's own way down: instead of the ship's, and tied to the door.
	python3 - "$WORK/plan/example.json" "$WORK/plan" <<'PY'
import json, sys
for name, change, ship in (('exit-ladder', {'exit': 'ladder'}, None), ('exit-none', {'exit': 'none'}, None),
		('exit-ramp', {'exit': 'ramp'}, 'none'), ('exit-ladder-open', {'exit': 'ladder', 'exitOpenOnly': True}, None),
		('exit-ramp-long', {'exit': 'ramp', 'rampRun': 4}, None)):
	plan = json.load(open(sys.argv[1]))
	door = [d for d in plan['doors'] if d['y'] == 15]
	assert len(door) == 1
	door[0].update(change)
	if ship:
		plan['hullDoorExit'] = ship
	json.dump(plan, open('%s/%s.json' % (sys.argv[2], name), 'w'))
PY
	for name in exit-ladder exit-none exit-ramp exit-ladder-open exit-ramp-long; do
		"$SHIPGEN" build-plan "$WORK/plan/$name.json" --out "$WORK/plan/$name.SoH" > "$WORK/plan/$name.txt" 2>&1
		"$SHIPGEN" door-check "$WORK/plan/$name.SoH" > "$WORK/plan/$name.doors" 2>&1
	done
	ladders() { "$SHIPGEN" parts "$WORK/plan/$1.SoH" | grep -c 'Ladder Path'; }
	if grep -q '^Ramps        : 0$' "$WORK/plan/exit-ladder.txt" && [ "$(ladders exit-ladder)" = "2" ] &&
			grep -q '^Ramps        : 0$' "$WORK/plan/exit-none.txt" && [ "$(ladders exit-none)" = "1" ] &&
			grep -q '^Ramps        : 1$' "$WORK/plan/exit-ramp.txt" && [ "$(ladders exit-ramp)" = "1" ]; then
		pass=$((pass+1))
		echo "  ok    a hull door's own way down overrides the ship's"
	else
		fail=$((fail+1))
		echo "  FAIL  a hull door's own way down is not what it asks for"
	fi
	# "Only while the door is open" did nothing in the game and is retired:
	# an old plan that asks for it builds the same as one that does not.
	if cmp -s "$WORK/plan/exit-ladder-open.SoH" "$WORK/plan/exit-ladder.SoH"; then
		pass=$((pass+1))
		echo "  ok    an old plan's \"only while the door is open\" is ignored"
	else
		fail=$((fail+1))
		echo "  FAIL  an old plan's \"only while the door is open\" still changes the build"
	fi
	# From 15 ft out, people walk in to an open hull door, and the ladder is
	# climbed up and in (the game's zone round the hull stopped them 1 to 2 m
	# out beside the arness, 2026-11-07).
	if grep -q '^hull door [0-9]*: walk in from 15 ft out clear' "$WORK/plan/exit-ladder.doors" &&
			! grep '^hull door' "$WORK/plan/exit-ladder.doors" "$WORK/plan/exit-ramp.doors" | qgrep 'stopped' &&
			grep -qE '^ladder .*climb closed .*, open clear$' "$WORK/plan/exit-ladder.doors"; then
		pass=$((pass+1))
		echo "  ok    an open hull door can be walked into from outside, and its ladder climbed up and in"
	else
		fail=$((fail+1))
		echo "  FAIL  an open hull door cannot be reached from outside"
		grep -E '^hull door|^ladder' "$WORK/plan/exit-ladder.doors" "$WORK/plan/exit-ramp.doors" | head -6
	fi
	# The ladder from a hull door can be taken hold of from the ground, with
	# the door shut or open (the user could not, 2026-11-06: the game keeps
	# everyone 2 ft off the hull).
	if grep -qE '^ladder [0-9]+ always.*from the ground closed [3-5] of 5, open [3-5] of 5' "$WORK/plan/exit-ladder.doors" &&
			! grep -E '^ladder [0-9]+ ' "$WORK/plan/exit-ladder.doors" | qgrep -vE 'from the ground closed [3-5] of 5, open [3-5] of 5'; then
		pass=$((pass+1))
		echo "  ok    a hull door's ladder can be taken hold of from the ground"
	else
		fail=$((fail+1))
		echo "  FAIL  a hull door's ladder cannot be taken hold of from the ground"
		grep -E '^ladder ' "$WORK/plan/exit-ladder.doors" | head -4
	fi
	# Walked up from the ground, a ramp has no step higher than its own slope
	# gives between looks (the game misses thin bits of it): a strip of the
	# hull's no-go zone stood 2.5 ft high past the foot (2026-11-07).
	steps="$(cat "$WORK/plan/exit-ramp.doors" "$WORK/plan/exit-ramp-long.doors" | sed -n 's/^ramp .*biggest step up from the ground \([0-9.]*\) ft$/\1/p')"
	if [ -n "$steps" ] && awk '{ if ($1 > .6) bad = 1 } END { exit bad ? 1 : 0 }' <<< "$steps"; then
		pass=$((pass+1))
		echo "  ok    a ramp can be walked up from the ground with no step (biggest $(sort -n <<< "$steps" | tail -1) ft)"
	else
		fail=$((fail+1))
		echo "  FAIL  a ramp has a step on the way up from the ground: $(tr '\n' ' ' <<< "$steps")"
	fi
	if "$SHIPGEN" crew-check "$WORK/plan/exit-ladder.SoH" > /dev/null 2>&1; then
		pass=$((pass+1))
		echo "  ok    the crew can still get around with a ladder at a hull door"
	else
		fail=$((fail+1))
		echo "  FAIL  crew routes break with a ladder at a hull door"
	fi
	rampLength() { sed -n 's/^ramp [0-9]* always, \([0-9.]*\) x \([0-9.]*\) ft.*/\1 \2/p' "$WORK/plan/$1.doors" | awk '{ print ($1 > $2) ? $1 : $2 }'; }
	if awk -v a="$(rampLength exit-ramp)" -v b="$(rampLength exit-ramp-long)" 'BEGIN { exit (a > 0 && b > a * 1.5) ? 0 : 1 }'; then
		pass=$((pass+1))
		echo "  ok    a door's ramp can be made longer and gentler ($(rampLength exit-ramp) ft to $(rampLength exit-ramp-long) ft)"
	else
		fail=$((fail+1))
		echo "  FAIL  a door's ramp length is not followed"
	fi
	if QT_QPA_PLATFORM=offscreen "$(dirname "$SHIPGEN")/shipgen-designer" --open "$WORK/plan/exit-ramp-long.json" --export "$WORK/plan/exit-kept.SoH" > /dev/null 2>&1 &&
			cmp -s "$WORK/plan/exit-kept.SoH" "$WORK/plan/exit-ramp-long.SoH" &&
			"$SHIPGEN" plan-flip "$WORK/plan/exit-ladder.json" "$WORK/plan/exit-flipped.json" --turn > /dev/null 2>&1 &&
			python3 - "$WORK/plan/exit-flipped.json" <<'PY'
import json, sys
sys.exit(0 if [d for d in json.load(open(sys.argv[1]))['doors'] if d.get('exit') == 'ladder'] else 1)
PY
	then
		pass=$((pass+1))
		echo "  ok    a door's way down goes through the designer and through a flip"
	else
		fail=$((fail+1))
		echo "  FAIL  a door's way down is lost in the designer or a flip"
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
			"$SHIPGEN" inspect "$WORK/plan/open-model.SoH" 2>&1 | qgrep 'closed solid'; then
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
			"$SHIPGEN" inspect "$WORK/plan/open-smooth.SoH" 2>&1 | qgrep 'closed solid' &&
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
				"$SHIPGEN" inspect "$WORK/plan/ellipsoid.SoH" 2>&1 | qgrep 'closed solid' &&
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
	# Windows that follow the hull: on the ellipsoid, a lounge's twenty
	# windows each take the hull's own triangles in front of them as glass,
	# cut to the window's outline, with a lining back to the room's wall.
	if command -v python3 >/dev/null 2>&1 && [ -f "$WORK/plan/ellipsoid.json" ]; then
		python3 "$HERE/testdata/add_windows.py" "$WORK/plan/ellipsoid.json" "$WORK/plan/hullwin.json" hull
		"$SHIPGEN" build-plan "$WORK/plan/hullwin.json" --out "$WORK/plan/hullwin.SoH" > "$WORK/plan/hullwin.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/hullwin.SoH" > "$WORK/plan/hullwin-parts.txt" 2>&1
		if grep -q '^Hull windows : 20$' "$WORK/plan/hullwin.txt" && grep -q 'Errors (0)' "$WORK/plan/hullwin.txt" &&
				[ "$(grep -c '  Window  \[Lounge 1\]' "$WORK/plan/hullwin-parts.txt")" -eq 20 ] &&
				"$SHIPGEN" inspect "$WORK/plan/hullwin.SoH" 2>&1 | qgrep 'closed solid' &&
				"$SHIPGEN" audit "$WORK/plan/hullwin.SoH" 2>&1 | qgrep 'Rooms outside the hull: none'; then
			pass=$((pass+1))
			echo "  ok    windows on a model hull take the hull's own triangles as glass"
		else
			fail=$((fail+1))
			echo "  FAIL  windows following the hull"
			grep -E '^Hull windows|^Windows|Error' "$WORK/plan/hullwin.txt"
		fi
		# Windows dragged out together (one row) are one window even without
		# joinWindows; a window of no row stays on its own.
		python3 - "$WORK/plan/hullwin.json" "$WORK/plan/winrow.json" <<'PY'
import json, sys
plan = json.load(open(sys.argv[1]))
plan.pop('windowStyle', None)
plan['joinWindows'] = False
for w in plan['windows']:
    w['row'] = 1 if w['y'] == 3 else 2
plan['windows'][-1].pop('row')
json.dump(plan, open(sys.argv[2], 'w'))
PY
		"$SHIPGEN" build-plan "$WORK/plan/winrow.json" --out "$WORK/plan/winrow.SoH" > "$WORK/plan/winrow.txt" 2>&1
		if grep -q '^Windows      : 3$' "$WORK/plan/winrow.txt" && grep -q 'Errors (0)' "$WORK/plan/winrow.txt"; then
			pass=$((pass+1))
			echo "  ok    windows dragged out together are one window"
		else
			fail=$((fail+1))
			echo "  FAIL  windows dragged out together"
			grep -E '^Windows|Error' "$WORK/plan/winrow.txt"
		fi
		# Each window can say how it is made: with the ship set to framed
		# windows, the ten marked "hull" follow the hull, the rest are framed.
		python3 - "$WORK/plan/hullwin.json" "$WORK/plan/winstyle.json" <<'PY'
import json, sys
plan = json.load(open(sys.argv[1]))
plan.pop('windowStyle', None)
for i, w in enumerate(plan['windows']):
    w['style'] = 'hull' if i < 10 else 'frame'
json.dump(plan, open(sys.argv[2], 'w'))
PY
		"$SHIPGEN" build-plan "$WORK/plan/winstyle.json" --out "$WORK/plan/winstyle.SoH" > "$WORK/plan/winstyle.txt" 2>&1
		if grep -q '^Hull windows : 10$' "$WORK/plan/winstyle.txt" && grep -q 'Errors (0)' "$WORK/plan/winstyle.txt" &&
				"$SHIPGEN" plan-flip "$WORK/plan/winstyle.json" "$WORK/plan/winstyle-turned.json" --clockwise >/dev/null 2>&1 &&
				[ "$(grep -o '"style": *"hull"' "$WORK/plan/winstyle-turned.json" | wc -l)" -eq 10 ]; then
			pass=$((pass+1))
			echo "  ok    each window follows the hull or has a frame, as set"
		else
			fail=$((fail+1))
			echo "  FAIL  window style per window"
			grep -E '^Hull windows|^Windows|Error' "$WORK/plan/winstyle.txt"
		fi
		# A livery colour on a model hull: a band six cells wide, painted red
		# and marked as a livery, comes out exactly as wide as drawn (30 ft,
		# y -15..15 in the game's axes) even when the hull is thinned to a
		# tight budget; the hull is cut along its edges.
		python3 - "$WORK/plan/ellipsoid.json" "$WORK/plan/livery.json" <<'PY'
import json, sys
def unrle(s):
    out = []
    for part in s.split(','):
        n, v = part.split('*'); out += [int(v)] * int(n)
    return out
def rle(a):
    parts = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        parts.append(f"{j-i}*{a[i]}"); i = j
    return ",".join(parts)
plan = json.load(open(sys.argv[1])); W = plan['width']
plan['palette'].append({"colour": "#c02020", "name": "Stripe", "theme": -1, "livery": True})
plan['maxFaces'] = 2000
for l in plan['layers']:
    p = unrle(l['paint'])
    for cy in range(plan['depth']):
        for cx in range(28, 34):
            for k in range(4): p[(cy * W + cx) * 4 + k] = 1
    l['paint'] = rle(p)
json.dump(plan, open(sys.argv[2], 'w'))
PY
		"$SHIPGEN" build-plan "$WORK/plan/livery.json" --out "$WORK/plan/livery.SoH" --no-furnish > "$WORK/plan/livery.txt" 2>&1
		liveryBox="$("$SHIPGEN" faces "$WORK/plan/livery.SoH" 1 --where 2>&1 | grep 'paint #ffc02020')"
		if grep -q 'Livery colours: the hull was cut' "$WORK/plan/livery.txt" && grep -q '^Hull         : 2000 faces' "$WORK/plan/livery.txt" &&
				grep -q ' y -15.00..15.00 ' <<< "$liveryBox"; then
			pass=$((pass+1))
			echo "  ok    a livery colour keeps its drawn edges on a thinned model hull"
		else
			fail=$((fail+1))
			echo "  FAIL  a livery colour on a model hull"
			grep -E '^Hull|Livery' "$WORK/plan/livery.txt"
			echo "$liveryBox"
		fi
		# A colour's finish (transparency, self-lit, polish, glow, effect)
		# goes onto every hull face painted in it, and survives a save.
		python3 - "$WORK/plan/livery.json" "$WORK/plan/finish.json" <<'PY'
import json, sys
plan = json.load(open(sys.argv[1]))
plan['palette'][0].update(shine=100, reflect=0.6)
plan['palette'][1].update(livery=False, opacity=0.4, selfLit=0.3, glow="#0055ff", effect=9)
json.dump(plan, open(sys.argv[2], 'w'))
PY
		"$SHIPGEN" build-plan "$WORK/plan/finish.json" --out "$WORK/plan/finish.SoH" --no-furnish > "$WORK/plan/finish.txt" 2>&1
		"$SHIPGEN" plan-flip "$WORK/plan/finish.json" "$WORK/plan/finish-saved.json" --clockwise > /dev/null 2>&1
		finishFaces="$("$SHIPGEN" faces "$WORK/plan/finish.SoH" 1 2>&1)"
		if grep -q 'shine 100  reflect 0.60  effect No Effect$' <<< "$finishFaces" &&
				grep -q 'opacity 0.40  self-lit 0.30  shine 0  reflect 0.00  effect Shield  glow #0055ff$' <<< "$finishFaces" &&
				python3 -c 'import json,sys; p=json.load(open(sys.argv[1]))["palette"]; sys.exit(0 if p[1].get("opacity")==0.4 and p[1].get("glow")=="#0055ff" and p[1].get("effect")==9 and p[0].get("reflect")==0.6 else 1)' "$WORK/plan/finish-saved.json"; then
			pass=$((pass+1))
			echo "  ok    a colour's finish (transparency, polish, glow, effect) is on its hull faces and saved"
		else
			fail=$((fail+1))
			echo "  FAIL  a colour's finish"
			echo "$finishFaces" | head -5
		fi
		# Ceiling windows by the triangle: following the hull, four cells and
		# two triangles make one outline whose glass is the hull's own; flat,
		# a whole cell, a half cell and one triangle are each cut. Looking up
		# the shaft to the hull's glass at a slant meets its walls (they were
		# missing, and the hull was seen through, 2026-11-07), straight up
		# the sky.
		python3 "$HERE/testdata/add_windows.py" "$WORK/plan/ellipsoid.json" "$WORK/plan/hullsky.json" "" skylights
		"$SHIPGEN" build-plan "$WORK/plan/hullsky.json" --out "$WORK/plan/hullsky.SoH" > "$WORK/plan/hullsky.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/hullsky.SoH" > "$WORK/plan/hullsky-parts.txt" 2>&1
		if grep -q '^Ceiling wind.: 4 (1 following the hull)$' "$WORK/plan/hullsky.txt" && grep -q 'Errors (0)' "$WORK/plan/hullsky.txt" &&
				[ "$(grep -c '  Window  \[Lounge 1\]' "$WORK/plan/hullsky-parts.txt")" -eq 4 ] &&
				"$SHIPGEN" audit "$WORK/plan/hullsky.SoH" 2>&1 | qgrep 'Rooms outside the hull: none' &&
				"$SHIPGEN" sight "$WORK/plan/hullsky.SoH" 10.3 -7.3 33 0.25 0 1 | qgrep 'Hull \[Frame\]' &&
				"$SHIPGEN" sight "$WORK/plan/hullsky.SoH" 10.3 -7.3 33 0 0.3 1 | qgrep 'Hull \[Frame\]' &&
				"$SHIPGEN" sight "$WORK/plan/hullsky.SoH" 10.3 -7.3 33 0 0 1 | qgrep '^nothing'; then
			pass=$((pass+1))
			echo "  ok    ceiling windows by the triangle, following the hull or flat; the shaft up to the hull is walled"
		else
			fail=$((fail+1))
			echo "  FAIL  ceiling windows by the triangle"
			grep -E '^Ceiling|Error|ceiling window' "$WORK/plan/hullsky.txt"
		fi
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
	# A spacecraft landing spot: a 238 ft square that need not lie in a room;
	# a second one overlapping it is dropped, and the build warns that it
	# reaches past the small example's hull.
	sed -e 's|^    "parking": \[$|    "parking": [ {"deck": 0, "x": 4, "y": 4, "kind": "spacecraft"}, {"deck": 0, "x": 6, "y": 4, "kind": "spacecraft"}|' \
		"$WORK/plan/example.json" > "$WORK/plan/landing.json"
	"$SHIPGEN" build-plan "$WORK/plan/landing.json" --out "$WORK/plan/landing.SoH" > "$WORK/plan/landing.txt" 2>&1
	"$SHIPGEN" parts "$WORK/plan/landing.SoH" > "$WORK/plan/landing-parts.txt" 2>&1
	if grep -q '^Parking      : 1$' "$WORK/plan/landing.txt" && grep -q 'Errors (0)' "$WORK/plan/landing.txt" &&
			qgrep 'landing spot on deck 1 (238 ft square) reaches past' "$WORK/plan/landing.txt" &&
			[ "$(grep -c 'Spacecraft Parking Spot' "$WORK/plan/landing-parts.txt")" = 1 ]; then
		pass=$((pass+1))
		echo "  ok    a spacecraft landing spot, kept clear of another one"
	else
		fail=$((fail+1))
		echo "  FAIL  the spacecraft landing spot is wrong"
		grep -E '^Parking|Errors|landing|Spacecraft' "$WORK/plan/landing.txt" "$WORK/plan/landing-parts.txt" | head -5
	fi
	# The owner's flag and the empire's name on the hull: upright screens on
	# the side walls (they stand vertical: no extent in x on the sides).
	python3 - "$WORK/plan/example.json" "$WORK/plan/flag.json" <<'PYEOF' 2>/dev/null
import json, sys
d = json.load(open(sys.argv[1]))
d['emitters'] = [{"deck": 0, "x": 10, "y": 2, "side": 0, "kind": "flag", "size": 10},
                 {"deck": 0, "x": 10, "y": 13, "side": 2, "kind": "name", "size": 16}]
json.dump(d, open(sys.argv[2], 'w'), indent=1)
PYEOF
	if [ -f "$WORK/plan/flag.json" ]; then
		"$SHIPGEN" build-plan "$WORK/plan/flag.json" --out "$WORK/plan/flag.SoH" > "$WORK/plan/flag.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/flag.SoH" > "$WORK/plan/flag-parts.txt" 2>&1
		if grep -q '^Markings     : 2$' "$WORK/plan/flag.txt" && grep -q 'Errors (0)' "$WORK/plan/flag.txt" &&
				awk '/Empire (Flag|Name)/{ for(i=1;i<=NF;i++){ if($i=="x"){ split($(i+1),b,"\\.\\."); if(b[2]-b[1] > 1) bad=1 } if($i=="z"){ split($(i+1),c,"\\.\\."); if(c[2]-c[1] < 3) bad=1 } } n++ } END{ exit (n==2 && !bad)?0:1 }' "$WORK/plan/flag-parts.txt"; then
			pass=$((pass+1))
			echo "  ok    an empire flag and name stand upright on the hull's sides"
		else
			fail=$((fail+1))
			echo "  FAIL  the empire flag or name is wrong"
			grep -E 'Markings|Errors' "$WORK/plan/flag.txt"; grep Empire "$WORK/plan/flag-parts.txt"
		fi
	else
		echo "  skip  empire flags (needs python3)"
	fi
	# A turbolift: a "Turbo Lift" room on the same 2 by 2 cells of both decks,
	# with a wide door into the hall on deck 1 and a door into the bridge on
	# deck 2, is one lift car with a stop and a lift door on each deck. A car
	# 14 ft high pokes out of the top of the hull at its top stop: a warning.
	python3 - "$WORK/plan/example.json" "$WORK/plan/lift.json" "$WORK/plan/lift14.json" <<'PYEOF' 2>/dev/null
import json, sys
def dec(sv):
    out = []
    for tok in sv.split(','):
        n, v = tok.split('*'); out += [int(v)] * int(n)
    return out
def enc(a):
    out = []; i = 0
    while i < len(a):
        j = i
        while j < len(a) and a[j] == a[i]: j += 1
        out.append('%d*%d' % (j - i, a[i])); i = j
    return ','.join(out)
for target, car in ((sys.argv[2], 0), (sys.argv[3], 14)):
    d = json.load(open(sys.argv[1])); W = d['width']
    d['rooms'].append({"type": "Turbo Lift", "label": "Lift 1", "colour": "#808080"})
    if car: d['rooms'][-1]['carHeight'] = car
    li = len(d['rooms']) - 1
    for deck in (0, 1):
        r = dec(d['layers'][deck]['room'])
        for cx in (16, 17):
            for cy in (10, 11):
                for k in range(4): r[(cy * W + cx) * 4 + k] = li
        d['layers'][deck]['room'] = enc(r)
    d['doors'] += [{"deck": 0, "side": 0, "x": 16, "y": 10}, {"deck": 0, "side": 0, "x": 17, "y": 10},
                   {"deck": 1, "side": 1, "x": 17, "y": 10}]
    json.dump(d, open(target, 'w'), indent=1)
PYEOF
	if [ -f "$WORK/plan/lift.json" ]; then
		"$SHIPGEN" build-plan "$WORK/plan/lift.json" --out "$WORK/plan/lift.SoH" > "$WORK/plan/lift.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/lift.SoH" > "$WORK/plan/lift-parts.txt" 2>&1
		"$SHIPGEN" crew-check "$WORK/plan/lift.SoH" > "$WORK/plan/lift-crew.txt" 2>&1
		"$SHIPGEN" build-plan "$WORK/plan/lift14.json" --out "$WORK/plan/lift14.SoH" > "$WORK/plan/lift14.txt" 2>&1
		if grep -q '^Lifts        : 1 (2 stops)$' "$WORK/plan/lift.txt" && grep -q 'Errors (0)' "$WORK/plan/lift.txt" &&
				grep -q 'Warnings (0)' "$WORK/plan/lift.txt" &&
				[ "$(grep -c '  Turbo Lift  \[Lift 1\]' "$WORK/plan/lift-parts.txt")" = 1 ] &&
				[ "$(grep -c 'Turbo Lift Door' "$WORK/plan/lift-parts.txt")" = 2 ] &&
				qgrep -E 'Crew routes +: ([0-9]+) of \1 ' "$WORK/plan/lift-crew.txt" &&
				grep -q 'Errors (0)' "$WORK/plan/lift14.txt" &&
				qgrep "car of turbolift 'Lift 1' (14.0 ft high) leaves the hull" "$WORK/plan/lift14.txt"; then
			pass=$((pass+1))
			echo "  ok    a turbolift: one car, a stop and a lift door on each deck"
		else
			fail=$((fail+1))
			echo "  FAIL  the turbolift is wrong"
			grep -E 'Lifts|Errors|Warning|lift' "$WORK/plan/lift.txt" "$WORK/plan/lift14.txt" | head -8
			grep -E 'Turbo' "$WORK/plan/lift-parts.txt" | head -4; grep 'Crew routes' "$WORK/plan/lift-crew.txt"
		fi
	else
		echo "  skip  turbolifts (needs python3)"
	fi
	# Door buttons: two on the walls either side of the door between the
	# engine room and the hall; the door's switches go on them and it is no
	# longer automatic (shipgen parts shows the switches).
	python3 - "$WORK/plan/example.json" "$WORK/plan/buttons.json" <<'PYEOF' 2>/dev/null
import json, sys
d = json.load(open(sys.argv[1]))
d['doors'][0]['buttons'] = [[63, 68], [65, 52]]
json.dump(d, open(sys.argv[2], 'w'), indent=1)
PYEOF
	if [ -f "$WORK/plan/buttons.json" ]; then
		"$SHIPGEN" build-plan "$WORK/plan/buttons.json" --out "$WORK/plan/buttons.SoH" > "$WORK/plan/buttons.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/buttons.SoH" > "$WORK/plan/buttons-parts.txt" 2>&1
		if grep -q '^Door buttons : 2$' "$WORK/plan/buttons.txt" && grep -q 'Errors (0)' "$WORK/plan/buttons.txt" &&
				grep -q 'Warnings (0)' "$WORK/plan/buttons.txt" &&
				qgrep -E '  Door  \[Door\]  switches \(-2\.5, -50\.2, 4\.5\) \(7\.5, -49\.8, 4\.5\)' "$WORK/plan/buttons-parts.txt" &&
				[ "$(grep -cE '  Mesh  \[(Engine Room|Hall) 1\]  24 tris' "$WORK/plan/buttons-parts.txt")" = 2 ]; then
			pass=$((pass+1))
			echo "  ok    door buttons on the walls either side, with the door's switches on them"
		else
			fail=$((fail+1))
			echo "  FAIL  the door buttons are wrong"
			grep -E 'Door buttons|Errors|Warning' "$WORK/plan/buttons.txt"; grep -E '  Door  \[Door\]|Mesh' "$WORK/plan/buttons-parts.txt" | head -4
		fi
	else
		echo "  skip  door buttons (needs python3)"
	fi
	# A hull door's buttons: one inside on the wall beside it, where it was
	# clicked along the wall (not at the middle of a cell edge), and one
	# outside on the hull (off the grid), a hull part; the door's inside and
	# outside switches go on them.
	python3 - "$WORK/plan/example.json" "$WORK/plan/hullbuttons.json" <<'PYEOF' 2>/dev/null
import json, sys
d = json.load(open(sys.argv[1]))
d['doors'][2]['buttons'] = [[90.88, 127.84], [91.84, 128.96]]
json.dump(d, open(sys.argv[2], 'w'), indent=1)
PYEOF
	if [ -f "$WORK/plan/hullbuttons.json" ]; then
		"$SHIPGEN" build-plan "$WORK/plan/hullbuttons.json" --out "$WORK/plan/hullbuttons.SoH" > "$WORK/plan/hullbuttons.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/hullbuttons.SoH" > "$WORK/plan/hullbuttons-parts.txt" 2>&1
		if grep -q '^Door buttons : 2$' "$WORK/plan/hullbuttons.txt" && grep -q 'Errors (0)' "$WORK/plan/hullbuttons.txt" &&
				! grep -q 'A button of a door' "$WORK/plan/hullbuttons.txt" &&
				qgrep -E '  Hull Door  \[Hull Door\]  switches \(-39\.8, -33\.2, 4\.5\) \(-42\.4, -32\.6, 4\.5\)' "$WORK/plan/hullbuttons-parts.txt" &&
				qgrep -E '  Hull  \[Hull Door\]  24 tris' "$WORK/plan/hullbuttons-parts.txt"; then
			pass=$((pass+1))
			echo "  ok    a hull door's buttons inside where clicked along the wall, and outside on the hull"
		else
			fail=$((fail+1))
			echo "  FAIL  a hull door's buttons are wrong"
			grep -E 'Door buttons|Errors|button' "$WORK/plan/hullbuttons.txt"; grep -E '  Hull Door  \[|\[Hull Door\]' "$WORK/plan/hullbuttons-parts.txt" | head -4
		fi
	else
		echo "  skip  hull door buttons (needs python3)"
	fi
	# A window two cells wide between the hall and the crew quarters, and a
	# balcony between the engine room and the hall; one more "window" on a
	# hull edge is not between two rooms and is dropped.
	python3 - "$WORK/plan/example.json" "$WORK/plan/openings.json" <<'PYEOF' 2>/dev/null
import json, sys
d = json.load(open(sys.argv[1]))
d['doors'] += [{"deck": 0, "side": 0, "x": 17, "y": 6, "row": 50, "kind": "window"},
               {"deck": 0, "side": 0, "x": 18, "y": 6, "row": 50, "kind": "window"},
               {"deck": 0, "side": 3, "x": 8, "y": 8, "row": 51, "kind": "balcony"},
               {"deck": 0, "side": 0, "x": 3, "y": 2, "row": 52, "kind": "window"}]
json.dump(d, open(sys.argv[2], 'w'), indent=1)
PYEOF
	if [ -f "$WORK/plan/openings.json" ]; then
		"$SHIPGEN" build-plan "$WORK/plan/openings.json" --out "$WORK/plan/openings.SoH" > "$WORK/plan/openings.txt" 2>&1
		"$SHIPGEN" parts "$WORK/plan/openings.SoH" > "$WORK/plan/openings-parts.txt" 2>&1
		if grep -q '^Inner windows: 1$' "$WORK/plan/openings.txt" && grep -q '^Balconies    : 1$' "$WORK/plan/openings.txt" &&
				grep -q '^Doors        : 3$' "$WORK/plan/openings.txt" && grep -q 'Errors (0)' "$WORK/plan/openings.txt" &&
				[ "$(grep -c '  Window  \[Hall 1\]  2 tris.* y -4\.7\.\.4\.7' "$WORK/plan/openings-parts.txt")" = 2 ]; then
			pass=$((pass+1))
			echo "  ok    a window and a balcony between rooms"
		else
			fail=$((fail+1))
			echo "  FAIL  the window or balcony between rooms is wrong"
			grep -E 'Inner|Balcon|Doors  |Errors|Warning' "$WORK/plan/openings.txt"; grep 'Window  \[Hall' "$WORK/plan/openings-parts.txt"
		fi
	else
		echo "  skip  openings between rooms (needs python3)"
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
			"$SHIPGEN" inspect "$WORK/plan/sloped.SoH" | qgrep 'Rooms outside the hull: 0'; then
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
				--step 6 --build --screenshot "$WORK/designer.png" > "$WORK/designer.log" 2>&1; then
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
