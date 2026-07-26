# qsktox 实现详解

基于 Qt 6.7 + QSkinny 的跨平台聊天客户端，支持 Android/Linux/macOS/Windows，不使用 QML。

---

## 架构概览

```
main.cpp (入口 + BackButtonFilter + extras线程)
  ├── PageManager (导航栈 + 生命周期管理 + LRU缓存 + 进程死亡保护)
  │     ├── LoginPage        — 服务器选择
  │     ├── MainPage         — 主聊天界面
  │     ├── SettingsPage     — 设置（主题/字体/动画）
  │     ├── AboutPage        — 系统信息
  │     └── LogPage          — 日志查看
  │
  ├── NetworkMonitor (4平台网络监控 + 桌面通知)
  ├── KeepAlive (Android前台服务保活)
  ├── androidutils (Android Toast)
  ├── shareintentreceiver.cpp (Android分享接收)
  ├── LogModel (内存日志缓冲，单例)
  └── extras: libgoso.so (Go) + libcso.so (C++)
```

---

## 文件清单

### 构建文件

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | 顶层 CMake，配置 Qt6 + QSkinny + Go/C++ extras |
| `build-x64.sh` | Linux/macOS x64 构建脚本 |
| `build-android.sh` | Android arm64 构建脚本（含 Go 交叉编译 + APK 打包） |
| `plan.md` | 设计笔记和构建环境参考 |

### 源码文件 (`src/`)

| 文件 | 行数 | 说明 |
|------|------|------|
| `main.cpp` | 448 | 入口：QGuiApplication、皮肤、字体、窗口、页面注册、extras线程、Push分发器选择 |
| `page.h/cpp` | 107+26 | Page 基类，Android 风格生命周期模型 |
| `pagemanager.h/cpp` | 122+505 | 导航栈 + 生命周期管理 + LRU缓存 + 状态保存 |
| `loginpage.h/cpp` | 16+47 | 登录页：3个硬编码服务器按钮 |
| `mainpage.h/cpp` | 37+219 | 主聊天界面：TopBar + ChatArea + InputBar |
| `settingspage.h/cpp` | 53+243 | 设置页：过渡动画/主题/字体/调试背景 |
| `aboutpage.h/cpp` | 16+90 | 关于页：版本/Qt/RHI/架构/设备信息 |
| `logpage.h/cpp` | 37+164 | 日志查看器：过滤 + 搜索 + 实时更新 |
| `logmodel.h/cpp` | 41+29 | 内存日志缓冲（单例，500条上限） |
| `menuoverlay.h/cpp` | 47+69 | 透明遮罩层，点击外部关闭 QskMenu |
| `networkmonitor.h/cpp` | 14+195 | 4平台网络监控 |
| `keepalive.h/cpp` | 14+42 | Android 前台服务保活 |
| `pushhandler.h/cpp` | 60+571 | UnifiedPush 注册、分发器管理、信号路由 |
| `androidutils.h/cpp` | 8+26 | Android Toast 工具函数 |
| `shareintentreceiver.cpp` | 45 | JNI 桥接：ShareActivity → MainPage |

### Android Java 文件 (`android/`)

| 文件 | 说明 |
|------|------|
| `AndroidManifest.xml` | 清单：权限、ShareActivity、KeepAliveService、FileProvider |
| `ShareActivity.java` | 主 Activity，处理 SEND 分享意图 |
| `NetworkMonitor.java` | BroadcastReceiver 监听网络变化 |
| `KeepAliveService.java` | 前台服务保活 |
| `PermissionHelper.java` | 运行时权限请求（通知/媒体） |
| `MobUtil.java` | 工具：状态栏/导航栏高度、屏幕尺寸、Toast |

### Extras 共享库

| 文件 | 说明 |
|------|------|
| `extras/go/goso.go` | Go 共享库：`gosoMainLoop()` 占位无限循环 |
| `extras/go/go.mod` | Go 模块定义 |
| `extras/cpp/cso.h/cpp` | C++ 共享库：`csoMainLoop()` 占位无限循环 |

---

## 核心组件详解

### 1. Page 生命周期模型 (`page.h/cpp`)

模仿 Android Activity/Fragment 生命周期：

```cpp
enum PageState {
    None, Created, Started, Resumed,
    Paused, Stopped, Destroyed
};

enum LaunchMode {
    Standard,       // 每次创建新实例
    SingleTop,      // 栈顶复用，调 onNewIntent
    SingleInstance  // 栈中唯一，bring to front
};

enum CachePolicy {
    Transient,  // 返回时销毁（250ms延迟，配合动画）
    LRU,        // LRU缓存，超限淘汰
    Permanent   // 永不销毁
};
```

