#!/usr/bin/env bash
# Packs shipgen-gui into an AppImage, with the shipgen command line and the
# triangle designer inside it too:
#
#   ./build.sh && ./package-appimage.sh            -> build/shipgen-gui-x86_64.AppImage
#
#   ./shipgen-gui-x86_64.AppImage                   the generator's window (shipgen-gui)
#   ./shipgen-gui-x86_64.AppImage designer [...]    the triangle designer
#   ./shipgen-gui-x86_64.AppImage shipgen [...]     the command line
#
# Bundled: Qt 5 and its plugins (the X11 and Wayland platforms, image
# formats, icon engines), OpenAL and every other library the programs need,
# except those that have to come from the system the AppImage runs on: the C
# library, the OpenGL drivers, X11/xcb, fontconfig and freetype (the
# AppImage project's exclude list). The C library sets the floor: the
# AppImage runs on systems with a glibc at least as new as the one it was
# built on. For the widest reach, build it on an old distribution (for
# example in a container).
#
# appimagetool is downloaded into build/tools/ the first time
# (APPDIR_ONLY=1 makes only the folder, build/AppDir, without it).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$HERE/build/bin"
OUT="$HERE/build"
APPDIR="$OUT/AppDir"
TOOLS="$OUT/tools"
ARCH="$(uname -m)"
QT_PLUGINS="${QT_PLUGINS:-$(qmake-qt5 -query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib64/qt5/plugins)}"

for program in shipgen-gui shipgen-designer shipgen; do
	[ -x "$BIN/$program" ] || { echo "build/bin/$program is missing: run ./build.sh first" >&2; exit 1; }
done

# The libraries never bundled (from the AppImage project's excludelist:
# they must match the system's kernel, drivers or C library).
EXCLUDE='^(ld-linux.*|libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libnsl|libmvec|libthread_db|libBrokenLocale|libnss_.*|libstdc\+\+|libgcc_s|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libGLESv2|libdrm|libgbm|libglapi|libxcb|libX11|libX11-xcb|libxshmfence|libfontconfig|libfreetype|libharfbuzz|libexpat|libz|libasound|libjack|libpipewire-.*|libpulse.*|libsystemd|libudev|libdbus-1|libgpg-error|libcom_err|libuuid|libICE|libSM|libusb-1\.0)\.so'

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/plugins" "$APPDIR/usr/share/applications" \
	"$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$BIN/shipgen-gui" "$BIN/shipgen-designer" "$BIN/shipgen" "$APPDIR/usr/bin/"

# The Qt plugins the programs load.
for dir in platforms xcbglintegrations wayland-shell-integration wayland-graphics-integration-client \
		wayland-decoration-client imageformats iconengines platforminputcontexts; do
	[ -d "$QT_PLUGINS/$dir" ] && cp -r "$QT_PLUGINS/$dir" "$APPDIR/usr/plugins/"
done
# Only the platforms the AppImage needs: X11, Wayland, and offscreen for
# running without a display.
find "$APPDIR/usr/plugins/platforms" -name '*.so' ! -name 'libqxcb.so' ! -name 'libqwayland-*.so' \
	! -name 'libqoffscreen.so' -delete
# Qt's own image and icon plugins only (the desktop's own, KDE's for
# example, would bring the whole desktop's libraries along), and the plain
# input methods.
find "$APPDIR/usr/plugins/imageformats" "$APPDIR/usr/plugins/iconengines" -name '*.so' ! -name 'libq*.so' -delete 2>/dev/null || true
find "$APPDIR/usr/plugins/platforminputcontexts" -name '*.so' ! -name 'libcompose*.so' ! -name 'libibus*.so' -delete 2>/dev/null || true
cat > "$APPDIR/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
EOF

# Every library the programs and plugins need, but not the excluded ones.
bundle_deps() {
	ldd "$1" 2>/dev/null | awk '/=> \// { print $3 }' | while read -r lib; do
		name="$(basename "$lib")"
		if [[ "$name" =~ $EXCLUDE ]] || [ -e "$APPDIR/usr/lib/$name" ]; then
			continue
		fi
		cp -L "$lib" "$APPDIR/usr/lib/$name"
	done
}
for f in "$APPDIR"/usr/bin/shipgen* $(find "$APPDIR/usr/plugins" -name '*.so'); do
	bundle_deps "$f"
done
# The libraries' own dependencies (ldd lists them already, but a second
# pass catches anything a plugin brought in).
for f in "$APPDIR"/usr/lib/*.so*; do
	bundle_deps "$f"
done

# The launcher: the bundled libraries and plugins first, then the program
# asked for.
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
case "$1" in
	designer) shift; exec "$HERE/usr/bin/shipgen-designer" "$@" ;;
	shipgen)  shift; exec "$HERE/usr/bin/shipgen" "$@" ;;
	*)        exec "$HERE/usr/bin/shipgen-gui" "$@" ;;
esac
EOF
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/shipgen-gui.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Shipgen
GenericName=Ship and building designer
Comment=Designs ships and buildings for Shores of Hazeron and OpenShores
Exec=shipgen-gui
Icon=shipgen-gui
Categories=Graphics;3DGraphics;
Terminal=false
EOF
cp "$APPDIR/shipgen-gui.desktop" "$APPDIR/usr/share/applications/"
cp "$HERE/packaging/shipgen-gui.png" "$APPDIR/shipgen-gui.png"
cp "$HERE/packaging/shipgen-gui.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
ln -sf shipgen-gui.png "$APPDIR/.DirIcon"
desktop-file-validate "$APPDIR/shipgen-gui.desktop" 2>/dev/null || true

# APPDIR_ONLY=1 stops here: the folder build/AppDir is the whole program,
# and build/AppDir/AppRun runs it.
if [ "${APPDIR_ONLY:-0}" = "1" ]; then
	echo "built $APPDIR (run $APPDIR/AppRun)"
	exit 0
fi

# appimagetool, once.
mkdir -p "$TOOLS"
TOOL="$TOOLS/appimagetool-$ARCH.AppImage"
if [ ! -x "$TOOL" ]; then
	curl -fL -o "$TOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"
	chmod +x "$TOOL"
fi
TARGET="$OUT/shipgen-gui-$ARCH.AppImage"
rm -f "$TARGET"
# Without FUSE (containers), appimagetool can run unpacked.
ARCH="$ARCH" "$TOOL" --appimage-extract-and-run --no-appstream "$APPDIR" "$TARGET"
echo "built $TARGET ($(du -h "$TARGET" | cut -f1))"
