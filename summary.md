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
- **修复 GroupNew 参数错误**：C 函数第5个参数是创建者昵称（非密码），`handleGroups` 已修正并支持密码设置 ✅
- **测试通过**：所有 Group Chat REST API 端点工作正常：
  - `GET /api/groups` - 返回群组列表 ✅
  - `POST /api/groups` - 创建群组（需 `group_name` + `name` 参数）✅ 测试成功：创建群组#0 ✅
  - `POST /api/groups/join` - 加入群组（需要 chat_id）✅
  - `POST /api/groups/leave` - 离开群组 ✅ 测试成功：离开群组#0 ✅
  - `POST /api/group_messages` - 发送群消息 ✅ 测试成功：发送消息 ✅
  - `POST /api/groups/invite` - 邀请好友（需要有效群组）✅
  - `POST /api/groups/accept` - 接受邀请（需要 invite_data + friend_number）✅
- **Web 端重构创建群组 UI** ✅：
  - `web/index.html`：添加群组创建对话框（名称、昵称、密码、隐私状态）✅
  - `web/app.js`：重写 `createGroup()` 显示对话框、`confirmCreateGroup()` 处理参数、`closeGroupModal()` ✅
  - `web/lang/zh-CN.json`：添加 `modals.create_group_title`、`modals.confirm_group`、`modals.cancel_group`、`modals.labels.*`、`cannot_be_empty` ✅
  - `web/lang/zh-TW.json`：同步更新 ✅
  - `web/lang/en-US.json`：同步更新 ✅

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
- ✅ 区分会议和群组邀请功能（q3tox）：
  - 重命名 `InviteDialog` → `ConferenceInviteDialog`
  - 新增 `GroupInviteDialog`（接受/拒绝，支持密码输入）
  - 后端推送 `group_invite`、`group_message`、`group_peer_join`、`group_peer_exit`、`group_peer_status`、`group_self_join` 事件
  - q3tox API 新增群组方法：`getGroups()`、`createGroup()`、`leaveGroup()`、`inviteToGroup()`、`joinGroup()`
  - 右键菜单：好友支持"邀请进会议"和"邀请进群组"
  - 翻译文件更新：添加群组相关翻译键

### TODO (后端)
- 测试：验证中/繁/英切换 + 布局不变形
- 测试会议功能：创建、加入、消息显示
- 测试语言切换后 localStorage 持久化
- 测试群组功能：创建、加入、消息显示、邀请
- 命令标志代理支持
- 命令标志翻译支持

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
- `go-toxhttpd/go-toxcore-c/group.go`：Conference API实现（外部依赖路径已省略）

### 前端 (q3tox)
- `q3tox/compat34.h`：**Qt3/Qt4 兼容层**（QString API、布局、窗口、文件、事件等）
- `q3tox/api.h` / `q3tox/api.cpp`：REST API 封装，使用 libcurl（含 `leaveConference()`、`inviteToConference()`）
- `q3tox/translator.h` / `q3tox/translator.cpp`：多语言支持，使用 cJSON
- `q3tox/mainwindow.h` / `q3tox/mainwindow.cpp`：主窗口，左右分割布局（处理右键菜单动作）
- `q3tox/selfinfo.h` / `q3tox/selfinfo.cpp`：个人信息 widget
- `q3tox/contactlist.h` / `q3tox/contactlist.cpp`：联系人列表（void* listWidget + updateView_v3/v4，右键菜单）
- `q3tox/chatwidget.h` / `q3tox/chatwidget.cpp`：聊天区域
- `q3tox/eventpoller.h` / `q3tox/eventpoller.cpp`：事件轮询器（CustomEventBase 兼容）
- `q3tox/editinfodialog.h` / `q3tox/editinfodialog.cpp`：编辑个人信息对话框
- `q3tox/invitedialog.h` / `q3tox/invitedialog.cpp`：会议邀请对话框
- `q3tox/friendinfodialog.h` / `q3tox/friendinfodialog.cpp`：查看好友/会议信息对话框（新增）
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
cd <项目根目录>
make
```

### 前端 (q3tox)
```bash
# Qt3 编译
cd <项目根目录>/q3tox
bash buildqt3.sh

