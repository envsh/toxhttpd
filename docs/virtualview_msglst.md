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

### 1.5 现存问题（已修复项 √，未修复项 ×）

| 场景 | 当前做法 | 问题 | 状态 |
|------|----------|------|------|
| append 1 条消息 | `scrollBottomIfNeeded()` 头部已检查 `cachedWidth` 跳过 | observer 已处理则提前 return | √ |
| prepend 50 条 (DB) | `onInsertRange` 增量计算 + `relayout()` | cachedWidth guard 跳过宽度不变的元素 | √ |
| update 1 个元素 | `updateElement(i)` → O(1) calc + block 更新 | 不再调全量 relayout | √ |
| block 维护 | `rebuildBlocks()` 全量重建 | 永远 O(n) | × 可优化 |
| 非当前 chat 消息 | `if (m_observer)` 条件跳过通知 | observer 为空时不崩，消息存缓冲区但无 height | × setBuffer 时 relayout 补算 |
| 手动通知冗余 | `scrollBottomIfNeeded()` 在 observer 后仍被调用 | 靠缓存 guard 跳过，语义上仍多余 | × |

---

## 2. MVC 式 Observer 改造规划

### 2.1 设计目标

1. **模型主动通知**：ChatHistory 数据变化自动调用 observer 回调，不再依赖 MainWindow 手动通知
2. **增量更新**：单条 append/prepend/update 只做对应 block 增量，不触发全量 `relayout()`
3. **批处理合并**：同帧内连续 N 条 append 合并为一次批量操作
4. **保持 Qt3/Qt4 兼容**：纯虚接口 + 裸指针，无 `Q_OBJECT` 依赖

### 2.2 通知接口设计

```cpp
// chatview.h — 定义在 ChatView 类之前
class ChatHistoryObserver {
public:
    virtual ~ChatHistoryObserver() = default;
    virtual void onInsertOne(size_t index) = 0;         // 单条 append/insert
    virtual void onInsertRange(size_t start, size_t cnt) = 0;  // 批量 prepend/append
    virtual void onUpdateOne(size_t index) = 0;         // 单条 update
    virtual void onUpdateRange(size_t start, size_t cnt) = 0;  // 批量 update
    virtual void onRemoveOne(size_t index) = 0;         // trimOverflow 弹出
    virtual void onRemoveRange(size_t start, size_t cnt) = 0;  // 批量 remove
};
```

> **设计说明**：`onReset()` 最初存在但被移除，因为全量重置由 `setBuffer()` 处理（resetCanvas + relayout），observer 不应承担此责任。

### 2.3 ChatHistory 改动

```cpp
class ChatHistory {
    friend class ChatBuffer;
public:
    void setObserver(ChatHistoryObserver* obs) { m_observer = obs; }
    // ... 现有方法不变
private:
    ChatHistoryObserver* m_observer = nullptr;

    // 以下方法触发通知（m_observer 为空时跳过）：
    void append(const ChatElement& el);        // → onInsertOne
    void prepend(const std::vector<ChatElement>&); // → onInsertRange
    // trimOverflow → onRemoveRange
};
```

**通知时机**（observer 非空时才调用）：

| 方法 | 通知 | 时机 |
|------|------|------|
| `append(el)` | `onInsertOne(size-1)` | push_back + trimOverflow 后 |
| `prepend(els)` | `onInsertRange(0, N)` | insert 到开头后 |
| `trimOverflow()` | `onRemoveRange(0, excess)` | erase 后 |

> **与初始规划的区别**：`update(i, el)` 方法未实现，单体更新改为 `ChatView::updateElement(i)` 直接操作。`trimOverflow` 发货通知改为 `onRemoveRange` 而非 `onRemoveOne`（batch erase）。

### 2.4 BatchProcessor — deferred `_updateScrollState`（已实现）

实际实现采用更轻量的方案：**不改变 observer 的即时 O(1) 计算**（calcHeight + block 增量操作必须即时），**只延迟 `_updateScrollState()` 的调用**。

```cpp
void ChatView::scheduleScrollUpdate() {
    if (!m_scrollUpdatePending) {
        m_scrollUpdatePending = true;
        QTimer::singleShot(0, this, SLOT(flushScrollUpdate()));
    }
}
void ChatView::flushScrollUpdate() {
    m_scrollUpdatePending = false;
    if (!m_history) { return; }
    _updateScrollState();
}
```

