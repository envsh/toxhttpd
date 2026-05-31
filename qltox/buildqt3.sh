set -x

# for qt3
if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt338sh
    export QTDIR
fi
mkdir -p build-qt3 && cd build-qt3
/opt/qt338sh/bin/qmake -makefile ../qltox.pro
if [ x"$1" == x"c" ]; then
	make clean
fi
make

# qmake-qt4 && make