生命周期回调：`onCreate` → `onStart` → `onResume` → `onPause` → `onStop` → `onDestroy`

额外支持：`finish()` 发射 `finishRequested`，`setResult()`/`resultCode()`/`resultData()` 用于页面间传结果。

### 2. PageManager (`pagemanager.h/cpp`)

核心导航和生命周期管理器，管理 `QskStackBox`。

**关键数据结构：**
- `m_history: QList<QString>` — 导航历史栈
- `m_pages: QMap<QString, Page*>` — 所有存活页面
- `m_cacheLRU: QStringList` — LRU 缓存队列
- `m_factories/m_registrations` — 页面类型注册表
- `m_pendingResults` — `openForResult` 的回调
- `m_busy` — 重入锁
- `m_destroyTimer/m_pendingDestroy` — 延迟销毁（配合动画）

**关键方法：**

| 方法 | 说明 |
|------|------|
| `registerPage(id, factory, registration)` | 注册页面类型 |
| `open(id, args, mode)` | 压栈导航，处理 SingleTop/SingleInstance |
| `openForResult(id, args, callback, mode)` | 带结果回调的导航 |
| `back()` | 弹栈，延迟销毁（Transient用250ms timer配合动画） |
| `replace(id, args)` | 替换当前页面 |
| `saveAllStates()` / `restoreAllStates()` | QSettings 序列化/反序列化（进程死亡恢复） |

**动画安全设计：**
- `back()`: 先 activate 目标页，再延迟销毁当前页（250ms timer）
- `replace()`: 先销毁当前页，再 activate 新页（无动画过渡）
- `QStackBoxAnimator::itemAt()` 通过 `m_startIndex/m_endIndex` 访问 items，删除会导致引用失效
- `activatePage()` 会取消待销毁的页面（`cancelPendingDestroy()`）

### 3. LoginPage (`loginpage.cpp`)

简单的服务器选择页面，3个硬编码服务器按钮：
- `localhost:8181`
- `192.168.43.157:4004`
- `192.168.49.136:4004`

点击后调用 `pageManager()->replace("main", {{"url", url}})` 进入主页面。

### 4. MainPage (`mainpage.cpp`)

主聊天界面，UI 结构：

```
TopBar:
  [头像按钮] [标题] [菜单按钮]
                ↓
         QskMenu + MenuOverlay
  - Keep Screen On 切换
  - App Log
  - Settings
  - About
  - Logout

ChatArea:
  [欢迎文本 "Welcome to qsktox"]
  [Toast 标签]

InputBar:
  [文本输入框] [发送按钮]
```

**关键功能：**
- `jniKeepScreenOn(bool)`: 通过 JNI 设置 Android `FLAG_KEEP_SCREEN_ON`
- `showToast(msg)`: 显示临时文本标签
- `handleShareIntent(action, mimeType, text, uris)`: 处理 Android 分享意图（文本/图片/多文件）
- `registerMainPage(MainPage*)`: 原子指针注册，供 JNI 回调使用

### 5. SettingsPage (`settingspage.cpp`)

5个设置项：

| 设置 | 控件 | 持久化键 |
|------|------|----------|
| Page Transition | QskComboBox (None/Slide Fade) | `transition` |
| Theme | QskComboBox (Fusion) | `skin` |
| Color Scheme | QskSwitchButton | `darkMode` |
| Font Size | QskComboBox + 动态调节 | `fontScale` |
| Debug Background | QskSwitchButton | `debugBackground` |

`FontSizes` 结构体：`body, title, caption, global`（int 字号）

`applyAndroidFonts()`: 设置 QSkinny 字体角色（仅 Android），桌面端不生效。

### 6. AboutPage (`aboutpage.cpp`)

显示信息：
- App Version
- Qt Version
- RHI Backend
- Architecture（ARM64/x86_64 等）
- Device Info（Android 版本/SDK 等）

### 7. LogPage (`logpage.cpp`)

日志查看器：
- 级别过滤：All / Info / Warn / Error
- 搜索：防抖（QTimer）+ 大小写不敏感
- 实时更新：监听 `LogModel::entryAdded/cleared` 信号
- 底部状态栏：条目计数

### 8. LogModel (`logmodel.cpp`)

内存日志缓冲（单例）：
- 最大 500 条，超出淘汰最旧
- 每条包含：Level（Debug/Info/Warn/Error）、时间戳（HH:mm:ss.zzz）、tag、message
- 发射信号：`entryAdded(index)` / `cleared()`

### 9. MenuOverlay (`menuoverlay.cpp`)

解决 QSkinny 的 `CloseOnPressOutside` 对兄弟控件不生效的问题。

透明全屏遮罩层，拦截鼠标/触摸事件，点击菜单外部时关闭菜单。通过 `stackBefore(menu)` 控制 z-order。