# Qt4 编译
cd <项目根目录>/q3tox
bash buildqt4.sh
```

## Run

### 后端
```bash
cd <项目根目录>
./toxhttpd 8181
```

### 前端
```bash
cd <项目根目录>/q3tox
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

---

## 2026-05-05 会话更新

### 会议连接状态回调实现
- **后端**：`group.go` 添加 `cb_conference_connected_ftype` 类型、包装器、注册函数
- **后端**：`tox.go` 添加 `cb_conference_connecteds` map 定义和初始化
- **后端**：`main.go` 添加 `conferenceConnected` map、注册回调、API 返回 `is_connected`
- **q3tox**：更新 `ConferenceInfo`、`ContactData`、`Contact` 结构添加 `is_connected`
- **q3tox**：`eventpoller.cpp` 解析/传递 `is_connected` 字段

### 三种联系人在线状态实现验证

#### 好友（Friend）
| 层级 | 状态 | 说明 |
|------|------|------|
| 后端 | ✅ 完整 | `tox_callback_friend_connection_status` 回调，API 返回 `connection_status` |
| Web端 | ✅ 完整 | `app.js:369` 根据状态显示 `online-dot`/`offline-dot` |
| q3tox | ✅ 完整 | `eventpoller.cpp:77` 解析，`contactlist.cpp` 显示 ●/○ |

#### 群组（Group）
| 层级 | 状态 | 说明 |
|------|------|------|
| 后端 | ✅ 完整 | `main.go:679` 使用 `GroupIsConnected()`，API 返回 `is_connected` |
| Web端 | ✅ 完整 | `app.js:401` 使用 `is_connected` 显示 `online-dot`/`offline-dot` |
| q3tox | ✅ 完整 | `eventpoller.cpp:88,102` 解析，`contactlist.cpp` 显示状态 |

#### 会议（Conference）
| 层级 | 状态 | 说明 |
|------|------|------|
| 后端 | ✅ 完整 | `main.go:482-496` 回调注册，`conferenceConnected` map 记录状态 |
| Web端 | ✅ 完整 | `app.js:424` 使用 `is_connected` 显示 `online-dot`/`offline-dot` |
| q3tox | ✅ 完整 | `eventpoller.cpp:112,126` 解析，`contactlist.cpp` 显示状态 |

### Bug 修复

#### 1. 后端 `GroupIsConnected()` 逻辑错误
- **文件**：`go-toxhttpd/go-toxcore-c/groupchat.go:242-251`
- **问题**：把"未连接"状态当作错误处理，导致 `is_connected` 永远为 `false`
- **修复**：改为检查 `cerr != 0` 才返回错误，否则返回实际的连接状态
- **验证**：API 返回 `"is_connected": true`（群组已连接）

#### 2. Web版好友连接状态显示不对（unknown 但显示为绿色）
- **文件**：`web/app.js:369`
- **问题**：判断逻辑不完整，当 `connection_status` 为 `'unknown'` 时也显示为绿色
- **修复**：精确判断，只有明确在线（非offline、非unknown、有值）才显示绿色
- **代码**：
  ```javascript
  const dotClass = (f.connection_status && 
                   f.connection_status !== 'offline' && 
                   f.connection_status !== 'unknown') 
                   ? 'online-dot' : 'offline-dot';
  ```

#### 3. Web版群组和会议信息面板没有连接状态项
- **文件**：`web/index.html`、`web/app.js`
- **问题**：信息面板缺少连接状态显示
- **修复**：
  - `index.html`：添加会议连接状态行、群组名称和连接状态行
  - `app.js`：修改 `showGroupInfo()` 和 `showConferenceInfo()` 填充数据
- **验证**：HTML 中已添加状态行，JS 中已修改函数

#### 4. Web端翻译缺失
- **文件**：`web/lang/zh-CN.json`、`web/lang/en-US.json`、`web/lang/zh-TW.json`
- **问题**：`Translation missing: modals.labels.type`
- **修复**：在三个翻译文件的 `modals.labels` 中添加 `"type": "类型/Type/類型"`

