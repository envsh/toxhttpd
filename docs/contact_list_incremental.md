# 联系人列表增量更新实现（当前状态）

## 1. 架构概览

### 1.1 数据流

```
MainWindow (事件驱动/网络轮询)
    │
    ├── setContacts(seedList)          ──→  allContacts 全量 + rebuildSortFilter
    ├── addContact(c)                   ──→  allContacts.append + 创建 item + sort
    ├── removeContact(id, type)         ──→  allContacts.remove + 删除 item
    ├── updateContact(id, type, ...)     ──→  改 allContacts + findAndUpdateItem
    ├── updateFriendName(id, name)       ──→  改 allContacts + findAndUpdateItem
    ├── updateFriendConnectionStatus(...) ──→  改 allContacts + findAndUpdateItem
    ├── updateContactLastMessage(...)     ──→  改 allContacts + m_lastMessages + findAndUpdateItem
    ├── incrementUnread(id, type, count)  ──→  m_unreadCounts++ + findAndUpdateItem
    ├── resetUnread(id, type)             ──→  m_unreadCounts=0 + findAndUpdateItem
    ├── onSearchTextChanged(text)         ──→  rebuildSortFilter
    ├── onSortMenuClicked()               ──→  rebuildSortFilter
    └── retranslateUi()                   ──→  rebuildSortFilter
```

### 1.2 Qt3 控件

| 层 | 类 | 基类 | 职责 |
|----|-----|------|------|
| View | `ContactListView` | `QListView` | 列管理、事件转发、选择信号 |
| Item | `ContactListViewItem` | `QListViewItem` | 自绘、排序、数据持有 |
| 绘制 | `paintContactRow()` | 全局函数 | 共享渲染逻辑 |

### 1.3 Qt4 控件

| 层 | 类 | 基类 | 职责 |
|----|-----|------|------|
| View | `QListView` | — | 标准 View |
| Model | `ContactListModel` | `QAbstractListModel` | 数据持有、排序、过滤、信号通知 |
| Delegate | `ContactListDelegate` | `QStyledItemDelegate` | 渲染（委托 `paintContactRow()`） |

---

## 2. 增量操作（13 个入口点）

### 2.1 Qt3 实现

| 入口 | Qt3 操作 |
|------|----------|
| `addContact(c)` | `new ContactListViewItem(lv, ...)` + `lv->sort()` |
| `removeContact(id, type)` | 遍历 `lv->firstChild()` → `dynamic_cast<ContactListViewItem*>` → `delete ci` |
| `updateFriendName` / `updateFriendConnectionStatus` / `updateContact` / `updateContactLastMessage` / `incrementUnread` / `resetUnread` | 改 `allContacts` + `findAndUpdateItem(id, type)` |
| `setContacts(seedList)` | `lv->clear()` + 批量 `new ContactListViewItem` + `rebuildSortFilter()` |
| `onSearchTextChanged` / `onSortMenuClicked` / `retranslateUi` | `rebuildSortFilter()` |

### 2.2 Qt4 实现

| 入口 | Qt4 操作 |
|------|----------|
| `addContact(c)` | `m_model->addContact(c, ...)` |
| `removeContact(id, type)` | `m_model->removeContact(id, type)` |
| `updateFriendName` / `updateFriendConnectionStatus` / `updateContact` / `updateContactLastMessage` / `incrementUnread` / `resetUnread` | 改 `allContacts` + `m_model->updateContact(id, type, ...)` |
| `setContacts(seedList)` | `m_model->setContacts(...)` |
| `onSearchTextChanged` | `m_model->applyFilter(text)` |
| `onSortMenuClicked` | `m_model->applySort(criteria)` |
| `retranslateUi` | `rebuildSortFilter()` |

### 2.3 `findAndUpdateItem()` 流程

```cpp
// Qt3: 遍历 QListViewItem 链表，按 (id, type) 匹配
QListView* lv = static_cast<QListView*>(listWidget);
for (QListViewItem* item = lv->firstChild(); item; item = item->nextSibling()) {
    ContactListViewItem* ci = dynamic_cast<ContactListViewItem*>(item);
    if (ci && ci->itemId() == id && ci->itemType() == type) {
        ci->updateContact(c->name, c->status, c->is_connected);  // setText(0, m_name)
        ci->updateData(unread, lastMsg, lastTime);                // setText(0, m_name)
        break;
    }
}
```