底部有 `#include "moc_menuoverlay.cpp"`（预包含 MOC，因为 Q_OBJECT 在 `#ifdef` 外）。

### 10. NetworkMonitor (`networkmonitor.cpp`)

4平台独立实现：

| 平台 | 网络检测 | 通知方式 |
|------|----------|----------|
| Android | JNI → `NetworkMonitor.java` (BroadcastReceiver) | Android Toast (LENGTH_LONG ~3.5s) |
| Linux | `QNetworkInformation::loadDefaultBackend()` + `instance()` | `notify-send -t 7000` |
| Windows | `QNetworkInformation` | PowerShell Toast API |
| macOS | `QNetworkInformation` | `osascript` (AppleScript) |

**重要：** `QNetworkInformation` 是单例，析构函数是 private 的。必须用 `loadDefaultBackend()` + `instance()`，不能 `new`/`delete`。`stop()` 只能 `disconnect` + 置 nullptr。

### 11. KeepAlive (`keepalive.cpp`)

仅 Android 有效：
1. 请求通知权限 (`PermissionHelper.requestNotificationPermission`)
2. 启动前台服务 (`KeepAliveService.startService`)

桌面端为 no-op。

### 12. androidutils (`androidutils.cpp`)

`showAndroidToast(const QString& message)`: 通过 JNI 在 Android 主线程调用 `android.widget.Toast.makeText().show()`。桌面端 no-op。

### 13. shareintentreceiver.cpp

JNI 桥接：Java `ShareActivity` → C++ `MainPage::handleShareIntent()`

使用 `static std::atomic<MainPage*> s_mainPage` 原子指针存储 MainPage 实例。

---

## JNI 桥接映射

| C++ 侧 | Java 侧 | 方向 |
|---------|---------|------|
| `showAndroidToast()` | `android.widget.Toast.makeText().show()` | C++ → Java |
| `KeepAlive::start()` | `PermissionHelper` + `KeepAliveService.startService()` | C++ → Java |
| `KeepAlive::stop()` | `KeepAliveService.stopService()` | C++ → Java |
| `NetworkMonitor::start()` | `NetworkMonitor.startMonitoring()` | C++ → Java |
| `NetworkMonitor::stop()` | `NetworkMonitor.stopMonitoring()` | C++ → Java |
| `Java_..._onNetworkChanged()` | `NetworkMonitor.onNetworkChanged()` | Java → C++ |
| `Java_..._onShareIntentReceived()` | `ShareActivity.onShareIntentReceived()` | Java → C++ |

---

## Extras 共享库

### Go 共享库 (`extras/go/goso.go`)

```go
package main

import "C"
import "time"

//export gosoMainLoop
func gosoMainLoop() {
    for {
        time.Sleep(time.Hour)
    }
}
```

编译：`CGO_ENABLED=1 go build -buildmode=c-shared -o libgoso.so`

### C++ 共享库 (`extras/cpp/cso.cpp`)

```cpp
#include "cso.h"
#include <unistd.h>

extern "C" void csoMainLoop() {
    while (1) {
        sleep(3600);
    }
}
```

所有平台均使用共享库（.so），通过 CGO 编译。主程序在 detached 线程中启动两个入口函数。

---

## 平台特定代码

| 文件 | `#ifdef` 守卫 |
|------|---------------|
| `main.cpp` | `Q_OS_ANDROID`（渲染循环、皮肤路径、窗口显示、KeepAlive、NetworkMonitor）；`!Q_OS_ANDROID`（桌面图标、快捷键、窗口大小） |
| `networkmonitor.cpp` | `Q_OS_ANDROID` / `Q_OS_LINUX` / `Q_OS_WINDOWS` / `Q_OS_MACOS`（4套独立实现） |
| `keepalive.cpp` | `Q_OS_ANDROID`（整个实现） |
| `androidutils.cpp` | `Q_OS_ANDROID`（JNI 实现，否则 no-op） |
| `shareintentreceiver.cpp` | `Q_OS_ANDROID`（JNI 回调） |
| `mainpage.cpp` | `Q_OS_ANDROID`（JNI KeepScreenOn） |
| `aboutpage.cpp` | `Q_PROCESSOR_ARM_V8`（架构标签）；`Q_OS_ANDROID`（Android 版本） |
| `CMakeLists.txt` | `ANDROID`（ABI 目标修复、额外链接库、Go 交叉编译环境） |
| `build-x64.sh` | `uname != Darwin`（Linux 专用 sed 修复） |

**注意：** Android NDK 定义了 `Q_OS_LINUX`，所以必须用 `#if defined(Q_OS_ANDROID)` / `#elif defined(Q_OS_LINUX)` 模式。

---

## 构建系统

