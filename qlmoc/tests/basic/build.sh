#!/bin/bash
# build.sh for tests/basic/
# Tests traditional .h vs .cppm module paths
set -e
TOP=$(dirname "$0")
QLMOC=$(realpath "$TOP/../..")
CXX=g++
CXXFLAGS="-std=c++2a -fmodules -g -O0 -fPIC"
OUTDIR=/tmp/qlmoc_test_basic

MODE=${1:-all}  # all | qt3 | qt4 | traditional_module
MOC=

case "$MODE" in
    qt3|all)
        MOC=/opt/qt338sh/bin/moc
        QT_INC=-I/opt/qt338sh/include
        QT_LIBS="-L/opt/qt338sh/lib -lqt-mt"
        QT_DEFS="-DQT3_BUILD -DQT_THREAD_SUPPORT -DQT_SHARED"
        ;;
    qt4|*)
        MOC=/usr/bin/moc-qt4
        QT_INC="-I/usr/include/qt4 -I/usr/include/qt4/QtCore -I/usr/include/qt4/QtGui"
        QT_LIBS="-lQtCore -lQtGui"
        QT_DEFS=
        ;;
esac

mkdir -p "$OUTDIR"
echo "=== basic test (${MODE}) ==="

# ---------------------- Traditional .h path ----------------------
echo "--- Traditional .h path ---"
cd "$TOP"

# 编译传统实现（#include "hello.h"）
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c hello_impl_traditional.cpp \
    -o "$OUTDIR/hello_impl_traditional.o" 2>&1

# moc 处理 hello.h
$MOC hello.h -o "$OUTDIR/moc_hello.cpp"
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c "$OUTDIR/moc_hello.cpp" \
    -o "$OUTDIR/moc_hello.o" 2>&1

# 编译 main
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c hello_main.cpp \
    -o "$OUTDIR/hello_main.o" 2>&1

# 链接
$CXX "$OUTDIR/hello_impl_traditional.o" "$OUTDIR/moc_hello.o" \
    "$OUTDIR/hello_main.o" $QT_LIBS -o "$OUTDIR/test_traditional" 2>&1

echo "  Traditional: OK ($OUTDIR/test_traditional)"

# ---------------------- Module .cppm path ----------------------
echo "--- Module .cppm path ---"

# 编译模块接口
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c hello.cppm \
    -o "$OUTDIR/hello_mod.o" 2>&1

# 预处理 + moc + 后处理
REAL_MOC=$MOC QLMOC_DIR=$QLMOC \
    bash "$QLMOC/moc-wrapper.sh" hello.cppm -o "$OUTDIR/moc_module.cpp" 2>&1

# 编译 moc 输出（模块实现单元）
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c "$OUTDIR/moc_module.cpp" \
    -o "$OUTDIR/moc_module.o" 2>&1

# 编译模块实现（module hellomod;）
$CXX $CXXFLAGS $QT_INC $QT_DEFS -c hello_impl.cpp \
    -o "$OUTDIR/hello_impl_module.o" 2>&1

# 编译模块 main
$CXX $CXXFLAGS $QT_INC $QT_DEFS -DMODULE_BUILD -c hello_main.cpp \
    -o "$OUTDIR/hello_main_module.o" 2>&1

# 链接
$CXX "$OUTDIR/hello_mod.o" "$OUTDIR/moc_module.o" \
    "$OUTDIR/hello_impl_module.o" "$OUTDIR/hello_main_module.o" \
    $QT_LIBS -o "$OUTDIR/test_module" 2>&1

echo "  Module: OK ($OUTDIR/test_module)"

echo "=== basic test ($MODE): PASS ==="
