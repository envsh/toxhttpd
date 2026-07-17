# 插件式 UI App / Noui 系统设计

## 概述

qltox 支持通过 `.so` 动态库加载两种插件：

- **UI App**：有界面的独立窗口应用（`lib*_ui.so`）
- **Noui**：无界面的功能插件（`lib*_noui.so`）

主菜单新增 **Etapps** 菜单，动态列出已发现的 UI App 插件。
配置窗口新增 **插件管理** 页面，管理所有插件的启用/禁用。

---

## 命名体系

| 概念 | 命名 | 说明 |
|------|------|------|
| 有界面插件类型 | `uiapp` | 有独立窗口 |
| 无界面插件类型 | `noui` | 后台功能 |
| UI App .so 后缀 | `lib*_ui.so` | 如 `libfmt_codec_ui.so` |
| Noui .so 后缀 | `lib*_noui.so` | 如 `libdummy_noui.so` |
| UI App 目录 | `uiapps/` | pluginBaseDirs 下的子目录 |
| Noui 目录 | `noui/` | pluginBaseDirs 下的子目录 |
| UI App 接口头文件 | `plugin_api.h` | C 接口 |
| Noui 接口头文件 | `noui_api.h` | C 接口 |
| UI App 函数前缀 | `plugin_*` | plugin_name, plugin_create 等 |
| Noui 函数前缀 | `noui_*` | noui_name, noui_init 等 |

---

## 目录结构

```
toxhttpd/
├── etapps/                           # 插件源码 + 框架（项目根，与 qltox/ 同级）
│   ├── plugin_api.h                  # UI App C 接口
│   ├── noui_api.h                    # Noui C 接口
│   ├── plugin_loader.h/.cpp          # 插件发现/加载引擎
│   ├── plugin_manager_dialog.h/.cpp  # 插件管理对话框
│   ├── build_all.sh                  # 顶层构建脚本
│   │
│   ├── fmt_codec/                    # JSON/XML/YAML/TOML 格式化
│   │   ├── plugin.pro
│   │   ├── main.cpp
│   │   ├── buildqt3.sh
│   │   └── buildqt4.sh
│   ├── hash_calc/                    # MD5/Base64/SHA256/SHA512
│   ├── bookmark_mgr/                 # 浏览器书签管理
│   ├── m3u8dl/                       # M3U8 下载器
│   ├── dotfile_cleaner/              # dot 文件 & 临时文件清理
│   ├── mirror_mgr/                   # pip/go/node 镜像管理
│   ├── dummy/                        # UI App 最小模板示例
│   └── dummy_noui/                   # Noui 最小模板示例
│
├── qltox/
│   ├── plugins/                      # .so 运行时目录（pluginBaseDirs[1]）
│   │   ├── uiapps/                   # lib*_ui.so
│   │   └── noui/                     # lib*_noui.so
│   └── ...
```

etapps/ 编译输出的 .so 文件放入 `qltox/plugins/uiapps/` 或 `qltox/plugins/noui/`。

---

## pluginBaseDirs — 插件搜索路径列表

运行时扫描的插件根目录列表（按优先级排序）：

```cpp
static QStringList pluginBaseDirs() {
    QStringList dirs;

    // 0. 当前工作目录（便携运行）
    dirs.append(QDir::currentPath() + "/plugins");

    // 1. 二进制同级（Qt 惯例）
#if defined(__APPLE__)
    QString bundlePlugIns = qAppDir() + "/../PlugIns";
    if (QDir(bundlePlugIns).exists())
        dirs.append(bundlePlugIns);
    else
        dirs.append(qAppDir() + "/../plugins");
#else
    dirs.append(qAppDir() + "/../plugins");
#endif

    // 2. 系统级
#if defined(__APPLE__)
    dirs.append("/Library/Application Support/qltox/plugins");
#else
    dirs.append("/usr/local/share/qltox/plugins");
    dirs.append("/usr/share/qltox/plugins");
#endif

    // 3. 用户级
#if defined(__APPLE__)
    dirs.append(qGetHomePath() + "/Library/Application Support/qltox/plugins");
#else
    dirs.append(qGetHomePath() + "/.config/qltox/plugins");
#endif

    return dirs;
}
```

