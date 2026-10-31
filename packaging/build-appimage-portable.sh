#!/usr/bin/env bash
# Builds the AppImage on Ubuntu 22.04 in a container (podman or docker), so
# it runs on every distribution with glibc 2.35 or newer instead of only
# those as new as the machine it was built on:
#
#   packaging/build-appimage-portable.sh   -> build/shipgen-designer-<version>-x86_64.AppImage
#
# The image (packaging/containers/Containerfile.linux) is made the first
# time. The programs are built into build/ubuntu-22.04/, apart from the
# machine's own build in build/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${CONTAINER_ENGINE:-$(command -v podman || command -v docker)}"
IMAGE=shipgen-ubuntu22
OUTDIR="$HERE/build/ubuntu-22.04"

if ! "$ENGINE" image exists "$IMAGE" 2>/dev/null && ! "$ENGINE" image inspect "$IMAGE" >/dev/null 2>&1; then
	"$ENGINE" build -t "$IMAGE" -f "$HERE/packaging/containers/Containerfile.linux" "$HERE/packaging/containers"
fi
mkdir -p "$OUTDIR"
userns=()
[[ "$ENGINE" == *podman ]] && userns=(--userns=keep-id)
"$ENGINE" run --rm "${userns[@]}" --user "$(id -u):$(id -g)" --env HOME=/tmp \
	-v "$HERE:/src:z" "$IMAGE" /bin/bash -ec '
		# build.sh expects the toolchain laid out as Fedora has it.
		tc=/tmp/toolchain
		mkdir -p $tc/usr/include $tc/bin
		ln -sfn /usr/include/x86_64-linux-gnu/qt5 $tc/usr/include/qt5
		ln -sfn /usr/lib/x86_64-linux-gnu $tc/usr/lib64
		ln -sfn /usr/bin/g++ $tc/bin/g++
		TOOLCHAIN=$tc BUILD=/src/build/ubuntu-22.04 bash /src/build.sh > /src/build/ubuntu-22.04/build.log 2>&1 \
			|| { tail -30 /src/build/ubuntu-22.04/build.log; exit 1; }
		if grep -q "error:" /src/build/ubuntu-22.04/build.log; then
			grep -m5 "error:" /src/build/ubuntu-22.04/build.log; exit 1
		fi
		BIN=/src/build/ubuntu-22.04/bin OUT=/src/build/ubuntu-22.04 APPDIR_ONLY=1 bash /src/package-appimage.sh
	'
# appimagetool on this machine (it is downloaded once into build/tools/).
ARCH="$(uname -m)"
TOOL="$HERE/build/tools/appimagetool-$ARCH.AppImage"
if [ ! -x "$TOOL" ]; then
	mkdir -p "$(dirname "$TOOL")"
	curl -fL -o "$TOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"
	chmod +x "$TOOL"
fi
VERSION="$(sed -n 's/^#define SHIPGEN_VERSION "\(.*\)"$/\1/p' "$HERE/src/version.h")"
TARGET="$HERE/build/shipgen-designer-$VERSION-$ARCH.AppImage"
rm -f "$TARGET"
ARCH="$ARCH" "$TOOL" --appimage-extract-and-run --no-appstream "$OUTDIR/AppDir" "$TARGET"
echo "built $TARGET ($(du -h "$TARGET" | cut -f1)), for glibc 2.35 and newer"
