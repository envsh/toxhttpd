# ChatView 性能调优记录

## 背景

qltox ChatView 基于 `QWidget` 自绘实现，与 tdesktop 的 `HistoryInner` 架构思路一致（自定义 QWidget + 逐消息布局缓存 + paint 自绘）。但在实际使用中，**界面闪烁和卡顿明显**，远不如 tdesktop 流畅。本文记录根因分析和修复方案。

---

## 根因：3 个致命差异

### 1. 滚动模型差异（占 90% 闪烁）

| | tdesktop | qltox |
|---|---|---|
| 滚动方式 | `QScrollArea` + 内部 `HistoryInner` | 裸 `QWidget` + 手动 `LimeScrollBar` |
| 滚动时重绘 | Qt 自动做 **bit blit 内容移位**，只画**新露出的一条细带**（~像素级） | `onScrollChanged` → **`updateFull()`** → 全 widget 重绘 |
| 每帧绘制量 | 新露出的一窄条（~10-30px 高） | 整个 viewport（几百×几千像素） |

qltox 每次滚动都触发 `onScrollChanged` 调 `updateFull()`，强制所有可见消息重绘。tdesktop 利用 `QScrollArea` 内置的 `scrollContentsBy()`，Qt 底层做像素移位（bit blit），只在新露出区域执行 `update()`。

### 2. 缺少 `WA_OpaquePaintEvent`

```cpp
// 构造函数未设置
// setAttribute(Qt::WA_OpaquePaintEvent);
```

- 不设此 flag → Qt 在 `paintEvent` 前用 widget 背景色系统级擦除
- 又在 `paintEvent` 内做 `p.fillRect(event->rect(), windowBg)` → **双重重绘**
- 经历：系统擦除 → fillRect → 画消息 → 画 scrollbar → 画 pill，中间步骤肉眼可见

### 3. Scrollbar 动画叠加刷新

`LimeScrollBar::showTemporarily()` → fade timer 每 30ms 触发 → 每帧调 `scrollbar->update()`，与 `chatview->updateFull()` 叠加 → GPU 负载翻倍，VSync 不稳定。

---

## 次要问题

| 问题 | 影响 |
|------|------|
| `manageAnimations()` 在 `paintEvent` 开头调用 | 每次重绘都遍历 GIF 可见性（虽已优化为块查找，仍是额外遍历） |
| `p.fillRect(event->rect(), ...)` 清全屏再画 | 即使设了 OpaquePaintEvent，依然是 fill+paint 两阶段开销 |

---

## 与 tdesktop 的滚动路径对比

```
qltox 滚动路径（每 tick）：
  LimeScrollBar::valueChange()
    → showTemporarily()                 ← 启动 fade 每 30ms 更新 scrollbar
    → emit valueChanged(int)
      → onScrollChanged(int)
        → updateFull()                  ← 全 widget 重绘
        → triggerVisibleDownloads()
          → visibleMessageRange()
            → findByAbsY()
        → scrollbar->showTemporarily()
          → scrollbar->update()         ← scrollbar 独自又重绘一次

tdesktop 滚动路径（每 tick）：
  QScrollBar::valueChanged
    → QScrollArea::scrollContentsBy(dx, dy)
      → bit blit 移位视口内容          ← 微秒级
      → viewport->update(新露出条带)    ← 极小矩形
```

---

## 无法移植的 tdesktop 高级特性

### 1. RpWidget 双/三缓冲渲染引擎

tdesktop 的 `RpWidget` → `RpPaintBuffer` → `QImage` 离屏渲染 + 条件性 `bitBlt` 管线。在 Qt 原生双缓冲上再加应用层缓冲，完全自控绘制时机。**移植需重写 widget 基类，代码量数千行，且与 Qt3 的 QWSServer/qpaintengine 交互不可控。**

### 2. 线程化离屏预渲染

tdesktop 在后台线程将消息渲染到 `QImage`，主线程只做一次 `bitBlt`。QPainter 非线程安全，Qt3 下跨线程图像操作脆弱。**可做但工程成本高、收益递减。**

### 3. QPainterPath 字形缓存

富文本布局预计算为 `QPainterPath` 缓存，避免重复 shaping。依赖 `QTextLayout`（Phase 3 待实现）。**非架构限制，仅未实现。**

### 4. OpenGL 合成加速

Qt3 无 `QOpenGLWidget`，Qt4 支持有限。tdesktop 移动端用 OpenGL 做垂直同步合成。**此路不通。**

---

## 可移植方案（消除剩余闪烁）

| 方案 | 效果 | Qt3 兼容 |
|------|------|----------|
| **`WA_OpaquePaintEvent`** | Qt 不在 paintEvent 前擦除背景 → 去掉"清屏闪" | `setWFlags(WPaintCleared)` / `setAttribute(Qt::WA_OpaquePaintEvent)` |
| **`WA_StaticContents`** | Qt 优化已知内容区域，只重绘 damage rect | Qt3 也有 |
| **`QWidget::scroll()` 增量滚动** | bit blit 替代全重绘，每帧只画新露出条带 | `scroll(0, delta, rect())` Qt3 存在 |
| **在 paintEvent 入口裁剪** | 已设 clip rect，但可加事件 rect 跳过完全不可见块 | 已实现 |

### P0 — 增量滚动（消除 90% 闪烁）

`onScrollChanged` 不做 `updateFull()`，改为：

```cpp
void ChatView::onScrollChanged(int value) {
    int delta = m_scrollPos - value;
    m_scrollPos = value;
    QWidget::scroll(0, delta, rect());  // bit blit 移位
    if (delta > 0) {
        // 向上滚动，上端露出新区域
        updateRect(QRect(0, 0, contentWidth(), delta));
    } else if (delta < 0) {
        // 向下滚动，下端露出新区域
        updateRect(QRect(0, height() + delta, contentWidth(), -delta));
    }
    // pill、scrollbar 更新不变
    ...
}
```

### P1 — OpaquePaintEvent

```cpp
// 构造函数
setAttribute(Qt::WA_OpaquePaintEvent);
// Qt3 等效：setWFlags(getWFlags() | WPaintCleared);
```

### P2 — 管理 GIF 动画可用独立 timer

`manageAnimations` 移出 `paintEvent`，改为每 200ms 的 `QTimer` 驱动，降低 paint 入口开销。

---

## 阶段成果

### 已完成

- 块索引（`MsgBlock` / `kBlockSize=50`）实现 O(log n) 二分查找，消除 6 处 O(n) 全量遍历
- `updateRect()` / `updateFull()` 分离策略，局部修改用增量矩形
- image preview 固定尺寸 260×260

### 待完成

- `QWidget::scroll()` 增量滚动（P0）
- `WA_OpaquePaintEvent`（P1）
- GIF timer 分离（P2）
- Phase 3: `QTextLayout` 富文本渲染
- Phase 4: 嵌入媒体（MediaItem）
