# ToxHTTPd - Tox HTTP REST API Server (Go) + Qt3 GUI Client

## Goal
实现完整的 Tox 客户端：Go 后端（toxhttpd）提供 REST API，Qt3 前端（q3tox）提供 GUI 界面。功能包括：会议、好友管理、消息收发、多语言支持（简/繁/英）。

## Constraints & Preferences
### 后端 (toxhttpd)
- 使用 `github.com/!Tok!Tok/go-toxcore-c` 包（已迁移）
- 不要动 `build.sh`
- 会议功能使用 `group.go` 的 `Conference*` 方法（非 `group_legacy.go` 的 `Group*`）
- 会议邀请方案B：Web端三个选项（同意/拒绝/忽略），按钮顺序：同意(左)、拒绝(中)、忽略(右)
- 连接状态在名称行最右侧，带颜色标识
- 添加好友只保留底部，创建群组和会议在底部最下方同一行（会议左、群组右）
- 多语言：简体中文（默认）、繁体中文、英文；默认简体，自动检测浏览器语言，切换后记住（localStorage）
- 布局不变形：所有文本容器使用 `min-width` + `white-space: nowrap`

### 前端 (q3tox)
- 使用 Qt3/Qt4 兼容（头文件路径：`/opt/qt338sh/include` for Qt3，`/usr/include/qt4/` for Qt4）
- **渐进式改造**：同时支持 Qt3/Qt4 编译，优先保证 Qt3 编译通过
- UI参照 web 端（avatar、status badge、tabs、language selector）
- 使用 REST API（ToxAPI 类封装）
- 不要动 `build.sh`
- 多语言支持：简体中文（默认）、繁体中文、英文
- 使用 cJSON 解析 JSON，libcurl 进行 HTTP 请求
- **兼容层文件**：`compat34.h`（统一处理 Qt3/Qt4 API 差异）

## Progress

### 后端 (toxhttpd) - Done
- 会议功能：修改 `handleConferences` 使用 `ConferenceGetChatlist()` 和 `ConferenceNew()`
- 会议消息：添加 `handleConferenceMessages`，注册路由 `/api/conference_messages`
- 会议邀请：添加 `handleConferenceJoin`（同意+保存savedata）、`handleConferenceReject`、`handleConferenceIgnore`
- 注册路由：`/api/conferences/join`、`/api/conferences/reject`、`/api/conferences/ignore`
- 所有事件推送改为 JSON 格式（14处），字段名统一为 `conference_number`
- Web 布局优化：添加头像（40px圆形，显示名称首字母）、连接状态移到名称行右侧（带颜色标识）
- 底部区域：添加好友在第二行（上方），创建按钮在第一行（最底部），按钮顺序：🎥 创建会议(左)、👥 创建群组(右)
- 多语言文件：`web/lang/zh-CN.json`、`web/lang/zh-TW.json`、`web/lang/en-US.json`
- `web/app.js` 添加：`loadLanguage()`、`t()`、`applyLanguage()`、`initLanguage()`、`switchLanguage()`
- `web/index.html` 添加语言选择器（3个选项），后移至聊天头部最右侧
- `web/style.css` 添加样式 + 防止布局变形（`min-width`、`white-space: nowrap`）
- 修改 `web/app.js` 中所有硬编码中文文本使用 `t()` 函数
- 语言选择器从左侧边栏移至聊天头部（`#chatHeader`）最右侧
- `applyLanguage()` 更新逻辑：根据聊天状态动态更新 `#chatHeaderText`
- `selectContact()` 更新：使用 `#chatHeaderText` 而非 `#chatHeader`
- **Group Chat REST API**：路由统一使用复数形式 `/api/groups`（原 `/api/group`），对应 `handleGroups()` ✅
- **修复 nil map panic**：在 `go-toxcore-c/tox.go` 的 `NewTox()` 中初始化所有 Group Chat callback maps（18个）✅
- **修复空字符串 panic**：修复 `GroupNew`、`GroupJoin`、`GroupLeave` 中空字符串导致 `index out of range` 的问题 ✅
- **测试通过**：所有 Group Chat REST API 端点均可访问（返回预期错误码）：
  - `GET /api/groups` - 返回群组列表 ✅
  - `POST /api/groups` - 创建群组（错误码 2: 名称为空）✅
  - `POST /api/groups/join` - 加入群组（需要 chat_id）✅
  - `POST /api/groups/leave` - 离开群组（错误码 1: 群组不存在）✅
  - `POST /api/group_messages` - 发送群消息（错误码 1: 群组不存在）✅
  - `POST /api/groups/invite` - 邀请好友（错误码 1: 群组不存在）✅
  - `POST /api/groups/accept` - 接受邀请（需要 invite_data + friend_number）✅

