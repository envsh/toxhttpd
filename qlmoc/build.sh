#!/bin/bash
# qlmoc/build.sh — 运行所有测试
set -e
TOP=$(dirname "$0")
cd "$TOP"

echo "============================================"
echo " qlmoc — MOC + C++20 Modules 测试套件"
echo "============================================"
echo ""

# Phase 1: 冒烟测试 — 包装器对普通 .h 与原始 moc 一致
echo "── Phase 1: 冒烟测试 (透传验证) ──"
for mode in qt3 qt4; do
    echo "  Running diff_test ($mode)..."
    bash "tests/cmp_${mode}/diff_test.sh"
done
echo ""

# Phase 2: 功能测试 — .cppm 模块编译 + 链接
echo "── Phase 2: 功能测试 (模块编译) ──"
for mode in qt3 qt4; do
    echo "  Running basic test ($mode)..."
    bash tests/basic/build.sh "$mode"
done
echo ""

echo "============================================"
echo " 全部测试通过!"
echo "============================================"
