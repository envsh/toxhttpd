# 虚拟视图聊天消息列表设计文档

## 1. 目标

为 q3tox 聊天消息列表实现虚拟视图，达到以下目标：

1. **只渲染可见区域**：当消息量很大（5000+）时，保持流畅滚动
2. **支持懒加载**：滚动到顶部时，从后端加载更多历史消息
3. **内存效率**：只存储数据，视图按需绘制
4. **Qt3/Qt4 兼容**：两个版本都能工作，优先保证 Qt3 实现

---

## 2. 当前问题分析

### 2.1 现有实现

| 项目 | 说明 |
|------|------|
| **消息控件** | `QTextEdit* messageArea` |
| **添加消息** | `messageArea->append(html)` → 所有消息在一个 HTML 文档中 |
| **清空消息** | `messageArea->clear()` → 切换联系人时清空 |
| **问题** | `QTextEdit` 将内容存储在单个文档中，消息多了之后：<br>- 内存占用大（几千条 HTML 内容）<br>- 滚动和渲染变慢<br>- 无法部分释放早期消息 |

### 2.2 后端现状

| 项目 | 说明 |
|------|------|
| **消息存储** | ❌ 只有内存事件队列（最多256条），服务器重启后丢失 |
| **历史消息 API** | ❌ 没有 `/api/friend_messages` 或 `/api/conference_messages` 的 GET 接口 |
| **数据库** | ❌ 没有 SQLite 或文件存储 |

---

## 3. Qt3/Qt4 技术分析

### 3.1 Qt3 可用控件

通过检查 `/opt/qt338sh/include/` 头文件，发现：

| 控件 | 说明 |
|------|------|
| **`QScrollView`** | ✅ 虚拟视图基类！<br>- `drawContents(QPainter*, int cx, int cy, int cw, int ch)` - 只绘制可见区域<br>- `resizeContents(w, h)` - 设置虚拟大小<br>- `verticalScrollBar()` - 获取滚动条<br>- `contentsHeight()`、`setContentsPos()` - 控制滚动 |
| **`QListView`** | ✅ 继承自 `QScrollView`，添加了列表项管理<br>- `QListViewItem` 管理项<br>- 但富文本支持有限 |
| **`QTextEdit`** | 现有控件，不适合虚拟视图 |

**结论**：Qt3 可以用 `QScrollView` 子类实现虚拟视图！

### 3.2 Qt4 可用控件

| 控件 | 说明 |
|------|------|
| **`QListView` + `QAbstractListModel`** | ✅ 真正的 Model/View 架构<br>- Model 只提供数据，View 只渲染可见项<br>- 内置虚拟渲染 |
| **`QTextEdit`** | 现有控件，不适合虚拟视图 |

**结论**：Qt4 可以用标准 Model/View 实现。

---

## 4. 整体架构设计

### 4.1 分层架构

```
┌─────────────────────────────────────────────┐
│            ChatWidget (聊天窗口)              │
│  ┌─────────────────────────────────────┐  │
│  │     MessageView (虚拟视图)           │  │
│  │  Qt3: QScrollView 子类          │  │
│  │  Qt4: QListView + Model         │  │
│  └─────────────────────────────────────┘  │
│  ┌─────────────────────────────────────┐  │
│  │     MessageItem 数据列表             │  │
│  │  allMessages: QList<MessageItem> │  │
│  └─────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

### 4.2 数据流

```
后端 API (历史消息)
    ↓
MainWindow::handleEvents() / loadMoreMessages()
    ↓
ChatWidget::appendMessage() / insertMessage()
    ↓
MessageView::appendMessage() / insertMessage()
    ↓
MessageView::drawContents() (Qt3) 或 Model::data() (Qt4)
    ↓
