# ToxHTTP Go 版本实现总结

## 项目概述
将 Go 版本的 toxhttpd 从自定义 `tox` 包（cgo 直接封装）迁移到使用 `github.com/TokTok/go-toxcore-c` 第三方库，并添加多项功能改进。

## 完成的修改

### 1. 迁移到 github.com/TokTok/go-toxcore-c
**文件**：`go-toxhttpd/main.go`

- ✅ Import 路径修改：
  ```go
  tox "github.com/TokTok/go-toxcore-c"
  ```

- ✅ 适配新包 API：
  | 原方法 | 新方法 | 说明 |
  |--------|--------|------|
  | `tox.NewTox()` | `tox.NewTox(opts)` | 使用 ToxOptions |
  | `GetAddress()` | `SelfGetAddress()` | 返回 string |
  | `GetSelfName()` | `SelfGetName()` | 返回 string |
  | `GetSelfStatus()` | `SelfGetStatusMessage()` | 返回 (string, error) |
  | `GetConnectionStatus()` | 通过回调跟踪 | 返回 int |
  | `GetFriendList()` | `SelfGetFriendList()` | 返回 []uint32 |
  | `AddFriend(pubkey)` | `FriendAdd(pubkeyStr, message)` | 接受 (string, string) |
  | `AddFriend(pubkey, msg)` | `FriendAdd(pubkeyStr, message)` | 同上 |
  | `DeleteFriend(id)` | `FriendDelete(id)` | 返回 (bool, error) |
  | `GetFriendName(id)` | `FriendGetName(id)` | 返回 (string, error) |
  | `GetFriendConnectionStatus(id)` | `FriendGetConnectionStatus(id)` | 返回 (int, error) |
  | `GetFriendPublicKey(id)` | `FriendGetPublicKey(id)` | 返回 (string, error) |
  | `SendMessage(id, msg)` | `FriendSendMessage(id, msg)` | 返回 (uint32, error) |
  | `NewConference()` | 暂未实现 | - |
  | `Iterate()` | `Iterate()` | - |
  | `IterationInterval()` | `IterationInterval()` | 返回 int |
  | `Close()` | `Kill()` | - |

- ✅ 回调注册方式修改：
  - 原：`s.tox.SetXxxCallback(func...)`
  - 新：`s.tox.CallbackXxx(func..., nil)`

### 2. 添加命令行参数
**文件**：`go-toxhttpd/main.go`

```go
udpEnabled := flag.Bool("udp", false, "Enable UDP mode (default: TCP only)")
port := flag.String("port", "8181", "HTTP server port")
debugLevel := flag.String("debug", "info", "Log level: trace, debug, info, warn, error")
```

**使用方式**：
```bash
# 默认：TCP only，端口 8181，日志级别 info
./toxhttpd-go

# 开启 UDP
./toxhttpd-go -udp

# 自定义端口和日志级别
./toxhttpd-go -port 9090 -debug debug
```

### 3. 添加日志系统
**文件**：`go-toxhttpd/main.go`

- ✅ **Tox 回调日志**：所有回调添加 `[TOX_CALLBACK]` 前缀
  ```go
  s.tox.CallbackSelfConnectionStatus(func(this *tox.Tox, status int, userData interface{}) {
      log.Printf("[TOX_CALLBACK] SelfConnectionStatus: %s (%d)", statusStr, status)
      ...
  }, nil)
  ```

- ✅ **Tox 运行状态日志**：添加 `logToxStatus()` 函数
  ```go
  [TOX] Status: name=, addr=4D9C0FF9..., connection=OFFLINE (0), friends=0
  ```

- ✅ **日志级别控制**：添加 `setLogLevel()` 函数
  - `-debug trace` 或 `debug`：显示文件名和行号
  - `-debug info`（默认）：只显示时间
  - `-debug warn` 或 `error`：只显示重要信息

- ✅ **Bootstrap 日志**：显示连接的节点
  ```go
  [TOX] Starting bootstrap to 3 nodes...
  Bootstrap 0: UDP 104.225.141.59:33445 (maintainer: velusip (C version))
  Bootstrap 0: TCP relay 104.225.141.59:33445 (maintainer: velusip (C version))
  ...
  [TOX] Bootstrap completed
  ```

### 4. 添加 Tox 数据保存/加载机制
**文件**：`go-toxhttpd/main.go`

- ✅ **启动时加载**：`NewServer()` 函数中
  ```go
  if data, err := os.ReadFile("data/savedata.bin"); err == nil && len(data) > 100 {
      saveData = data
      log.Printf("[TOX] Loaded save data from %s (%d bytes)", saveDataPath, len(data))
  }
  ```

- ✅ **关闭时保存**：信号处理函数中
  ```go
  go func() {
      <-sig
      log.Println("Shutting down...")
      saveToxData(server.tox, "data/savedata.bin")
      server.tox.Kill()
      os.Exit(0)
  }()
  ```

- ✅ **保存函数**：`saveToxData()`
  ```go
  func saveToxData(t *tox.Tox, path string) {
      os.MkdirAll("data", 0700)
      err := t.WriteSavedata(path)
      ...
  }
  ```

### 5. 从 C 版本移植自动 Bootstrap
**文件**：`go-toxhttpd/main.go`