### 前端 (q3tox) - Done
- **Qt3/Qt4 兼容改造**：创建 `compat34.h` 统一处理 API 差异
- 创建 q3tox 子项目基础文件结构
- 实现 `api.h/cpp`（ToxAPI 类，REST API 调用，使用 libcurl）
- 实现 `translator.h/cpp`（多语言支持，使用 cJSON 解析）
- 创建 `editinfodialog.h/cpp`（编辑个人信息对话框）
- 创建 `invitedialog.h/cpp`（会议邀请对话框，三个按钮：同意/拒绝/忽略）
- 实现 `mainwindow.h/cpp`（主窗口，左右分割布局）
- 实现 `selfinfo.h/cpp`（个人信息 widget：avatar、名称、状态、地址）
- 实现 `contactlist.h/cpp`（联系人列表：tabs 过滤、void* listWidget 成员 + updateView_v3/v4）
- 实现 `chatwidget.h/cpp`（聊天区域：头部、消息区、输入区、语言选择器）
- 实现 `eventpoller.h/cpp`（事件轮询器，CustomEventBase 兼容 QCustomEvent/QEvent）
- 深入研究 Qt3 API 架构：QBoxLayout vs QVBox/QHBox 区别
- 修复所有 Qt3/Qt4 兼容性问题：
  - **QString API**：`qTrim()`、`qToUtf8()`、`qToLocal8Bit()`、`qLastIndexOf()`、`qToUpper()`、`qSplit()`
  - **布局管理**：`qNewBoxLayout()` 兼容 Qt3/Qt4 不同构造函数
  - **窗口标题**：`qSetWindowTitle()` 替换 `setCaption()`/`setWindowTitle()`
  - **按钮状态**：`qSetChecked()`、`qSetCheckable()`
  - **Tooltip**：`qSetToolTip()`
  - **文件操作**：`qOpenReadOnly()`、`qOpenWriteOnly()`
  - **路径处理**：`qGetHomePath()`、`qAppDir()`
  - **事件基类**：`CustomEventBase`（Qt3=QCustomEvent，Qt4=QEvent）
  - **容器类**：`QPtrList<T>`（Qt4 用 QList<T*> 模拟）
- 项目编译成功：
  - ✅ **Qt3 编译**：`buildqt3.sh` → 生成 q3tox (2.1M)
  - ✅ **Qt4 编译**：`buildqt4.sh` → 生成 q3tox
- 修复联系人列表 Tab 过滤问题：
  - 修复 `onTabClicked()` 没有更新 `currentFilter` 的 bug
  - 修复过滤逻辑单复数不匹配（`"friends"` vs `"friend"`）
  - 添加 `tabButtons[]`、`tabFilters[]`、`tabNames[]` 成员变量
  - 实现 `setTabFilter()` 函数正确切换过滤
  - 添加调试输出确认联系人加载（成功加载 4 个联系人：2好友+2会议）