屏幕渲染
```

---

## 5. 数据结构设计

### 5.1 MessageItem 结构

```cpp
// chatwidget.h 或 messageview.h 中定义
struct MessageItem {
    uint64_t id;           // 消息唯一ID（来自后端）
    QString html;           // 富文本格式（用于显示）
    QString plainText;      // 纯文本（Qt3 备用）
    QString type;           // "self" / "other"
    QString sender;         // 发送者名称
    uint64_t timestamp;     // 时间戳
    int height;             // 预计高度（用于虚拟布局，如 60px）
};
```

### 5.2 数据管理

```cpp
// 在 ChatWidget 或 MessageView 中
QList<MessageItem> allMessages;  // 所有消息（按时间顺序）
int maxDisplayMessages;          // 最大显示数量（如 200，可选）
uint64_t oldestDisplayedId;       // 当前显示的最早消息ID
uint64_t newestDisplayedId;       // 当前显示的最新消息ID
```

---

## 6. Qt3 实现方案（QScrollView 子类）

### 6.1 创建 MessageView 类

**文件**：`messageview.h`、`messageview.cpp`

#### `messageview.h`

```cpp
#ifdef QT3_BUILD
#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <qscrollview.h>
#include <qpainter.h>
#include "compat34.h"

struct MessageItem;  // 前置声明

class MessageView : public QScrollView {
    Q_OBJECT
public:
    MessageView(QWidget* parent = 0);
    
    // 数据操作
    void appendMessage(const MessageItem& msg);
    void insertMessage(int index, const MessageItem& msg);  // 插入历史消息
    void clearMessages();
    
    // 虚拟大小
    void updateContentsSize();
    
signals:
    void requestMoreHistory();  // 滚动到顶部时触发
    
protected:
    void drawContents(QPainter* p, int cx, int cy, int cw, int ch);
    void contentsMousePressEvent(QMouseEvent* e);
    void contentsWheelEvent(QWheelEvent* e);
    
private:
    QList<MessageItem> items;
    int itemHeight;  // 每条消息的固定高度（如 60px）
    
    // 辅助函数
    int calcTotalHeight() const;
    int getVisibleStart() const;
    int getVisibleEnd() const;
};

#endif  // MESSAGEVIEW_H
#endif  // QT3_BUILD
```

#### `messageview.cpp`

```cpp
#ifdef QT3_BUILD
#include "messageview.h"
#include <qpen.h>
#include <qcolor.h>
#include <qrect.h>

MessageView::MessageView(QWidget* parent) : QScrollView(parent, "messageView"), 
    itemHeight(60) {
    setVScrollBarMode(AlwaysOn);
    setHScrollBarMode(AlwaysOff);
    viewport()->setPaletteBackgroundColor(QColor("#0d1117"));  // 背景色
}

void MessageView::appendMessage(const MessageItem& msg) {
    items.append(msg);
    updateContentsSize();
    
    // 滚动到底部（如果是新消息）
    ensureVisible(0, contentsHeight());
}

void MessageView::insertMessage(int index, const MessageItem& msg) {
    items.insert(index, msg);
    updateContentsSize();
    // 不自动滚动，因为是在前面插入历史消息
}

void MessageView::clearMessages() {
    items.clear();
    updateContentsSize();
}

void MessageView::updateContentsSize() {
    int totalH = items.count() * itemHeight;
    resizeContents(viewport()->width(), totalH);
}

void MessageView::drawContents(QPainter* p, int cx, int cy, int cw, int ch) {
    // cx, cy, cw, ch 是可见区域
    // 只绘制这个区域内的消息！
    
    if (items.isEmpty()) return;
    
    int firstVisible = cy / itemHeight;
    int lastVisible = (cy + ch) / itemHeight;
    if (lastVisible >= items.count()) lastVisible = items.count() - 1;
    
    for (int i = firstVisible; i <= lastVisible; ++i) {
        if (i < 0 || i >= items.count()) continue;
        
        const MessageItem& msg = items[i];
        int y = i * itemHeight;
        
        // 绘制背景
        QRect rect(0, y, viewport()->width(), itemHeight);
        if (msg.type == "self") {
            p->fillRect(rect, QColor("#00d4aa"));
        } else {
            p->fillRect(rect, QColor("#21262d"));
        }
        
        // 绘制文本
        p->setPen(QColor("#c9d1d9"));
        QRect textRect = rect.adjust(8, 8, -8, -8);  // 边距
        p->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, msg.plainText);
        
        // 如果有 sender，绘制 sender
        if (!msg.sender.isEmpty()) {
            p->setPen(QColor("#6e7681"));
            p->setFontSize(10);
            p->drawText(rect, Qt::AlignLeft | Qt::AlignBottom, msg.sender);
        }
    }
}