- ✅ **Bootstrap 节点**：使用 C 版本中的 3 个节点
  ```go
  var bootstrapNodes = []BootstrapNode{
      {IPv4: "104.225.141.59", Port: 33445, PublicKey: "933BA20B2E258B4C0D475B6DECE90C7E827FE83EFA9655414E7841251B19A72C", ...},
      {IPv4: "43.198.227.166", Port: 3389, PublicKey: "AD13AB0D434BCE6C83FE2649237183964AE3341D0AFB3BE1694B18505E4E135E", ...},
      {IPv4: "3.0.24.15", Port: 33445, PublicKey: "E20ABCF38CDBFFD7D04B29C956B33F7B27A3BB7AF0618101617B036E4AEA402D", ...},
  }
  ```

- ✅ **Bootstrap 函数**：`bootstrapAll()`
  ```go
  func bootstrapAll(t *tox.Tox) {
      for _, node := range bootstrapNodes {
          t.Bootstrap(node.IPv4, node.Port, node.PublicKey)
      }
  }
  ```

### 6. 修复 Web 界面问题
**文件**：`web/app.js`

- ✅ **修复 "请先选择聊天对象"**：
  - 问题：JavaScript 中 `!currentChatId` 对 `0` 是 falsy
  - 修复：改为 `currentChatId === null || currentChatId === undefined || !currentChatType`

- ✅ **添加调试输出**：
  ```javascript
  function sendMessage() {
      console.log('sendMessage called: currentChatId=' + currentChatId + '...');
      if (currentChatId === null || currentChatId === undefined || !currentChatType) {
          alert('请先选择聊天对象. currentChatId=' + currentChatId + '...');
          return;
      }
      ...
  }
  ```

- ✅ **添加 `selectContact` 调试**：
  ```javascript
  function selectContact(id, type) {
      console.log('selectContact called: id=' + id + ', type=' + type + '...');
      currentChatId = id;
      currentChatType = type;
      ...
  }
  ```

### 7. 会议功能（部分实现）
**文件**：`go-toxhttpd/main.go`

- ⚠️ **限制**：`go-toxcore-c` 包中没有实现会议（Conference）相关 API
- ✅ **添加跟踪字段**：
  ```go
  type Server struct {
      ...
      conferences map[uint32]bool // Track conference list
  }
  ```
- ⚠️ **当前状态**：会议创建返回提示 "conference creation not fully implemented"

### 8. 编译环境
**文件**：`build.sh`

```bash
export CGO_CFLAGS=-I/opt/oldlibc-devsys/include
export CGO_LDFLAGS="-L/opt/oldlibc-devsys/lib -ltoxav -ltoxencryptsave -lopus -lsodium -lm"
cd go-toxhttpd/
CGO_ENABLED=1 go build -v
```

## 编译和运行

### 编译
```bash
cd /home/gzleo/aprog/toxhttpd
bash ./build.sh
```

### 运行
```bash
cd /home/gzleo/aprog/toxhttpd/go-toxhttpd
LD_LIBRARY_PATH=/opt/oldlibc-devsys/lib ./toxhttpd-go -port 8181 -debug info
```

### REST API 测试
```bash
# 获取自身信息
curl -s http://localhost:8181/api/self | jq .

# 获取好友列表
curl -s http://localhost:8181/api/friends | jq .

# Bootstrap
curl -s -X POST http://localhost:8181/api/bootstrap | jq .

# 事件流
curl -s "http://localhost:8181/api/events?after=0" | jq .
```

## 已知问题和限制

### 1. 会议功能未完全实现
- `go-toxcore-c` 缺少会议相关 API
- 需要等待包更新或自己实现

### 2. 添加好友错误
- 错误码 4：`TOX_ERR_FRIEND_ADD_OWN_KEY`（不能添加自己）
- 错误码 3：`TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_FOUND`（好友不在线）

### 3. Web 界面
- 会议和群组功能显示为空
- 需要添加真实 Tox 好友才能测试聊天功能

## 文件变更列表

| 文件 | 变更说明 |
|------|----------|
| `go-toxhttpd/main.go` | 完全重写，迁移到 go-toxcore-c，添加日志、bootstrap、数据保存等 |
| `go-toxhttpd/go.mod` | 更新依赖：添加 go-toxcore-c、移除旧 tox 包 |
| `go-toxhttpd/go.sum` | 自动生成 |
| `go-toxhttpd/tox/` | 已删除（不再需要自定义 tox 包） |
| `web/app.js` | 修复 currentChatId=0 问题，添加调试输出 |
| `build.sh` | 保持不变，使用原有交叉编译环境 |

## 下一步建议

1. **实现会议功能**：等待 `go-toxcore-c` 更新，或自己添加 cgo 封装
2. **添加更多 API**：`/api/self/name`、`/api/self/status` 等（已在 `app.js` 中调用，但 Go 端未实现）
3. **改进错误处理**：前端显示更友好的错误信息
4. **添加测试**：为 REST API 添加单元测试

## 总结

成功将项目从自定义 tox 封装迁移到标准的 `github.com/TokTok/go-toxcore-c` 库，添加了命令行参数、日志系统、自动 bootstrap、数据保存等功能。项目现在更符合 Go 生态，维护性更好。
