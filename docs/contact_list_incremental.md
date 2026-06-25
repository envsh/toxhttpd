# 联系人列表增量更新设计

## 1. 目标

完全消除 `ContactListWidget` 中所有 `.clear()` + 全量重建导致的闪烁问题。
每个修改操作（改名、更新状态、未读数变化、新消息等）只操作对应的单行，
不变的行不做任何操作。

## 2. 现状分析

### 2.1 当前架构

```
MainWindow (事件驱动)
    │
    ├── setContacts(seedList)          ──→  allContacts.clear + 全量重建 (仅初始化)
    ├── addContact(c)                   ──→  allContacts.append → updateView*
    ├── removeContact(id, type)         ──→  allContacts.remove → updateView*
    ├── updateContact(id, type, ...)     ──→  修改 allContacts → updateView*
    ├── updateFriendName(id, name)       ──→  修改 allContacts → updateView*
    ├── updateFriendConnectionStatus(...) ──→  修改 allContacts → updateView*
    ├── updateContactLastMessage(...)     ──→  修改 allContacts → updateView*
    ├── incrementUnread(id, type, count)  ──→  m_unreadCounts++ → updateView*
    ├── resetUnread(id, type)             ──→  m_unreadCounts=0 → updateView*
    ├── onSearchTextChanged(text)         ──→                 → updateView*
    ├── onSortMenuClicked()               ──→                 → updateView*
    └── retranslateUi()                   ──→                 → updateView*
```

`updateView_v3()`（Qt3）和 `updateView_v4()`（Qt4）的内部流程：

```
1. 遍历 allContacts → 按搜索过滤 + 排序 → visible[]
2. countLabel->setText(visible.size())
3. 保存当前选中项 (selectedId, selectedType)
4. 保存滚动位置 (scrollId, scrollType)
5. lb->clear() / lw->clear()       ← 全量删除
6. 遍历 visible[] → 逐个 new QListBoxItem / QListWidgetItem
7. 恢复选中项
8. 恢复滚动位置
```

### 2.2 闪烁根因

- `lb->clear()` 立即从 QListBox 中移除所有项，视图闪白
- `lw->clear()` + `addItem()` 即使 `setUpdatesEnabled(false)` 也会触发 Qt4 的
  `rowsInserted`/`rowsRemoved` 信号，导致 Delegate 重建
- 即使只改一个未读数，全部重建，UI 线程阻塞几十毫秒

### 2.3 13 个调用点的操作分类

| 调用点 | 操作类型 | 是否影响排序 | 频率 |
|--------|----------|-------------|------|
| `addContact(c)` | 新增 | 是（需要插入到正确位置） | 低 |
| `removeContact(id, type)` | 删除 | 是 | 低 |
| `updateContact(id, type, name, chatId, status)` | 修改多字段 | 可能（name/status 变化） | 中 |
| `updateFriendName(id, name)` | 改名称 | 可能（name 排序时） | 低 |
| `updateFriendConnectionStatus(id, status)` | 改状态 | 可能（online_first 排序时） | 中 |
| `updateContactLastMessage(id, type, msg, time)` | 更新最后消息+时间 | **是**（lastActive 是主排序键） | **高** |
| `incrementUnread(id, type, count)` | 未读数+1 | 否 | 高 |
| `resetUnread(id, type)` | 未读数清零 | 否 | 中 |
| `setContacts(seedList)` | 全量设置 | 是 | 仅启动时 |
| `onSearchTextChanged(text)` | 搜索过滤 | 是 | 用户操作 |
| `onSortMenuClicked()` | 排序切换 | 是 | 用户操作 |
| `retranslateUi()` | 翻译刷新 | 否 | 用户操作 |

核心问题：**高频率的 `updateContactLastMessage`**（每次收到消息都触发）和
`incrementUnread` 都走全量重建。实际上每次只影响一行。

## 3. 参考设计：TG PeerListBox

Telegram Desktop 的 `PeerListBox` 提供了精确的行级操作方法：

