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
	# the working designs. Walk paths lie on the floors, and the hull door,
	# 3.4 ft above the ground, has a ramp down to below the hull (a Barrier
	# to walk on and a walk path down it, as at the user's aircraft factory),
	# with only the ladder between the decks; with --no-ramps it has a
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
			[ "$(grep -E 'Walk Path .* z [0-9.-]+\.\.' "$WORK/plan/parts.txt" | grep -vcE ' z (0\.5\.\.0\.5|10\.5\.\.10\.5)$')" = "1" ] &&
			grep -E 'Walk Path .* z -2\.4\.\.0\.5$' "$WORK/plan/parts.txt" > /dev/null; then
		pass=$((pass+1))
		echo "  ok    the walk paths lie on the floors and a ramp leads down from the hull door"
	else
		fail=$((fail+1))
		echo "  FAIL  the walk paths are off the floors, or the hull door has no ramp"
		grep -E 'Ladder Path|Walk Path|Barrier' "$WORK/plan/parts.txt" | grep -vE ' z (0\.5\.\.0\.5|10\.5\.\.10\.5)$' | head -5
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
			grep -E 'Walk Path .* z 0\.5\.\.10\.5$' "$WORK/plan/stairs-parts.txt" > /dev/null &&
			! awk '/Walk Path .* z 10\.5\.\.10\.5$/{ for(i=1;i<=NF;i++){ if($i=="x") split($(i+1),a,"\\.\\."); if($i=="y") split($(i+1),b,"\\.\\.") } if(a[2] > -4.4 && a[1] < -0.6 && b[2] > 0.7 && b[1] < 14.8) bad=1 } END{ exit bad?0:1 }' "$WORK/plan/stairs-parts.txt"; then
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
	if [ -x "$TC/x11/Xvfb" ]; then
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
		if [ -n "$GUI_DISPLAY" ]; then
			preview_size=$(stat -c %s "$WORK/preview.png" 2>/dev/null || echo 0)
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
	if [ -x "$SHIPGEN_DESIGNER" ] && [ -n "$GUI_DISPLAY" ]; then
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
