set -x

# for qt3
QTDIR=/opt/qt338sh
export QTDIR
export QT3_BUILD=1
/opt/qt338sh/bin/qmake -makefile
make

# qmake-qt4 && make
