#!/usr/bin/env bash
#
# Put the Windows GUI and mesh tool into a folder that can be copied to a
# Windows machine.
#
# Run this in the MSYS2 MINGW64 shell after ./build.sh:
#
#   ./deploy-windows.sh
#
# It uses Qt's windeployqt for the Qt DLLs and plugins, then copies the
# MinGW runtime and OpenAL DLLs that the executables need. The result is
# build-windows/dist/. Keep the whole folder together; the DLLs are found
# next to the .exe files.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST="$(uname -s)"
case "$HOST" in
	MINGW*|MSYS*|CYGWIN*) ;;
	*) echo "error: run this in the MSYS2 MINGW64 shell on Windows" >&2; exit 1 ;;
esac

die() { echo "error: $*" >&2; exit 1; }

unix_path()
{
	if command -v cygpath >/dev/null 2>&1; then
		cygpath -u "$1"
	else
		printf '%s' "$1"
	fi
}

BUILD="${BUILD:-$HERE/build-windows}"
BIN="$BUILD/bin"
DIST="${DIST:-$BUILD/dist}"
QMAKE="${QMAKE:-}"
MINGW_PREFIX="$(unix_path "${MINGW_PREFIX:-/mingw64}")"

if [ -z "$QMAKE" ]; then
	for candidate in qmake-qt5 qmake qmake-qt5.exe qmake.exe; do
		if command -v "$candidate" >/dev/null 2>&1; then
			QMAKE="$candidate"
			break
		fi
	done
fi
[ -n "$QMAKE" ] || die "qmake was not found; open the MSYS2 MINGW64 shell"
command -v "$QMAKE" >/dev/null 2>&1 || die "qmake '$QMAKE' is not on PATH"

QT_BINS="$(unix_path "$("$QMAKE" -query QT_INSTALL_BINS)")"
WINDEPLOYQT=""
for candidate in windeployqt-qt5.exe windeployqt.exe windeployqt; do
	if [ -f "$QT_BINS/$candidate" ]; then
		WINDEPLOYQT="$QT_BINS/$candidate"
		break
	fi
done
[ -n "$WINDEPLOYQT" ] || die "windeployqt was not found in $QT_BINS"

# Some Qt 5.15 builds in MSYS2 shipped windeployqt under a name that looks
# for a plain qmake.exe. Current packages have this fixed, but the note is
# worth having if an older installation is used.
if [ ! -e "$QT_BINS/qmake.exe" ] && [ -e "$QT_BINS/qmake-qt5.exe" ]; then
	echo "note: Qt is using qmake-qt5.exe; if windeployqt reports 'Unable to query qmake', copy qmake-qt5.exe to qmake.exe in $QT_BINS and rerun." >&2
fi

for exe in shipgen-gui shipgen-mesh shipgen-designer; do
	[ -f "$BIN/$exe.exe" ] || die "missing $BIN/$exe.exe; run ./build.sh first"
done

echo "==> Collecting the Windows programs in $DIST"
rm -rf "$DIST"
mkdir -p "$DIST"
for exe in shipgen shipgen-gui shipgen-mesh shipgen-designer; do
	[ -f "$BIN/$exe.exe" ] && cp "$BIN/$exe.exe" "$DIST/"
done

# Qt's own deployment tool knows about the platform plugin, the image
# formats and the Qt DLLs. Running it once for each program is harmless and
# avoids relying on multi-file argument handling in older Qt versions.
for exe in shipgen-gui shipgen-mesh shipgen-designer; do
	echo "==> Deploying Qt for $exe.exe"
	PATH="$QT_BINS:$PATH" "$WINDEPLOYQT" --release --no-translations --dir "$DIST" "$DIST/$exe.exe"
done

# windeployqt does not know about OpenAL or the MinGW runtime. Copy every
# non-system DLL that ldd reports, then repeat over the DLLs that were just
# copied so transitive dependencies (for example OpenAL's libmysofa) come
# along too.
if command -v ldd >/dev/null 2>&1; then
	for pass in 1 2 3 4 5; do
		added=0
		for file in "$DIST"/*.exe "$DIST"/*.dll "$DIST"/*/*.dll; do
			[ -f "$file" ] || continue
			while read -r dll; do
				[ -n "$dll" ] || continue
				dll="$(unix_path "$dll")"
				case "$dll" in
					"$MINGW_PREFIX"/*|*/mingw64/*|*/clang64/*|*/ucrt64/*)
						name="$(basename "$dll")"
						if [ ! -e "$DIST/$name" ]; then
							cp "$dll" "$DIST/"
							added=1
						fi
						;;
				esac
			done < <(ldd "$file" 2>/dev/null | awk '/=> \// { print $3 }')
		done
		[ "$added" -eq 0 ] && break
	done
fi

# These are the common MinGW/OpenAL runtime names. ldd normally finds them,
# but copy them explicitly so a missing optional dependency does not stop
# the program from starting. Qt5OpenGL is here because the engine's optional
# rendering helpers may use it even when windeployqt does not see an import.
for dll in libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll \
	libgcc_s_dw2-1.dll libssp-0.dll OpenAL32.dll libopenal-1.dll libopenal.dll \
	libmysofa-1.dll zlib1.dll Qt5OpenGL.dll; do
	[ -f "$MINGW_PREFIX/bin/$dll" ] && cp -n "$MINGW_PREFIX/bin/$dll" "$DIST/"
done
# OpenAL Soft uses libmysofa for HRTF. Its file name carries a version, so
# copy whatever is installed.
for dll in "$MINGW_PREFIX"/bin/libmysofa*.dll; do
	[ -f "$dll" ] && cp -n "$dll" "$DIST/"
done

for file in Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll Qt5Sql.dll \
	platforms/qwindows.dll; do
	[ -e "$DIST/$file" ] || die "the deployment is missing $file"
done

if [ ! -e "$DIST/OpenAL32.dll" ] && [ ! -e "$DIST/libopenal-1.dll" ] && [ ! -e "$DIST/libopenal.dll" ]; then
	die "the OpenAL runtime was not found; install mingw-w64-x86_64-openal"
fi

echo
echo "deployed: $DIST"
echo "Copy that whole folder to the Windows machine and run shipgen-gui.exe or shipgen-mesh.exe."
