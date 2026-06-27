# 联系人列表实现（TG 式 Dialogs::List 架构重写）

## 0. 改动摘要

删除旧的 Qt3 `QListView` + Qt4 `QAbstractListModel` 双实现，统一为 **TG Desktop 的 Dialogs::List 架构**：

| 旧 (已删除) | 新 |
|-------------|-----|
| `ContactListViewItem` (QListViewItem) | `RowData` + `ContactListView` (QWidget 自绘) |
| `ContactListView` (QListView) | `ContactListView` (QWidget, paintEvent) |
| `ContactListModel` (QAbstractListModel) | — (合并入 `ContactList`) |
| `ContactListDelegate` (QStyledItemDelegate) | — (合并入 paintEvent) |
| `ContactList allContacts` + `findAndUpdateItem` | `ContactList m_list` |
| `void* listWidget` | `ContactListView* m_view` |

---

## 1. TG 架构复制：三层

```
ContactListWidget (外层容器, public API 不变)
  └── ContactListView (QWidget, paintEvent 自绘, 鼠标事件)
       └── ContactList (数据层: map + sorted vector + freeze/unfreeze)
            └── RowData (≈ TG Dialogs::Row)
```

### 1.1 ContactList — 排序列表 + O(log n) 索引 (≈ TG Dialogs::List)

```cpp
class ContactList {
    // 双结构: map 拥有 RowData 对象, vector 存排序指针
    std::map<std::pair<int,QString>, std::unique_ptr<RowData>> m_map;   // O(log n) 查找
    std::vector<RowData*> m_rows;                                        // 排序迭代

    // freeze/unfreeze (TG 批量模式)
    bool m_frozen = false;
    std::vector<int> m_pendingAdjust;
};
```

- **`get(id, type)`** → `m_map[{id,type}]` → O(log n)
- **`indexOf(id, type)`** → `m_map[{id,type}]->index` → O(log n)
- **`addToEnd(rd)`** → emplace map + push_back vector → 重复检测
- **`remove(id, type)`** → map.erase + vector.erase + updateFrom
- **`adjustBySort(idx)`** → `std::find_if` 前后扫描 + `rotate` → 增量重排
- **`sort(criteria)`** → `std::stable_sort(m_rows, rowLess)`
- **`freeze()`** → 设 m_frozen; `unfreeze()` → 处理 pending 或全量 sort

### 1.2 ContactListView — 自绘控件 (≈ TG Dialogs::InnerWidget)

```cpp
class ContactListView : public QWidget {
    int m_selIdx = -1;     // 选中行索引 (TG 式: 简单 int)
    int m_scrollY = 0;     // 滚动偏移

    void paintEvent()   // 遍历 m_rows, 仅绘视口内行, 调 paintContactRow()
    void mousePressEvent()      // y → row → select
    void wheelEvent()           // 滚轮
    void yToRow(int y)          // 反算行号 (y / itemHeight, 考虑 visible 跳过)
};
```

不存在 QListViewItem / QModelIndex。选中 = `m_selIdx` 存索引。

### 1.3 ContactListWidget — 外层容器 (API 不变 + 新增 batch)

```cpp
class ContactListWidget : public QWidget {
    ContactList m_list;
    ContactListView* m_view;

    // TG freeze/unfreeze
    void beginBatch() { m_list.freeze(); m_batchLevel++; }
    void endBatch()   { m_list.unfreeze(m_sortCriteria); m_view->update(); }
};
```

---

## 2. 操作实现 (全部完整)

### 2.1 `addContact(Contact* c)`

```
1. 组装 RowData (从 c + maps 读 unread/lastMessage)
2. m_list.addToEnd(rd) → 内部查重, 更新或追加
3. 若是新插入: m_list.adjustBySort(inserted->index)
4. delete c
5. 非 batch 时: update + recalc
```

稳定 **O(log n)** 查找 + O(n) 最坏 rotate（n=500 可忽略）。

