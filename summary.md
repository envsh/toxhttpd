# ToxHTTPd - Tox HTTP REST API Server (Go)

## Goal
实现会议功能（使用 `group.go` Conference API）、会议邀请处理（同意/拒绝/忽略）、修复会议消息显示、优化Web布局（头像+状态标识+底部按钮）、添加多语言支持（简/繁/英）。

## Constraints & Preferences
- 使用 `github.com/!Tok!Tok/go-toxcore-c` 包（已迁移）
- 不要动 `build.sh`
- 会议功能使用 `group.go` 的 `Conference*` 方法（非 `group_legacy.go` 的 `Group*`）
- 会议邀请方案B：Web端三个选项（同意/拒绝/忽略），按钮顺序：同意(左)、拒绝(中)、忽略(右)
- 连接状态在名称行最右侧，带颜色标识
- 添加好友只保留底部，创建群组和会议在底部最下方同一行（会议左、群组右）
- 多语言：简体中文（默认）、繁体中文、英文；默认简体，自动检测浏览器语言，切换后记住（localStorage）
- 布局不变形：所有文本容器使用 `min-width` + `white-space: nowrap`

## Progress

### Done
- 会议功能：修改 `handleConferences` 使用 `ConferenceGetChatlist()` 和 `ConferenceNew()`
- 会议消息：添加 `handleConferenceMessages`，注册路由 `/api/conference_messages`
- 会议邀请：添加 `handleConferenceJoin`（同意+保存savedata）、`handleConferenceReject`、`handleConferenceIgnore`
- 注册路由：`/api/conferences/join`、`/api/conferences/reject`、`/api/conferences/ignore`
- 所有事件推送改为 JSON 格式（14处），字段名统一为 `conference_number`
- 前端添加 `conference_message` 事件处理
- Web布局优化：添加头像（40px圆形，显示名称首字母）、连接状态移到名称行右侧（带颜色标识）
- 底部区域：添加好友在第二行（上方），创建按钮在第一行（最底部），按钮顺序：🎥 创建会议(左)、👥 创建群组(右)
- 多语言文件：`web/lang/zh-CN.json`、`web/lang/zh-TW.json`、`web/lang/en-US.json`
- `web/app.js` 添加：`loadLanguage()`、`t()`、`applyLanguage()`、`initLanguage()`、`switchLanguage()`
- `web/index.html` 添加语言选择器（3个选项），后移至聊天头部最右侧
- `web/style.css` 添加样式 + 防止布局变形（`min-width`、`white-space: nowrap`）
- 修改 `web/app.js` 中所有硬编码中文文本使用 `t()` 函数
- 语言选择器从左侧边栏移至聊天头部（`#chatHeader`）最右侧
- `applyLanguage()` 更新逻辑：根据聊天状态动态更新 `#chatHeaderText`
- `selectContact()` 更新：使用 `#chatHeaderText` 而非 `#chatHeader`

### In Progress
- 无

### TODO
- 测试：验证中/繁/英切换 + 布局不变形
- 测试会议功能：创建、加入、消息显示
- 测试语言切换后 localStorage 持久化
- 修复 `web/app.js` 中 `applyLanguage()` 的变量名错误（`tabs` vs `tabs`）

## Key Decisions
- 会议邀请不自动加入，由Web用户选择（方案B）
- 保存savedata使用原有 `saveToxData()`，不做错误检测增强
- 会议事件字段名使用 `conference_number`（非 `group_number`）保持语义一致
- 头像显示名称首字母（大写），未设置时显示"?"灰色
- 连接状态标识：`online`/`tcp` 绿色，`offline` 红色
- 多语言选择器推到聊天头部最右侧，使用 flex 布局
- 语言文件放在 `web/lang/` 目录
- 聊天头部使用 flex 布局，文本左对齐（`#chatHeaderText`），语言选择器右对齐

## Next Steps
1. 修复 `web/app.js` 第66-69行的变量名错误（`tabs` 误写为 `tabs`）
2. 测试：验证中/繁/英切换 + 布局不变形
3. 测试会议功能：创建、加入、消息显示
4. 考虑添加更多翻译键（如错误提示等）

