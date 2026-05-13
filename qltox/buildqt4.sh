set -x

# for qt4
qmake-qt4

sed -i 's/\-O2/\-O1/g' Makefile

if [ x"$1" == x"c" ]; then
	make clean
fi
make
if [ -f "qltox" ]; then
	cp -v qltox q4tox
fi
