#!/bin/bash
set -e
set -x

QTDIR=/opt/qt/6.7.3/gcc_64
if [ ! -d "$QTDIR" ]; then
	# macos
	QTDIR=/opt/qt/6.7.3/macos
fi
export LD_LIBRARY_PATH=$QTDIR/lib

mkdir -p build-x64 && cd build-x64
$QTDIR/bin/qt-cmake .. \
    -DQt6_DIR=$QTDIR/lib/cmake/Qt6 \
    -DCMAKE_PREFIX_PATH="$QTDIR;/opt/qt/qskinny" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_VERBOSE_MAKEFILE=on \
    -D CMAKE_CXX_FLAGS="-O1" -D CMAKE_C_FLAGS="-O1"

# hotfix
sed -i 's/-lQt6Qml -lQt6Quick -lQt6OpenGL//g' CMakeFiles/qsktox.dir/link.txt

make -j$(nproc)
echo "=== done: LD_LIBRARY_PATH=/opt/qt/qskinny/lib/qskinny:$QTDIR/lib ./build-x64/qsktox ==="