| TG 方法 | 行为 | 我们对应的操作 |
|----------|------|---------------|
| `peerListUpdateRow(row)` | 更新单个行的数据和显示 | 单行数据更新 + `update()` |
| `peerListRemoveRow(row)` | 移除单个行 | 单行删除 |
| `addRowEntry(row)` | 在排序后的正确位置插入 | 排序后插入 |
| `rebuildRows()` | 全量重建（搜索/排序切换时） | 保留全量重建 |

TG 的 `updateRowHook(row)` 是虚函数，子类可以覆盖来实现自定义更新逻辑。

核心差异：TG 的 `PeerListRow` 是一个 long-lived 对象，创建后常驻内存，
不会被 clear+rebuild。我们的 `ContactListItem`/`QListWidgetItem` 被反复销毁重建。

## 4. 设计方案

### 4.1 核心策略：diff 算法

不再为每个操作单独写排序/插入逻辑。保持现有的 `visible` 计算逻辑不变，
但把 `clear` + 全量 `addItem` 替换为 **三路 diff**：

```
新 visible[] (computed from allContacts + filter + sort)
    ↓
与当前 list widget 中的 items (by identity key = id+type) 对比
    │
    ├── 在新 visible 但不在当前 items 中 →  add (insert at correct index)
    ├── 在当前 items 但不在新 visible 中 →  remove (single item)
    └── 同在两者中 → update data in place (不变的不动)
```

**算法复杂度**：O(n) 摊还，n 是 visible 长度（通常 < 200），远比重建 O(n) + 内存分配 + 绘制快。

### 4.2 Identity key

每个 `Contact` 用 `(id, type)` 作为唯一标识。对应地，Qt3 的 `ContactListItem` 已有的
`itemId()` / `itemType()`, Qt4 的 `QListWidgetItem` 的 `UserRole` / `UserRole+1`。

```
using IdentityKey = std::pair<int, std::string>;
```

### 4.3 三路 diff 算法实现

```cpp
struct DiffOp {
    enum Type { Noop, Update, Remove, Add };
    Type type;
    int newIndex;    // 在新 visible 中的位置
    Contact* data;   // 新数据（Update/Add 时有效）
};

std::vector<DiffOp> computeDiff(
    const std::vector<Contact*>& newVisible,
    QListBox* lb)   // Qt3
    // 或
    const std::vector<Contact*>& newVisible,
    QListWidget* lw)  // Qt4
{
    // 1. 构建当前 items 的 identity → index 映射
    std::map<IdentityKey, int> currentMap;
    for (int i = 0; i < itemCount; ++i) {
        IdentityKey key = getItemIdentity(i);
        currentMap[key] = i;
    }
    
    // 2. 构建新 visible 的 identity → index 映射
    std::map<IdentityKey, int> newMap;
    for (int i = 0; i < newVisible.size(); ++i) {
        IdentityKey key = {newVisible[i]->id, qToUtf8(newVisible[i]->type).toStdString()};
        newMap[key] = i;
    }
    
    // 3. 遍历新 visible，确定每个 items 的操作
    std::vector<DiffOp> ops;
    
    // 3a. 收集需要 remove 的项（在 current 但不在 new 中）
    for (auto& [key, oldIdx] : currentMap) {
        if (newMap.find(key) == newMap.end()) {
            ops.push_back({DiffOp::Remove, oldIdx, nullptr});
        }
    }
    
    // 3b. 按新顺序遍历，确定 add/update
    // 使用 two-pointer 技术追踪 current 中剩余的 items
    std::set<int> consumedOldIndices;
    for (int newIdx = 0; newIdx < newVisible.size(); ++newIdx) {
        Contact* c = newVisible[newIdx];
        IdentityKey key = {c->id, qToUtf8(c->type).toStdString()};
        auto it = currentMap.find(key);
        if (it == currentMap.end()) {
            ops.push_back({DiffOp::Add, newIdx, c});
        } else {
            int oldIdx = it->second;
            consumedOldIndices.insert(oldIdx);
            // 检查数据是否真的变了（避免不必要的 repaint）
            if (hasDataChanged(oldIdx, c)) {
                ops.push_back({DiffOp::Update, newIdx, c});
            } else {
                ops.push_back({DiffOp::Noop, newIdx, nullptr});
            }
        }
    }
    
    return ops;
}
```