| 序号 | Linux | macOS | 用途 |
|------|-------|-------|------|
| 0 | `$CWD/plugins` | `$CWD/plugins` | 便携运行 |
| 1 | `<bin>/../plugins` | `<bin>/../PlugIns` 或 `../plugins` | 内置/Qt 惯例 |
| 2 | `/usr/local/share/qltox/plugins` | `/Library/Application Support/qltox/plugins` | 系统级安装 |
| 3 | `/usr/share/qltox/plugins` | — | 发行版包管理 |
| 4 | `~/.config/qltox/plugins` | `~/Library/Application Support/qltox/plugins` | 用户级安装 |

macOS 检测用 `__APPLE__`（Qt3/Qt4 通用宏）。

每条路径下扫描两个子目录：

```
<pluginBaseDirs[i]>/
├── uiapps/        # 扫描 lib*_ui.so
└── noui/          # 扫描 lib*_noui.so
```

---

## C 接口规范

### UI App 插件接口 (`plugin_api.h`)

```c
#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 元信息 ── */
const char*  plugin_name(void);
const char*  plugin_version(void);
const char*  plugin_description(void);

/* ── 窗口生命周期 ── */
/*  parent = 主窗口指针（NULL = 独立窗口）*/
/*  返回值为 QWidget*，转为 void* 避免 Qt 头文件依赖 */
void* plugin_create(void* parent);
void  plugin_destroy(void* widget);

#ifdef __cplusplus
}
#endif

#endif
```

### Noui 插件接口 (`noui_api.h`)

```c
#ifndef NOUI_API_H
#define NOUI_API_H

#ifdef __cplusplus
extern "C" {
#endif

const char* noui_name(void);
const char* noui_version(void);
const char* noui_description(void);
int         noui_init(void);     /* 返回 0 成功 */
void        noui_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
```

**规则：**
- 插件内部 include `compat34.h` 处理 Qt3/Qt4 差异
- 插件需自行编译依赖的 qlcomp 源码（compat34.cpp 等），不依赖主程序导出符号
- 插件 .so 必须与主程序使用相同 Qt 版本编译
- C++ 代码不抛异常，不 try-catch

---

## 插件加载器

### PluginInfo 结构

```cpp
struct PluginInfo {
    /* ── 元信息（从 .so 符号获取）── */
    QString name;
    QString version;
    QString description;

    /* ── 文件信息 ── */
    QString soPath;          // .so 完整路径
    int     type;            // PLUGIN_TYPE_UIAPP / PLUGIN_TYPE_NOUI

    /* ── 运行时状态 ── */
    void*   handle;          // dlopen handle
    bool    enabled;         // 用户是否启用（QSettings）

    /* ── UI App 函数指针 ── */
    void*  (*create)(void* parent);
    void   (*destroy)(void* widget);
    QWidget* widget;         // 已创建的窗口（懒加载，NULL=未创建）

    /* ── Noui 函数指针 ── */
    int    (*nouiInit)(void);
    void   (*nouiUninit)(void);
};
```

### PluginLoader API

```cpp
class PluginLoader {
public:
    /* ── 扫描 ── */
    static void scanPlugins();              // 扫描所有 pluginBaseDirs
    static void rescan();                   // 重新扫描（热更新列表）

    /* ── 查询 ── */
    static int  uiappCount();
    static int  nouiCount();
    static const PluginInfo& uiappAt(int i);
    static const PluginInfo& nouiAt(int i);

    /* ── UI App 窗口操作 ── */
    static QWidget* createUiApp(int index, QWidget* parent);
    static void     destroyUiApp(int index);

    /* ── Noui 插件生命周期 ── */
    static void initNouiPlugins();          // 启动时调用

    /* ── 启用/禁用 ── */
    static void setEnabled(int index, bool on);
    static bool isEnabled(int index);

    /* ── 持久化 ── */
    static void loadState();                // 从 QSettings 读
    static void saveState();                // 写入 QSettings
};
```

### 扫描算法

