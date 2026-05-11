# LimeStyle — 4 风格 ×2 色调自定义 QStyle 方案

## 架构

```
g_activeParams (StyleParams*) ← ThemeManager::setStyle(StyleId, bool dark)
       │
       ├── LimeStyle (QStyle/QProxyStyle)  → drawControl/drawPrimitive/drawComplexControl
       ├── LimeScrollBar (QScrollBar)      → 淡入淡出 + 3s 自动隐藏
       ├── QPalette (qApp->setPalette)     → 全局颜色
       └── ChatView/widgets                → 颜色从 g_activeParams 读取
```

## StyleParams 结构

```cpp
struct StyleParams {
    struct Palette {
        QColor windowBg;       // Background/Window
        QColor surfaceBg;     // Button
        QColor baseBg;        // Base（输入框、列表）
        QColor hoverBg;       // hover 态
        QColor activeBg;      // pressed 态
        QColor textPrimary;   // 主文字
        QColor textMuted;     // 次要文字
        QColor textDisabled;  // 禁用文字
        QColor accent;        // 强调/选中色
        QColor accentText;    // 选中文字
        QColor border;        // 边框
        QColor borderFocus;   // 聚焦边框
        QColor link;          // 链接
        QColor scrollbarSlider;
        QColor scrollbarHover;
    };
    Palette dark;
    Palette light;

    // 几何与行为（暗亮共用）
    int    buttonRadius;
    int    inputRadius;
    int    scrollbarWidth;
    int    spacing;
    int    touchTarget;
    enum ScrollbarMode { AlwaysFaint, OverlayFade } scrollbarMode;
    enum ButtonStyle  { Flat, Border, Capsule, Elevated } buttonStyle;
};

enum StyleId { StyleQtFusion, StyleMacOS, StyleWindows, StyleMaterial };

extern const StyleParams* g_activeParams;
```

## 4 风格 ×2 色调配色

### 1. QtFusion（VS Code / GitHub Desktop 风格）

| 角色 | Dark | Light |
|------|------|-------|
| windowBg | `#0d1117` | `#f0f0f0` |
| surfaceBg | `#21262d` | `#ffffff` |
| baseBg | `#161b22` | `#ffffff` |
| hoverBg | `#30363d` | `#e5e5e5` |
| activeBg | `#3d444d` | `#d0d0d0` |
| textPrimary | `#c9d1d9` | `#1a1a1a` |
| textMuted | `#8b949e` | `#666666` |
| accent | `#1f6feb` | `#0969da` |
| accentText | `#ffffff` | `#ffffff` |
| border | `#30363d` | `#c0c0c0` |

- radius: 6, scrollbar: 8px + AlwaysFaint, button: Flat

### 2. macOS Sequoia（系统偏好设置风格）

| 角色 | Dark | Light |
|------|------|-------|
| windowBg | `#323232` | `#ededed` |
| surfaceBg | `#3a3a3c` | `#ffffff` |
| baseBg | `#1e1e1e` | `#ffffff` |
| hoverBg | `#48484a` | `#e8e8e8` |
| activeBg | `#555557` | `#d4d4d4` |
| textPrimary | `#ffffff` | `#1d1d1f` |
| textMuted | `#8e8e93` | `#6c6c70` |
| accent | `#007aff` | `#007aff` |
| accentText | `#ffffff` | `#ffffff` |
| border | `#48484a` | `#d2d2d7` |

- radius: capsule (height/2), scrollbar: 4px + OverlayFade, button: Capsule

### 3. Windows 11（设置应用风格）

| 角色 | Dark | Light |
|------|------|-------|
| windowBg | `#202020` | `#f9f9f9` |
| surfaceBg | `#2c2c2c` | `#ffffff` |
| baseBg | `#1f1f1f` | `#ffffff` |
| hoverBg | `#3d3d3d` | `#eaeaea` |
| activeBg | `#484848` | `#d6d6d6` |
| textPrimary | `#ffffff` | `#1a1a1a` |
| textMuted | `#a0a0a0` | `#616161` |
| accent | `#60cdff` | `#0078d4` |
| accentText | `#000000` | `#ffffff` |
| border | `#555555` | `#d2d2d2` |

- radius: 4, scrollbar: 7px + AlwaysFaint (hover expand 10px), button: Border

### 4. Material You（Android 15 基线）

| 角色 | Dark | Light |
|------|------|-------|
| windowBg | `#1c1b1f` | `#fffbfe` |
| surfaceBg | `#2b2930` | `#f3edf7` |
| baseBg | `#1c1b1f` | `#fffbfe` |
| hoverBg | `#3b3940` | `#e8e3ec` |
| activeBg | `#48464d` | `#dbd6e0` |
| textPrimary | `#e6e1e5` | `#1c1b1f` |
| textMuted | `#cac4d0` | `#79747e` |
| accent | `#d0bcff` | `#6750a4` |
| accentText | `#381e72` | `#ffffff` |
| border | `#938f99` | `#79747e` |