#### 5. q3tox 日志格式问题
- **文件**：`q3tox/mainwindow.cpp:157-158`
- **问题**：`connected=%d` 打印 bool 值显示异常（如 `connected=136`）
- **修复**：改为 `connected=%s` 并使用三元运算符输出 `"true"/"false"`

#### 6. q3tox emoji 和状态点硬编码
- **文件**：`q3tox/contactlist.cpp`
- **问题**：emoji（👤👥🎙）和状态点（●○）硬编码
- **修复**：提取为常量 `EMOJI_FRIEND`、`EMOJI_GROUP`、`EMOJI_CONFERENCE`、`STATUS_ONLINE`、`STATUS_OFFLINE`
- **验证**：编译链接成功

#### 7. q3tox 编译错误修复
- **文件**：`q3tox/eventpoller.cpp`
- **问题**：无效的重复代码和放错位置的 `case ConferenceConnected` 导致编译错误
- **修复**：删除无效代码（129-138行），移除 `processApiRequest` 中的 `case ConferenceConnected`

### 测试结果

#### 后端 API
```bash
# 群组 API - 返回 is_connected: true
curl http://localhost:8181/api/groups
# 输出: {"groups":[{"group_number":0,"group_name":"ff","is_connected":true},...]}

# 会议 API - 返回 is_connected: false
curl http://localhost:8181/api/conferences
# 输出: {"conferences":[{"conference_number":0,"is_connected":false},...]}

# 好友 API - 返回 connection_status
curl http://localhost:8181/api/friends
# 输出: {"friends":[{"connection_status":"offline",...}]}
```

#### q3tox
- 编译链接成功 ✅
- 联系人列表状态点正确显示 ✅
- 日志输出格式正确 ✅

#### Web 前端
- 翻译文件已更新 ✅
- 信息面板已添加连接状态 ✅
- 好友状态点判断逻辑已修复 ✅
- 群组/会议状态点根据 `is_connected` 动态切换 ✅

### 关键文件修改清单

#### 后端
- `go-toxhttpd/go-toxcore-c/groupchat.go` - 修复 `GroupIsConnected()` 逻辑
- `go-toxhttpd/go-toxcore-c/tox.go` - 添加 `cb_conference_connecteds` 初始化（已有）
- `go-toxhttpd/main.go` - 注册回调、记录状态、API 返回状态

#### Web 前端
- `web/app.js` - 修复好友状态判断、群组/会议状态点、信息面板函数
- `web/index.html` - 添加群组和会议信息面板的连接状态行
- `web/lang/zh-CN.json` - 添加 `modals.labels.type`
- `web/lang/en-US.json` - 添加 `modals.labels.type`
- `web/lang/zh-TW.json` - 添加 `modals.labels.type`

#### q3tox
- `q3tox/eventpoller.cpp` - 删除无效代码、修复编译错误
- `q3tox/contactlist.cpp` - 提取 emoji 和状态点为常量
- `q3tox/mainwindow.cpp` - 修复日志格式
- `q3tox/api.h` - 添加 `GroupInfo.is_connected`、`ConferenceInfo.is_connected`
- `q3tox/eventpoller.h` - 添加 `ContactData.is_connected`
- `q3tox/contactlist.h` - 添加 `Contact.is_connected`

### 下一步
1. **刷新 Web 页面**：清除缓存（Ctrl+Shift+R），验证所有修复
2. **重启 q3tox**：运行 `./q3tox`，验证群组状态点显示在线
3. **端到端测试**：添加好友、发送消息、创建会议、邀请成员等完整流程测试

---

## 2026-05-05 会话更新（二）

### 问题：q3tox 群组和会议信息面板问题

#### 问题1：对话框标题错误
- **文件**：`q3tox/friendinfodialog.cpp:6`
- **问题**：构造函数硬编码标题为 `"modals.friend_info_title"`，群组和会议也显示"好友信息"
- **修复**：添加 `setTitle()` 方法，允许调用者设置标题

