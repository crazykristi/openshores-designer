#!/usr/bin/env bash
#
# Build script for the Shores of Hazeron "Ship Layout Generator".
#
# Builds the headless subset of the Shores of Hazeron engine
# (AuUtil, AuMath, AuGL, AuGlobal, AuData) and links it into
# a standalone command line tool. No game client, no OpenGL context,
# no database and no network are required.
#
# Usage:
#   ./build.sh            # build libraries + tools
#   ./build.sh clean      # remove build artifacts
#   ./build.sh -j8        # build with 8 parallel jobs
#
# On Linux this writes to build/. In an MSYS2/MinGW-w64 shell on Windows it
# writes to build-windows/ and makes .exe files. See docs/WINDOWS.md.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOH_SRC="${SOH_SRC:-$HERE/third_party/soh}"

# MSYS2 reports something like MINGW64_NT-10.0; Git for Windows reports
# MINGW64_NT too. Cygwin is close enough to handle the same way.
HOST="$(uname -s)"
case "$HOST" in
	MINGW*|MSYS*|CYGWIN*) ON_WINDOWS=1 ;;
	*) ON_WINDOWS=0 ;;
esac

# ---------------------------------------------------------------- helpers --

die() { echo "error: $*" >&2; exit 1; }

need_file() { [ -e "$1" ] || die "missing $1"; }

# Include and link flags are passed as shell words, so a space in the
# checkout path would split them. Say so plainly instead of letting the
# compiler report a missing file with half a path.
case "$HERE" in
	*[[:space:]]*) die "the project path contains a space; move it to a path without spaces" ;;
esac

# qmake on Windows may print C:/... paths. The MinGW tools accept those, but
# normalising to /c/... keeps the shell tests and the -I flags predictable.
unix_path()
{
	if command -v cygpath >/dev/null 2>&1; then
		cygpath -u "$1"
	else
		printf '%s' "$1"
	fi
}

# ------------------------------------------------------------- toolchain --

if [ "$ON_WINDOWS" = 1 ]; then
	BUILD="${BUILD:-$HERE/build-windows}"
else
	BUILD="${BUILD:-$HERE/build}"
fi
OBJ="$BUILD/obj"
LIBDIR="$BUILD/lib"
BIN="$BUILD/bin"