### 4.4 Helper 函数

```cpp
// ——— 新增到 contactlist.h ———

// 查找项在当前列表中的索引（用于 Qt3/Qt4 内部）
int findItemById(int id, const QString& type) const;

// 更新索引处的项
void updateItemAt(int index, Contact* c);

// 删除索引处的项
void removeItemAt(int index);

// 在索引处插入新项
void insertItemAt(int index, Contact* c);

// 检查某项的数据是否真的变化了（避免无谓更新）
bool hasItemDataChanged(int oldIndex, Contact* newData) const;
```

### 4.5 修改后的 updateView_v3/v4

```cpp
void ContactListWidget::updateView_v3() {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;

    // 1. 收集 + 过滤 + 排序（不变）
    std::vector<Contact*> visible;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (!m_searchText.isEmpty()
            && !qToUpper(c->name).contains(qToUpper(m_searchText))) {
            continue;
        }
        visible.push_back(c);
    }
    sortVisible(visible);

    // 更新计数
    countLabel->setText(QString::number(visible.size()));

    // 保存选中/滚动（仅在全量重建时使用）
    // ...

    // 2. 计算 diff
    std::vector<DiffOp> ops = computeDiff(visible, lb);

    // 3. 倒序执行 remove 操作（从后往前删除，索引不变）
    //    然后正序执行 add/update
    //
    // 注意：QListBox 没有 insert/remove 单行的方法，
    // 需要通过替换 item 内容来实现.
    // 简化策略：先删除（从后往前），再按新顺序重建 visible
    //          但不调用 clear()，只删除需要删的项

    // 更实际的策略（QListBox 限制）：
    // QListBox 没有 replaceItem()，只能用 block 构造然后 entire change
    // 但我们可以：setUpdatesEnabled(false) → 快速重建 → updatesEnabled(true)
    // 加上只删除新增后 addItem，不重建已存在的 item
    //
    // 实际做法：
    // 1. 从后往前 remove 掉不在 visible 中的项
    // 2. 从前往后 check 每个位置：
    //    a. 如果位置 i 的 item 与 visible[i] 匹配 → 更新其数据 + repaint
    //    b. 如果不匹配 → 在该位置插入新 item
    // 3. 如果 visible 比当前长 → append 剩余
    // 4. 如果 visible 比当前短 → 删除末尾多余的
    //
    // QListBox 的限制：只能用 insertItem() 和 removeItem()，
    // 再用 setItem() 来替换内容
    //
    // 最终方案见下文第 5 节
#endif
}
```

## 5. Qt3 实现细节（QListBox）

### 5.1 QListBox 的局限性

Qt3 的 `QListBox` **没有**原生按 item 替换的 API：
- `QListBox::insertItem(QListBoxItem*, int index)` — 在 index 处插入
- `QListBox::removeItem(int index)` — 移除 index 处的项
- `QListBox::item(int index)` → `QListBoxItem*` — 获取项
- **没有** `replaceItem()` 或 `setItem()`。

但我们可以**复用已有 item**：删除旧 item 后在原位置创建新 item。

### 5.2 Qt3 逐条操作策略