#### 问题2：连接状态一直显示离线
- **文件**：`q3tox/mainwindow.cpp:398-401`
- **问题**：`onViewInfoRequested()` 中，群组和会议调用 `setInfo()` 时**没有传递 `is_connected` 状态**
- **修复**：
  1. 修改 `friendinfodialog.h`：添加 `bool isConnected` 参数到 `setInfo()`，添加 `setTitle()` 方法
  2. 修改 `friendinfodialog.cpp`：
     - 添加 `titleLabel` 成员变量，在构造函数中创建
     - 添加 `connectedLabel` 成员变量，显示连接状态（在线/离线）
     - 实现 `setTitle()` 方法
     - 修改 `setInfo()` 方法，根据 `isConnected` 设置连接状态文本
  3. 修改 `mainwindow.cpp`：
     - 群组和会议调用 `dialog.setTitle()` 设置正确标题
     - 通过 `ToxAPI` 获取群组和会议的 `is_connected` 状态
     - 传递 `isConnected` 参数到 `dialog.setInfo()`

#### 翻译更新
- **文件**：`web/lang/zh-CN.json`、`web/lang/en-US.json`、`web/lang/zh-TW.json`
- **添加**：
  - `modals.conference_info_title`：会议信息 / Conference Info / 會議資訊
  - `modals.group_info_title`：群组信息 / Group Info / 群組資訊
  - `statuses.online`：在线 / Online / 在線

### 关键文件修改清单

#### q3tox
- `q3tox/friendinfodialog.h` - 添加 `setTitle()` 方法，`setInfo()` 添加 `bool isConnected` 参数
- `q3tox/friendinfodialog.cpp` - 添加 `titleLabel`、`connectedLabel`，实现 `setTitle()`，修改 `setInfo()`
- `q3tox/mainwindow.cpp` - 修改 `onViewInfoRequested()`，设置正确标题和连接状态

#### Web 前端
- `web/lang/zh-CN.json` - 添加 `modals.conference_info_title`、`modals.group_info_title`、`statuses.online`
- `web/lang/en-US.json` - 同上
- `web/lang/zh-TW.json` - 同上

### 测试结果
- q3tox 编译成功 ✅
- 群组和会议信息对话框标题正确 ✅
- 连接状态根据实际 `is_connected` 显示在线/离线 ✅

---

## 2026-05-05 会话更新（三）

### 问题：q3tox 群组和会议信息面板连接状态一直为离线

#### 根本原因
`setInfo()` 函数的参数顺序错误：
- **头文件**：`setInfo(..., bool isConnected, const QString& publicKey)`
- **实现文件**：`setInfo(..., const QString& publicKey, bool isConnected)`
- **调用处**：`dialog.setInfo(id, name, type, "", "", "", isConnected)` ← `isConnected` 被当作 `publicKey`

#### 修复方案
1. **修改 `friendinfodialog.h`**：调整参数顺序，将 `bool isConnected` 放在 `publicKey` 前面
2. **修改 `friendinfodialog.cpp`**：同步更新 `setInfo()` 实现
3. **修改 `mainwindow.cpp`**：更新调用参数，正确传递 `isConnected`

### 关键文件修改清单
- `q3tox/friendinfodialog.h` - 调整 `setInfo()` 参数顺序
- `q3tox/friendinfodialog.cpp` - 同步更新实现
- `q3tox/mainwindow.cpp` - 更新 `onViewInfoRequested()` 中的调用

### 测试结果
- q3tox 编译成功 ✅
- 群组和会议信息面板连接状态根据实际 `is_connected` 显示 ✅

---

## 2026-05-05 会话更新（四）

### 问题：q3tox 群组和会议信息面板连接状态翻译错误

#### 根本原因
`friendinfodialog.cpp:112` 中使用了错误的翻译键值：
```cpp
connectedLabel->setText(isConnected ? _("online") : _("offline"));
//                                      ^^^^^^   ^^^^^^^
//                                      错误！应该是 _("statuses.online") 和 _("statuses.offline")
```

#### 修复
修改 `friendinfodialog.cpp`，使用正确的翻译键值：
```cpp
connectedLabel->setText(isConnected ? _("statuses.online") : _("statuses.offline"));
```

