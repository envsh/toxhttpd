set -e fail
set -x

export PKG_CONFIG_PATH=/opt/vcpkg/installed/x64-osx/lib/pkgconfig
QMAKE_EXTRA=""
if [ x"$1" == x"asan" ]; then
    QMAKE_EXTRA="CONFIG+=asan"
fi
/opt/qt/5.15.2/clang_64/bin/qmake -r $QMAKE_EXTRA
make

# package
tar zcf qltox-qt5-osx-x64.tar.gz qltox.app/
ls -lh qltox-*.gz
ls -lh qltox.app/Contents/MacOS/