```
scanPlugins():
  loadState()

  for each base in pluginBaseDirs:
    scanDir(base + "/uiapps",  PLUGIN_TYPE_UIAPP)
    scanDir(base + "/noui",    PLUGIN_TYPE_NOUI)

scanDir(path, type):
  if !QDir(path).exists():
    ALOG_INFO("[plugins] dir not found, skip:", path)
    return

  ALOG_INFO("[plugins] scanning:", path)

  filter = (type == UIAPP) ? "lib*_ui.so" : "lib*_noui.so"
  files = QDir(path).entryInfoList(filter)

  for each file:
    ALOG_INFO("[plugins]   found:", file.fileName())

    handle = dlopen(file.absoluteFilePath(), RTLD_LAZY)
    if !handle:
      ALOG_WARN("[plugins]   dlopen failed:", dlerror())
      continue

    // 验证必需符号
    if type == UIAPP:
      nameFn    = dlsym(handle, "plugin_name")
      versionFn = dlsym(handle, "plugin_version")
      createFn  = dlsym(handle, "plugin_create")
      destroyFn = dlsym(handle, "plugin_destroy")
      if !nameFn || !versionFn || !createFn || !destroyFn:
        ALOG_WARN("[plugins]   symbol missing in", file.fileName())
        dlclose(handle)
        continue
    else:
      nameFn    = dlsym(handle, "noui_name")
      versionFn = dlsym(handle, "noui_version")
      initFn    = dlsym(handle, "noui_init")
      uninitFn  = dlsym(handle, "noui_uninit")
      if !nameFn || !versionFn:
        ALOG_WARN("[plugins]   symbol missing in", file.fileName())
        dlclose(handle)
        continue

    info.handle = handle
    info.name = nameFn()
    info.version = versionFn()
    info.soPath = file.absoluteFilePath()
    info.type = type
    info.enabled = stateFromSettings(info.name)

    ALOG_INFO("[plugins]   loaded:", info.name, "v" + info.version,
              info.enabled ? "(enabled)" : "(disabled)")
    plugins.append(info)

  ALOG_INFO("[plugins] total:", count, "plugins found in", path)

// Noui 插件立即初始化
for each noui plugin where enabled:
  result = plugin.nouiInit()
  if result == 0:
    ALOG_INFO("[plugins] noui_init OK:", plugin.name)
  else:
    ALOG_WARN("[plugins] noui_init FAILED:", plugin.name, "returned", result)
```

### 窗口生命周期

```
用户点击菜单项
  → onEtappClicked(index)
  → PluginLoader::createUiApp(index, parent)
      → if widget == NULL:
          plugin.create(parent)
          widget = result
      → widget->show()
      → widget->raise()
      → widget->activateWindow()

关闭窗口
  → widget->hide()  // 不 destroy，可再次打开

插件卸载/禁用
  → PluginLoader::destroyUiApp(index)
  → plugin.destroy(widget)
  → widget = NULL
  → dlclose(handle)
```

---

## 启用/禁用状态存储

在 `~/.q3tox_settings`（QSettings INI）中新增 `[plugins]` 组：

```ini
[plugins]
; 启用的 UI App（逗号分隔列表）
enabled_uiapps=libfmt_codec_ui,libhash_calc_ui,libdummy_ui
; 禁用的 UI App
disabled_uiapps=libm3u8dl_ui
; 启用的 Noui
enabled_noui=libdummy_noui
; 禁用的 Noui
disabled_noui=
```

**规则：**
- 首次扫描时，所有新发现的插件默认启用（加入 enabled 列表）
- 已知禁用的插件不加载（从 disabled 列表检查）
- 用户在插件管理界面切换开关 → 立即 saveState()

---

## 主菜单集成

MainWindow 菜单栏新增 **Etapps(&E)** 菜单：

```
Etapps(&E)
  ├── JSON/XML/YAML/TOML 格式化    → onEtappClicked(0)
  ├── 哈希计算器                   → onEtappClicked(1)
  ├── 浏览器书签管理               → onEtappClicked(2)
  ├── M3U8 下载器                  → onEtappClicked(3)
  ├── dot 文件清理                 → onEtappClicked(4)
  ├── 镜像管理                     → onEtappClicked(5)
  ├── Dummy 示例                  → onEtappClicked(6)
  ├── ── separator ──
  └── 插件管理...                  → openPluginManager()
```

**连接方式（Qt3/Qt4 兼容）：** 使用 `QSignalMapper` 将每个菜单项映射到 index。
仅显示 enabled 的插件。菜单项文本取自 `plugin_name()` 返回值。

```cpp
// mainwindow.cpp 中菜单构建示意
MenuWidget34* etappsMenu = mb->addSubMenu(menuBar, qFromUtf8("Etapps(&E)"));
m_etappMapper = new QSignalMapper(this);
connect(m_etappMapper, SIGNAL(mapped(int)), this, SLOT(onEtappClicked(int)));
int idx = 0;
for (int i = 0; i < PluginLoader::uiappCount(); i++) {
    const PluginInfo& p = PluginLoader::uiappAt(i);
    if (!p.enabled) continue;
    EmbeddedMenuBar::addItem(etappsMenu, qFromUtf8(p.name),
                             m_etappMapper, SLOT(map()));
    m_etappMapper->setMapping(/* menu item widget */, idx);
    idx++;
}
EmbeddedMenuBar::addSeparator(etappsMenu);
EmbeddedMenuBar::addItem(etappsMenu, qFromUtf8("插件管理..."),
                         this, SLOT(openPluginManager()));
```