- 同帧内 N 个 `onInsertOne` → 只触发一次 `flushScrollUpdate`（QTimer::singleShot + bool flag）
- setBuffer 时 `m_scrollUpdatePending = false` 取消 pending（防止切换 chat 后过期 timer 脏读）

### 2.5 ChatView 增量更新实现

ChatView 继承 `ChatHistoryObserver`，另新增 `updateElement(int)` 用于 MainWindow 直接触发的单体更新：

```cpp
class ChatView : public QWidget, public ChatHistoryObserver {
    // ...
public:
    void updateElement(int msgIndex);              // O(1) 单体更新，替代 triggerRelayout(msgIndex)

    // ChatHistoryObserver
    void onInsertOne(size_t index) override;       // 计算单个高度，追加到 blocks，_updateScrollState
    void onInsertRange(size_t start, size_t cnt) override;  // 批量计算高度，prepend 到 blocks
    void onUpdateOne(size_t index) override;       // 重算单个高度，修正 blocks 累计值
    void onUpdateRange(size_t start, size_t cnt) override;  // 多条更新（当前简化为逐条处理）
    void onRemoveOne(size_t index) override;       // 单体移除（调用 _removeFromBlocks）
    void onRemoveRange(size_t start, size_t cnt) override;  // 批量移除

private:
    // Block 增量操作
    void _appendToBlocks(int elementHeight);
    void _prependToBlocks(int count);              // 传入 prepend 元素个数
    void _updateBlockFor(int idx, int oldHeight);  // 更新块累计值
    void _removeFromBlocks(int idx);               // 移除后重建块（当前全量 rebuild）
    void _updateScrollState();  // 滚动决策：是否到底、是否偏移
};
```

各回调的增量工作范围：

| 回调/方法 | 新增计算量 | Block 操作 | 重绘范围 |
|-----------|-----------|------------|----------|
| `onInsertOne` | 1 个 `calcHeight` | `_appendToBlocks` | 全 widget（_updateScrollState 支配） |
| `onInsertRange` | N 个 `calcHeight` | `_prependToBlocks` | 全 widget |
| `onUpdateOne` | 1 个 `calcHeight` | `_updateBlockFor` | 单个消息矩形 |
| `onUpdateRange` | 全量 | 全量 rebuild（当前简化实现） | 全 widget |
| `onRemoveOne` | 0 | `_removeFromBlocks`（全量 rebuildBlocks） | 全 widget |
| `onRemoveRange` | 0 | `_removeFromBlocks` | 全 widget |
| `updateElement` | 1 个 `calcHeight` | `_updateBlockFor` | 全 widget |

### 2.6 scroll 决策分离

Observer 回调**只做 layout 增量计算**，然后调 `_updateScrollState()` 统一处理 scrollbar + auto-scroll + repaint：

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

| 场景 | 当前 | 状态 |
|------|------|------|
| append 消息 | `m_chatbuf.append(...);` → observer 自动通知 + `scrollBottomIfNeeded()` 被 cache guard 跳过 | ⚠️ 语义上仍多余，靠 guard 不造成浪费 |
| prepend DB 消息 | `m_chatbuf.prepend(...);` → observer `onInsertRange` 增量计算 + `chatWidget->relayout()` | ✅ relayout 无 invalidation，cachedWidth guard 跳过旧元素 |
| 单体更新（翻译、下载、重试） | 直接修改 `ChatElement` 字段后调 `chatWidget->updateElement(i)` | ✅ 替换了 `triggerRelayout(i)` |
| 非当前 chat 消息到达 | `if (m_observer)` 条件跳过通知 | ✅ 不崩，消息存缓冲区 |

单体更新不再通过 `hist->update(i, el)` 模式，而是直接修改 ChatElement 字段 + `updateElement(i)`。

### 2.9 Phase 实施计划

