# qlmoc — MOC + C++20 Modules 实验

## 目标

为不支持 C++20 modules 的 Qt3/4 moc 添加模块中的 Q_OBJECT 宏解析能力。

## 原理

moc 不理解 `export module`、`import` 等 C++20 关键字。
通过包装器在 moc 前/后预处理和后处理，实现模块兼容。

## 架构

### Pipeline

```
.cppm (含 Q_OBJECT 类)
  │
  ▼ preprocess_cppm.py
  │ 去掉 export module/import/export 关键字
  │ 保留 Q_OBJECT 类声明
  │ 输出 .moc_input.h (传统可解析头文件)
  │
  ▼ moc (原版，不变)
  │ 处理 .moc_input.h
  │ 输出 moc_raw.cpp (传统格式)
  │
  ▼ postprocess_moc.py
  │ 移除对输入文件的 #include
  │ 添加 module; + GMF includes + module NAME;
  │ 输出 .moc_module.cpp (模块实现单元)
  │
  ▼ g++ -std=c++2a -fmodules
  │ 编译 .moc_module.cpp → .o
  │ 与模块接口单元 .cppm 链接
```

### 编译模型

qlmoc 的模块编译模型：

- **模块接口单元** (`hello.cppm`)：`export module hellomod;`，GMF 中 `#include <qobject.h>`、`<qstring.h>`。Qt 类型通过 GMF 传播到 CMI。
- **模块实现单元** (`hello_impl.cpp`)：`module hellomod;`，无 GMF（不直接 `#include` 任何 Qt 头）。Qt 类型和基类（`QObject`、`QString`）来自 CMI 传播；`emit` 宏为 no-op，省略。
- **moc 输出** (`moc_module.cpp`)：`postprocess_moc.py` 生成，GMF 中包含 `<QtCore/qobjectdefs.h>`（Qt4）或 `<qmetaobject.h>`（Qt3），以及 `<string.h>`、`<cassert>`、`Q_ASSERT`/`QT_BEGIN_MOC_NAMESPACE` 宏定义。
- **Consumer TU** (`hello_main.cpp` for MODULE_BUILD)：仅 `import hellomod;`，不直接 `#include` 任何 Qt 头。所有 Qt 类型来自模块接口的 GMF 传播。

## 目录结构

```
qlmoc/
├── moc-wrapper.sh          ← 入口脚本，替代 $(MOC)
├── preprocess_cppm.py      ← 预处理 .cppm → .moc_input.h
├── postprocess_moc.py      ← 后处理 moc_raw → 模块实现单元
├── build.sh                ← 一键运行所有测试
├── tests/
│   ├── basic/              ← 基础 Q_OBJECT 类测试
│   │   ├── hello.h          ← 传统 .h 版（对照组）
│   │   ├── hello.cppm       ← 模块接口（含 Q_OBJECT）
│   │   ├── hello_impl.cpp   ← 模块实现（module hellomod;）
│   │   ├── hello_impl_traditional.cpp ← 传统实现（#include "hello.h"）
│   │   ├── hello_main.cpp   ← main 入口（传统/模块双模式）
│   │   └── build.sh         ← 编译 .h 版和 .cppm 版并对比
│   ├── cmp_qt3/            ← Qt3 透传一致性测试
│   │   └── diff_test.sh     ← 包装器输出 == 原始 moc 输出
│   └── cmp_qt4/            ← Qt4 透传一致性测试
│       └── diff_test.sh
└── README.md
```

## 工作流程

```bash
cd qlmoc
bash build.sh                  # 运行全部测试
bash tests/basic/build.sh      # 只跑基础测试
bash tests/cmp_qt3/diff_test.sh  # 只跑 Qt3 透传验证
```

## 关键约束

### 1. Qt 头必须在 GMF 中，不能在 module purview

将 `<qobject.h>`、`<qstring.h>` 等 Qt 头放入模块 purview（`export module M;` 之后），会导致 `<qglobal.h>` 牵扯的 STL 头（`<algorithm>`、`<utility>`、`<type_traits>` 等）获得模块链接，与全局模块中的 STL 声明冲突。Qt 必须留在 GMF（`module;` 之后）。

### 2. Consumer TU 不能直接 #include <qobject.h>/<qstring.h>

GCC 将模块接口 GMF 中的 `#include` 声明通过 CMI 传播给所有 import 方。当 consumer TU 同时 `import hellomod;` 和 `#include <qobject.h>`（→`<qstring.h>`）时，`QString::fromLocal8Bit(const char*, int = -1)` 等默认参数在同一作用域被声明两次，导致编译错误。

**解决**：consumer TU 不直接包含 Qt 头；`QObject`、`QString` 等类型通过 CMI 传播获得。非模块版本（传统 .h 路径）保持不变。

### 3. <qapplication.h> 不能放入 GMF

`<qglobal.h>` 中的 `static inline bool qIsNull(double d)` 包含匿名 `union U`（无链接），GCC 无法将此类实体通过 CMI 传播，报 hard error。因此 `<qapplication.h>`（→`<qwidget.h>`→`<qpoint.h>`→`qIsNull`）不能放入模块的 GMF。

### 4. GMF 传播机制

GCC 会将 GMF 中所有 `#include` 的声明（包括类、函数、变量）通过 CMI 传播给 import 方。这意味着：
- 模块接口的 GMF 声明在 importing TU 的全局模块作用域中可见
- 与直接 `#include` 相同的声明可能产生冲突（默认参数、重定义等）
- 手动编写的 GMF 声明（如 `struct Foo {};`）**不会**被传播

## 测试结果

全部测试在 GCC 16.1.1 + Qt3 3.5.0 + Qt4 4.8.7 上通过。

| 测试 | Qt3 | Qt4 |
|------|:---:|:---:|
| 透传 diff（wrapper == original moc） | ✅ | ✅ |
| 传统 .h 编译 + 链接 + 运行 | ✅ | ✅ |
| 模块 .cppm 编译 + 链接 + 运行 | ✅ | ✅ |

## 已知局限

| 场景 | 状态 |
|------|------|
| `export class { Q_OBJECT }` | ✅ |
| `export { class { Q_OBJECT } }` | ✅ |
| `import <header>` | ✅ 跳过 |
| `import module;` | ✅ 跳过 |
| 多个 Q_OBJECT 类 | ✅ |
| `export template<...> class { Q_OBJECT }` | ✅ |
| `export class Nested<...>::Inner { Q_OBJECT }` | ❌ 模板展开 |
| `export import other_module;` (重导出) | ❌ |
| `module M:part;` (分区) | ⚠️ 有限支持 |
| `import std;` | ❌ |
| Qt3 + Qt4 双构建 | ✅ 同一套脚本 |
| 普通 .h 透传 | ✅ diff 验证通过 |
| Consumer TU 不直接 #include Qt 头 | ⚠️ 约束见上 |
| `<qapplication.h>` 不可放 GMF | ⚠️ 约束见上 |

## 建议编译选项

```bash
# 关键编译标志
-std=c++2a -fmodules

# 建议抑制的警告（Qt 头在 GMF 中的固有行为）
-Wno-module-expose-global-module-tu-local
```

## 编译要求

- GCC ≥ 14 或 Clang ≥ 17（支持 C++20 `-fmodules`）
- Qt3 (可选): `/opt/qt338sh` 或自定义路径
- Qt4 (可选): `libqt4-dev` 或等价包
- Python 3
