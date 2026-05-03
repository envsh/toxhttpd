#!/bin/bash
# Tox HTTP API 测试脚本 - 使用 curl 测试 GET REST API
# 用法: ./api_test.sh [conference_id] [group_number]
# 示例: ./api_test.sh 0 0

SERVER_URL=${SERVER_URL:-"http://localhost:8181"}

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "===== Tox HTTP API 测试 ====="
echo "服务器: $SERVER_URL"
echo ""

# 检查服务是否运行
echo -n "检查服务状态... "
if ! curl -s --connect-timeout 3 "$SERVER_URL/api/self" > /dev/null; then
    echo -e "${RED}失败${NC}"
    echo "错误: 无法连接到 $SERVER_URL"
    echo "请先启动服务端: cd go-toxhttpd && ./toxhttpd"
    exit 1
fi
echo -e "${GREEN}成功${NC}"

# 测试函数
test_get_api() {
    local name="$1"
    local endpoint="$2"
    local expected_field="$3"  # 可选：检查响应中是否包含此字段
    
    echo -n "测试 GET $endpoint... "
    response=$(curl -s "$SERVER_URL$endpoint")
    curl_exit=$?
    
    if [ $curl_exit -ne 0 ]; then
        echo -e "${RED}失败${NC} (curl 错误)"
        return 1
    fi
    
    # 检查是否是有效的 JSON
    if ! echo "$response" | jq empty 2>/dev/null; then
        echo -e "${RED}失败${NC} (无效 JSON)"
        echo "响应: $response"
        return 1
    fi
    
    # 如果指定了期望字段，检查是否存在
    if [ -n "$expected_field" ]; then
        if ! echo "$response" | jq -e ".$expected_field" > /dev/null 2>&1; then
            echo -e "${RED}失败${NC} (缺少字段: $expected_field)"
            echo "响应: $response"
            return 1
        fi
    fi
    
    echo -e "${GREEN}成功${NC}"
    echo "响应: $response" | jq . 2>/dev/null || echo "$response"
    return 0
}

# 1. 测试基础 API
echo ""
echo "=== 基础 API 测试 ==="
test_get_api "获取自身信息" "/api/self" "address"
test_get_api "获取好友列表" "/api/friends" "friends"
test_get_api "获取群组列表" "/api/groups" "groups"
test_get_api "获取会议列表" "/api/conferences" "conferences"

# 2. 测试成员列表 API
echo ""
echo "=== 成员列表 API 测试 ==="

# 从命令行参数获取 ID
CONF_ID=${1:-""}
GROUP_ID=${2:-""}

if [ -z "$CONF_ID" ]; then
    echo "提示: 未提供 conference_id，跳过会议成员测试"
    echo "用法: ./api_test.sh <conference_id> <group_number>"
    echo "示例: ./api_test.sh 0 0"
else
    test_get_api "获取会议成员" "/api/conference/members?conference_id=$CONF_ID" "members"
fi

if [ -z "$GROUP_ID" ]; then
    echo "提示: 未提供 group_number，跳群组成员测试"
else
    test_get_api "获取群组成员" "/api/group/members?group_number=$GROUP_ID" "members"
fi

# 3. 测试事件 API（可选）
echo ""
echo "=== 事件 API 测试 ==="
test_get_api "获取事件（after=0）" "/api/events?after=0" ""

echo ""
echo "===== 测试完成 ====="