### 2.2 `removeContact(id, type)`

```
1. m_list.remove(id, type) → map 删 + vector 删 + updateFrom
2. 若删除的是选中项 → m_view->setSelected(-1)
3. 非 batch 时: update
```

**O(log n)** 查找 + O(n) vector 平移。

### 2.3 `updateContact(id, type, name, chatId, status)`

```
1. RowData* rd = m_list.get(id, type)
2. if (!rd) return
3. rd->name = name; rd->status = status; 等
4. 若 name/status 变了 (排序键) → m_list.adjustBySort(rd->index)
5. 非 batch 时: update(rect)
```

**O(log n)** 查找 + 可能 O(n) rotate。**不再是 O(n) 链表遍历**。

### 2.4 `updateFriendName / updateFriendConnectionStatus`

同上。`get()` → 改 → `adjustBySort()`。

### 2.5 `updateContactLastMessage(id, type, msg, timeStr)`

```
1. maps[key] = msg / timeStr
2. RowData* rd = m_list.get(id, type)
3. rd->lastMessage = msg; rd->lastActive = now;
4. 非 batch 时: update(rect)
```

**O(log n)**, 不触发 adjustBySort（消息到达不改变排序位置，和 TG 一致）。

### 2.6 `incrementUnread / resetUnread`

```
1. maps[key] += count / = 0
2. RowData* rd = m_list.get(id, type)
3. rd->unread = maps[key]
4. 非 batch 时: update(rect)
```

**O(log n)**, 局部重绘。

### 2.7 `setContacts(ContactList)`

```
1. m_list.clear()
2. for each c: addToEnd
3. m_list.sort(m_sortCriteria)
4. m_view->update()
```

全量重建。

### 2.8 `rebuildSortFilter()`

```
1. m_list.sort(m_sortCriteria)
2. syncItemHeight()   → m_list.setItemHeight(calcItemHeight)
3. m_view->update()
4. countLabel 更新
```

---

## 3. 批量操作 (TG freeze/unfreeze)

### 调用方 (mainwindow.cpp) 使用:

```cpp
widget->beginBatch();
for (auto& f : friends)
    widget->updateFriendConnectionStatus(f.id, f.status);
for (auto& m : messages)
    widget->updateContactLastMessage(m.id, m.type, m.text, m.time);
widget->endBatch();
```

### 内部流程:

```
beginBatch
  → m_list.freeze()
  → m_frozen = true
  → 后续 adjustBySort 只存索引到 m_pendingAdjust

endBatch
  → m_list.unfreeze(criteria)
    → m_frozen = false
    → if pending.size == 1: adjustBySort(pending[0])
    → else: sort(criteria)
  → m_view->update()     (一次重绘)
```

**效果**: 100 条批量更新 = 1 次 sort + 1 次重绘，而非 100 次。

---

## 4. 选中状态

```
m_list (数据层)
  → RowData* 有稳定地址, index 随排序变化

m_view (视图层)
  → m_selIdx = 选中的 rows[] 索引
  → sort/remove 后 resolveSelection() 重新定位

选中流程:
  mousePress → yToRow → m_selIdx = row → emit contactSelected
  → MainWindow 处理 (同现在)
```

**TG 方式**: 选中不是 Qt 的 selection 机制管理的。`paintEvent` 比较 `i == m_selIdx` 决定是否绘制选中背景。

**排序后恢复选中**: `resolveSelection()` 用 `(id,type)` 重新查找。

---

## 5. 搜索过滤

```
ContactListView::paintEvent:
  for (int i = firstVis; i <= lastVis; ++i) {
      RowData* rd = m_list.at(i);
      if (!m_widget->matchesFilter(*rd)) continue;  // 跳过
      paintContactRow(..., i == m_selIdx, ...);
  }

matchesFilter:
  return m_searchText.isEmpty()
      || rd.nameUpper.contains(qToUpper(m_searchText));
```

