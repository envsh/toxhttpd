# Qt3 Tooltip 修复记录

## 问题

Qt3 下消息头部 translate/source 按钮的 tooltip 不随 hover 更新，停留在第一个按钮上直到 3s 超时后才消失。

## 根因

1. **`WType_Popup` 抢事件** — 旧的 `TooltipManager` 用 `WType_Popup` 创建弹出窗口。Qt3 中 `WType_Popup` 会截获鼠标事件（类似 XGrabPointer），导致父窗口 `ChatView` 在 tooltip 弹出期间收不到任何 `mouseMoveEvent`。鼠标移到第二个按钮时 ChatView 不知情，tooltip 无法更新。
2. **缺少 `setMouseTracking(true)`** — 即使 tooltip 没弹出，`mouseMoveEvent` 也只在按下鼠标按键时触发，hover 时不会。

## 改动

### 1. `qltox/chatview.cpp` — 开启鼠标跟踪

第 69 行添加：

```cpp
setMouseTracking(true);
```

无 `#ifdef`，Qt3/Qt4 共享。

### 2. `qlcomp/compat34.cpp` — 移植 Qt4 QTipLabel

删除旧的 `TooltipManager`（`QObject` 基类、`WType_Popup` 窗口、eventFilter 装在 parent widget 上），替换为 `TipLabel`（`QLabel` 基类、移植自 Qt4 的 `QTipLabel` 实现）。

| 特性 | 旧 TooltipManager | 新 TipLabel |
|------|-------------------|-------------|
| 基类 | QObject | QLabel |
| 窗口标志 | `WType_Popup \| WStyle_StaysOnTop` | `WStyle_Customize \| WStyle_Tool \| WStyle_NoBorder \| WStyle_StaysOnTop` |
| eventFilter | 装在 parent ChatView 上 | 装在 `qApp` 上（全局） |
| 刷新策略 | `close()` + 重建 | `setText()` + `adjustSize()` 原地复用 |
| 隐藏延迟 | 无，直接关 | 300ms `hideTimer`（匹配 Qt4） |
| 离开检测 | eventFilter 只在 ChatView 上有 MouseMove 时触发 | qApp 全局 MouseMove + TipLabel 自身 mouseMoveEvent |
| 定时器 | `QObject::startTimer` | expireTimer + hideTimer 两个独立定时器 |

#### 关键实现细节

**构造** (`TipLabel::TipLabel`):
- 父窗口为 0（顶层窗口），避免被 ChatView 裁剪
- 设置黄色背景 (`QColor(255, 220, 220)`) + 黑色文字 + `QFrame::Box` 边框
- `qApp->installEventFilter(this)` + `setMouseTracking(true)`
- 单例模式：`delete instance; instance = this;`

**showTip**:
- 检测 text/widget/rect 是否变化，变化时 `setText` + `adjustSize` + `placeTip` + `show`
- 未变化时仅重启 expireTimer
- `restartExpireTimer` 会取消 hideTimer（防快速移动时的闪烁）

**eventFilter** (qApp 级别):
- `MouseMove`: 检查 `me->globalPos()` 是否在 `m_trackingRect` 外 → 调用 `hideTip()`（启动 300ms hideTimer）
- `MouseButtonPress/Release/DblClick`、`Wheel`、`KeyPress/KeyRelease`: `hideTipImmediately()`
- `Leave`: `hideTip()`

**placeTip**:
- 定位到按钮中心正上方 4px
- 用 `QApplication::desktop()->screenGeometry()` 做屏幕边界裁剪

**hideTip / hideTipImmediately**:
- `hideTip` 启动 300ms hideTimer（防短暂离开）
- `hideTipImmediately` 关窗口、清所有定时器和状态
- expireTimer 触发时也会调 `hideTip`（优雅隐藏），而非直接关

#### 额外修正

- X11 `<X11/Xlib.h>` 的 `KeyPress`/`KeyRelease` 宏会污染 `QEvent::KeyPress`/`KeyRelease` 枚举值 → 在 include 后加 `#undef`
- `QPalette::Background` 在 Qt3 中不存在 → 改用 `QPalette::setColor(QPalette::Active, QColorGroup::Background, ...)` 三组分别设置

## 事件流

```
cursor 移到 translate 按钮
→ ChatView::mouseMoveEvent (因 setMouseTracking 触发)
→ showTempTooltip("Translate")
→ TipLabel::showTip → 设文本/定位/显示/启 expireTimer

cursor 移到 source 按钮
→ ChatView::mouseMoveEvent 再次触发
→ showTempTooltip("Source")
→ TipLabel::showTip → text 变了 → 原地更新文本/重定位/重启 expireTimer → tooltip 立即切换

cursor 离开按钮区域
→ qApp eventFilter 捕获 MouseMove
→ globalPos 不在 m_trackingRect → hideTip() → 300ms hideTimer → 自动隐藏
→ 若 300ms 内回到按钮 → showTip 重启 expireTimer + 取消 hideTimer → 不闪烁
```

## 影响范围

只改了两个文件：
- `qltox/chatview.cpp`（1 行）
- `qlcomp/compat34.cpp`（替换 ~90 行旧代码 + 添加 include/undef）

Qt4 侧的 `QToolTip::showText` 调用完全不受影响。

## 构建验证

- `bash buildqt3.sh` — 通过
- `bash buildqt4.sh` — 通过
