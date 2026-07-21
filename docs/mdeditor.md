# MdEditor — Markdown 编辑器

## 架构

```
MdEditor (QWidget)
├── MdToolbar         — 工具栏（EmojiPushButton + QComboBox）
├── QSplitter         — 水平分割器
│   ├── QTextEdit     — 纯文本编辑器
│   │   └── MdHighlighter (QSyntaxHighlighter) — 语法高亮
│   └── MdPreview     — HTML 预览（QTextBrowser）
├── QTimer m_autoSaveTimer     — 自动保存定时器
└── QTimer m_scrollSyncGuard   — 滚动同步防抖定时器
```

## 文件

| 文件 | 用途 |
|------|------|
| `mdeditor.h/cpp` | 主窗口，管理布局、事件、滚动同步 |
| `mdtoolbar.h/cpp` | 工具栏按钮、heading 子菜单、自动保存下拉框 |
| `mdpreview.h/cpp` | Markdown→HTML 渲染（md4c） |
| `mdhighlighter.h/cpp` | 编辑器语法高亮规则 |
| `vendor/md4c.c/h` | Markdown 解析库（C） |
| `vendor/md4c-html.c/h` | Markdown→HTML 转换（C） |

## 已有功能

### 工具栏按钮（21个）

| 按钮 | 图标 | 功能 | 语法 |
|------|------|------|------|
| 撤销 | ↩ | Undo | — |
| 重做 | ↪ | Redo | — |
| 标题 | H▾ | H1–H6 子菜单 | `# ` ~ `###### ` |
| 粗体 | B | 加粗选中文本 | `**text**` |
| 斜体 | I | 斜体选中文本 | `*text*` |
| 下划线 | U | 下划线 | `__text__` |
| 删除线 | S | 删除线 | `~~text~~` |
| 行内代码 | ⟨⟩ | 行内代码 | `` `text` `` |
| 代码块 | ⌨ | 围栏代码块 | ` ```\ncode\n``` ` |
| 引用 | ❝ | 块引用 | `> ` 前缀 |
| 无序列表 | • | 无序列表 | `- ` 前缀 |
| 有序列表 | 1. | 有序列表 | `1. ` 前缀 |
| 任务列表 | ☑ | 任务列表 | `- [ ] ` 前缀 |
| 水平线 | — | 水平分隔线 | `\n---\n\n` |
| 链接 | 🔗 | 超链接 | `[text](url)` |
| 图片 | 🖼 | 图片 | `![alt](url)` |
| 表格 | ⊞ | 3列表格模板 | Markdown 表格 |
| 日期 | 📅 | 插入当前日期 | `yyyy-MM-dd` |
| 脚注 | ⌃ | 脚注 | `[^text]` |
| 目录 | ≡ | 目录标记 | `[toc]` |
| 预览 | 👁 | 切换预览面板 | — |

另有自动保存下拉框：关闭 / 1–10 分钟。

### 键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| Escape | 关闭（有修改时弹确认框）|
| Ctrl+A | 全选 |
| Ctrl+S | 保存（标题去掉星号）|

### 语法高亮（14条规则）

| 规则 | 正则 | 样式 |
|------|------|------|
| 标题 | `^#{1,6}\s+.*` | 加粗 + 深蓝 |
| 粗体 `**` | `\*\*[^*]+\*\*` | 加粗 |
| 粗体 `__` | `__[^_]+__` | 加粗 |
| 斜体 `*` | `\*[^*]+\*` | 斜体 |
| 斜体 `_` | `_[^_]+_` | 斜体 |
| 行内代码 | `` `[^`]+` `` | 等宽字体 + 灰底 |
| 链接 | `\[[^\]]+\]\([^)]+\)` | 深洋红 |
| 图片 | `!\[[^\]]*\]\([^)]+\)` | 深绿 |
| 引用 | `^>.*` | 灰色 + 斜体 |
| 水平线 | `^(-{3,}|\*{3,}|_{3,})$` | 灰色 |
| 无序列表 | `^\s*[-*+]\s+` | 深红 |
| 有序列表 | `^\s*\d+\.\s+` | 深红 |
| 任务列表 | `^\s*-\s+\[[ x]\]\s+` | 深青 |
| 标签 | `#[\w]+` | 深黄 |

### 预览

- 使用 **md4c** 库将 Markdown 转换为 HTML
- 支持：表格、任务列表、删除线、自动链接（URL）、围栏代码块、脚注等
- 内置 CSS 样式表，响应式图片、代码块横向滚动

### 滚动同步