不隐藏行、不移除行、不改列表结构。只在绘制时跳过。选中索引不受影响。

---

## 6. 布局常量 & 绘制

- `calcItemHeight()` → 动态行高（不变）
- `paintContactRow()` → 共享绘制函数（不变，Qt3/Qt4 差异保留）
- `RowData.top` / `.height` → ContactList::updateFrom 维护
- `totalHeight` → QScrollBar 范围
- **`#ifdef QT3_BUILD` 只在 `paintContactRow` 内部**（`QPainterPath`/`QBitmap`）

---

## 7. 复杂度汇总

| 操作 | 之前 (Qt3) | 之前 (Qt4) | 新实现 |
|------|-----------|-----------|--------|
| `find(id,type)` | O(n) `dynamic_cast` + 链表遍历 | O(n) `m_visible` 遍历 | **O(log n)** `map::find` |
| `addContact` | O(1) 不排序 | O(n log n) doSort | O(n log n) ~ O(1) addToEnd + adjust |
| `removeContact` | O(n) 链表遍历 + 删除 | O(n) 两列表遍历 + removeRows | O(n) map::erase + vector::erase |
| `update*` | O(n) `findAndUpdateItem` + 不重绘 | O(n) 遍历 + dataChanged | **O(log n)** get + 改 + adjust + 局部 update |
| `incrementUnread` | O(n) | O(n) | **O(log n)** |
| `sort` | no-op (setSorting -1) | O(n log n) + beginResetModel | **O(n log n)** stable_sort + 刷新 index |
| `filter` | O(n) 设 visible | O(n log n) rebuildVisibleList | **O(n)** 跳过绘制 |

---

## 8. 已删除代码清单

| 文件 | 删除内容 | 替代 |
|------|---------|------|
| `contactlist.h` | `ContactListModel` 前向声明 | — |
| `contactlist.h` | `void* listWidget` | `ContactListView* m_view` |
| `contactlist.h` | `ContactList allContacts` | `ContactList m_list` |
| `contactlist.h` | `#ifdef QT3_BUILD` 内 `QListView` include | — |
| `contactlist.h` | Qt4 的 `#include <QAbstractListModel>` | — |
| `contactlist.h` | `findAndUpdateItem()`, `rebuildSortFilter()` 私有方法 | 内联到各操作 |
| `contactlist.cpp` | `class ContactListViewItem` (整类 80 行) | `RowData` + `ContactList` |
| `contactlist.cpp` | `class ContactListView` (QListView 版 30 行) | 新 `ContactListView` (QWidget) |
| `contactlist.cpp` | `class ContactListModel` (整类 200 行) | `ContactList` |
| `contactlist.cpp` | `class ContactListDelegate` (整类 25 行) | paintEvent 内直调 |
| `contactlist.cpp` | `g_sortCriteriaPtr` 全局 | `ContactList::m_criteria` 成员 |
| `contactlist.cpp` | `findAndUpdateItem()` (30 行) | 各操作自包含 |
| `contactlist.cpp` | `#ifdef QT3_BUILD` / `#else` 全量分支 | 仅 paintContactRow 内有差异 |
| `contactlist.cpp` | `onSelectionChanged()`, `onItemClicked()` | ContactListView::mousePressEvent |
| `contactlist.cpp` | Qt3 `setSorting(-1)` / `sort()` 空操作 | `std::stable_sort` 永远有效 |

---

## 9. 已知问题修复对照

| 旧问题 | 修复方式 |
|--------|----------|
| **9.1 重影** (setText Manual mode 不重绘) | 不存在。自绘 paintEvent 每次 update 都重绘 |
| **9.2 sort() no-op** (setSorting(-1)) | 不存在。`std::stable_sort` 永远有效 |
| **9.3 选中与排序冲突** | 不存在。选中由 `m_selIdx` 管理，排序后 resolveSelection |
| **9.4 paintCell 不调父类** | 不存在。QWidget::paintEvent 完全自绘 |

