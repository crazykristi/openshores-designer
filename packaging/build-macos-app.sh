#!/usr/bin/env bash
# Packs the programs for macOS, on a Mac, after ./build.sh:
#
#   packaging/build-macos-app.sh   -> build/shipgen-designer-<version>-macos-<arm64|x86_64>.dmg
#
# The dmg holds "Shipgen Designer.app", with Qt inside it. The designer is
# the app; shipgen-gui, shipgen-mesh and the shipgen command line are in
# Contents/MacOS beside it. It is signed ad hoc, not with an Apple Developer
# account, so macOS asks the first time it is opened (packaging/macos-readme.txt).
# It runs on the machine's kind of processor and on the macOS it was built on
# or newer. .github/workflows/macos.yml runs this on GitHub's Macs.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
die() { echo "error: $*" >&2; exit 1; }
[ "$(uname -s)" = Darwin ] || die "this script runs on macOS"
# The private building designs are never for a release.
[ ! -e "$HERE/src/prototypes.cpp" ] || die "src/prototypes.cpp is here: move it away, touch src/aether.cpp and build again"

VERSION="$(sed -n 's/^#define SHIPGEN_VERSION "\(.*\)"$/\1/p' "$HERE/src/version.h")"
[ -n "$VERSION" ] || die "no version in src/version.h"
ARCH="$(uname -m)"
BIN="$HERE/build/bin"
QMAKE="${QMAKE:-$(brew --prefix qt@5)/bin/qmake}"
QT_BINS="$("$QMAKE" -query QT_INSTALL_BINS)"
QT_LIBS="$("$QMAKE" -query QT_INSTALL_LIBS)"
PROGRAMS="shipgen-designer shipgen-gui shipgen-mesh shipgen"
for p in $PROGRAMS; do
	[ -x "$BIN/$p" ] || die "$BIN/$p is missing: run ./build.sh first"
done

OUT="$HERE/build/macos"
APP="$OUT/stage/Shipgen Designer.app"
rm -rf "$OUT"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
for p in $PROGRAMS; do
	cp "$BIN/$p" "$APP/Contents/MacOS/$p"
done

# The icon, from the 256 pixel one the AppImage uses.
ICONSET="$OUT/shipgen-designer.iconset"
mkdir -p "$ICONSET"
for size in 16 32 64 128 256; do
	sips -z "$size" "$size" "$HERE/packaging/shipgen-designer.png" --out "$ICONSET/icon_${size}x${size}.png" > /dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/shipgen-designer.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Shipgen Designer</string>
	<key>CFBundleDisplayName</key><string>Shipgen Designer</string>
	<key>CFBundleIdentifier</key><string>io.github.crazykristi.shipgen-designer</string>
	<key>CFBundleExecutable</key><string>shipgen-designer</string>
	<key>CFBundleIconFile</key><string>shipgen-designer</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>${VERSION%%-*}</string>
	<key>CFBundleVersion</key><string>${VERSION}</string>
	<key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST
plutil -lint "$APP/Contents/Info.plist"

# Qt and what it needs go into the app. The database plugins other than
# SQLite pull in client libraries nothing here uses.
args=()
for p in shipgen-gui shipgen-mesh shipgen; do
	args+=("-executable=$APP/Contents/MacOS/$p")
done
"$QT_BINS/macdeployqt" "$APP" "${args[@]}" -always-overwrite
find "$APP/Contents/PlugIns/sqldrivers" -type f ! -name 'libqsqlite*' -delete 2>/dev/null || true

# The way to the machine's own Qt comes out, and nothing may still lean on it:
# it would run on this Mac and on nobody else's.
for p in $PROGRAMS; do
	install_name_tool -delete_rpath "$QT_LIBS" "$APP/Contents/MacOS/$p" 2>/dev/null || true
done
bad=0
while IFS= read -r -d '' file; do
	if file -b "$file" | grep -q 'Mach-O'; then
		if otool -L "$file" | tail -n +2 | grep -E '/opt/homebrew|/usr/local/(opt|Cellar|lib)'; then
			echo "  ... in $file" >&2
			bad=1
		fi
	fi
done < <(find "$APP/Contents" -type f -print0)
[ "$bad" = 0 ] || die "the app still needs libraries from outside it"

# Apple's processors run nothing unsigned, and the changes above broke the
# signatures that were there.
codesign --force --deep --sign - "$APP"
codesign --verify --deep "$APP"

# It has to work from inside the app, with no help from the machine's Qt.
WORK="$OUT/try"
mkdir -p "$WORK"
got="$("$APP/Contents/MacOS/shipgen" --version)"
[ "$got" = "shipgen $VERSION" ] || die "the app's shipgen says '$got'"
"$APP/Contents/MacOS/shipgen" plan-example "$WORK/example.json" > /dev/null
"$APP/Contents/MacOS/shipgen" build-plan "$WORK/example.json" --out "$WORK/example.SoH" > "$WORK/build.txt"
grep -q '^Errors (0):' "$WORK/build.txt" || { cat "$WORK/build.txt" >&2; die "the example plan did not build"; }

STAGE="$OUT/stage"
ln -s /Applications "$STAGE/Applications"
cp "$HERE/packaging/macos-readme.txt" "$STAGE/READ ME FIRST.txt"
cp "$HERE/RELEASE_NOTES.md" "$STAGE/RELEASE_NOTES.txt"
cp "$HERE/third_party/soh/LICENSE" "$STAGE/LICENSE-Shores-of-Hazeron.txt"
DMG="$HERE/build/shipgen-designer-$VERSION-macos-$ARCH.dmg"
rm -f "$DMG"
# hdiutil now and then finds the disk busy on a build machine.
for attempt in 1 2 3 4 5; do
	if hdiutil create -volname "Shipgen Designer $VERSION" -srcfolder "$STAGE" -format UDZO -ov "$DMG"; then
		break
	fi
	[ "$attempt" != 5 ] || die "hdiutil could not make the dmg"
	sleep 10
done
echo "built $DMG ($(du -h "$DMG" | cut -f1))"
