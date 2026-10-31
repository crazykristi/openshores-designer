#!/usr/bin/env bash
# Builds the Windows programs on Linux, with the MinGW-w64 cross compiler
# and Qt 5 in a Fedora container (podman or docker), and packs them in a zip:
#
#   packaging/build-windows-cross.sh   -> build/shipgen-designer-<version>-windows-x86_64.zip
#
# The image (packaging/containers/Containerfile.windows) is made the first
# time. Inside the zip, the folder shipgen-designer-<version>/ holds
# shipgen-designer.exe (the designer), shipgen-gui.exe, shipgen-mesh.exe,
# shipgen.exe (the command line) and the DLLs and Qt plugins they need.
# Try it without Windows: wine build/windows-dist/shipgen-designer-<version>/shipgen-designer.exe
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${CONTAINER_ENGINE:-$(command -v podman || command -v docker)}"
IMAGE=shipgen-windows
VERSION="$(sed -n 's/^#define SHIPGEN_VERSION "\(.*\)"$/\1/p' "$HERE/src/version.h")"

if ! "$ENGINE" image inspect "$IMAGE" >/dev/null 2>&1; then
	"$ENGINE" build -t "$IMAGE" -f "$HERE/packaging/containers/Containerfile.windows" "$HERE/packaging/containers"
fi
userns=()
[[ "$ENGINE" == *podman ]] && userns=(--userns=keep-id)
"$ENGINE" run --rm "${userns[@]}" --user "$(id -u):$(id -g)" --env HOME=/tmp --env VERSION="$VERSION" \
	-v "$HERE:/src:z" "$IMAGE" /bin/bash -ec '
		cd /src
		CROSS=x86_64-w64-mingw32 ./build.sh > build-windows/build.log 2>&1 \
			|| { grep -m10 "error" build-windows/build.log; tail -5 build-windows/build.log; exit 1; }
		if grep -q "error:" build-windows/build.log; then
			grep -m10 "error:" build-windows/build.log; exit 1
		fi
		PREFIX=$(x86_64-w64-mingw32-qmake-qt5 -query QT_INSTALL_PREFIX)
		PLUGINS=$(x86_64-w64-mingw32-qmake-qt5 -query QT_INSTALL_PLUGINS)
		NAME=shipgen-designer-$VERSION
		DIST=/src/build/windows-dist/$NAME
		rm -rf /src/build/windows-dist; mkdir -p "$DIST"
		for exe in shipgen-designer shipgen-gui shipgen-mesh shipgen; do
			x86_64-w64-mingw32-strip -o "$DIST/$exe.exe" build-windows/bin/$exe.exe
		done
		# The Qt plugins: the Windows platform, its native style, the image
		# formats and icon engines.
		mkdir -p "$DIST/plugins/platforms" "$DIST/plugins/styles" "$DIST/plugins/imageformats"
		cp "$PLUGINS/platforms/qwindows.dll" "$DIST/plugins/platforms/"
		cp "$PLUGINS"/styles/qwindowsvistastyle.dll "$DIST/plugins/styles/" 2>/dev/null || true
		for f in "$PLUGINS"/imageformats/q*.dll; do
			case "$f" in *d.dll) [ -e "${f%d.dll}.dll" ] && continue ;; esac
			cp "$f" "$DIST/plugins/imageformats/"
		done
		[ -d "$PLUGINS/iconengines" ] && cp -r "$PLUGINS/iconengines" "$DIST/plugins/"
		printf "[Paths]\nPlugins = plugins\n" > "$DIST/qt.conf"
		# Every DLL the programs and plugins import that comes with MinGW or
		# Qt (Windows itself has the rest), and theirs in turn.
		for pass in 1 2 3 4 5 6; do
			added=0
			for f in "$DIST"/*.exe "$DIST"/*.dll $(find "$DIST/plugins" -name "*.dll"); do
				[ -f "$f" ] || continue
				for dll in $(x86_64-w64-mingw32-objdump -p "$f" | sed -n "s/^\s*DLL Name: //p"); do
					[ -e "$DIST/$dll" ] && continue
					src=$(find "$PREFIX/bin" -maxdepth 1 -iname "$dll" | head -1)
					[ -n "$src" ] || continue
					cp "$src" "$DIST/"; added=1
				done
			done
			[ $added = 0 ] && break
		done
		cp /src/third_party/soh/LICENSE "$DIST/LICENSE-Shores-of-Hazeron.txt"
		sed "s/\$/\r/" /src/RELEASE_NOTES.md > "$DIST/RELEASE_NOTES.txt"
		sed "s/\$/\r/" /src/packaging/windows-readme.txt > "$DIST/README.txt"
		cd /src/build/windows-dist
		rm -f /src/build/$NAME-windows-x86_64.zip
		zip -qr9 /src/build/$NAME-windows-x86_64.zip "$NAME"
		echo "DLLs: $(ls "$DIST"/*.dll | wc -l)"
	'
echo "built $HERE/build/shipgen-designer-$VERSION-windows-x86_64.zip ($(du -h "$HERE/build/shipgen-designer-$VERSION-windows-x86_64.zip" | cut -f1))"
