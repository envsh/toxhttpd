set -x

# for qt4
unset QT3_BUILD
qmake-qt4 && make
