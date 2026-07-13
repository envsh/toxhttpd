# 虚拟视图聊天消息列表设计文档 / MVC 式 Observer 改造规划

> **文档说明**：本文档分两部分。
> - §1-§3：当前架构描述 + MVC 式 Observer 改造规划（2026-07 新增）
> - §A-§C：原始设计背景、后端 API、懒加载逻辑（历史参考，保留未改）
>
> 原始文档创建于 2026-05，当时计划采用 `QScrollView`/`QListView` 实现虚拟视图。
> 当前实现已演变为 `ChatView`（`QWidget` + `LimeScrollBar`） + `ChatBuffer`/`ChatHistory` 环形缓冲区。

---

## 1. 当前架构

### 1.1 架构总览

```
┌──────────────────────────────────────────────────┐
│               MainWindow                          │
│  ┌────────────────────────────────────────┐      │
│  │           ChatWidget                    │      │
│  │  ┌─────────────────────────────────┐   │      │
│  │  │  ChatView (QWidget+QScrollBar)  │   │      │
│  │  │  • m_blocks[] (MsgBlock)        │   │      │
│  │  │  • m_history → ChatHistory*     │   │      │
│  │  │  • m_totalHeight, m_scrollPos   │   │      │
│  │  │  • paintEvent: 只绘制可见元素    │   │      │
│  │  └─────────────────────────────────┘   │      │
│  └────────────────────────────────────────┘      │
│                                                   │
│  ChatBuffer                                       │
│  ┌─────────────────────────────────────┐          │
│  │  map<(id,type), ChatHistory>       │          │
│  │  ┌──────────────────────────────┐  │          │
│  │  │ ChatHistory                  │  │          │
│  │  │  deque<ChatElement> m_items  │  │          │
│  │  │  oldestRowid, newestRowid    │  │          │
│  │  │  loadedLatest50FromDB/Net    │  │          │
│  │  │  → insert/update 时通知      │  │          │
│  │  │    ChatHistoryObserver       │  │          │
│  │  └──────────────────────────────┘  │          │
│  └─────────────────────────────────────┘          │
└──────────────────────────────────────────────────┘
```

### 1.2 ChatBuffer / ChatHistory

- **ChatBuffer** (`qltox/chatbuffer.h`): 全局单例，所有聊天的环形消息缓冲区
  - key = `std::pair<int, std::string>` (chatId, chatType)
  - `append(id, type, el)` — 新消息追加到队尾
  - `prepend(id, type, els)` — 历史消息插入队首
  - `getOrCreate(id, type)` — 惰性创建 ChatHistory
  - `trimOverflow()` — 超出容量（200）时 pop_front

- **ChatHistory**: 每个聊天的消息队列
  - `m_items`: `std::deque<ChatElement>` (环状缓冲)
  - `ChatView` 通过 `m_history` 指针**只读**访问
  - 只有 `friend class ChatBuffer` 可以修改数据

### 1.3 ChatView 虚拟视图

- **基类**: `QWidget` + 自定义 `LimeScrollBar`（非 Qt 标准 QScrollView/QListView）
- **Block 系统**: `MsgBlock[]`，每块 50 条消息，记录块起始 Y 坐标
  - `rebuildBlocks()` — 全量重建（O(n)）
  - `blockForIndex()` — 二分查找消息所在块
  - `msgAbsY()` — 计算消息绝对 Y 坐标
  - `findByAbsY()` — 根据 Y 坐标反向查找消息
- **渲染**: `paintEvent()` 只绘制 `visibleMessageRange()` 内的元素
- **消息高度缓存**: `ChatElement.cachedWidth` / `height`（TG 风格，只算一次）

### 1.4 数据流

```
Long-Poll Event
    ↓
MainWindow::handleEvents()
    ↓  m_chatbuf.append(id, type, el)
ChatBuffer → ChatHistory
    ↓  如当前聊天处于活动状态
chatWidget->scrollBottomIfNeeded()  (手动通知)
```

### 1.5 当前存在问题

| 场景 | 当前做法 | 问题 |
|------|----------|------|
| append 1 条消息 | `scrollBottomIfNeeded()` 算最后一个元素 | 与 relayout 重复维护 block |
| prepend 50 条 (DB) | `triggerRelayout(-1)` → 全量 relayout | 50 条新消息而已，却遍历所有现有元素 |
| update 1 个元素 (翻译/下载) | `triggerRelayout(i)` → 依然全量 relayout | 只改一个，全部重算 |
| block 维护 | `rebuildBlocks()` 全量重建 | 永远 O(n) |
| 通知机制 | MainWindow 手动 `if (active) call()` | 两步式易遗漏，新 channel 可能忘记通知 |