```cpp
// Qt4: model->updateContact 发出 dataChanged 信号
m_visible[i].unread = unread;  // 更新 data
m_visible[i].lastMessage = lastMsg;  // 更新 data
emit dataChanged(index(i), index(i));  // 通知 View
```

---

## 3. `ContactListViewItem`（Qt3）

### 3.1 类定义

```cpp
class ContactListViewItem : public QListViewItem {
    int m_id, m_unread;
    QString m_type, m_name, m_status, m_lastMessage, m_timeStr;
    bool m_isConnected;

    void paintCell(QPainter* p, const QColorGroup& cg, int col, int width, int align);
    void setup();
    int compare(QListViewItem* other, int col, bool ascending) const;
    void updateData(int unread, const QString& lastMessage, const QString& timeStr);
    void updateContact(const QString& name, const QString& status, bool isConnected);
};
```

### 3.2 `paintCell()`

```cpp
void ContactListViewItem::paintCell(QPainter* p, const QColorGroup&, int, int width, int) {
    paintContactRow(*p, 0, 0, width, height(),
                    isSelected(), m_type, m_name, m_status, m_isConnected, m_unread,
                    m_lastMessage, m_timeStr);
}
```

不调用父类 `QListViewItem::paintCell()`，完全自定义绘制。传递给 `paintContactRow` 的是成员变量（非 `text(0)`）。

### 3.3 `setup()`

```cpp
void ContactListViewItem::setup() {
    QListViewItem::setup();
    if (listView()) setHeight(calcItemHeight(listView()->font()));
}
```

QListView 在 item 插入和 `sort()` 后自动调用。`QListViewItem::setup()` 设置默认最小高度，然后覆盖为我们的动态行高。

### 3.4 `compare()`

```cpp
int ContactListViewItem::compare(QListViewItem* other, int, bool) const {
    const std::vector<QString>& criteria = *g_sortCriteriaPtr;
    for (int i = (int)criteria.size() - 1; i >= 0; --i) {
        if (c == "online_first") {
            // online/tcp 优先于 offline
        } else if (c == "name_asc") {
            // localeAwareCompare
        } else if (c == "name_desc") {
            // localeAwareCompare 反向
        } else if (c == "by_type") {
            // 按类型排序
        }
    }
    return 0;
}
```

多条件比较：从低优先级到高优先级遍历，最先非零值决定顺序。`g_sortCriteriaPtr` 是文件级静态指针，由调用方在 `sort()` 前设置。

### 3.5 数据更新方法

```cpp
void ContactListViewItem::updateData(int unread, const QString& lastMessage, const QString& timeStr) {
    m_unread = unread;
    m_lastMessage = lastMessage;
    m_timeStr = timeStr;
    setText(0, m_name);  // 企图触发 repaint
}

void ContactListViewItem::updateContact(const QString& name, const QString& status, bool isConnected) {
    m_name = name;
    m_status = status;
    m_isConnected = isConnected;
    setText(0, m_name);  // 企图触发 repaint
}
```

**注意**：`setText(0, m_name)` 在 Manual width mode 下不触发 `itemWidthChanged()`，不产生 repaint。见第 9 节已知问题。

---

## 4. `ContactListView`（Qt3）

### 4.1 构造

```cpp
ContactListView::ContactListView(ContactListWidget* w) : QListView(w), m_widget(w) {
    addColumn("", 1);          // Manual width mode
    header()->hide();
    setRootIsDecorated(false); // 无缩进
    setSorting(-1);            // 手动 sort()
}
```

### 4.2 `resizeEvent`

```cpp
void resizeEvent(QResizeEvent* e) {
    QListView::resizeEvent(e);
    setColumnWidth(0, viewport()->width());  // 单列自适应
}
```

### 4.3 右键菜单

```cpp
void contentsMousePressEvent(QMouseEvent* e) {
    QListView::contentsMousePressEvent(e);  // 处理选中 + selectionChanged 信号
    if (e->button() == Qt::RightButton) {
        // 通过 itemAt(e->pos()) 找到对应 item → m_widget->showContextMenuAt(...)
    }
}
void contentsContextMenuEvent(QContextMenuEvent* e) {
    e->accept();  // 阻止默认右键菜单
}
```

### 4.4 选择信号

```cpp
connect(lv, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

void ContactListWidget::onSelectionChanged() {
    QListView* lv = static_cast<QListView*>(listWidget);
    ContactListViewItem* citem = static_cast<ContactListViewItem*>(lv->selectedItem());
    if (!citem) { return; }
    emit contactSelected(citem->itemId(), citem->itemType(), citem->itemName());
}
```