void MessageView::contentsMousePressEvent(QMouseEvent* e) {
    // 检测是否滚动到顶部
    if (verticalScrollBar()->value() == verticalScrollBar()->minValue()) {
        emit requestMoreHistory();
    }
    QScrollView::contentsMousePressEvent(e);
}

void MessageView::contentsWheelEvent(QWheelEvent* e) {
    QScrollView::contentsWheelEvent(e);
    
    // 检查是否滚动到顶部
    if (verticalScrollBar()->value() == verticalScrollBar()->minValue()) {
        emit requestMoreHistory();
    }
}

int MessageView::calcTotalHeight() const {
    return items.count() * itemHeight;
}

int MessageView::getVisibleStart() const {
    return verticalScrollBar()->value() / itemHeight;
}

int MessageView::getVisibleEnd() const {
    return (verticalScrollBar()->value() + visibleHeight()) / itemHeight;
}
#endif  // QT3_BUILD
```

---

## 7. Qt4 实现方案（Model/View）

### 7.1 创建 MessageModel 类

#### `messageview.h`

```cpp
#ifndef QT3_BUILD
#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <QListView>
#include <QAbstractListModel>
#include <QVariant>
#include <QModelIndex>
#include "compat34.h"

struct MessageItem;

// Model
class MessageModel : public QAbstractListModel {
    Q_OBJECT
public:
    MessageModel(QObject* parent = 0);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    
    void appendMessage(const MessageItem& msg);
    void insertMessage(int index, const MessageItem& msg);
    void clearMessages();
    
private:
    QList<MessageItem> items;
};

// View
class MessageView : public QListView {
    Q_OBJECT
public:
    MessageView(QWidget* parent = 0);
    
    void appendMessage(const MessageItem& msg);
    void insertMessage(int index, const MessageItem& msg);
    void clearMessages();
    
signals:
    void requestMoreHistory();
    
protected slots:
    void onScrollChanged(int value);
    
private:
    MessageModel* model;
};

#endif  // MESSAGEVIEW_H
#endif  // QT3_BUILD
```

#### `messageview.cpp`

```cpp
#ifndef QT3_BUILD
#include "messageview.h"
#include <QScrollBar>

MessageModel::MessageModel(QObject* parent) : QAbstractListModel(parent) {
}

int MessageModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return items.count();
}

QVariant MessageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();
    if (index.row() >= items.count()) return QVariant();
    
    const MessageItem& msg = items[index.row()];
    
    if (role == Qt::DisplayRole) {
        return msg.plainText;
    } else if (role == Qt::UserRole) {
        return QVariant::fromValue(msg);  // 传递整个结构
    }
    return QVariant();
}

void MessageModel::appendMessage(const MessageItem& msg) {
    beginInsertRows(QModelIndex(), items.count(), items.count());
    items.append(msg);
    endInsertRows();
}

void MessageModel::insertMessage(int index, const MessageItem& msg) {
    beginInsertRows(QModelIndex(), index, index);
    items.insert(index, msg);
    endInsertRows();
}

void MessageModel::clearMessages() {
    beginResetModel();
    items.clear();
    endResetModel();
}

MessageView::MessageView(QWidget* parent) : QListView(parent) {
    model = new MessageModel(this);
    setModel(model);
    
    // 监听滚动
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), 
            this, SLOT(onScrollChanged(int)));
}