---

## 插件管理对话框

在 ConfigDialog 中新增标签页 **插件管理**：

```
插件管理
├── UI App 插件
│   ├── [✓] JSON 格式化    v1.0.0  JSON/XML/YAML/TOML 编解码  [打开] [卸载]
│   ├── [✓] 哈希计算器     v1.0.0  MD5/Base64/SHA256/SHA512   [打开] [卸载]
│   ├── [✓] 浏览器书签管理  v1.0.0  浏览器书签导入/编辑/导出    [打开] [卸载]
│   ├── [ ] M3U8 下载器    v1.0.0  M3U8 播放列表解析下载       [打开] [卸载]
│   ├── [✓] dot 文件清理   v1.0.0  系统缓存/临时文件清理       [打开] [卸载]
│   ├── [✓] 镜像管理       v1.0.0  pip/go/node 换源           [打开] [卸载]
│   └── [✓] Dummy 示例    v1.0.0  最小模板示例               [打开] [卸载]
└── Noui 插件
    └── [✓] dummy_noui     v1.0.0  最小模板示例              [卸载]
```

- 开关切换 → `PluginLoader::setEnabled()` → `saveState()`
- [打开] → `PluginLoader::createUiApp()` → widget->show()
- [卸载] → `PluginLoader::destroyUiApp()` → dlclose

---

## 日志规范

所有日志前缀 `[plugins]`，使用 `ALOG_INFO` / `ALOG_WARN`（limelog.h）：

| 阶段 | 日志内容 |
|------|---------|
| 启动扫描 | `scanning pluginBaseDirs:` + 各路径 |
| 目录不存在 | `dir not found, skip: <path>` |
| 目录存在 | `scanning: <path>` |
| 发现 .so | `found: <filename>` |
| dlopen 失败 | `dlopen failed: <dlerror()>` |
| 符号缺失 | `symbol missing in <file>` |
| 加载成功 | `loaded: <name> v<ver> (enabled/disabled)` |
| 总计 | `total: N plugins found in <path>` |
| Noui 初始化 | `noui_init OK: <name>` / `noui_init FAILED: <name> returned <code>` |
| 窗口创建 | `createUiApp: <name> → widget created` |

---

## 示例插件列表

### UI App（7 个）

| # | 目录 | .so 名 | 功能 | 额外依赖 |
|---|------|--------|------|---------|
| 1 | `fmt_codec` | `libfmt_codec_ui.so` | JSON/XML/YAML/TOML 编解码/格式化/美化 | cJSON（已有） |
| 2 | `hash_calc` | `libhash_calc_ui.so` | MD5/Base64/SHA256/SHA512 哈希计算 | `-lcrypto`（OpenSSL） |
| 3 | `bookmark_mgr` | `libbookmark_mgr_ui.so` | 浏览器书签导入/浏览/编辑/导出 | 无 |
| 4 | `m3u8dl` | `libm3u8dl_ui.so` | M3U8 播放列表解析、分片下载 | `-lcurl`（已有） |
| 5 | `dotfile_cleaner` | `libdotfile_cleaner_ui.so` | 系统 dot 文件 & 临时文件扫描清理 | 无 |
| 6 | `mirror_mgr` | `libmirror_mgr_ui.so` | pip/go/node 下载镜像源管理 | 无 |
| 7 | `dummy` | `libdummy_ui.so` | 最小模板：QWidget + QLabel + 按钮 | 无 |

### Noui（1 个）

| # | 目录 | .so 名 | 功能 |
|---|------|--------|------|
| 1 | `dummy_noui` | `libdummy_noui.so` | 最小模板：noui_init 打印日志，noui_uninit 清理 |

### dummy 示例代码

```cpp
// etapps/dummy/main.cpp — UI App 最小模板
#include "compat34.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qmessagebox.h>
#include "plugin_api.h"

extern "C" const char* plugin_name(void)       { return "Dummy Plugin"; }
extern "C" const char* plugin_version(void)    { return "1.0.0"; }
extern "C" const char* plugin_description(void){ return "模板示例插件"; }

extern "C" void* plugin_create(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);
    QLabel* lbl = new QLabel("Hello from Dummy Plugin!", w);
    QPushButton* btn = new QPushButton("Click Me", w);
    // TODO: 添加布局
    w->setWindowTitle("Dummy Plugin");
    w->resize(300, 200);
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
```

