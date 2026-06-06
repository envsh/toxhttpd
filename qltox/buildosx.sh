set -e fail
set -x

export PKG_CONFIG_PATH=/opt/vcpkg/installed/x64-osx/lib/pkgconfig
/opt/qt/5.15.2/clang_64/bin/qmake -r
make

# package
tar zcf qltox-qt5-osx-x64.tar.gz qltox.app/
ls -lh qltox-*.gz