| Phase | 内容 | 涉及文件 | 状态 |
|-------|------|----------|------|
| **P1** | 定义 `ChatHistoryObserver` 接口，ChatHistory 持 observer 指针 + setObserver | `chatview.h`, `chatbuffer.h` | ✅ 完成 |
| **P2** | ChatHistory 方法触发通知（append/prepend/trimOverflow → 对应回调） | `chatbuffer.cpp` | ✅ 完成（`if (m_observer)` 条件判断） |
| **P3** | Block 增量操作（_appendToBlocks/_prependToBlocks/_updateBlockFor/_removeFromBlocks） | `chatview.cpp` | ✅ 完成 |
| **P4** | ChatView 实现 ChatHistoryObserver（6 回调 + _updateScrollState） | `chatview.h`, `chatview.cpp` | ✅ 完成（含 updateElement） |
| **P5** | BatchProcessor — deferred _updateScrollState | `chatview.h`, `chatview.cpp` | ✅ 完成（QTimer::singleShot 延迟 + flag 防重） |
| **P6** | MainWindow 接入：单体更新改为 updateElement(msgIndex)，移除冗余 scrollBottomIfNeeded 调用 | `mainwindow.cpp`, `chatwidget.cpp` | ✅ 完成 |
| **P7** | 清理：删除 triggerRelayout()，scrollBottomIfNeeded() 改为 ChatWidget private | `chatview.cpp/h`, `chatwidget.cpp/h` | ✅ 完成 |

#### 与初始规划的偏差

| 规划项 | 原计划 | 实际 |
|--------|--------|------|
| `onReset()` | 接口含 onReset | 已删除（setBuffer 是重置入口） |
| `NullObserver` 空对象 | 全局 NullObserver 实例 | `if (m_observer)` 条件跳过，无空对象 |
| `hist->update(i, el)` | 单体更新通过 ChatHistory 方法 | `updateElement(i)` 直接在 ChatView 上操作 |
| `scrollBottomIfNeeded` | 彻底移除 | 保留 + 缓存 guard 跳过 |
| `trimOverflow` 通知 | `onRemoveOne` 逐个通知 | `onRemoveRange` 批量通知 |
| `_prependToBlocks` 参数 | `const std::vector<int>& heights` | `int count`（仅传个数） |
| `_updateBlockFor` 参数 | `(index, oldH, newH)` | `(idx, oldHeight)`（从 element 读 newH） |

**构建要求**：
- 每 Phase 完成执行 `bash buildqt3.sh` + `bash buildqt4.sh` 验证
- 两个版本并行兼容

### 2.10 与行业方案对比

| 方案 | 通知机制 | 增量更新 | 批处理 | 适用性 |
|------|----------|----------|--------|--------|
| Qt QAbstractItemModel | dataChanged/rowsInserted/rowsRemoved | 依赖 View 实现 | 需手动 begin/end | ❌ 太重，泛型到 QVariant |
| Telegram Web (tweb) | Event bus: history_update, message_sent | 单条/批量 | BatchProcessor | ✅ Chat UI 实际标准 |
| **本方案** | ChatHistoryObserver 纯虚接口 | 6 增量回调 + updateElement | deferred _updateScrollState (P5) | ✅ 轻量，Qt3/Qt4 兼容 |

---

## 3. 本次 Session 改动总结（2026-07-13）

### 3.1 改动列表

| # | 文件 | 改动 |
|---|------|------|
| 1 | `chatview.h` | 新增 `updateElement(int msgIndex)` 声明 |
| 2 | `chatview.cpp` | `scrollBottomIfNeeded()` 头部加 `cachedWidth` 缓存跳过 |
| 3 | `chatview.cpp` | 新增 `updateElement()` 实现（O(1) calc + block + scrollbar） |
| 4 | `chatview.cpp` | `setBuffer()` 加 `relayout()` 调用，重排 `scrollToBottom()` 顺序 |
| 5 | `chatwidget.h` | 新增 `updateElement(int)` inline passthrough |
| 6 | `chatbuffer.cpp` | 3 处 `assertf` → `if (m_observer)`（append/prepend/trimOverflow） |
| 7 | `mainwindow.cpp` | 5 处 `triggerRelayout(msgIndex)` → `updateElement(msgIndex)` |
| 8 | `chatwidget.cpp` | 4 处 `messageArea->triggerRelayout(msgIndex)` → `messageArea->updateElement(msgIndex)` |

### 3.2 效果

| 指标 | 改动前 | 改动后 |
|------|--------|--------|
| 每条消息 append 的 layout 成本 | 2 次 calcHeight（observer + scrollBottomIfNeeded） | 1 次 calcHeight（observer，scrollBottomIfNeeded 被 guard 跳过） |
| 单体更新（翻译/下载/重试） | O(n) relayout | O(1) calcHeight + block update |
| 非当前 chat 消息到达 | 崩溃（assertf） | 写入缓冲区，切回时 relayout 补算 |
| `setBuffer` 后旧元素高度 | 不重新计算（可能为 0） | relayout 重新计算全部元素 |

### 3.3 遗留问题