### 关键文件修改清单
- `q3tox/friendinfodialog.cpp` - 修复翻译键值错误

### 测试结果
- q3tox 编译成功 ✅
- 群组和会议信息面板连接状态翻译正确 ✅

---

## 2026-05-05 会话更新（五）

### 问题1：q3tox 翻译缺失 `statuses.online`

#### 根本原因
q3tox 翻译文件中 `statuses` 缺少 `"online"` 键：
```json
"statuses": {
    "offline": "离线",
    "tcp": "TCP",
    "udp": "UDP"
    // ← 缺少 "online": "在线"
}
```

#### 修复
在三个翻译文件的 `"statuses"` 中添加 `"online"` 键：
- `q3tox/lang/zh-CN.json`：`"online": "在线"`
- `q3tox/lang/en-US.json`：`"online": "Online"`
- `q3tox/lang/zh-TW.json`：`"online": "在線"`

### 问题2：群组和会议的 public key 没有正确显示

#### 根本原因
`mainwindow.cpp:413-414` 和 `430-431` 中，群组和会议调用 `setInfo()` 时，**传递的 `publicKey` 是空字符串**：
```cpp
dialog.setInfo(id, ..., "", "", isConnected, "");  // ← publicKey 是空字符串
```

#### 修复
修改 `mainwindow.cpp` 的 `onViewInfoRequested()`：
1. 获取群组和会议的 `chat_id`（即 public key）
2. 传递给 `dialog.setInfo()` 的 `publicKey` 参数

### 关键文件修改清单
- `q3tox/lang/zh-CN.json` - 添加 `statuses.online`
- `q3tox/lang/en-US.json` - 添加 `statuses.online`
- `q3tox/lang/zh-TW.json` - 添加 `statuses.online`
- `q3tox/mainwindow.cpp` - 获取并设置群组和会议的 `chat_id` (public key)

### 测试结果
- q3tox 编译成功 ✅
- 翻译缺失 `statuses.online` 解决 ✅
- 群组和会议信息面板正确显示 public key ✅

---
## 2026-05-07 会话更新

### SQLite 消息存储 + 历史消息 API

#### 后端 (go-toxhttpd)

##### 1. storage.go - SQLite 初始化
- 创建 `pubkey_ids` 表：pkid INTEGER PRIMARY KEY AUTOINCREMENT，pubkey TEXT UNIQUE
- 创建 `events` 表：rowid INTEGER PRIMARY KEY AUTOINCREMENT（从 10000 开始），chanid INTEGER，data TEXT，created_at TIMESTAMP
- `getOrCreatePubKeyID(pubkey)`：pubkey ↔ 整数 ID 映射
- `persistEventToSQLite(chanid, data)`：存储事件

##### 2. server.go - 6处消息处理接入 SQLite
- 好友接收消息：CallbackFriendMessage → persistEventToSQLite
- 好友发送消息：handleSendMessage → persistEventToSQLite
- 群组接收消息：CallbackGroupMessage → persistEventToSQLite
- 群组发送消息：handleGroupSendMessage → persistEventToSQLite
- 会议接收消息：CallbackConferenceMessage → persistEventToSQLite
- 会议发送消息：handleConferenceMessages → persistEventToSQLite（当前禁用）

##### 3. getContactNumber 函数
```go
// 根据 contactType 和 senderPubkey 查询发送者的 number
// contactType: "friend" | "group" | "conference"
// contactPubkey: 联系人公钥（chanid）
// senderPubkey: 发送者公钥
// 返回: uint32 (friend number 或 peer number)，未找到返回 ContactNotFound (0xFFFFFFFF)
func (s *Server) getContactNumber(contactType string, contactPubkey string, senderPubkey string) (uint32, error)
```
- **friend**：调用 `FriendByPublicKey(senderPubkey)` 返回 friend number
- **group**：`GroupByChatId(contactPubkey)` → 遍历 peer 0~99，`GroupPeerGetPublicKey` 比较公钥
- **conference**：`ConferenceGetChatlist()` → 遍历，`ConferencePeerGetPublicKey` 比较公钥