---

## 2. MVC 式 Observer 改造规划

### 2.1 设计目标

1. **模型主动通知**：ChatHistory 数据变化自动调用 observer 回调，不再依赖 MainWindow 手动通知
2. **增量更新**：单条 append/prepend/update 只做对应 block 增量，不触发全量 `relayout()`
3. **批处理合并**：同帧内连续 N 条 append 合并为一次批量操作
4. **保持 Qt3/Qt4 兼容**：纯虚接口 + 裸指针，无 `Q_OBJECT` 依赖

### 2.2 通知接口设计

```cpp
// chatbuffer.h 新增
class ChatHistoryObserver {
public:
    virtual ~ChatHistoryObserver() = default;
    virtual void onInsertOne(size_t index) = 0;         // 单条 append/insert
    virtual void onInsertRange(size_t start, size_t cnt) = 0;  // 批量 prepend/append
    virtual void onUpdateOne(size_t index) = 0;         // 单条 update
    virtual void onUpdateRange(size_t start, size_t cnt) = 0;  // 批量 update
    virtual void onRemoveOne(size_t index) = 0;         // trimOverflow 弹出
    virtual void onReset() = 0;                          // 全量重置
};
```

### 2.3 ChatHistory 改动

```cpp
class ChatHistory {
    friend class ChatBuffer;
public:
    void setObserver(ChatHistoryObserver* obs) { m_observer = obs; }
    // ... 现有方法不变
private:
    ChatHistoryObserver* m_observer = nullptr;

    // 以下方法触发通知：
    void append(const ChatElement& el);        // → onInsertOne
    void prepend(const std::vector<ChatElement>&); // → onInsertRange
    void update(size_t i, const ChatElement&); // → onUpdateOne
    // trimOverflow → onRemoveOne
};
```

各方法的通知时机：

| 方法 | 通知 | 时机 |
|------|------|------|
| `append(el)` | `onInsertOne(size-1)` | push_back 后 |
| `prepend(els)` | `onInsertRange(0, N)` | insert 到开头后 |
| `update(i, el)` | `onUpdateOne(i)` | m_items[i] 赋值后 |
| `trimOverflow()` | `onRemoveOne(0)` | pop_front 后 |
| `clear()` / 全部替换 | `onReset()` | 清空后 |

### 2.4 BatchProcessor（同帧合并）

```cpp
class BatchProcessor {
    // 同帧内连续 N 个 onInsertOne → 合并为 1 个 onInsertRange
    // 以 QTimer::singleShot(0) 或 paintEvent 为 flush 时机
};
```

- 多个 `onInsertOne` 在同一事件循环内 → 收集 index → flush 时合并为 `onInsertRange`
- 多个 `onUpdateOne` 在同一帧 → 合并为 `onUpdateRange`
- `onReset` 立即执行（优先级最高，清空队列）

### 2.5 ChatView 增量更新实现

ChatView 继承 `ChatHistoryObserver`：

```cpp
class ChatView : public QWidget, public ChatHistoryObserver {
    // ...
    // ChatHistoryObserver
    void onInsertOne(size_t index) override;    // 计算单个高度，追加到 blocks，处理 scroll/pill
    void onInsertRange(size_t start, size_t cnt) override;  // 批量计算高度，插入 blocks
    void onUpdateOne(size_t index) override;    // 重算单个高度，修正 blocks 累计值
    void onUpdateRange(size_t start, size_t cnt) override;  // 批量重算，批量修正
    void onRemoveOne(size_t index) override;    // 从 blocks 移除，调整后续累计值
    void onReset() override;                    // 全量 rebuildBlocks()

private:
    // Block 增量操作
    void _appendToBlocks(int elementHeight);
    void _prependToBlocks(const std::vector<int>& heights);
    void _updateBlockFor(int elementIndex, int oldHeight, int newHeight);
    void _removeFromBlocks(int elementIndex);
    void _updateScrollState();  // 滚动决策：是否到底、是否偏移
};
```

各回调的增量工作范围：

| 回调 | 新增计算量 | Block 操作 | 重绘范围 |
|------|-----------|------------|----------|
| `onInsertOne` | 1 个 `calcHeight` | `_appendToBlocks` | 新增矩形 |
| `onInsertRange` | N 个 `calcHeight` | `_prependToBlocks` | 完整重绘 |
| `onUpdateOne` | 1 个 `calcHeight` | `_updateBlockFor` | 单个消息矩形 |
| `onRemoveOne` | 0 | `_removeFromBlocks` | 完整重绘 |
| `onReset` | 全量 | `rebuildBlocks` | 完整重绘 |