---

## 5. `ContactListModel`（Qt4）

### 5.1 数据存储

```cpp
struct Item {
    Contact* contact;
    int unread;
    QString lastMessage;
    QString lastMessageTime;
};

QList<Item> m_allItems;   // 全部
QList<Item> m_visible;    // 通过 filter 后的可见项
```

### 5.2 关键方法

| 方法 | 实现 |
|------|------|
| `rowCount()` | 返回 `m_visible.size()` |
| `data()` | 返回 `m_visible[row]` 的 UserRole 字段 |
| `flags()` | `ItemIsSelectable \| ItemIsEnabled` |
| `addContact()` | 追加到 `m_allItems`；通过 filter 后 `beginInsertRows` + 追加到 `m_visible` + `doSortItems` + `emit layoutChanged()` |
| `removeContact()` | 从 `m_allItems` 和 `m_visible` 删除；`beginRemoveRows`/`endRemoveRows` |
| `updateContact()` | 更新 `m_allItems` 和 `m_visible` 数据；`emit dataChanged(index(i), index(i))` |
| `applyFilter()` | `beginResetModel()` + 重建 `m_visible` + `endResetModel()` |
| `applySort()` | `beginResetModel()` + `doSortItems(m_visible)` + `endResetModel()` |

### 5.3 排序实现

```cpp
void doSortItems(QList<Item>& items) {
    std::stable_sort(items.begin(), items.end(), [this](const Item& a, const Item& b) {
        for (int i = (int)m_sortCriteria.size() - 1; i >= 0; --i) {
            // 同 Qt3 compare() 逻辑
        }
        return a.contact->lastActive > b.contact->lastActive;
    });
}
```

同一 `m_sortCriteria` 向量驱动，尾元素最高优先级。默认 `"online_first"`，回退按 `lastActive` 降序。

### 5.4 身份查找

```cpp
QModelIndex findIndex(int id, const QString& type) const;
// 遍历 m_visible 匹配 (id, type) → index(i)
```

---

## 6. `ContactListDelegate`（Qt4）

```cpp
void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    // 从 idx.data(UserRole+1..7) 读取字段
    paintContactRow(*p, opt.rect.x(), opt.rect.y(), opt.rect.width(), opt.rect.height(),
                    sel, type, name, status, conn, unread, lastMessage, timeStr);
}

QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const {
    return QSize(minW, calcItemHeight(opt.font));
}
```

---

## 7. 共享组件

### 7.1 `paintContactRow()`

```cpp
static void paintContactRow(QPainter& p, int x, int y, int w, int h,
    bool selected, const QString& type, const QString& name,
    const QString& status, bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr);
```

绘制顺序（从左到右）：
1. **选中背景**（选中时）— `lerpColor(baseBg, accent, 0.25)`
2. **状态点** — 绿色/灰色椭圆，`kDotR=5`
3. **圆形头像** — emoji 渲染 + `QPixmap::setMask`（Qt3）/ `QPainterPath` clip（Qt4）
4. **第 1 行**：名称（bold，自适应截断）+ 时间（右对齐，small font）
5. **第 2 行**：最后消息（略透明，自适应截断）+ 未读徽标（右对齐）

### 7.2 `calcItemHeight()`

```cpp
static int calcItemHeight(const QFont& f) {
    QFont sf = f;
    if (f.pointSize() > 4) sf.setPointSize(f.pointSize() - 2);
    int lh = QFontMetrics(f).lineSpacing();
    int lh2 = QFontMetrics(sf).lineSpacing();
    int textH = 6 + lh + 1 + lh2 + 6;    // 文字区域高度
    int avH   = 6 + kAvatarSz + 6;       // 头像区域高度 (kAvatarSz=36)
    return std::max(textH, avH);
}
```

无 `kRowH` 硬编码。

### 7.3 布局常量

```cpp
static const int kPad = 8;
static const int kRightPad = 12;
static const int kRightAreaW = 55;  // time + unread + 右外边距
static const int kDotR = 5;
static const int kAvatarSz = 36;
```

### 7.4 `rebuildSortFilter()`

```cpp
void ContactListWidget::rebuildSortFilter() {
    // Qt3:
    g_sortCriteriaPtr = &m_sortCriteria;
    // 遍历 items 设置可见性
    lv->sort();
    countLabel->setText(visibleCount);

    // Qt4:
    m_model->applyFilter(m_searchText);
    m_model->applySort(m_sortCriteria);
}
```

---

## 8. 与旧设计的差异

