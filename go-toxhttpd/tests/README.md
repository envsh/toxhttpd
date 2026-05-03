# Tox HTTP API 测试脚本

## 目录结构
```
tests/
├── api_test.sh        # 完整 API 测试（推荐）
├── test_members.sh    # 成员列表专项测试
├── quick_test.sh     # 快速验证服务
└── README.md         # 本文件
```

## 使用方法

### 1. 启动服务端
```bash
cd /home/gzleo/aprog/toxhttpd/go-toxhttpd
./toxhttpd -port 8181
```

### 2. 运行完整测试
```bash
cd tests/
chmod +x *.sh
./api_test.sh 0 0  # 假设 conference_id=0, group_number=0
```

### 3. 只测试成员列表
```bash
./test_members.sh 0 0
```

### 4. 快速验证
```bash
./quick_test.sh
```

## 环境变量
- `SERVER_URL`: 自定义服务器地址（默认: `http://localhost:8181`）
```bash
SERVER_URL="http://192.168.1.100:8181" ./api_test.sh
```

## 测试内容

### api_test.sh 测试项目
1. 服务状态检查
2. 基础 API:
   - `GET /api/self` - 获取自身信息
   - `GET /api/friends` - 获取好友列表
   - `GET /api/groups` - 获取群组列表
   - `GET /api/conferences` - 获取会议列表
3. 成员列表 API:
   - `GET /api/conference/members?conference_id=` - 会议成员
   - `GET /api/group/members?group_number=` - 群组成员
4. 事件 API:
   - `GET /api/events?after=0` - 获取事件

### test_members.sh 测试项目
- 详细测试会议成员列表 API
- 详细测试群组成员列表 API
- 显示成员数量和详细信息

### quick_test.sh 测试项目
- 快速验证 `/api/self` 端点
- 快速验证 `/api/conferences` 端点
- 快速验证 `/api/groups` 端点

## 依赖
- `curl`: HTTP 客户端（必需）
- `jq`: JSON 解析工具（可选，用于美化输出）

### 安装 jq
```bash
# Debian/Ubuntu
sudo apt-get install jq

# macOS
brew install jq
```

## 注意事项
1. **服务端必须运行**：测试前确保服务端已启动
2. **有效的 ID**：测试成员列表 API 需要提供实际存在的 `conference_id` 和 `group_number`
3. **端口配置**：如果服务端使用非默认端口，设置环境变量 `SERVER_URL`
4. **JSON 输出**：如果未安装 `jq`，脚本仍然可以运行，但输出不会格式化

## 示例输出

### api_test.sh 成功输出
```
===== Tox HTTP API 测试 =====
服务器: http://localhost:8181

检查服务状态... ✓ 成功

=== 基础 API 测试 ===
测试 GET /api/self... ✓ 成功
{
  "address": "1234567890ABCDEF...",
  "name": "My Tox",
  ...
}
...

=== 成员列表 API 测试 ===
测试 GET /api/conference/members?conference_id=0... ✓ 成功
{
  "conference_id": 0,
  "members": [
    {"peer_number": 0, "name": "Alice"},
    {"peer_number": 1, "name": "Bob"}
  ]
}
...

===== 测试完成 =====
```

### test_members.sh 成功输出
```
===== 成员列表 API 测试 =====

>>> 测试会议成员 (conference_id=0)
请求: GET http://localhost:8181/api/conference/members?conference_id=0
{
  "conference_id": 0,
  "members": [...]
}
成员数量: 2

>>> 测试群组成员 (group_number=0)
请求: GET http://localhost:8181/api/group/members?group_number=0
{
  "group_number": 0,
  "members": [...]
}
成员数量: 2

===== 测试完成 =====
```