```cpp
// Qt3 增量更新
void applyDiffQt3(QListBox* lb, const std::vector<DiffOp>& ops,
                   const std::vector<Contact*>& visible) {
    lb->setUpdatesEnabled(false);

    // Step 1: 收集要删除的索引，从后往前删除
    std::vector<int> removeIndices;
    for (const auto& op : ops) {
        if (op.type == DiffOp::Remove) {
            removeIndices.push_back(op.newIndex); // newIndex 在这里是 oldIndex
        }
    }
    // 从后往前排序，高位先删
    std::sort(removeIndices.begin(), removeIndices.end(), std::greater<int>());
    for (int idx : removeIndices) {
        delete lb->item(idx);
        lb->removeItem(idx);
    }

    // Step 2: 遍历新 visible，逐个同步
    int currentCount = lb->count();
    int newCount = visible.size();
    int maxLen = qMax(currentCount, newCount);

    for (int i = 0; i < maxLen; ++i) {
        if (i >= newCount) {
            // visible 比当前短 → 删除末尾多余项
            delete lb->item(currentCount - 1);
            lb->removeItem(currentCount - 1);
            --currentCount;
        } else {
            Contact* c = visible[i];
            auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
            int unread = (m_unreadCounts.find(key) != m_unreadCounts.end())
                         ? m_unreadCounts[key] : 0;
            QString lastMsg = c->lastMessage.isEmpty()
                ? m_lastMessages[key] : c->lastMessage;
            QString lastTime = c->lastMessageTime.isEmpty()
                ? m_lastMessageTimes[key] : c->lastMessageTime;

            if (i < currentCount) {
                // 检查现有 item 是否匹配
                ContactListItem* existing = (ContactListItem*)lb->item(i);
                if (existing && existing->itemId() == c->id
                    && existing->itemType() == c->type) {
                    // 数据可能有变化，创建新 item 替换旧 item
                    delete existing;
                    lb->removeItem(i);
                    ContactListItem* newItem = new ContactListItem(
                        lb, c->id, c->type, c->name, c->status,
                        c->is_connected, unread, lastMsg, lastTime);
                    lb->insertItem(newItem, i);
                } else {
                    // 不匹配 → 替换为正确的
                    delete existing;
                    lb->removeItem(i);
                    ContactListItem* newItem = new ContactListItem(
                        lb, c->id, c->type, c->name, c->status,
                        c->is_connected, unread, lastMsg, lastTime);
                    lb->insertItem(newItem, i);
                }
            } else {
                // 追加新项
                ContactListItem* newItem = new ContactListItem(
                    lb, c->id, c->type, c->name, c->status,
                    c->is_connected, unread, lastMsg, lastTime);
                lb->insertItem(newItem, lb->count());
                ++currentCount;
            }
        }
    }

    // 恢复选中和滚动
    restoreSelectionAndScroll(lb);

    lb->setUpdatesEnabled(true);
    lb->repaint();
}
```

### 5.3 优化点：QListBoxItem 原地更新

更好的办法：给 `ContactListItem` 添加 `updateData()` 方法，**不销毁重建 item**，
只改内部数据然后触发重绘。这样可以完全避免 QListBox 内部索引漂移。

```cpp
// 在 ContactListItem 中新增
void ContactListItem::updateData(const QString& name, const QString& status,
    bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr) {
    m_name = name;
    m_status = status;
    m_isConnected = isConnected;
    m_unread = unread;
    m_lastMessage = lastMessage;
    m_timeStr = timeStr;
    update();  // 触发 QListBoxItem 重绘
}
```

这样 Qt3 的 per-item update 变成：

```cpp
if (existing && existing->itemId() == c->id && existing->itemType() == c->type) {
    existing->updateData(c->name, c->status, c->is_connected, unread, lastMsg, lastTime);
} else {
    // 只有位置不匹配时才销毁重建（由排序变化导致）
    // ...
}
```

**这样 90% 的日常更新（收到消息、未读数）只需要改 item 数据 + repaint，不需要增删 item。**

## 6. Qt4 实现细节（QListWidget）

### 6.1 QListWidget 的优势

Qt4 的 `QListWidgetItem` 支持直接改 UserRole data，然后 `repaint()` 即可，
不需要销毁重建 item。

### 6.2 Qt4 逐条操作策略