| 旧设计（doc 原始版本） | 当前实现 |
|------------------------|----------|
| QListBox / QListWidget | Qt3: QListView / Qt4: QAbstractListModel |
| `updateView_v3()`/`v4()` 全量重建 | 13 个独立入口点，无通用 updateView |
| `sortVisible()` 全量排序 | `compare()` 虚函数 / `doSortItems()` lambda |
| 三路 diff 算法 | 无 diff；`findAndUpdateItem()` 遍历匹配 |
| `ContactListItem`（QListBoxItem） | `ContactListViewItem`（QListViewItem） |
| `kRowH` 硬编码 | `calcItemHeight()` 动态计算 |
| `hasItemDataChanged()` / `restoreSelectionAndScroll()` | 不存在 |
| `applyDiffQt3` / `applyDiffQt4` | 不存在 |
| 需要恢复选中/滚动位置 | 不恢复（只在 sort 后 QListView 自动保持选中） |

---

## 9. 已知问题

### 9.1 `setText(0, m_name)` 在 Manual mode 无效（重影/无重绘）

`ContactListView` 构造函数调用 `addColumn("", 1)`，使 column 0 进入 Manual width mode。Qt3 中 `QListViewItem::setText()` 内部调 `widthChanged(col)`，后者在 Manual mode 下**立即返回**，不调用 `itemWidthChanged()`，不安排重绘。

效果：
- `updateContact()` / `updateData()` 修改了成员变量，但**无 repaint 被触发**
- 除非有其他事件（选中切换、滚动）触发 QListView 重绘，否则数据变更对用户不可见
- 选中切换时 QListView 自身会重绘选中/非选中 items，此时 `paintCell()` 读取更新后的成员变量，显示正确

**影响**：非选中触发的更新（如网络轮询更新状态/未读数）不会立即反映在界面上。

### 9.2 `setSorting(-1)` 可能阻止 `sort()` 生效

`QListView::sort()` 可能检查 `sortColumn() >= 0`，而 `setSorting(-1)` 使 `sortColumn()` 返回 -1。若 Qt3 实现在 `sortColumn() < 0` 时跳过排序，则所有 `lv->sort()` 调用均为空操作（no-op）。

若此为真：
- `addContact()` 后新 item 始终追加到末尾，不会插入到正确位置
- `rebuildSortFilter()` 无法重新排序
- `compare()` 不会被调用

**待验证**：需在 Qt3 运行时确认 `sort()` 是否实际触发。

### 9.3 选中排序与数据更新的冲突

`findAndUpdateItem()` 曾被设计为调用 `lv->sort()` 以保持排序一致性。但在选择事件链中调用 `sort()` 会破坏 Qt3 QListView 的选中状态（items 被 `takeItem()`/`insertItem()` 操作，选中项漂移/丢失）。

**当前修复**：从 `findAndUpdateItem()` 中移除 `sort()` 调用。代价是数据更新后列表顺序可能不随排序条件变化。

### 9.4 `paintCell` 不调用父类

未调用 `QListViewItem::paintCell(p, cg, col, width, align)`，因此：
- 无默认文本绘制（但 `paintContactRow` 完全覆盖了需求）
- 无默认选中背景绘制（但 `paintContactRow` 自行绘制选中背景）
- 无焦点指示器（当前未实现）

---

## 10. 关键知识点

### Qt3 `setText()` 调用链

```
setText(col, text)
  → m_text[col] = text
  → widthChanged(col)
    → if columnWidthMode == Manual → return (no-op!)
    → else → recalculate width → itemWidthChanged(this) → repaintItem(this)
```

在 Manual mode 下，`setText()` 只写 `m_text[col]`，不触发重绘。

### Qt3 `QListViewItem::setup()` 触发时机

- Item 第一次插入 QListView（`insertItem` → `setup()`）
- `sort()` 后 items 被 take+re-insert（每个 item 再次触发 `setup()`）
- 不在 `paintCell()` 调用链中

### Qt3 `sort()` 的副作用

- 取走所有 item → 重新排序 → 逐个 `insertItem`（触发 `setup()`、高度重算、位置重排）
- QListView 自动保持当前 `selectedItem()`（只要 item 指针不变）
- QListView 自动保持滚动位置（基于 `currentItem()`）

### Qt3/4 互斥编译

- `#ifdef QT3_BUILD` 包裹 Qt3-only 代码
- `contactlist.cpp` 末尾 `#include "contactlist.moc"` 用于 Qt4 的 `Q_OBJECT` 内类