### 2.6 scroll 决策分离

Observer 回调**只做 layout 增量计算**，不直接操作 scrollbar。
`_updateScrollState()` 在每次增量更新后被调用，统一决定：

```cpp
void ChatView::_updateScrollState() {
    // 不改变 scrollPos 的场景：用户在阅读历史
    // 需要滚底的场景：prepend 前已在底部，或 append 时在底部
    // 需要偏移的场景：prepend 时用户不在底部，内容插在可见区域上方
}
```

### 2.7 setBuffer 时注册 observer

```cpp
void ChatView::setBuffer(ChatHistory* hist) {
    if (m_history) m_history->setObserver(nullptr);
    resetCanvas();
    m_history = hist ? hist : &ChatHistory::kEmpty;
    if (m_history) {
        m_history->setObserver(this);
        m_gifFrameUpdated.assign(m_history->size(), 0);
    }
    relayout();
    scrollToBottom();
    updateFull();
}
```

### 2.8 MainWindow 接入变化

| 当前 | 改造后 |
|------|--------|
| `m_chatbuf.append(...); if (active) chatWidget->scrollBottomIfNeeded();` | `m_chatbuf.append(...)` ← observer 自动通知 |
| `m_chatbuf.prepend(...); chatWidget->triggerRelayout(-1);` | `m_chatbuf.prepend(...)` ← observer 自动通知 |

单体更新（翻译、下载等），MainWindow 改为通过 ChatHistory 方法更新：

```cpp
// 当前：
(*hist)[i] = el;
chatWidget->triggerRelayout(i);

// 改造后：
hist->update(i, el);  // → 自动触发 onUpdateOne
```

### 2.9 Phase 实施计划

| Phase | 内容 | 涉及文件 |
|-------|------|----------|
| **P1** | 定义 `ChatHistoryObserver` 接口，ChatHistory 持 observer 指针 + setObserver | `chatbuffer.h` |
| **P2** | ChatHistory 方法触发通知（append/prepend/update/trimOverflow → 对应回调） | `chatbuffer.cpp` |
| **P3** | Block 增量操作（_appendToBlocks/_prependToBlocks/_updateBlockFor/_removeFromBlocks） | `chatview.cpp` |
| **P4** | ChatView 实现 ChatHistoryObserver（6 个回调 + _updateScrollState） | `chatview.h`, `chatview.cpp` |
| **P5** | BatchProcessor 合并同帧密集通知 | 新增 `batchprocessor.h/cpp` 或内联在 ChatView |
| **P6** | MainWindow 接入：移除手动 triggerRelayout/scrollBottomIfNeeded；单体更新改为 hist.update() | `mainwindow.cpp` |
| **P7** | 清理：移除 scrollBottomIfNeeded()、triggerRelayout(int) 或简化为 observer-only | `chatview.cpp`, `chatwidget.cpp` |

**构建要求**：
- 每 Phase 完成执行 `bash buildqt3.sh` + `bash buildqt4.sh` 验证
- 两个版本并行兼容

### 2.10 与行业方案对比

| 方案 | 通知机制 | 增量更新 | 批处理 | 适用性 |
|------|----------|----------|--------|--------|
| Qt QAbstractItemModel | dataChanged/rowsInserted/rowsRemoved | 依赖 View 实现 | 需手动 begin/end | ❌ 太重，泛型到 QVariant |
| Telegram Web (tweb) | Event bus: history_update, message_sent | 单条/批量 | BatchProcessor | ✅ Chat UI 实际标准 |
| **本方案** | ChatHistoryObserver 纯虚接口 | 6 种增量回调 | BatchProcessor (P5) | ✅ 轻量，Qt3/Qt4 兼容 |

---

## 附录

---

## A. 历史技术分析：Qt3/Qt4 原始方案

> 本附录为 2026-05 原始设计文档的 Qt3/Qt4 技术分析部分，保留作为参考。
> 当前实现已不使用 `QScrollView`/`QListView`，而是自定义 `ChatView(QWidget+QScrollBar)`。

### A.1 Qt3 可用控件

通过检查 `/opt/qt338sh/include/` 头文件，发现：

| 控件 | 说明 |
|------|------|
| **`QScrollView`** | 虚拟视图基类<br>- `drawContents(QPainter*, int cx, int cy, int cw, int ch)` - 只绘制可见区域<br>- `resizeContents(w, h)` - 设置虚拟大小<br>- `verticalScrollBar()` - 获取滚动条<br>- `contentsHeight()`、`setContentsPos()` - 控制滚动 |
| **`QListView`** | 继承自 `QScrollView`，添加了列表项管理<br>- `QListViewItem` 管理项<br>- 但富文本支持有限 |
| **`QTextEdit`** | 现有控件，不适合虚拟视图 |

