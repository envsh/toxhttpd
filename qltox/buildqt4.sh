set -x

# for qt4
qmake-qt4

sed -i 's/\-O2/\-O1/g' Makefile

if [ x"$1" == x"c" ]; then
	make clean
fi
if [ -f "q3tox" ]; then
	cp q3tox q3tox.bak && rm -f q3tox
fi
make
if [ -f "q3tox" ]; then
	cp -v q3tox q4tox && cp q3tox.bak q3tox
fi