void MessageView::appendMessage(const MessageItem& msg) {
    model->appendMessage(msg);
    scrollToBottom();
}

void MessageView::insertMessage(int index, const MessageItem& msg) {
    model->insertMessage(index, msg);
}

void MessageView::clearMessages() {
    model->clearMessages();
}

void MessageView::onScrollChanged(int value) {
    if (value == verticalScrollBar()->minimum()) {
        emit requestMoreHistory();
    }
}
#endif  // QT3_BUILD
```

---

## 8. 后端 API 设计

### 8.1 数据库表设计（SQLite）

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

### 8.2 API 接口

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
    // ... 按时间倒序（id从大到小）
  ],
  "has_more": true  // 是否还有更早的消息
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

### 8.3 分页逻辑示例

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

## 9. 懒加载逻辑

### 9.1 前端（q3tox）实现

#### 监听滚动到顶部

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

#### ChatWidget 处理 `requestMoreHistory()` 信号

```cpp
// chatwidget.h
class ChatWidget : public QWidget {
    // ... 现有成员
signals:
    void loadMoreMessages(int friendId, uint64_t beforeId);  // 向后端请求更多消息
};

// mainwindow.cpp
void MainWindow::onContactSelected(int id, const QString& type) {
    // ... 现有逻辑
    
    // 连接懒加载信号
    disconnect(chatWidget, SIGNAL(loadMoreMessages(int, uint64_t)), 0, 0);
    connect(chatWidget, SIGNAL(loadMoreMessages(int, uint64_t)), 
            this, SLOT(onLoadMoreMessages(int, uint64_t)));
}

void MainWindow::onLoadMoreMessages(int friendId, uint64_t beforeId) {
    // 异步调用 API：GET /api/friend_messages?friend_id=X&before_id=Y&limit=20
    ApiRequestEvent* req = new ApiRequestEvent(ApiLoadFriendMessages);
    req->id = friendId;
    req->message = std::to_string(beforeId);  // 用 message 字段传递 beforeId
    eventPoller->postApiRequest(req);
}

