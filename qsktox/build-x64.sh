#!/bin/bash
set -e
mkdir -p build-x64 && cd build-x64
cmake .. \
    -DCMAKE_PREFIX_PATH="/opt/qt/6.7.3/gcc_64;/opt/qt/qskinny" \
    -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
echo "=== done: LD_LIBRARY_PATH=/opt/qt/qskinny/lib/qskinny:/opt/qt/6.7.3/gcc_64/lib ./build-x64/qsktox ==="