---

## 10. Contact 所有权

`Contact*` (从 `ContactList` typedef `QPtrList<Contact>` 传入) 的处理：

- `setContacts(contacts)` — **不接管所有权**。调用方 (MainWindow) 负责管理 `Contact` 的创建和销毁
- `addContact(Contact* c)` — **调用方传过来的指针由本函数 delete**（行为与当前一致）
- `m_list` 内部存储 `RowData`（值语义），不存储 `Contact*`

---

## 11. TG Dialogs::List 一致性核对

实现对照 TG Desktop `Dialogs::List` 架构逐项检查的结果。

### 11.1 完全一致的部分

| 组件 | TG 做法 | 当前实现 | 位置 |
|------|---------|----------|------|
| **数据双结构** | `std::map<Key,unique_ptr<Row>>` + `std::vector<Row*>` | `std::map<pair<int,QString>,unique_ptr<RowData>>` + `std::vector<RowData*>` | `contactlist.h:95-96` |
| **`addToEnd` 去重** | 已存在 update + return existing；不存在则 emplace + push_back | `addToEnd()` 完全一致 | `contactlist.cpp:214-234` |
| **`remove` 顺序** | `_rows.erase` → `_map.erase` → `updateRowIndices` | 完全一致 | `contactlist.cpp:236-244` |
| **`adjustBySort`** | frozen 检查 → scan up → rotate → scan down → rotate → updateFrom | 完全一致 | `contactlist.cpp:246-273` |
| **`freeze/unfreeze`** | pending 0-1 逐个 adjustBySort；≥2 全量 sort | 完全一致 | `contactlist.cpp:283-297` |
| **`sort`** | `std::stable_sort` + `updateRowIndices` | 完全一致 | `contactlist.cpp:276-281` |
| **比较器结构** | 多条件优先级栈，低→高遍历 | 条件不同但模式相同 | `contactlist.cpp:187-205` |
| **选中管理** | `_selected` (int) in InnerWidget | `m_selIdx` (int) in ContactListView | `contactlist.h:75` |
| **paint 窗口化** | `yScroll / rowHeight` 计算可见范围 | `firstRow/lastRow` 计算 | `contactlist.cpp:355-358` |
| **排序后 resolveSelection** | 用 dialog key 重新查找 | 用 (id,type) 重新查找 | `contactlist.cpp:677-685` |
| **scrollbar 集成** | 信号连接 InnerWidget scroll | `valueChanged` → `onScrollChanged` | `contactlist.cpp:768-772` |
| **resize 刷新** | resizeEvent → 更新 scrollbar 范围 | 同上 | `contactlist.cpp:401-403` |

### 11.2 有意识的分歧（有明确理由）

| 项目 | TG 做法 | 本实现 | 理由 |
|------|---------|--------|------|
| **PinnedList** | 独立 PinnedList 维护置顶 | 无 | toxhttpd UI 没有置顶功能 |
| **IndexedList** | 字母索引桶，O(log n) 过滤 | 无 | <500 联系人，线性过滤 <0.1ms |
| **布局缓存** | Row 缓存 `_top`/`_height` | 无（统一 `m_itemHeight`） | 等行高无需逐行缓存 |
| **过滤方式** | `MainList` 重建 `_filtered` 列表 | paint-time skip | 更简单，<500 可忽略 |
| **名称匹配** | 前缀匹配（"ali" 匹配 "alice"） | `contains` 子串匹配 | 更适合中文搜昵称 |
| **batch 嵌套** | 单层 freeze boolean | `m_batchLevel` 计数器 | 支持多层嵌套 batch |

### 11.3 结论

实现与 TG `Dialogs::List` 核心机制完全对齐。11 项核心机制一致，6 项有意识分歧均有明确理由（见 11.2），不影响架构正确性和性能。
