#!/bin/bash
# diff_test.sh — 验证包装器对普通 .h 文件与原始 moc 输出一致
set -e
TOP=$(dirname "$0")
QLMOC=$(realpath "$TOP/../..")
OUTDIR=/tmp/qlmoc_test_cmp_qt3

MOC=/opt/qt338sh/bin/moc

mkdir -p "$OUTDIR"

echo "=== diff_test (Qt3) ==="

# 用 basic/hello.h 作为测试源
TEST_HDR="$TOP/../basic/hello.h"

# 原始 moc（Qt3 moc 不接受 -I/-D，只接受 -o 和输入文件）
$MOC "$TEST_HDR" -o "$OUTDIR/real_moc.cpp"

# 包装器（REAL_MOC 环境变量）
REAL_MOC=$MOC QLMOC_DIR=$QLMOC \
    bash "$QLMOC/moc-wrapper.sh" "$TEST_HDR" \
    -o "$OUTDIR/wrapper_moc.cpp"

# diff
if diff -q "$OUTDIR/real_moc.cpp" "$OUTDIR/wrapper_moc.cpp" > /dev/null 2>&1; then
    echo "  PASS: wrapper output == original moc output"
else
    echo "  FAIL: outputs differ!" >&2
    diff -u "$OUTDIR/real_moc.cpp" "$OUTDIR/wrapper_moc.cpp" | head -40
    exit 1
fi

rm -rf "$OUTDIR"
echo "=== diff_test (Qt3): PASS ==="
