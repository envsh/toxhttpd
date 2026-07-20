set -x

# for qt4
mkdir -p build-qt4 && cd build-qt4
qmake-qt4 ../qltox.pro

sed -i 's/\-O2/\-O1/g' Makefile

if [ x"$1" == x"c" ]; then
	make clean
fi
make
ret=$?
if [ x"$ret" == x"0" ] && [ -f "qltox" ]; then
	cp -v qltox q4tox
fi