```cpp
// etapps/dummy_noui/main.cpp — Noui 最小模板
#include "noui_api.h"
#include <stdio.h>

extern "C" const char* noui_name(void)        { return "Dummy Noui"; }
extern "C" const char* noui_version(void)     { return "1.0.0"; }
extern "C" const char* noui_description(void) { return "Noui 最小模板示例"; }

extern "C" int noui_init(void) {
    printf("[dummy_noui] initialized\n");
    return 0;
}

extern "C" void noui_uninit(void) {
    printf("[dummy_noui] uninitialized\n");
}
```

---

## 构建系统

### 顶层构建脚本 `etapps/build_all.sh`

```bash
#!/bin/bash
# 编译所有插件，Qt3 先于 Qt4（避免 .o 目录冲突）
cd "$(dirname "$0")"
for src_dir in */; do
    [ -f "$src_dir/buildqt3.sh" ] || continue
    echo "=== Building $src_dir (Qt3) ==="
    bash "$src_dir/buildqt3.sh"
done
for src_dir in */; do
    [ -f "$src_dir/buildqt4.sh" ] || continue
    echo "=== Building $src_dir (Qt4) ==="
    bash "$src_dir/buildqt4.sh"
done
```

### 单个插件 `plugin.pro` 示例

```pro
TEMPLATE = lib
TARGET = fmt_codec_ui
CONFIG += plugin qt
QT += core gui widgets

INCLUDEPATH += ../../qlcomp         # compat34.h
INCLUDEPATH += ..                   # plugin_api.h

SOURCES = main.cpp
# 需要 compat34 功能时取消注释：
# SOURCES += ../../qlcomp/compat34.cpp

# 插件特定依赖
# hash_calc 需要：LIBS += -lcrypto
# m3u8dl 需要：LIBS += -lcurl

# 输出到 etapps/output/，再由 buildqt*.sh 拷贝到 qltox/plugins/
DESTDIR = ../output/uiapps
```

### 单个插件 `buildqt3.sh` 示例

```bash
#!/bin/bash
cd "$(dirname "$0")"
mkdir -p build-qt3 && cd build-qt3
/opt/qt338sh/bin/qmake -makefile ../plugin.pro
make -j1
# 拷贝 .so 到 qltox/plugins/
cp -v ../output/uiapps/lib*.so ../../qltox/plugins/uiapps/
```

### 构建约束

- **先 Qt3，后 Qt4**（同一 .o 目录不并行编译）
- 插件 .so 必须与主程序使用相同 Qt 版本
- C++11，无异常，无 try-catch

---

## 主程序集成文件清单

### 新建文件

| 文件 | 说明 |
|------|------|
| `etapps/plugin_api.h` | UI App C 接口 |
| `etapps/noui_api.h` | Noui C 接口 |
| `etapps/plugin_loader.h` | 加载器声明 |
| `etapps/plugin_loader.cpp` | 加载器实现 |
| `etapps/plugin_manager_dialog.h` | 管理对话框声明 |
| `etapps/plugin_manager_dialog.cpp` | 管理对话框实现 |
| `etapps/build_all.sh` | 顶层构建脚本 |
| `etapps/fmt_codec/*` | JSON/XML/YAML/TOML 格式化插件 |
| `etapps/hash_calc/*` | 哈希计算器插件 |
| `etapps/bookmark_mgr/*` | 浏览器书签管理插件 |
| `etapps/m3u8dl/*` | M3U8 下载器插件 |
| `etapps/dotfile_cleaner/*` | dot 文件清理插件 |
| `etapps/mirror_mgr/*` | 镜像管理插件 |
| `etapps/dummy/*` | UI App 模板示例 |
| `etapps/dummy_noui/*` | Noui 模板示例 |
| `qltox/plugins/uiapps/` | UI App .so 输出目录 |
| `qltox/plugins/noui/` | Noui .so 输出目录 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `qltox/mainwindow.h` | 新增 onEtappClicked(int)、openPluginManager()、QSignalMapper* 成员 |
| `qltox/mainwindow.cpp` | 新增 Etapps 菜单构建（~30行）、onEtappClicked 实现（~10行）|
| `qltox/qltox.pro` | 新增 INCLUDEPATH += ../etapps |