**结论**：Qt3 可以用 `QScrollView` 子类实现虚拟视图。

### A.2 Qt4 可用控件

| 控件 | 说明 |
|------|------|
| **`QListView` + `QAbstractListModel`** | 真正的 Model/View 架构<br>- Model 只提供数据，View 只渲染可见项<br>- 内置虚拟渲染 |
| **`QTextEdit`** | 现有控件，不适合虚拟视图 |

**结论**：Qt4 可以用标准 Model/View 实现。

---

## B. 后端 API 设计

> 以下为 2026-05 设计的后端 API 方案，仍可作为后续实现参考。

### B.1 数据库表设计（SQLite）

```sql
-- 好友消息表
CREATE TABLE IF NOT EXISTS friend_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    friend_id INTEGER NOT NULL,
    sender_id INTEGER NOT NULL,  -- 0=自己，其他=好友ID
    message TEXT NOT NULL,
    message_type TEXT DEFAULT 'normal',
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 会议消息表
CREATE TABLE IF NOT EXISTS conference_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conference_number INTEGER NOT NULL,
    peer_number INTEGER NOT NULL,
    sender_name TEXT,
    message TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_friend ON friend_messages(friend_id, id DESC);
CREATE INDEX idx_conf ON conference_messages(conference_number, id DESC);
```

### B.2 API 接口

#### GET /api/friend_messages

**请求参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `friend_id` | int | 是 | 好友ID |
| `before_id` | int | 否 | 获取此ID之前的消息（不含此ID），用于向上滚动加载历史 |
| `after_id` | int | 否 | 获取此ID之后的消息（不含此ID），用于下拉刷新 |
| `limit` | int | 否 | 返回数量，默认20，最大100 |

**响应格式**：

```json
{
  "messages": [
    {
      "id": 95,
      "friend_id": 1,
      "message": "hello",
      "timestamp": "2026-05-01T21:30:00Z",
      "is_self": false
    }
  ],
  "has_more": true
}
```

#### GET /api/conference_messages

**请求参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `conference_id` | int | 是 | 会议ID |
| `before_id` | int | 否 | 获取此ID之前的消息 |
| `after_id` | int | 否 | 获取此ID之后的消息 |
| `limit` | int | 否 | 返回数量，默认20 |

**响应格式**：同好友消息，包含 `conference_number`、`peer_number`、`sender_name`。

### B.3 分页逻辑示例

假设数据库中有消息 ID：1, 2, 3, ..., 100（ID递增=时间顺序）

当前用户看到了 ID：96, 97, 98, 99, 100（最新的5条）

**场景1：向上滚动，加载更早的消息**
- 请求：`GET /api/friend_messages?friend_id=1&before_id=96&limit=20`
- SQL：`SELECT * FROM friend_messages WHERE friend_id=1 AND id < 96 ORDER BY id DESC LIMIT 20`
- 返回：ID 76-95 的消息（插入到现有消息前面）

**场景2：下拉刷新，获取新消息**
- 请求：`GET /api/friend_messages?friend_id=1&after_id=100&limit=50`
- SQL：`SELECT * FROM friend_messages WHERE friend_id=1 AND id > 100 ORDER BY id ASC LIMIT 50`
- 返回：ID 101+ 的消息（追加到现有消息后面）

---

## C. 历史懒加载逻辑参考

> 以下逻辑在后续 ChatView 实现中可复用。

### C.1 监听滚动到顶部

**Qt3**（在 `MessageView` 中已实现）：
```cpp
void MessageView::contentsMousePressEvent(QMouseEvent* e) {
    if (verticalScrollBar()->value() == verticalScrollBar()->minValue()) {
        emit requestMoreHistory();
    }
    QScrollView::contentsMousePressEvent(e);
}
```

**Qt4**（在 `MessageView` 中已实现）：
```cpp
void MessageView::onScrollChanged(int value) {
    if (value == verticalScrollBar()->minimum()) {
        emit requestMoreHistory();
    }
}
```

### C.2 客户端缓存参考

为了减少 API 调用，可以在客户端内存中缓存消息：
- 切换联系人时，先检查内存缓存
- 缓存未命中时，才调用 API 加载
- 缓存结构：`QMap<QString, QList<MessageItem>>`，key = "friend_X" 或 "conference_X"

---

*文档创建时间：2026-05-01*
*MVC Observer 规划新增：2026-07-13*
