set -x

# for qt4
qmake-qt4

sed -i 's/\-O2/\-O1/g' Makefile

make