（无 — 所有 Phase 已完成）

---

### §4 第二次 Session 改动（2026-07-13 Session 2）

#### 4.1 改动列表

| # | 文件 | 改动 |
|---|------|------|
| 1 | `mainwindow.cpp` | 删 10 处冗余 `scrollBottomIfNeeded()` 调用，observer 单向通知 |
| 2 | `chatview.cpp` | `onUpdateRange` 改为逐元素 `calcHeight`+`_updateBlockFor`+`QWidget::update()`，消除 `relayout()` |
| 3 | `chatview.h` | `relayout()` 改为 `public`；删 `triggerRelayout()` 声明 |
| 4 | `chatview.cpp` | 删 `triggerRelayout()` 实现 |
| 5 | `chatwidget.h` | 加 `void relayout()` passthrough；`scrollBottomIfNeeded()` 改为 `private`；删 `triggerRelayout()` 声明 |
| 6 | `chatwidget.cpp` | 删 `triggerRelayout()` 实现 |
| 7 | `mainwindow.cpp` | DB prepend 后 `triggerRelayout(-1)` → `relayout()`（无 invalidation） |

#### 4.2 效果增量

| 指标 | 改动前 | 改动后 |
|------|--------|--------|
| mainwindow 手动通知 | 10 处 `scrollBottomIfNeeded` 调用 | 全部移除，observer 单向处理 |
| `onUpdateRange` | O(n) relayout | O(m) 逐元素（m ≤ n，仅差异项更新 block） |
| DB prepend | invalidation + relayout | `relayout()` only（cachedWidth guard 跳过宽度不变的元素） |
| `triggerRelayout(int)` | 有声明 + 实现 + 调用者 | 全部删除 |

#### 4.3 Phase 状态总结

| Phase | 内容 | 状态 |
|-------|------|------|
| P1-P4 | Observer 接口、通知、Block 增量、ChatView 回调 | ✅ |
| P5 | BatchProcessor（deferred _updateScrollState） | ✅ |
| P6 | MainWindow 接入 | ✅ |
| P7 | 清理 | ✅ |

---

### §5 第三次 Session 改动（2026-07-13 Session 3）

#### 5.1 改动列表

| # | 文件 | 改动 |
|---|------|------|
| 1 | `chatview.h` | 加 `m_scrollUpdatePending`、`scheduleScrollUpdate()`、`flushScrollUpdate()` slot |
| 2 | `chatview.cpp` | 实现 `scheduleScrollUpdate()`/`flushScrollUpdate()` |
| 3 | `chatview.cpp` | `onInsertOne`/`onUpdateRange` 中 `_updateScrollState` → `scheduleScrollUpdate` |
| 4 | `chatview.cpp` | `setBuffer` 加 `m_scrollUpdatePending = false` 取消 pending |
| 5 | `chatview.cpp` | `_removeFromBlocks` 合并 blocks + totalHeight 一步 O(n)，取代 `rebuildBlocks()` |
| 6 | `chatview.cpp` | `onRemoveOne` 删多余 `m_totalHeight` 循环（O(n) → O(n) 但消除重复迭代） |
| 7 | `chatview.cpp` | `onRemoveRange` 直接处理而非转发 `onRemoveOne(0)`（正确处理 start+cnt 参数） |

#### 5.2 效果增量

| 指标 | 改动前 | 改动后 |
|------|--------|--------|
| burst 消息（同帧 10 条，底部） | 10× `_updateScrollState` → 10× `updateFull` → 10× `rebuildBlocks` O(10n) | 1× `flushScrollUpdate` → 1× `_updateScrollState` O(n) |
| `onRemoveOne` | `rebuildBlocks` O(n) + `m_totalHeight` 循环 O(n) = O(2n) | `_removeFromBlocks` 一次 O(n) |
| `onRemoveRange` | 忽略参数，调用 `onRemoveOne(0)` | 正确处理 start+cnt |

#### 5.3 最终 Phase 状态

| Phase | 内容 | 状态 |
|-------|------|------|
| **P1** | ChatHistoryObserver 接口 | ✅ |
| **P2** | ChatHistory 触发通知 | ✅ |
| **P3** | Block 增量操作 | ✅ |
| **P4** | ChatView 实现 observer | ✅ |
| **P5** | BatchProcessor（deferred _updateScrollState） | ✅ |
| **P6** | MainWindow 接入 | ✅ |
| **P7** | 清理 | ✅ |

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