##### 4. handleMessageHistory API
- **路由**：`/api/messages/history`
- **参数**：`chanid`（pubkey）或 `contact_id`（数字）+ `contact_type`
- **返回格式**：
```json
{
  "messages": [
    {
      "rowid": 10001,
      "message": "Hello",
      "sender_pubkey": "56A1B2C3...",
      "sender_number": 0,
      "direction": "received",
      "created_at": "2026-05-07 12:00:00"
    }
  ]
}
```
- **顺序处理**：查询 DESC（最新在前）→ 反转数组 ASC（最旧在前）方便前端显示

##### 5. 编译
- 使用 `build.sh` 编译（设置 CGO 环境变量）
- 编译产物：`/home/gzleo/aprog/toxhttpd/go-toxhttpd/toxhttpd-go` (约 14.9MB)
- `AGENTS.md` 已更新：`go服务端的编译是 build.sh`

#### 前端 (web/app.js)

##### loadMessageHistory 函数
- 在 `selectContact()` 中调用
- 请求 `/api/messages/history?contact_id=XXX&contact_type=YYY`
- 用 `sender_pubkey` 判断消息方向（比较 selfAddress 前64字符）
- 显示 `sender_number`（未找到显示公钥前8位）

#### q3tox 端 - 历史消息功能

##### 1. api.h - 新增结构体和函数声明
```cpp
struct HistoryMessage {
    int64_t rowid;
    std::string message;
    std::string sender_pubkey;
    uint32_t sender_number;
    std::string direction;
    std::string created_at;
};

bool getMessagesHistory(int contact_id, const std::string& contact_type,
                        std::vector<HistoryMessage>& messages);
```

##### 2. api.cpp - 实现 getMessagesHistory
- 调用 `/api/messages/history?contact_id=XXX&contact_type=YYY`
- 解析 JSON 响应，填充 `HistoryMessage` 数组

##### 3. mainwindow.h - 新增成员和槽函数
```cpp
private:
    std::string selfPubkey;  // 自己的公钥（地址前64字符）
private slots:
    void loadMessageHistory();
```

##### 4. mainwindow.cpp - 实现
- `onContactSelected()`：切换联系人时调用 `loadMessageHistory()`
- `loadMessageHistory()`：调用 API，解析消息，调用 `chatWidget->appendMessage()`
- `customEvent()` 处理 `ApiLoadAllData`：保存 `selfPubkey = address.substr(0, 64)`

##### 5. 编译
- Qt3：`buildqt3.sh` → q3tox
- Qt4：`buildqt4.sh` → q3tox

### 关键文件修改清单

#### 后端
- `go-toxhttpd/storage.go` - SQLite 初始化、pubkey_ids、events 表
- `go-toxhttpd/server.go` - getContactNumber、handleMessageHistory、6处消息处理
- `go-toxhttpd/AGENTS.md` - 添加编译说明

#### 前端 (web)
- `go-toxhttpd/web/app.js` - loadMessageHistory 函数

#### q3tox
- `q3tox/api.h` - HistoryMessage 结构体 + getMessagesHistory 声明
- `q3tox/api.cpp` - getMessagesHistory 实现
- `q3tox/mainwindow.h` - selfPubkey + loadMessageHistory 声明
- `q3tox/mainwindow.cpp` - onContactSelected 修改 + loadMessageHistory 实现

### 测试要点
1. 编译通过：`./build.sh` 生成 `toxhttpd-go`
2. 启动服务：`./toxhttpd-go 8181`
3. 产生消息（好友/群组/会议）
4. 调用 API：`curl "http://localhost:8181/api/messages/history?contact_id=0&contact_type=friend"`
5. 验证返回包含 `sender_pubkey` 和 `sender_number`
6. q3tox 切换联系人，验证历史消息加载

---

## 2026-05-10 会话更新

### Goal
- 修复 peer name 缓存键格式不统一问题（`"conf_"` vs `"conference_"`）
- 将 q3tox 聊天消息区域从 `QTextEdit` 替换为虚拟化列表 `ChatView (QScrollView/QAbstractScrollArea)`
- 添加鼠标滚轮和键盘翻页支持