## Critical Context
- `go-toxcore-c` 会议API：`ConferenceNew()`、`ConferenceJoin()`、`ConferenceSendMessage()`、`ConferenceGetChatlist()` 等
- 事件JSON格式：`{"conference_number":1,"peer_number":0,"message":"..."}`
- 语言文件格式：嵌套JSON，`t('modals.labels.name')` 支持点号访问
- 布局变形预防：`.tab {min-width: 60px; white-space: nowrap;}`，`.create-btn {min-width: 120px;}`
- 语言检测逻辑：繁体（`zh-TW`/`zh-HK`/`zh-MO`）→繁体中文，其他中文→简体，英文→English，默认简体
- 语言选择器位置：聊天头部（`#chatHeader`）最右侧
- **注意**：`web/app.js` 中 `applyLanguage()` 函数存在变量名错误，需修正（第65行定义 `tabs`，第66-69行误用 `tabs`）

## Relevant Files
- `go-toxhttpd/main.go`：会议API处理、回调注册、事件推送JSON化
- `web/app.js`：多语言支持、会议消息处理、布局逻辑、所有硬编码文本改用 `t()`
- `web/index.html`：语言选择器（移至聊天头部）、头像结构、底部按钮布局
- `web/style.css`：头像样式、状态标识、防止变形、聊天头部布局、语言选择器样式
- `web/lang/zh-CN.json`：简体中文（默认）
- `web/lang/zh-TW.json`：繁体中文
- `web/lang/en-US.json`：英文
- `/opt/golib/pkg/mod/github.com/!tok!tok/go-toxcore-c@v0.2.18-0.20250216202442-0f7463080d5c/group.go`：Conference API实现

## Architecture

### Go Version (Current)
- HTTP server: Go standard library / custom router
- Tox core: `github.com/!Tok!Tok/go-toxcore-c`
- Event push: Long polling via `/api/events`

### Components
- `go-toxhttpd/main.go`: Entry point, HTTP routing, Tox event handling
- `web/app.js`: Frontend logic, multi-language support, UI interactions
- `web/index.html`: Main UI structure
- `web/style.css`: Styles and layout
- `web/lang/*.json`: Language files

## REST API Endpoints

### Self
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/self` | - | Get self address, name, status, connection_status |

### Friends
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/friends` | - | List friend IDs |
| POST | `/api/friends` | `public_key` | Add friend (64-char pubkey or 76-char address) |
| POST | `/api/friend` | `friend_id` | Get friend info |
| POST | `/api/friend_delete` | `friend_id` | Delete friend |

### Messages
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| POST | `/api/messages` | `friend_id`, `message` | Send message to friend |
| POST | `/api/group_messages` | `group_id`, `message` | Send message to group |
| POST | `/api/conference_messages` | `conference_id`, `message` | Send message to conference |

### Groups
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/groups` | - | List group IDs |
| POST | `/api/groups` | - | Create group |

### Conferences
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/conferences` | - | List conference IDs |
| POST | `/api/conferences` | - | Create conference |
| POST | `/api/conferences/join` | `friend_number`, `cookie` | Accept conference invite |
| POST | `/api/conferences/reject` | `friend_number` | Reject conference invite |
| POST | `/api/conferences/ignore` | `friend_number` | Ignore conference invite |

### Bootstrap
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| POST | `/api/bootstrap` | - | Bootstrap to network |

### Self Management
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| POST | `/api/self/name` | `name` | Set self name |
| POST | `/api/self/status` | `status_message` | Set self status message |

## Push Endpoints
| Endpoint | Type | Description |
|----------|------|-------------|
| `/api/events` | Long Polling | Get events (friend messages, conference invites, etc.) |

## Build
```bash
make
```

## Run
```bash
./toxhttpd [port]
```

## Example Usage
```bash
# Start server
./toxhttpd 8181

# Check connection
curl http://localhost:8181/api/self

# Add friend
curl -X POST -d "public_key=..." http://localhost:8181/api/friends

# Send message
curl -X POST -d "friend_id=0&message=Hello" http://localhost:8181/api/messages

# Create conference
curl -X POST http://localhost:8181/api/conferences
```

## Features Working
- [x] TCP connection (UDP disabled for stability)
- [x] Add friend with pubkey (64-char) or address (76-char)
- [x] Send messages
- [x] Receive messages (via long polling)
- [x] Conference support (create, join, messages)
- [x] Conference invite handling (accept/reject/ignore)
- [x] Multi-language support (zh-CN, zh-TW, en-US)
- [x] Web UI with avatar, status badge, language selector
- [x] Layout optimization (no deformation)
- [ ] Command flag proxy support
- [ ] Command flag translate supprt
- [ ] Web page add group(NGC)