- **编辑器→预览**：编辑器滚动时，预览按比例同步
- **预览→编辑器**：预览滚动时，编辑器反向同步
- 基于 `ScrollEntry` 的线性插值映射（按段落 Y 坐标）
- 50ms 单次定时器防抖，防止递归

### 自动保存

- 可配置 1–10 分钟间隔
- 超时后若文档已修改，发射 `saveRequested()` 信号

### 修改状态跟踪

- 窗口标题前加 `*` 表示已修改
- 保存或关闭后清除星号

### 关闭确认

- 关闭或按 Escape 时，若文档已修改，弹出确认对话框

### 窗口管理

- Qt3: `WDestructiveClose`（关闭自动删除）
- Qt4: `WA_DeleteOnClose`（同上）
- `deleteLater()` 确保安全析构
- 析构时手动删除 `MdHighlighter`（Qt3 中非 QObject）

## 已知限制

1. **快捷键少** — 仅 Escape/Ctrl+A/Ctrl+S，无 Ctrl+B/I 等格式化快捷键
2. **无查找替换** — 没有 Ctrl+F/R 功能
3. **无字数统计** — 没有状态栏显示字数/字符数
4. **无行号** — 编辑器左侧无行号显示
5. **无 Tab 缩进** — Tab 键不支持增加/减少缩进
6. **无导出** — 不支持导出 HTML/PDF
7. **工具栏按钮无状态同步** — 光标在粗体内时 B 按钮不高亮
8. **预览无代码高亮** — 代码块在预览中无语法高亮（需 highlight.js）
9. **无暗色主题** — 仅支持亮色
10. **无多文档** — 每次打开创建新实例，不支持标签页
11. **无拖拽打开** — 不支持拖拽 .md 文件到窗口
12. **无最近文件** — 无文件历史记录
13. **无拼写检查** — 无 hunspell 集成

## 未来改进路线

### Phase 1 — 基础体验（高优先级）

1. **快捷键格式化** — Ctrl+B 粗体、Ctrl+I 斜体、Ctrl+K 链接、Ctrl+E 行内代码
2. **查找/替换** — Ctrl+F 查找、Ctrl+R 替换（正则可选）
3. **字数统计状态栏** — 底部显示字数、字符数、预计阅读时间

### Phase 2 — 编辑增强（中优先级）

4. **Tab 缩进** — Tab 增加缩进、Shift+Tab 减少缩进
5. **导出 HTML** — 菜单或按钮导出完整 HTML 文件
6. **行号显示** — 编辑器左侧行号
7. **工具栏按钮状态同步** — 根据光标位置高亮对应按钮
8. **预览代码块高亮** — 集成 highlight.js 或 Pygments

### Phase 3 — 体验优化（低优先级）

9. **暗色/亮色主题切换** — 参考 CuteMarkEd 多主题方案
10. **目录侧边栏** — 从标题自动生成，点击跳转
11. **拖拽文件打开** — 支持拖拽 .md 文件到窗口
12. **最近文件列表** — 菜单中显示最近打开的文件
13. **跳转到行** — Ctrl+G 跳转到指定行号
14. **拼写检查** — hunspell 集成，红色波浪线下划线
15. **全屏/专注模式** — F11 全屏，隐藏工具栏只留编辑器
16. **多文档标签页** — 支持同时编辑多个文件

## 技术细节

### Qt3/Qt4 兼容

- `compat34.h` 统一 API 差异
- Qt3: `QSyntaxHighlighter` 非 QObject，需手动 `delete`
- Qt3: `highlightParagraph()` / Qt4: `highlightBlock()`
- Qt3: `setFormat(index, length, font, color)` / Qt4: `setFormat(index, length, format)`

### md4c 集成

- 纯 C 库，通过 `extern "C"` 链接
- `md_html()` 将 UTF-8 Markdown 转为 HTML
- 回调函数 `processOutput()` 将结果追加到 `HtmlBuffer`
- 支持的扩展：表格、任务列表、删除线、自动链接

### 构建

```bash
# Qt3 构建
cd qltox && bash buildqt3.sh

# Qt4 构建
cd qltox && bash buildqt4.sh

# Qt3 + ASan
cd qltox && bash buildqt3.sh asan
```

### 编译参数（qltox.pro）

```
QMAKE_CXXFLAGS += -std=c++11 -O0 -fstack-protector-strong
QMAKE_CFLAGS += -O0 -std=c11 -fstack-protector-strong
# ASan（通过 CONFIG+=asan 启用）
contains(CONFIG, asan) {
    QMAKE_CXXFLAGS += -fsanitize=address
    QMAKE_CFLAGS += -fsanitize=address
    QMAKE_LFLAGS += -fsanitize=address
}
```