```cpp
// Qt4 增量更新
void applyDiffQt4(QListWidget* lw, const std::vector<DiffOp>& ops,
                   const std::vector<Contact*>& visible) {
    lw->setUpdatesEnabled(false);

    // Step 1: 删除被移除的 items（从后往前）
    std::vector<int> removeIndices;
    for (const auto& op : ops) {
        if (op.type == DiffOp::Remove) {
            // newIndex 存储了 oldIndex
            removeIndices.push_back(op.newIndex);
        }
    }
    std::sort(removeIndices.begin(), removeIndices.end(), std::greater<int>());
    for (int idx : removeIndices) {
        QListWidgetItem* item = lw->takeItem(idx);
        delete item;
    }

    // Step 2: 遍历同步
    int currentCount = lw->count();
    int newCount = visible.size();

    for (int i = 0; i < newCount; ++i) {
        Contact* c = visible[i];
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        int unread = (m_unreadCounts.find(key) != m_unreadCounts.end())
                     ? m_unreadCounts[key] : 0;
        QString lastMsg = c->lastMessage.isEmpty()
            ? m_lastMessages[key] : c->lastMessage;
        QString lastTime = c->lastMessageTime.isEmpty()
            ? m_lastMessageTimes[key] : c->lastMessageTime;

        if (i < currentCount) {
            QListWidgetItem* item = lw->item(i);
            int itemId = item->data(Qt::UserRole).toInt();
            QString itemType = item->data(Qt::UserRole + 1).toString();
            if (itemId == c->id && itemType == c->type) {
                // 匹配 → 原地更新数据
                item->setData(Qt::UserRole + 2, c->name);
                item->setData(Qt::UserRole + 3, unread);
                item->setData(Qt::UserRole + 4, c->is_connected);
                item->setData(Qt::UserRole + 5, c->status);
                item->setData(Qt::UserRole + 6, lastMsg);
                item->setData(Qt::UserRole + 7, lastTime);
            } else {
                // 不匹配（排序变化导致位置变动）
                // 删除旧 item，插入新 item
                delete lw->takeItem(i);
                QListWidgetItem* newItem = createContactItem(c, unread, lastMsg, lastTime);
                lw->insertItem(i, newItem);
            }
        } else {
            // 追加（addContact）
            QListWidgetItem* newItem = createContactItem(c, unread, lastMsg, lastTime);
            lw->addItem(newItem);
        }
    }

    // Step 3: 删除多余的尾部项（visible 变少时）
    while (lw->count() > newCount) {
        delete lw->takeItem(lw->count() - 1);
    }

    // 恢复选中 + 滚动
    restoreSelectionAndScroll(lw);

    lw->setUpdatesEnabled(true);
    lw->repaint();
}

QListWidgetItem* ContactListWidget::createContactItem(Contact* c,
    int unread, const QString& lastMsg, const QString& lastTime) {
    QListWidgetItem* item = new QListWidgetItem();
    item->setData(Qt::UserRole,     c->id);
    item->setData(Qt::UserRole + 1, c->type);
    item->setData(Qt::UserRole + 2, c->name);
    item->setData(Qt::UserRole + 3, unread);
    item->setData(Qt::UserRole + 4, c->is_connected);
    item->setData(Qt::UserRole + 5, c->status);
    item->setData(Qt::UserRole + 6, lastMsg);
    item->setData(Qt::UserRole + 7, lastTime);
    return item;
}
```

### 6.3 简化方案：直接比较 identity

由于 Qt4 的 `QListWidgetItem` 支持原地更新，对于不改变位置的更新
（如 unread 只增不减、改名且 name 排序未启用），我们只需要：

```cpp
// 简单单行更新（不涉及排序变化）
void ContactListWidget::simpleUpdateItem(int id, const QString& type) {
    QListWidget* lw = (QListWidget*)listWidget;
    for (int i = 0; i < lw->count(); ++i) {
        QListWidgetItem* item = lw->item(i);
        if (item->data(Qt::UserRole).toInt() == id
            && item->data(Qt::UserRole + 1).toString() == type) {
            // 从 allContacts + m_unreadCounts 取最新数据
            Contact* c = findContactById(id, type);
            if (!c) return;
            auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
            int unread = (m_unreadCounts.find(key) != m_unreadCounts.end())
                         ? m_unreadCounts[key] : 0;
            item->setData(Qt::UserRole + 2, c->name);
            item->setData(Qt::UserRole + 3, unread);
            item->setData(Qt::UserRole + 4, c->is_connected);
            item->setData(Qt::UserRole + 5, c->status);
            // 如果 lastMessage 变了，也要通知重新排序
            lw->repaint();
            return;
        }
    }
}
```

但**排序变化**才是核心难点。`updateContactLastMessage` 会影响 lastActive，
而 lastActive 是主排序键。所以这个更新可能让项移动到顶部。

当排序可能变化时，还是需要走完整的 diff 流程。

## 7. 需要保留全量重建的场景

