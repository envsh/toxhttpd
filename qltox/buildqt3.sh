set -x

# for qt3
if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt338sh
    export QTDIR
fi
/opt/qt338sh/bin/qmake -makefile
if [ x"$1" == x"c" ]; then
	make clean
fi
make

# qmake-qt4 && make
