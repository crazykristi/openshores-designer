#!/usr/bin/env bash
# Builds one of the small programs in tests/ against the engine libraries.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC="${TOOLCHAIN:-$HOME/.local/hazeron-toolchain}"
SOH="$HERE/third_party/soh"
mkdir -p "$HERE/build/obj" "$HERE/build/bin"
"$TC/bin/g++" -std=c++11 -O0 -g -w -fPIC \
 -I$TC/usr/include/qt5 -I$TC/usr/include/qt5/QtCore -I$TC/usr/include/qt5/QtGui -I$TC/usr/include/qt5/QtWidgets \
 -I$TC/usr/include/qt5/QtNetwork -I$TC/usr/include/qt5/QtSql -I$TC/usr/include/qt5/QtOpenGL -I$TC/usr/include \
 -I$SOH/auutil -I$SOH/aumath -I$SOH/augl -I$SOH/auglobal -I$SOH/audata -I$HERE/src \
 -c "$HERE/tests/$1.cpp" -o "$HERE/build/obj/$1.o"
"$TC/bin/g++" -o "$HERE/build/bin/$1" "$HERE/build/obj/$1.o" -L"$HERE/build/lib" \
 -lShipgenCore -lAuData -lAuGlobal -lAuGL -lAuMath -lAuUtil -L$TC/usr/lib64 \
 -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Network -lQt5Sql -lQt5OpenGL -lopenal -lGL
