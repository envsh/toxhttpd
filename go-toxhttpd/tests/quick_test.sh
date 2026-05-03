#!/bin/bash
# 快速测试 - 验证服务基本功能
# 用法: ./quick_test.sh

SERVER_URL=${SERVER_URL:-"http://localhost:8181"}

echo "快速测试 Tox HTTP 服务..."
echo "服务器: $SERVER_URL"
echo ""

# 测试 self 端点
echo -n "测试 /api/self... "
response=$(curl -s "$SERVER_URL/api/self")
if echo "$response" | grep -q "address"; then
    echo "✓ 成功"
else
    echo "✗ 失败"
    exit 1
fi

# 测试 conferences 端点
echo -n "测试 /api/conferences... "
response=$(curl -s "$SERVER_URL/api/conferences")
if echo "$response" | grep -q "conferences"; then
    echo "✓ 成功"
else
    echo "✗ 失败"
fi

# 测试 groups 端点
echo -n "测试 /api/groups... "
response=$(curl -s "$SERVER_URL/api/groups")
if echo "$response" | grep -q "groups"; then
    echo "✓ 成功"
else
    echo "✗ 失败"
fi

echo ""
echo "服务正常运行！"