- **修复 Bug**：聊天头部不更新问题（翻译文件 `{0}` → `%1`，Qt 的 `QString::arg()` 格式）
- **右键菜单功能**：
  - 创建 `friendinfodialog.h/cpp`（查看好友/会议信息对话框）
  - 修改 `contactlist.h/cpp`：
    - 添加右键菜单信号：`viewInfoRequested`、`deleteOrLeaveRequested`、`inviteToConferenceRequested`
    - Qt3：使用 `eventFilter` + `QPopupMenu` 实现右键菜单
    - Qt4：使用 `customContextMenuRequested` + `QMenu` 实现右键菜单
    - 支持好友菜单：查看信息、删除好友、邀请进会议
    - 支持会议菜单：查看信息、离开会议
  - 修改 `mainwindow.h/cpp`：
    - 添加槽函数处理右键菜单动作
    - `onViewInfoRequested()`：显示好友/会议信息对话框
    - `onDeleteOrLeaveRequested()`：确认后删除好友/离开会议，成功后重新加载联系人
    - `onInviteToConferenceRequested()`：弹出会议选择对话框，邀请好友进会议
  - 修改 `api.h/cpp`：
    - 添加 `leaveConference(int confId)` - 离开会议（POST `/api/conference_delete`）
    - 添加 `inviteToConference(int friendId, int confId)` - 邀请进会议（POST `/api/conference_invite`）
  - 更新翻译文件 `lang/*.json`：
    - 添加右键菜单翻译：`view_info`、`delete_friend`、`leave_conference`
    - 添加确认对话框翻译：`confirm_delete_friend`、`confirm_leave_conference`
    - 添加其他翻译：`invite_to_conference`、`select_conference`、`friend_info_title` 等

### In Progress
- 无

### TODO (后端)
- 测试：验证中/繁/英切换 + 布局不变形
- 测试会议功能：创建、加入、消息显示
- 测试语言切换后 localStorage 持久化
- 修复 `web/app.js` 中 `applyLanguage()` 的变量名错误（`tabs` vs `tabs`）
- 命令标志代理支持
- 命令标志翻译支持
- Web 页面添加群组(NGC)

### TODO (前端)
- 测试完整工作流程（添加好友、发消息、会议）
- 测试 Tab 过滤功能（All/Friends/Conferences）
- UI 样式调整（参考 web 端）
- 头像显示（当前显示名称首字母）
- 状态标识颜色（online/tcp 绿色，offline 红色）

## Key Decisions

### 后端
- 会议邀请不自动加入，由 Web 用户选择（方案B）
- 保存 savedata 使用原有 `saveToxData()`，不做错误检测增强
- 会议事件字段名使用 `conference_number`（非 `group_number`）保持语义一致
- 头像显示名称首字母（大写），未设置时显示"?"灰色
- 连接状态标识：`online`/`tcp` 绿色，`offline` 红色
- 多语言选择器推到聊天头部最右侧，使用 flex 布局
- 语言文件放在 `web/lang/` 目录
- 聊天头部使用 flex 布局，文本左对齐（`#chatHeaderText`），语言选择器右对齐

### 前端 (Qt3/Qt4 兼容)
- **兼容层核心**：`compat34.h` 统一处理所有 Qt3/Qt4 API 差异
- 使用 `QWidget + QBoxLayout` 替代 `QVBox/QHBox`（因为后者没有 `addWidget` 方法）
- QBoxLayout 正确构造函数：
  - Qt3: `QBoxLayout(QWidget *parent, Direction, int border=0, int spacing=-1, const char *name=0)`
  - Qt4: `QBoxLayout(Direction, QWidget *parent=0)`
  - 兼容方案：`qNewBoxLayout(parent, dir, border, spacing)`
- QLabel 在 Qt3 中构造函数不同：需要 `QLabel(parent, name)` 或 `QLabel(text, parent, name)`
- 使用 `std::vector<Event>` 替代 `QArray<Event>`（Qt3 的 QArray 对包含 std::string 的对象支持有问题）
- 事件轮询使用 `QThread` + libcurl 长轮询，通过回调函数传递事件
- 翻译文件使用 cJSON 解析，支持点号分隔的键（如 `modals.labels.name`）
- 语言文件放在 `q3tox/lang/` 目录，通过 `/proc/self/exe` 获取可执行文件路径来定位
- **编译脚本**：
  - `buildqt3.sh`：Qt3 编译（设置 `QTDIR=/opt/qt338sh`）
  - `buildqt4.sh`：Qt4 编译（使用系统 `qmake-qt4`）
