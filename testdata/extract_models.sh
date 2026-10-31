#!/usr/bin/env bash
#
# Puts the 3D models into the project so that they can be used as test data.
#
# The models live in zip files in "3d models" next to this project. They come
# from sites like Sketchfab and Thingiverse and are meant for 3D printing, so
# they are in millimetres and they are solid objects rather than hollow
# shells.
#
#   ./extract_models.sh [source directory]
#
# The default source directory is "3d models" next to this project.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${1:-$HERE/../../3d models}"
DEST="$HERE/models"

if [ ! -d "$SOURCE" ]; then
	echo "There is no directory '$SOURCE'."
	echo "Point this script at the folder that holds the zip files."
	exit 1
fi

mkdir -p "$DEST"

# The name of the file inside the zip, and where it goes.
extract() {
	local zip="$1" pattern="$2" name="$3"
	if [ ! -f "$SOURCE/$zip" ]; then
		echo "  skipping $zip, it is not there"
		return
	fi
	if [ -f "$DEST/$name" ]; then
		echo "  $name is already there"
		return
	fi
	local tmp="$DEST/.tmp"
	mkdir -p "$tmp"
	unzip -o -j "$SOURCE/$zip" "$pattern" -d "$tmp" > /dev/null
	local found
	found="$(find "$tmp" -name '*.stl' | head -1)"
	if [ -n "$found" ]; then
		mv "$found" "$DEST/$name"
	fi
	rm -rf "$tmp"
	echo "  $name"
}

echo "Taking the models out of the zip files:"

# The Aetherite is the Final Fantasy XIV crystal. It is in three parts that
# have to be put together: a bottom, a ring and a top.
extract "ffxiv-aetherite.zip" "files/Jewerly_bottom.stl" "aetherite_bottom.stl"
extract "ffxiv-aetherite.zip" "files/Jewerly_ring.stl" "aetherite_ring.stl"
extract "ffxiv-aetherite.zip" "files/Jewerly_top.stl" "aetherite_top.stl"

# The Moogle Warrior. The repaired file is the one that is solid, so it is
# the one that is used.
extract "moogle-warrior-final-fantasy-xiv.zip" "repaired_2. moogle_pre-supported.stl" "moogle_warrior.stl"
extract "moogle-warrior-final-fantasy-xiv.zip" "1. Moogle_Unsupported.stl" "moogle_warrior_unsupported.stl"

# The other miniatures.
extract "G'raha Final Fantasy XIV Mini for Table Top Gamming - 6869594.zip" "files/Graha.stl" "graha.stl"
extract "Uriangier Final Fantasy XIV Mini for Table Top Gamming - 6869598.zip" "files/Uriangier.stl" "uriangier.stl"
extract "Final Fantasy XI - Arc Angel Mithra - 3462067.zip" "files/arcangelmithra_fixed1.stl" "mithra.stl"
extract "y-shtola-final-fantasy-xiv-32mm.zip" "repaired_shtola-supported.stl" "shtola.stl"

echo
echo "The models are in $DEST:"
ls -la "$DEST"
