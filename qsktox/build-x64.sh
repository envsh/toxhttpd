#!/bin/bash
set -e
set -x

QTDIR=/opt/qt/6.7.3/gcc_64
if [ ! -d "$QTDIR" ]; then
	# macos
	QTDIR=/opt/qt/6.7.3/macos
fi

mkdir -p build-x64 && cd build-x64
$QTDIR/bin/qt-cmake .. \
    -DCMAKE_PREFIX_PATH="$QTDIR;/opt/qt/qskinny" \
    -DCMAKE_BUILD_TYPE=Debug \
    -D CMAKE_CXX_FLAGS="-O1" -D CMAKE_C_FLAGS="-O1" 

make -j$(nproc)
echo "=== done: LD_LIBRARY_PATH=/opt/qt/qskinny/lib/qskinny:$QTDIR/lib ./build-x64/qsktox ==="