### CMakeLists.txt

- C++17，Qt6（Core, Quick, Network）
- `CMAKE_AUTOMOC = ON`
- Android ABI 目标修复：为 `Qt6Gui_arm64-v8a` 等创建别名接口库
- Go 共享库：自定义命令 `go build -buildmode=c-shared`，跨编译环境按平台设置
- C++ 共享库：`add_library(cso SHARED ...)`
- 链接：`Qsk::QSkinny Qt6::Quick Qt6::Network goso cso`，Android 额外链接 `log android`
- Go 库需要 `IMPORTED_NO_SONAME TRUE` 防止绝对路径写入 RPATH

### build-x64.sh

1. Go: `CGO_ENABLED=1 go build -buildmode=c-shared -o libgoso.so`
2. CMake: `qt-cmake` 配置（Qt 6.7.3, QSkinny）
3. Hotfix: Linux 上移除 `-lQt6Qml -lQt6Quick -lQt6OpenGL`（macOS 不需要）
4. Build: `make -j$(nproc)`

### build-android.sh

1. Go 交叉编译: `GOOS=android GOARCH=arm64 CGO_ENABLED=1 CC=$NDK/toolchains/llvm/.../aarch64-linux-android24-clang go build -buildmode=c-shared`
2. CMake: `qt-cmake` 配置（Android SDK/NDK）
3. Build: `make -j$(nproc)` (native .so only)
4. Prepare: 创建 `android-build/` 目录，复制 flat JNI libs
5. QSkinny 皮肤: 复制 `qskinny_fusion` 插件
6. `androiddeployqt --aux-mode`: 准备 Qt 依赖
7. Copy: 自定义 Java 源码 + manifest + icon
8. Patch Gradle: 移除 renderscript，添加 Qt 属性，减少堆大小
9. `./gradlew assembleDebug`

---

## 外部依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| Qt | 6.7.3 | Core, Quick, Network, Qml |
| QSkinny | 8bc872f | UI 框架（替代 QML） |
| Go | 1.22+ | 共享库编译（cgo） |
| Android NDK | r26b | arm64-v8a 交叉编译 |
| Android SDK | API 33-34 | Java 编译、Gradle、APK |
| JDK | 17 | Gradle 运行时 |

---

## 已知设计决策

1. **不使用 QML** — 全部 UI 使用 QSkinny 控件布局
2. **Android 风格生命周期** — Page 模仿 Fragment 生命周期，PageManager 管理状态
3. **延迟销毁** — `back()` 使用 250ms timer 延迟销毁当前页，避免 QStackBoxAnimator 引用失效
4. **单例模式** — `LogModel` 为单例，`QNetworkInformation` 为 Qt 单例（private destructor）
5. **原子指针** — `shareintentreceiver.cpp` 使用 `std::atomic<MainPage*>` 跨线程传递 MainPage 引用
6. **extras 独立** — Go/C++ 共享库与主程序无通信，仅作为独立线程运行
7. **平台通知差异** — Android Toast ~3.5s，Linux notify-send 7s，Windows/macOS 使用系统默认
8. **IMPORTED_NO_SONAME** — Go 共享库防止绝对路径写入 RPATH，Android 通过文件名查找 .so

---

## Push Handler 流程

### 文件
- `pushhandler.h/cpp` — UnifiedPush 注册、分发器管理
- `main.cpp` — 启动时弹出分发器选择对话框（`QskDialog::select`）
- `settingspage.cpp` — Settings 页查询已安装分发器，更新 combo 标签

### 信号路由

| 信号 | 触发源 | 消费方 |
|------|--------|--------|
| `distributorsFound` | `registerDevice()`（启动注册） | `main.cpp` → 弹 dialog 选择 |
| `distributorsUpdated` | `installedDistributors()`（Settings 页查询） | `settingspage.cpp` → 更新 combo 标签 |

### 启动注册流程 (`registerDevice`)

```
PushHandler::start()
  → registerDevice()
    → Android getSavedDistributor()
      → 有 saved → 直接 register(saved)，不弹任何东西
      → 无 saved → getDistributors()
            → 空 → 报错 "未找到 UnifiedPush 分发器"
            → 1个 → selectDistributor() 自动选
            → >1个 → emit distributorsFound → main.cpp 弹 dialog
```

### Settings 页查询流程 (`installedDistributors`)

```
SettingsPage::onCreate()
  → PushHandler::installedDistributors()（异步）
    → Android getDistributors()
      → emit distributorsUpdated → rebuildBackendLabels() 更新 combo 标签
```

### 分发器切换

用户在 Settings 页手动切换分发器：
- `switchDistributor(newDistributor)` → 重新注册
- `selectDistributor(distributor)` → 直接选择（Android native 调用）