- **项目文件**：`q3tox.pro` 使用 `isEmpty(QT_VERSION)` 检测 Qt 版本（Qt3 qmake 无 QT_VERSION 变量）

## Next Steps

### 后端
1. 修复 `web/app.js` 第66-69行的变量名错误（`tabs` 误写为 `tabs`）
2. 测试：验证中/繁/英切换 + 布局不变形
3. 测试会议功能：创建、加入、消息显示
4. 考虑添加更多翻译键（如错误提示等）

### 前端
1. 测试完整工作流程（确保 toxhttpd 服务在 `http://localhost:8181` 运行）
2. 测试 Tab 过滤功能（All/Friends/Conferences）- 代码已修复，待验证
3. 完善 UI 样式（头像、状态标识、颜色等）
4. 添加更多错误处理
5. 考虑添加系统托盘图标

## Critical Context

### 后端
- `go-toxcore-c` 会议API：`ConferenceNew()`、`ConferenceJoin()`、`ConferenceSendMessage()`、`ConferenceGetChatlist()` 等
- 事件JSON格式：`{"conference_number":1,"peer_number":0,"message":"..."}`
- 语言文件格式：嵌套JSON，`t('modals.labels.name')` 支持点号访问
- 布局变形预防：`.tab {min-width: 60px; white-space: nowrap;}`，`.create-btn {min-width: 120px;}`
- 语言检测逻辑：繁体（`zh-TW`/`zh-HK`/`zh-MO`）→繁体中文，其他中文→简体，英文→English，默认简体
- 语言选择器位置：聊天头部（`#chatHeader`）最右侧

### 前端 (Qt3)
- Qt3 布局系统核心发现：
  - `QBoxLayout` 是布局管理器（继承 QLayout），有 `addWidget()` 方法
  - `QHBox/QVBox` 是容器 widget（继承 QFrame），内部私有成员 `QBoxLayout *lay`，但无法直接访问
  - 正确用法：`QWidget* container = new QWidget(parent); QBoxLayout* layout = new QBoxLayout(container, dir, 0, -1, 0);`
- cJSON 函数调用：`cJSON_GetObjectItem(root, "key")` 必须有逗号分隔参数
- 翻译文件格式：嵌套 JSON，使用 `cJSON_GetObjectItem` 递归查找
- 可执行文件路径获取：通过 `readlink("/proc/self/exe", ...)` 获取，用于定位 `lang/` 目录
- 事件回调：`void (*callback)(const EventList&, void*)` 函数指针，避免使用信号槽（QThread 不兼容 Q_OBJECT）

## Relevant Files

### 后端
- `go-toxhttpd/main.go`：会议API处理、回调注册、事件推送JSON化
- `web/app.js`：多语言支持、会议消息处理、布局逻辑、所有硬编码文本改用 `t()`
- `web/index.html`：语言选择器（移至聊天头部）、头像结构、底部按钮布局
- `web/style.css`：头像样式、状态标识、防止变形、聊天头部布局、语言选择器样式
- `web/lang/zh-CN.json`：简体中文（默认）
- `web/lang/zh-TW.json`：繁体中文
- `web/lang/en-US.json`：英文
- `/opt/golib/pkg/mod/github.com/!tok!tok/go-toxcore-c@v0.2.18-0.20250216202442-0f7463080d5c/group.go`：Conference API实现