void MainWindow::handleEvents(const EventList& events) {
    // ... 现有代码
    
    // 处理历史消息加载结果
    if (e.type == "friend_messages") {
        // 解析响应，将消息插入到现有消息前面
        // 调用 chatWidget->insertMessages(...)
    }
}
```

---

## 10. 实施步骤

### 阶段1：后端实现（Go 版本）

1. 添加 SQLite 依赖：`github.com/mattn/go-sqlite3`
2. 在 `main.go` 中添加数据库初始化函数 `initDB()`
3. 创建消息表（见 8.1）
4. 修改 `CallbackFriendMessage` 和 `CallbackConferenceMessage`，保存消息到数据库
5. 实现 `handleFriendMessagesHistory()` 和 `handleConferenceMessagesHistory()`
6. 在 `Start()` 中注册新路由

### 阶段2：创建 MessageView 类

1. 创建 `messageview.h`：
   - Qt3：定义 `MessageView` 继承 `QScrollView`
   - Qt4：定义 `MessageModel` 和 `MessageView`
2. 创建 `messageview.cpp`：
   - 实现 `drawContents()`（Qt3）
   - 实现 `MessageModel::data()`（Qt4）
3. 测试编译（Qt3 和 Qt4）

### 阶段3：修改 ChatWidget

1. 在 `chatwidget.h` 中：
   - 将 `messageArea` 替换为 `void* messageView`
   - 添加 `MessageItem` 结构
   - 添加 `allMessages` 列表
2. 在 `chatwidget.cpp` 中：
   - 修改构造函数：创建 `MessageView` 替换 `QTextEdit`
   - 修改 `appendMessage()`：区分 Qt3/Qt4 调用
   - 添加 `insertMessage()`：用于插入历史消息
   - 添加 `loadMoreMessages()`：触发向后端请求

### 阶段4：修改 MainWindow

1. 在 `mainwindow.h` 中添加：
   - `onLoadMoreMessages()` 槽函数
   - 连接 `ChatWidget::loadMoreMessages` 信号
2. 在 `mainwindow.cpp` 中：
   - 实现 `onLoadMoreMessages()`：发送异步 API 请求
   - 修改 `handleEvents()`：处理历史消息响应
   - 修改 `onContactSelected()`：首次加载最近 N 条消息

### 阶段5：测试

1. Qt3 编译测试：`bash buildqt3.sh`
2. Qt4 编译测试：`bash buildqt4.sh`
3. 功能测试：
   - 发送消息，检查虚拟视图渲染
   - 滚动到顶部，检查是否触发懒加载
   - 验证历史消息正确插入到前面
   - 切换联系人，检查消息缓存

---

## 11. 关键注意事项

### 11.1 Qt3 富文本支持

Qt3 的 `QScrollView::drawContents()` 中，可以用 `QSimpleRichText` 或手动解析 HTML 绘制富文本。如果太复杂，可以：
- 先在 Qt3 中只显示纯文本（用 `MessageItem::plainText`）
- 后续再优化为富文本显示

### 11.2 消息高度计算

示例中使用固定高度（60px），但实际消息高度可能不同。可选方案：
1. **固定高度**：简单，但可能截断长消息
2. **动态计算**：用 `QFontMetrics` 计算文本高度，但会增加复杂度
3. **推荐**：先用固定高度，后续优化

### 11.3 后端分页参数

- `before_id` 和 `after_id` **不要同时使用**
- 都不传时，返回最新的 N 条消息
- 推荐用自增 ID 分页（不是时间戳），因为 ID 顺序 = 时间顺序，且查询效率高

### 11.4 客户端缓存

为了减少 API 调用，可以在客户端内存中缓存消息：
- 切换联系人时，先检查内存缓存
- 缓存未命中时，才调用 API 加载
- 缓存结构：`QMap<QString, QList<MessageItem>>`，key = "friend_X" 或 "conference_X"

---

## 12. 总结

| 项目 | Qt3 方案 | Qt4 方案 |
|------|-----------|-----------|
| **基类** | `QScrollView` | `QListView` |
| **虚拟机制** | 重写 `drawContents()`，只绘制可见区域 | `QAbstractListModel` + `QListView`，内置虚拟渲染 |
| **数据管理** | `QList<MessageItem> items` | `QList<MessageItem> items` + Model |
| **滚动监听** | `contentsMousePressEvent()` + `contentsWheelEvent()` | `QScrollBar::valueChanged()` |
| **懒加载** | `emit requestMoreHistory()` | `emit requestMoreHistory()` |
| **富文本** | 用 `QPainter` 手动绘制（或只用纯文本） | `setIndexWidget()` 或用 `Qt::UserRole` 返回富文本 |

---

## 13. 文件清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `q3tox/messageview.h` | MessageView 类定义（Qt3/Qt4 条件编译） |
| `q3tox/messageview.cpp` | MessageView 实现 |
| `go-toxhttpd/db.go` | 数据库初始化和操作（Go 版本） |
| `c-version/db.c` / `db.h` | 数据库操作（C 版本，可选） |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `q3tox/chatwidget.h` | 添加 `MessageItem` 结构，替换 `messageArea` 为 `messageView` |
| `q3tox/chatwidget.cpp` | 修改构造函数和 `appendMessage()`，使用 `MessageView` |
| `q3tox/mainwindow.h` | 添加 `onLoadMoreMessages()` 槽 |
| `q3tox/mainwindow.cpp` | 实现懒加载请求和响应处理 |
| `q3tox/q3tox.pro` | 添加 `messageview.cpp` 到编译列表 |
| `go-toxhttpd/main.go` | 添加数据库初始化、历史消息 API、修改回调保存消息 |
| `go-toxhttpd/main.go` | 注册新路由：`/api/friend_messages`、`/api/conference_messages` |

---

*文档创建时间：2026-05-01*
*作者：OpenCode AI*