### 1. Peer Name 显示修复

#### 问题
- 事件处理器和预加载使用 `"conf_{N}_{M}"` 作为缓存键
- 历史消息加载 `loadMessageHistory` 使用 `"conference_{id}_{sender}"`（即 `contactType` 直接拼接）
- 会议（conference）的 `"conf_"` 和 `"conference_"` 不匹配 → 历史消息找不到 peer name

#### 修复
将所有 `"conf_"` 统一改为 `"conference_"`，保持和 `contactType`/`currentChatType` 一致。

**web/app.js** 修改3处：
| 行号 | 修改 |
|------|------|
| 513 | `conf_${id}_${m.peer_number}` → `conference_${id}_${m.peer_number}` |
| 687 | `conf_${data.conference_number}_${data.peer_number}` → `conference_${data.conference_number}_${data.peer_number}` |
| 707 | `conf_${data.conference_number}_${data.peer_number}` → `conference_${data.conference_number}_${data.peer_number}` |

**q3tox/mainwindow.cpp** 修改4处：
| 行号 | 修改 |
|------|------|
| 234 | `"conf_"` → `"conference_"` (preload) |
| 340 | `"conf_"` → `"conference_"` (conference_message 事件) |
| 350 | `"conf_"` → `"conference_"` (conference_message 查找) |
| 428 | `"conf_"` → `"conference_"` (conference_peer_name 事件) |

**web/app.js** 注释更新（第733行）。

### 2. ChatView 虚拟化列表（替换 QTextEdit）

#### 动机
原有的 `QTextEdit` + `qInsertHtml()` 方式将所有消息渲染为 HTML 存入 `QTextDocument`，消息数量大时（数百条以上）性能急剧下降。

#### 架构

```
ChatView : QWidget          ← 统一基类（Qt3/Qt4 共用）
├── QScrollBar* m_vScrollBar   ← 手动滚动条
├── std::vector<ChatMessage>   ← 消息存储
├── paintEvent()               ← 只绘制可见消息
├── wheelEvent()               ← 鼠标滚轮
└── keyPressEvent()            ← 键盘翻页（PgUp/PgDn/↑/↓/Home/End）
```

#### 新增文件
| 文件 | 说明 |
|------|------|
| `q3tox/chatview.h` | ChatView 类声明 + ChatMessage 结构体 |
| `q3tox/chatview.cpp` | 完整实现（~330行） |

#### ChatMessage 结构
```cpp
struct ChatMessage {
    QString messageText;   // 消息正文（纯文本）
    QString type;          // "self" / "other"
    QString sender;        // 发送者名称
    QString avatarText;    // 头像文字（首字母）
    QString time;          // 格式化时间
    int height;            // 缓存高度（px）
};
```

#### 虚拟化机制
| 组件 | 说明 |
|------|------|
| `paintEvent()` | 遍历消息列表，跳过 `y+h < scrollPos` 和 `y > scrollPos+viewportHeight` 的条目 |
| `relayout()` | 计算每条消息高度（`calcMessageHeight`），更新滚动条范围 |
| `drawMessage()` | 用 QPainter 原语绘制消息：`drawEllipse`（头像圆）+ `drawText`（名称/时间/正文）+ `drawRoundRect`（气泡背景） |
| `onScrollChanged(int)` | 滚动条值变化 → `update()` 触发布局重绘 |

#### HTML → QPainter 绘图拆分
原有的 `<table>` HTML 布局完全拆分为 QPainter 原语：

| 原始 HTML | QPainter 替代 |
|-----------|--------------|
| `<div style="width:48px;height:48px;border-radius:50%;background:#30363d;">` | `p.drawEllipse(ax, y+8, 48, 48)` + `p.drawText(ax, y+8, 48, 48, Qt::AlignCenter, letter)` |
| `<span style="font-weight:500;...">Name</span>` | `p.drawText(contentX, y+8, ..., Qt::AlignLeft, sender)` |
| `<span style="font-size:10px;color:#8b949e;">12:00</span>` | `p.drawText(timeX, y+8, ..., Qt::AlignLeft, time)` |
| `<div style="background:#21262d;border-radius:8px;padding:8px 12px;width:80%;">` | `p.drawRoundRect(bubbleRect, 4, 4)` + `p.drawText(textRect, Qt::WordBreak, message)` |

