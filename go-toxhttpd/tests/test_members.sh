#!/bin/bash
# 成员列表 API 专项测试
# 用法: ./test_members.sh <conference_id> <group_number>
# 示例: ./test_members.sh 0 0

SERVER_URL=${SERVER_URL:-"http://localhost:8181"}

if [ $# -lt 2 ]; then
    echo "用法: $0 <conference_id> <group_number>"
    echo "示例: $0 0 0"
    exit 1
fi

CONF_ID=$1
GROUP_ID=$2

echo "===== 成员列表 API 测试 ====="
echo ""

# 测试会议成员
echo ">>> 测试会议成员 (conference_id=$CONF_ID)"
echo "请求: GET $SERVER_URL/api/conference/members?conference_id=$CONF_ID"
response=$(curl -s "$SERVER_URL/api/conference/members?conference_id=$CONF_ID")
echo "$response" | jq .
member_count=$(echo "$response" | jq '.members | length')
echo "成员数量: $member_count"
echo ""

# 测试群组成员
echo ">>> 测试群组成员 (group_number=$GROUP_ID)"
echo "请求: GET $SERVER_URL/api/group/members?group_number=$GROUP_ID"
response=$(curl -s "$SERVER_URL/api/group/members?group_number=$GROUP_ID")
echo "$response" | jq .
member_count=$(echo "$response" | jq '.members | length')
echo "成员数量: $member_count"
echo ""

echo "===== 测试完成 ====="