以下场景不应做 diff，直接全量 rebuild（因为它们本身是用户主动触发的低频操作，
且涉及整个列表结构的剧烈变化）：

| 场景 | 原因 |
|------|------|
| `setContacts(seedList)` | 仅启动时一次性调用 |
| `onSearchTextChanged(text)` | 搜索前后 visible 变化极大，diff 无意义 |
| `onSortMenuClicked()` | 排序条件变化导致所有项位置重排 |
| `retranslateUi()` | 仅语言/主题切换时，手动 rebuild |

这些场景继续使用现有的 `clear()` + 全量 `addItem`。

## 8. 滚动位置和选中项恢复

### 8.1 原则

增量更新时滚动位置和选中项的恢复方式**不变**，仍然按 identity key 恢复：

```cpp
void ContactListWidget::restoreSelectionAndScroll(QListBox* lb) {
    // 选中恢复
    if (m_pendingSelectedId >= 0) {
        for (int i = 0; i < lb->count(); ++i) {
            ContactListItem* item = (ContactListItem*)lb->item(i);
            if (item && item->itemId() == m_pendingSelectedId
                && item->itemType() == m_pendingSelectedType) {
                lb->setSelected(item, TRUE);
                break;
            }
        }
    }

    // 滚动恢复
    if (m_pendingScrollId >= 0) {
        for (int i = 0; i < lb->count(); ++i) {
            ContactListItem* item = (ContactListItem*)lb->item(i);
            if (item && item->itemId() == m_pendingScrollId
                && item->itemType() == m_pendingScrollType) {
                lb->setTopItem(i);
                break;
            }
        }
    }

    // 确保选中可见
    ContactListItem* sel = (ContactListItem*)lb->selectedItem();
    if (sel) {
        int selIdx = lb->index(sel);
        int topIdx = lb->topItem();
        int visRows = lb->height() / kRowH;
        if (visRows < 2) visRows = 2;
        if (selIdx < topIdx)
            lb->setTopItem(selIdx);
        else if (selIdx >= topIdx + visRows)
            lb->setTopItem(selIdx - visRows + 1);
    }
}
```

### 8.2 增量更新时滚动的特殊处理

当排序变化导致选中项位置移动时：

- 增量 `removeItemAt` + `insertItemAt` 后，选中项可能在新位置
- 需要在 applyDiff 完成后，按 item identity 重新选中
- 如果选中项在 diff 中被移除了（罕见，搜索/排序切换才会），走全量重建自然处理

## 9. 优化：批量处理标记

对于高频的 `incrementUnread`（收到消息时触发），如果在短时间内连续触发多次，
可以考虑用 `QTimer::singleShot(0, ...)` 延迟合并更新：

```cpp
void ContactListWidget::incrementUnread(int id, const QString& type, int count) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] += count;

    // 延迟合并：50ms 内多次 incrementUnread 只触发一次更新
    if (!m_updatePending) {
        m_updatePending = true;
        QTimer::singleShot(50, this, SLOT(flushPendingUpdates()));
    }
}

void ContactListWidget::flushPendingUpdates() {
    m_updatePending = false;
    // 这里的简单更新不需要 diff（排序不受未读数影响）
    for (const auto& [key, count] : m_pendingUnreadChanges) {
        simpleUpdateItem(key.first, qFromUtf8(key.second));
    }
    m_pendingUnreadChanges.clear();
}
```

但这增加了复杂度。初期可以先不加延迟合并，直接每步单行 update。

## 10 文件改动清单

### contactlist.h

| 改动 | 说明 |
|------|------|
| +`int findItemById(int id, const QString& type) const` | 在 QListBox/QListWidget 中查找项 |
| +`void updateItemAt(int index, Contact* c)` | 更新单行数据 |
| +`void removeItemAt(int index)` | 删除单行 |
| +`void insertItemAt(int index, Contact* c)` | 在位置插入 |
| +`bool hasItemDataChanged(int oldIndex, Contact* newData)` | 检查数据是否变化 |
| +`void restoreSelectionAndScroll()` | 恢复选中+滚动 |
| +`bool m_updatePending` | 延迟合并标记（可选） |

### contactlist.cpp