- radius: 16, scrollbar: 4px + OverlayFade, button: Capsule, touchTarget: 44px, spacing: 16px

## QPalette 映射

```cpp
pal.setColor(Background, s->windowBg);
pal.setColor(Foreground, s->textPrimary);
pal.setColor(Base,       s->baseBg);
pal.setColor(Text,       s->textPrimary);
pal.setColor(Button,     s->surfaceBg);
pal.setColor(ButtonText, s->textPrimary);
pal.setColor(Highlight,  s->accent);
pal.setColor(HighlightedText, s->accentText);
pal.setColor(Link,       s->link);
pal.setColor(Light,      s->hoverBg);
pal.setColor(Midlight,   lerp(surfaceBg, hoverBg, 0.5));
pal.setColor(Mid,        s->surfaceBg);
pal.setColor(Dark,       lerp(windowBg, surfaceBg, 0.3));
pal.setColor(Shadow,     s->windowBg);       // = 背景 → 3D 消失
pal.setColor(BrightText, QColor("#ffffff"));
```

## LimeStyle 覆盖方法

```
drawPrimitive
├── PE_PanelLineEdit / PE_FrameLineEdit → 圆角 background + border + focus 色
├── PE_FocusRect / PE_FrameFocusRect    → 跳过（不画虚线框）
└── PE_ButtonCommand / PE_PanelButtonCommand → 按钮扁平背景（参考 CE_PushButton）

drawControl
├── CE_PushButton → 圆角矩形 + 状态色 (normal/hover/pressed/disabled)
├── CE_PopupMenuItem / CE_MenuItem → 背景 + 高亮 + 分隔线
├── CE_CheckBox → 保持默认（palette 控制）
└── CE_CheckBoxLabel → 保持默认

drawComplexControl
├── CC_ScrollBar → 窄滑块 + 圆角（视觉计算，滑块颜色由 LimeScrollBar 控制）
└── CC_ComboBox  → 按钮面圆角 + 下拉箭头 ▲/▼（展开时朝上）

pixelMetric
├── PM_ScrollBarExtent → scrollbarWidth
├── PM_ButtonMargin    → 6
└── PM_DefaultFrameWidth → 1
```

## LimeScrollBar 行为

```
空闲: opacity 0.0 (透明)        → paintEvent 不绘制
滚动/按键: showTemporarily()    → fadeIn 300ms → opacity 1.0
                               → 重启 3s QTimer
超时/leaveEvent: fadeOut 300ms → opacity 0.0

滑块计算: 8px 宽, 圆角 4px, 上下留 4px 边距
颜色: slider=kBgSurface, hover=kBgHover, pressed=kBgActive
```

## 运行时交互

```
语言切换: QComboBox langSelector  → emit languageChanged(lang)
风格切换: QComboBox styleSelector → ThemeManager::setStyle(id, g_darkMode)
暗亮切换: QCheckBox themeCheckBox → ThemeManager::setStyle(g_activeStyleId, checked)
```

## 文件清单

| # | 文件 | 操作 | 行数 |
|---|------|------|------|
| 1 | **新建** `StyleParams.h` | 结构 + 4 风格 ×2 色调 | 180 |
| 2 | **新建** `LimeStyle.h` | LimeStyle 类声明 | 40 |
| 3 | **新建** `LimeStyle.cpp` | drawXxx 实现 | 400 |
| 4 | **新建** `LimeScrollBar.h` | LimeScrollBar 声明 | 45 |
| 5 | **新建** `LimeScrollBar.cpp` | fade + auto-hide + slider | 200 |
| 6 | `ThemeManager.h` | +setStyle, +styleChanged | +8 |
| 7 | `ThemeManager.cpp` | setStyle 实现 | +60 |
| 8 | `chatview.h` | QScrollBar* → LimeScrollBar* | 1 |
| 9 | `chatview.cpp` | 构造替换 + 事件触发 | +15 |
| 10 | `selfinfo.cpp` | 移除 setFrameStyle | -2 |
| 11 | `chatwidget.h` | +QComboBox* m_styleSelector | +1 |
| 12 | `chatwidget.cpp` | styleSelector 创建 + connect | +20 |
| 13 | `main.cpp` | 初始化默认风格 | +3 |
| 14 | `q3tox.pro` | 添加新文件 | +5 |

## 实现顺序

1. StyleParams.h — 结构 + 配色常量
2. LimeStyle.h + LimeStyle.cpp — 框架（所有函数委托基类）
3. LimeStyle.cpp — 逐个实现 drawXxx
4. LimeScrollBar.h + LimeScrollBar.cpp
5. ThemeManager — setStyle + QPalette
6. chatview — 集成 LimeScrollBar
7. chatwidget — 风格选择器
8. selfinfo — 清理
9. main — 初始化
10. q3tox.pro — 文件注册
11. 编译调试
12. 翻译键（"style.fusion", "style.macos", "style.windows", "style.material", "dark_mode" 等）