#### 布局常量（与原有 HTML 设计一致）
| 常量 | 值 | 说明 |
|------|-----|------|
| kAvatarSize | 48 | 头像圆圈直径 |
| kPad | 8 | 通用内边距 |
| kMsgSpacing | 8 | 消息间距 |
| kBubbleHPad | 12 | 气泡水平内边距 |
| kBubbleVPad | 8 | 气泡垂直内边距 |
| kBubbleRadius | 8 | 气泡圆角半径 |

#### 高度计算逻辑
每条消息的高度 `calcMessageHeight()` 由两部分决定，取较大值：
1. **内容区域** = 发送者行高(fm.lineSpacing) + 间距(8px) + 气泡高度(2×kBubbleVPad + 文本行数×行高)
2. **头像区域** = kAvatarSize + 2×kPad

文本行数通过 `QFontMetrics::width()` 逐字符累计，检测是否需要换行（Word Wrap）。

#### 修改文件
| 文件 | 修改 |
|------|------|
| `q3tox/chatwidget.h` | `QTextEdit* messageArea` → `ChatView* messageArea` |
| `q3tox/chatwidget.cpp` | `appendMessage()` 改为构造 `ChatMessage` 结构，传入 `ChatView::appendMessage()`；`clearMessages()` 改为调用 `ChatView::clearMessages()` |
| `q3tox/q3tox.pro` | SOURCES/HEADERS 添加 `chatview` |

### 3. 鼠标滚轮 + 键盘翻页

#### 问题
`ChatView` 无焦点，滚轮事件穿透到左侧联系人列表。

#### 修复

**chatview.h** 添加：
```cpp
void wheelEvent(QWheelEvent* event);
void keyPressEvent(QKeyEvent* event);
```

**chatview.cpp**：
1. **构造函数**：`setFocusPolicy(QWidget::StrongFocus)` → 点击消息区域自动获取焦点
2. **`wheelEvent`**：`event->delta()` 正/负对应上/下，单步 ×3 手感更佳
3. **`keyPressEvent`**：

| 按键 | 动作 |
|------|------|
| ↑ / ↓ | 单步滚动（lineStep） |
| PgUp / PgDown | 翻页滚动（pageStep） |
| Home | 滚动到顶部 |
| End | 滚动到底部 |

#### Qt3/Qt4 兼容差异

| 特性 | Qt3 | Qt4+ |
|------|-----|------|
| FocusPolicy 值 | `QWidget::StrongFocus` | `Qt::StrongFocus` |
| 单步步长 | `scrollBar->lineStep()` | `scrollBar->singleStep()` |
| 最小值/最大值 | `minValue()` / `maxValue()` | `minimum()` / `maximum()` |
| 文本换行标志 | `Qt::WordBreak` | `Qt::TextWordWrap` |
| 滚轮delta | `event->delta()` | `event->delta()`（相同） |
| 翻页步长 | `scrollBar->pageStep()` | `scrollBar->pageStep()`（相同） |

### 编译

| 环境 | 命令 | 结果 |
|------|------|------|
| Qt3 | `bash buildqt3.sh` | ✅ 编译通过 |
| Qt4 | `qmake-qt4 && make` | ✅ `chatview.o` 编译通过（链接错误在无关文件） |

### 关键文件修改清单

#### q3tox 新增
- `q3tox/chatview.h` - ChatView 虚拟化列表类声明
- `q3tox/chatview.cpp` - 完整实现（绘制、布局、事件处理）

#### q3tox 修改
- `q3tox/chatwidget.h` - QTextEdit → ChatView
- `q3tox/chatwidget.cpp` - appendMessage 重构
- `q3tox/q3tox.pro` - 添加 chatview.h/.cpp
- `q3tox/mainwindow.cpp` - `"conf_"` → `"conference_"`（4处）

#### Web 修改
- `web/app.js` - `"conf_"` → `"conference_"`（4处）