| 改动 | 说明 |
|------|------|
| 修改 `updateView_v3()` | 改为 diff 模式，保留全量分支 |
| 修改 `updateView_v4()` | 同上 |
| 修改 `updateFriendName()` | 改为调用 diff 或 simpleUpdate |
| 修改 `updateFriendConnectionStatus()` | 同上 |
| 修改 `updateContact()` | 同上 |
| 修改 `addContact()` | 改为 insert at sorted position |
| 修改 `removeContact()` | 改为 single item remove |
| 修改 `updateContactLastMessage()` | 可能触发位置变化 → diff |
| 修改 `incrementUnread()` | 直接改 unread data + repaint |
| 修改 `resetUnread()` | 同上 |
| 新增 `computeDiff()` | 三路 diff 算法 |
| 新增 `applyDiffQt3()` | Qt3 执行 diff |
| 新增 `applyDiffQt4()` | Qt4 执行 diff |
| 修改 `onSearchTextChanged()` | 保留全量重建 |
| 修改 `onSortMenuClicked()` | 保留全量重建 |
| 修改 `retranslateUi()` | 保留全量重建 |

### 不修改的文件

- `mainwindow.cpp` — 接口不变，调用方无需感知
- `mainwindow.h` — 不需改动

## 11. 实施步骤

### 第 1 天：添加 helper + Qt4 增量

1. `contactlist.h` 添加 5 个 helper 声明
2. `contactlist.cpp` 实现 `findItemById`、`updateItemAt`、`removeItemAt`、`insertItemAt`
3. 实现 Qt4 的 `computeDiff` + `applyDiffQt4`
4. 修改 `updateView_v4` 使用 diff（非全量路径）
5. `buildqt4.sh` 编译验证

### 第 2 天：Qt3 兼容 + ContactListItem::updateData

1. `ContactListItem` 添加 `updateData()` 方法
2. 实现 Qt3 的 `computeDiff` + `applyDiffQt3`
3. 修改 `updateView_v3` 使用 diff
4. `buildqt3.sh` 编译验证

### 第 3 天：逐个修改 13 个调用点

1. 简单更新（unread、name、status 不改排序的）→ `simpleUpdateItem`
2. 可能需要改排序的（lastMessage、lastActive）→ 触发 diff
3. 全量保留的（search、sort、retranslate）→ 保持原样

### 第 4–5 天：测试+调试

1. Qt3 编译 + 功能测试
2. Qt4 编译 + 功能测试
3. 发送消息→增量更新位置到顶部
4. 改名后 name 排序→验证位置变化
5. 搜索→全量重建验证
6. 空列表→不崩溃
7. 快速批量收到消息→不闪

### 第 6 天：缓冲

1. 修复边缘问题
2. 性能测量（确保增量确实比全量快）

## 12. 风险点

| 风险 | 严重度 | 应对 |
|------|--------|------|
| Qt3 QListBox 没有 replaceItem，频繁 remove+insert 可能导致索引漂移 | 中 | 使用 `updateData()` 避免销毁重建 |
| 排序变化 + 增量插入的组合逻辑复杂 | 高 | 对排序变化场景走全量重建，保守处理 |
| 选中项在 diff 中丢失 | 中 | diff 完后按 identity 重新选中 |
| Qt3/Qt4 行为差异 | 中 | 两个版本各自实现 applyDiff |

## 13. 验证方式

```bash
# Qt3 编译
cd qltox && bash buildqt3.sh

# Qt4 编译
cd qltox && bash buildqt4.sh

# 运行测试
# 验证：发送一条消息 → 联系人和最后消息更新到顶部，无闪烁
# 验证：收到多条消息 → 未读数递增，列表不动，无闪烁
# 验证：搜索 → 全量重建，无问题
# 验证：切换排序 → 全量重建，无问题
```

## 14. 时间估算汇总

| 阶段 | 时间 |
|------|------|
| Qt4 增量 | 2 天 |
| Qt3 兼容 | 1 天 |
| 13 个调用点改造 | 1 天 |
| 测试调试 | 2 天 |
| 缓冲 | 0.5 天 |
| **总计** | **6–6.5 天** |