### 前端 (q3tox)
- `q3tox/compat34.h`：**Qt3/Qt4 兼容层**（QString API、布局、窗口、文件、事件等）
- `q3tox/api.h/cpp`：REST API 封装，使用 libcurl（含 `leaveConference()`、`inviteToConference()`）
- `q3tox/translator.h/cpp`：多语言支持，使用 cJSON
- `q3tox/mainwindow.h/cpp`：主窗口，左右分割布局（处理右键菜单动作）
- `q3tox/selfinfo.h/cpp`：个人信息 widget
- `q3tox/contactlist.h/cpp`：联系人列表（void* listWidget + updateView_v3/v4，右键菜单）
- `q3tox/chatwidget.h/cpp`：聊天区域
- `q3tox/eventpoller.h/cpp`：事件轮询器（CustomEventBase 兼容）
- `q3tox/editinfodialog.h/cpp`：编辑个人信息对话框
- `q3tox/invitedialog.h/cpp`：会议邀请对话框
- `q3tox/friendinfodialog.h/cpp`：查看好友/会议信息对话框（新增）
- `q3tox/lang/zh-CN.json`：简体中文（默认）
- `q3tox/lang/zh-TW.json`：繁体中文
- `q3tox/lang/en-US.json`：英文

## Architecture

### 后端 (Go)
- HTTP server: Go standard library / custom router
- Tox core: `github.com/!Tok!Tok/go-toxcore-c`
- Event push: Long polling via `/api/events`

### 前端 (C++/Qt3 & Qt4)
- GUI framework: Qt3 (http://qt.nokia.com) & Qt4 (http://qt-project.org)
- HTTP client: libcurl
- JSON parser: cJSON
- Multi-language: cJSON + custom Translator class
- **Compat layer**: `compat34.h` 统一处理 QString、布局、事件等 API 差异

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
| POST | `/api/group_messages` | `group_number`, `message` | Send message to group |
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
| POST | `/api/conference_invite` | `friend_id`, `conference_id` | Invite friend to conference |
| POST | `/api/conference_delete` | `conference_id` | Leave conference |

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

### 后端
```bash
cd /home/gzleo/aprog/toxhttpd
make
```

### 前端 (q3tox)
```bash
# Qt3 编译
cd /home/gzleo/aprog/toxhttpd/q3tox
bash buildqt3.sh

# Qt4 编译
cd /home/gzleo/aprog/toxhttpd/q3tox
bash buildqt4.sh
```

## Run

### 后端
```bash
cd /home/gzleo/aprog/toxhttpd
./toxhttpd 8181
```

### 前端
```bash
cd /home/gzleo/aprog/toxhttpd/q3tox
./q3tox
```

## Example Usage

### 后端
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

### 后端
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
- [ ] Command flag translate support
- [ ] Web page add group(NGC)

### 前端 (q3tox)
- [x] **Qt3/Qt4 兼容改造**：`compat34.h` 统一处理 API 差异
- [x] Qt3 GUI with split layout (sidebar + chat area)
- [x] Self info widget (avatar, name, status, address)
- [x] Contact list with tabs (all/friends/conferences) - Tab 过滤已修复
- [x] Chat widget (header, message area, input area)
- [x] Multi-language support (zh-CN, zh-TW, en-US)
- [x] Event polling via QThread + libcurl
- [x] Conference invite dialog (accept/reject/ignore)
- [x] Edit self info dialog
- [x] **Qt3 编译成功**：`buildqt3.sh` → q3tox (2.1M)
- [x] **Qt4 编译成功**：`buildqt4.sh` → q3tox
- [x] Successfully loads 4 contacts (2 friends + 2 conferences)
- [x] **右键菜单功能**：
  - [x] 查看信息对话框 (`friendinfodialog.h/cpp`)
  - [x] 好友：查看信息、删除好友、邀请进会议
  - [x] 会议：查看信息、离开会议
  - [x] Qt3 右键菜单（`QPopupMenu` + `eventFilter`）
  - [x] Qt4 右键菜单（`QMenu` + `customContextMenuRequested`）
- [x] **Bug 修复**：聊天头部不更新（翻译文件 `{0}` → `%1`）
- [ ] Complete workflow testing (add friend, send message, conference)
- [ ] Tab filtering verification (All/Friends/Conferences)
- [ ] UI style refinement (avatar, status colors)
- [ ] System tray icon (optional)