# The commit of the upstream engine this tool is built against.
SOH_REPO="https://github.com/Hazeron-Freehold-LLC/Shores-of-Hazeron.git"
SOH_COMMIT="5e6933492d3ad1cd19642903012ff6ca12cb4d52"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
if [ "${1:-}" = "-j" ]; then
	JOBS="${2:-$JOBS}"
	shift
	# -j usually comes with a number; accept it on its own too.
	if [ $# -gt 0 ]; then shift; fi
elif [ "${1:-}" = "-j"* ]; then
	JOBS="${1#-j}"
	shift
fi

EXE=""
PTHREAD="-lpthread"
GUI_LDFLAGS=""
QTMAIN=""

if [ "$ON_WINDOWS" = 1 ]; then
	# Use the Qt and compiler that the MINGW64 shell has on its PATH. The
	# Linux-style ~/.local toolchain does not exist on Windows.
	CXX="${CXX:-g++}"
	QMAKE="${QMAKE:-}"
	if [ -z "$QMAKE" ]; then
		# MSYS2 names the Qt 5 tool qmake-qt5.exe. A plain qmake may also be
		# present in other distributions.
		for candidate in qmake-qt5 qmake qmake-qt5.exe qmake.exe; do
			if command -v "$candidate" >/dev/null 2>&1; then
				QMAKE="$candidate"
				break
			fi
		done
	fi
	command -v "$CXX" >/dev/null 2>&1 || die "C++ compiler '$CXX' is not on PATH"
	[ -n "$QMAKE" ] || die "qmake was not found; open the MSYS2 MINGW64 shell (see docs/WINDOWS.md)"
	command -v "$QMAKE" >/dev/null 2>&1 || die "qmake '$QMAKE' is not on PATH"

	QT_INC="$(unix_path "$("$QMAKE" -query QT_INSTALL_HEADERS)")"
	QT_LIBS="$(unix_path "$("$QMAKE" -query QT_INSTALL_LIBS)")"
	QT_BINS="$(unix_path "$("$QMAKE" -query QT_INSTALL_BINS)")"
	MOC="$QT_BINS/moc.exe"
	[ -f "$MOC" ] || MOC="$QT_BINS/moc"
	RCC="$QT_BINS/rcc.exe"
	[ -f "$RCC" ] || RCC="$QT_BINS/rcc"

	EXE=".exe"
	PTHREAD=""
	# MinGW ignores -fPIC; -mwindows gives the GUI programs no console
	# window. That needs Qt's qtmain wrapper, which is detected below.
	# WIN32 is what qmake defines on Windows; a couple of engine headers use
	# it rather than Q_OS_WIN for their Windows-only overloads.
	CXXFLAGS="-std=c++11 -O2 -w -DNOMINMAX -DWIN32"
	CXXFLAGS_TOOL="-std=c++11 -O2 -g -w -DNOMINMAX -DWIN32"

	MINGW_PREFIX="$(unix_path "${MINGW_PREFIX:-/mingw64}")"
	# The engine says #include <al.h> on Windows, while OpenAL Soft installs
	# its headers in include/AL. pkg-config knows; fall back to the usual
	# MSYS2 location.
	OPENAL_CFLAGS="${OPENAL_CFLAGS:-$(pkg-config --cflags openal 2>/dev/null || true)}"
	if [ -z "$OPENAL_CFLAGS" ] && [ -d "$MINGW_PREFIX/include/AL" ]; then
		OPENAL_CFLAGS="-I$MINGW_PREFIX/include/AL"
	fi
	OPENAL_LIBS="${OPENAL_LIBS:-$(pkg-config --libs openal 2>/dev/null || true)}"
	[ -n "$OPENAL_LIBS" ] || OPENAL_LIBS="-lopenal"

	INCS="-I$QT_INC -I$QT_INC/QtCore -I$QT_INC/QtGui -I$QT_INC/QtWidgets -I$QT_INC/QtNetwork -I$QT_INC/QtSql -I$QT_INC/QtOpenGL $OPENAL_CFLAGS"
	QLIBS="-L$QT_LIBS -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Network -lQt5Sql -lQt5OpenGL $OPENAL_LIBS -lopengl32"

	if [ -f "$QT_LIBS/libqtmain.a" ]; then
		QTMAIN="-lqtmain"
		GUI_LDFLAGS="-mwindows"
	elif [ -f "$QT_LIBS/libqtmaind.a" ]; then
		QTMAIN="-lqtmaind"
		GUI_LDFLAGS="-mwindows"
	else
		# Without Qt's WinMain wrapper, a -mwindows build would not link.
		# Keep a console window instead of failing the build.
		echo "warning: Qt's qtmain library was not found; the GUI programs will keep a console window" >&2
	fi
else
	TOOLCHAIN="${TOOLCHAIN:-$HOME/.local/hazeron-toolchain}"
	QTDIR="$TOOLCHAIN/usr"
	QT_INC="$QTDIR/include/qt5"
	QT_LIBS="$QTDIR/lib64"
	MOC="$QT_LIBS/qt5/bin/moc"
	RCC="$QT_LIBS/qt5/bin/rcc"

	CXX="${CXX:-$TOOLCHAIN/bin/g++}"
	# The engine libraries are big, so they are built without debug info. The
	# tool's own sources keep it.
	CXXFLAGS="-std=c++11 -fPIC -O2 -w -pthread"
	CXXFLAGS_TOOL="-std=c++11 -fPIC -O2 -g -w -pthread"
	INCS="-I$QT_INC -I$QT_INC/QtCore -I$QT_INC/QtGui -I$QT_INC/QtWidgets -I$QT_INC/QtNetwork -I$QT_INC/QtSql -I$QT_INC/QtOpenGL -I$QTDIR/include"
	QLIBS="-L$QT_LIBS -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Network -lQt5Sql -lQt5OpenGL -lopenal -lGL"
fi

# Compile every .cpp in a directory into objects under $OBJ/<tag>/
#   compile_tree <tag> <srcdir> <defines...> -- <extra include dirs...>
compile_tree() {
	local tag="$1"; shift
	local srcdir="$1"; shift
	local defines=() incs=()
	local seen_sep=0
	for a in "$@"; do
		if [ "$a" = "--" ]; then seen_sep=1; continue; fi
		if [ $seen_sep -eq 0 ]; then defines+=("-D$a"); else incs+=("-I$a"); fi
	done
	local outdir="$OBJ/$tag"
	mkdir -p "$outdir"
	local n=0
	# The library's own directory is implicitly on the include path (as qmake does).
	incs+=("-I$srcdir")
	for src in "$srcdir"/*.cpp; do
		[ -e "$src" ] || continue
		local base; base="$(basename "$src" .cpp)"
		# Skip sources that need assets we do not ship.
		case "$base" in
			auempirereport|main) continue ;;
		esac
		local obj="$outdir/${base}.o"
		# A source can depend on the headers beside it, so a change to any of
		# them has to rebuild the source as well. Without this a header that
		# grows a field leaves the other objects using the old layout.
		local header_newer=0
		for h in "$srcdir"/*.h; do
			[ -e "$h" ] || continue
			if [ "$h" -nt "$obj" ]; then header_newer=1; break; fi
		done
		if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ "$header_newer" -eq 1 ]; then
			# shellcheck disable=SC2086
			$CXX $CXXFLAGS "${defines[@]}" "${incs[@]}" $INCS -c "$src" -o "$obj" &
			n=$((n+1))
			[ $((n % JOBS)) -eq 0 ] && wait
		fi
	done
	wait
}

# Run moc over the headers of one library that declare Q_OBJECT and compile
# the results into that library's object directory.
#   compile_moc <tag> <owner-lib> <include dirs...>
compile_moc() {
	local tag="$1"; shift
	local owner="$1"; shift
	local incs=()
	for a in "$@"; do incs+=("-I$a"); done
	incs+=("-I$SOH_SRC/$owner")
	local outdir="$OBJ/$tag"
	mkdir -p "$outdir"
	for hdr in "$SOH_SRC/$owner"/*.h; do
		[ -e "$hdr" ] || continue
		grep -q "Q_OBJECT" "$hdr" 2>/dev/null || continue
		local base; base="$(basename "$hdr" .h)"
		local out="$outdir/moc_${base}.cpp"
		local obj="$outdir/moc_${base}.o"
		if [ ! -f "$obj" ] || [ "$hdr" -nt "$obj" ]; then
			"$MOC" "${incs[@]}" $INCS "$hdr" -o "$out"
			# shellcheck disable=SC2086
			$CXX $CXXFLAGS $INCS "${incs[@]}" -c "$out" -o "$obj" &
		fi
	done
	wait
}

archive_tree() {
	local tag="$1"; local name="$2"; shift 2
	mkdir -p "$LIBDIR"
	# MSYS2 has a 32k command line limit. xargs keeps each ar invocation
	# below it, even for the very large AuData and AuGlobal libraries.
	rm -f "$LIBDIR/lib$name.a"
	find "$OBJ/$tag" -maxdepth 1 -name '*.o' -print0 | \
		xargs -0 -r ar rcs "$LIBDIR/lib$name.a"
	ranlib "$LIBDIR/lib$name.a" 2>/dev/null || true
	echo "  lib$name.a"
}

# Compile the Qt resource files of one library.
#   compile_qrc <tag> <owner-lib>
compile_qrc() {
	local tag="$1"; shift
	local owner="$1"; shift
	local outdir="$OBJ/$tag"
	mkdir -p "$outdir"
	for qrc in "$SOH_SRC/$owner"/*.qrc; do
		[ -e "$qrc" ] || continue
		local base; base="$(basename "$qrc" .qrc)"
		local out="$outdir/qrc_${base}.cpp"
		local obj="$outdir/qrc_${base}.o"
		if [ ! -f "$obj" ] || [ "$qrc" -nt "$obj" ]; then
			"$RCC" -name "$base" -o "$out" "$qrc"
			# shellcheck disable=SC2086
			$CXX $CXXFLAGS $INCS -c "$out" -o "$obj" &
		fi
	done
	wait
}

# ------------------------------------------------------------------ setup --

if [ "${1:-}" = "clean" ]; then
	rm -rf "$BUILD"
	echo "cleaned."
	exit 0
fi

if [ "$ON_WINDOWS" = 1 ]; then
	need_file "$QT_INC/QtCore/qglobal.h"
	need_file "$MOC"
	need_file "$RCC"
else
	need_file "$QTDIR/include/qt5/QtCore/qglobal.h"
	need_file "$MOC"
fi
if [ ! -d "$SOH_SRC/audata" ]; then
	echo "Fetching Shores of Hazeron engine source into $SOH_SRC ..."
	mkdir -p "$(dirname "$SOH_SRC")"
	git clone --depth 1 "$SOH_REPO" "$SOH_SRC"
fi

# The upstream makefile generates this OpenGL version selector.
mkdir -p "$SOH_SRC/augl/.d" "$SOH_SRC/augl/.r"
echo '#define AUGL41' > "$SOH_SRC/augl/.d/AuGLVer.h"
echo '#define AUGL41' > "$SOH_SRC/augl/.r/AuGLVer.h"

mkdir -p "$OBJ" "$LIBDIR" "$BIN"

INC_UTIL="$SOH_SRC/auutil $SOH_SRC/auloginserver $SOH_SRC/aumailserver"
INC_MATH="$SOH_SRC/aumath"
INC_GL="$SOH_SRC/auutil $SOH_SRC/aumath"
INC_GLOBAL="$SOH_SRC/auutil $SOH_SRC/auloginserver $SOH_SRC/aumailserver $SOH_SRC/aumath $SOH_SRC/augl"
INC_DATA="$SOH_SRC/auutil $SOH_SRC/auglobal $SOH_SRC/aumath $SOH_SRC/augl $SOH_SRC/audata"

echo "==> Building engine libraries"

compile_tree auutil "$SOH_SRC/auutil" AUUTIL -- $INC_UTIL
compile_moc  auutil auutil $INC_UTIL
compile_qrc  auutil auutil
archive_tree auutil AuUtil AUUTIL

compile_tree aumath "$SOH_SRC/aumath" AUMATH -- $INC_MATH
compile_moc  aumath aumath $INC_MATH
archive_tree aumath AuMath AUMATH

compile_tree augl "$SOH_SRC/augl" AUGL -- $INC_GL
compile_moc  augl augl $INC_GL
compile_qrc  augl augl
archive_tree augl AuGL AUGL

compile_tree auglobal "$SOH_SRC/auglobal" AUGLOBAL -- $INC_GLOBAL
compile_moc  auglobal auglobal $INC_GLOBAL
archive_tree auglobal AuGlobal AUGLOBAL

compile_tree shipgen "$HERE/src" SHIPGEN -- $INC_DATA $INC_GLOBAL $INC_GL $INC_MATH $INC_UTIL
archive_tree shipgen ShipgenCore SHIPGEN

compile_tree audata "$SOH_SRC/audata" AUDATA -- $INC_DATA
compile_moc  audata audata $INC_DATA
archive_tree audata AuData AUDATA

echo "==> Building the command line tool"

rm -rf "$OBJ/tool"
mkdir -p "$OBJ/tool"
for src in "$HERE/src"/*.cpp; do
	base="$(basename "$src" .cpp)"
	case "$base" in
		brief|generate|building|paint|logging|audit|sohfile|interior|triplan|planbuild|aether) continue ;;	# These live in the core library.
	esac
	obj="$OBJ/tool/${base}.o"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$src" -o "$obj" &
done
wait

# shellcheck disable=SC2086
$CXX $CXXFLAGS_TOOL -o "$BIN/shipgen$EXE" "$OBJ/tool"/*.o \
	-L"$LIBDIR" -lShipgenCore -lAuData -lAuGlobal -lAuGL -lAuMath -lAuUtil \
	$QLIBS $PTHREAD

echo "==> Building the graphical tool"

rm -rf "$OBJ/gui"
mkdir -p "$OBJ/gui"

# The wizard uses signals and slots, so it needs moc.
for hdr in "$HERE/src/gui"/*.h; do
	grep -q "Q_OBJECT" "$hdr" || continue
	base="$(basename "$hdr" .h)"
	"$MOC" -I"$HERE/src/gui" -I"$HERE/src" -I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl \
		-I$SOH_SRC/auglobal -I$SOH_SRC/audata $INCS "$hdr" -o "$OBJ/gui/moc_${base}.cpp"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I"$HERE/src/gui" -I"$HERE/src" \
		-I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$OBJ/gui/moc_${base}.cpp" -o "$OBJ/gui/moc_${base}.o" &
done
wait

for src in "$HERE/src/gui"/*.cpp; do
	base="$(basename "$src" .cpp)"
	obj="$OBJ/gui/${base}.o"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-I"$HERE/src/gui" -I"$HERE/src" \
		-c "$src" -o "$obj" &
done
wait

# shellcheck disable=SC2086
$CXX $CXXFLAGS_TOOL $GUI_LDFLAGS -o "$BIN/shipgen-gui$EXE" "$OBJ/gui"/*.o \
	-L"$LIBDIR" -lShipgenCore -lAuData -lAuGlobal -lAuGL -lAuMath -lAuUtil \
	$QTMAIN $QLIBS $PTHREAD

echo "==> Building the mesh tool"

rm -rf "$OBJ/meshapp"
mkdir -p "$OBJ/meshapp"

for hdr in "$HERE/src/meshapp"/*.h; do
	grep -q "Q_OBJECT" "$hdr" || continue
	base="$(basename "$hdr" .h)"
	"$MOC" -I"$HERE/src/meshapp" -I"$HERE/src" -I"$HERE/src/gui" -I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl \
		-I$SOH_SRC/auglobal -I$SOH_SRC/audata $INCS "$hdr" -o "$OBJ/meshapp/moc_${base}.cpp"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I"$HERE/src/meshapp" -I"$HERE/src" -I"$HERE/src/gui" \
		-I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$OBJ/meshapp/moc_${base}.cpp" -o "$OBJ/meshapp/moc_${base}.o" &
done
wait

for src in "$HERE/src/meshapp"/*.cpp; do
	base="$(basename "$src" .cpp)"
	obj="$OBJ/meshapp/${base}.o"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I"$HERE/src/meshapp" -I"$HERE/src" -I"$HERE/src/gui" \
		-I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$src" -o "$obj" &
done
wait

# The mesh tool uses the same 3D view as the graphical tool.
# shellcheck disable=SC2086
$CXX $CXXFLAGS_TOOL $GUI_LDFLAGS -o "$BIN/shipgen-mesh$EXE" "$OBJ/meshapp"/*.o "$OBJ/gui/preview.o" "$OBJ/gui/moc_preview.o" \
	-L"$LIBDIR" -lShipgenCore -lAuData -lAuGlobal -lAuGL -lAuMath -lAuUtil \
	$QTMAIN $QLIBS $PTHREAD

echo "==> Building the designer"

rm -rf "$OBJ/designer"
mkdir -p "$OBJ/designer"

for hdr in "$HERE/src/designer"/*.h; do
	grep -q "Q_OBJECT" "$hdr" || continue
	base="$(basename "$hdr" .h)"
	"$MOC" -I"$HERE/src/designer" -I"$HERE/src" -I"$HERE/src/gui" -I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl \
		-I$SOH_SRC/auglobal -I$SOH_SRC/audata $INCS "$hdr" -o "$OBJ/designer/moc_${base}.cpp"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I"$HERE/src/designer" -I"$HERE/src" -I"$HERE/src/gui" \
		-I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$OBJ/designer/moc_${base}.cpp" -o "$OBJ/designer/moc_${base}.o" &
done
wait

for src in "$HERE/src/designer"/*.cpp; do
	base="$(basename "$src" .cpp)"
	obj="$OBJ/designer/${base}.o"
	# shellcheck disable=SC2086
	$CXX $CXXFLAGS_TOOL $INCS -I"$HERE/src/designer" -I"$HERE/src" -I"$HERE/src/gui" \
		-I$SOH_SRC/auutil -I$SOH_SRC/aumath -I$SOH_SRC/augl -I$SOH_SRC/auglobal -I$SOH_SRC/audata \
		-c "$src" -o "$obj" &
done
wait

# The designer uses the same 3D view as the graphical tool.
# shellcheck disable=SC2086
$CXX $CXXFLAGS_TOOL $GUI_LDFLAGS -o "$BIN/shipgen-designer$EXE" "$OBJ/designer"/*.o "$OBJ/gui/preview.o" "$OBJ/gui/moc_preview.o" \
	-L"$LIBDIR" -lShipgenCore -lAuData -lAuGlobal -lAuGL -lAuMath -lAuUtil \
	$QTMAIN $QLIBS $PTHREAD

echo
echo "built: $BIN/shipgen$EXE"
echo "built: $BIN/shipgen-gui$EXE"
echo "built: $BIN/shipgen-mesh$EXE"
echo "built: $BIN/shipgen-designer$EXE"
