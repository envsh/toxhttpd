# Qt3 qmake 变量命名空间

在 Qt3 qmake 1.07a 中，有四种不同的变量/宏语法，行为不同：

| 语法 | 类型 | 解析时机 | 示例 |
|------|------|----------|------|
| `$$(VAR)` | 环境变量 | **qmake 时**读取环境变量并原地展开为字符串 | `$$(QTDIR)` → `/opt/qt338sh`（已固定进 Makefile） |
| `$$VAR` | qmake 变量 | **qmake 时**展开为当前 qmake 变量值 | 可用于传递命令行参数 |
| `$(VAR)` | Makefile 变量 | **make 时**展开（由 make 解析） | `$(CC)`、`$(CXXFLAGS)` |
| `$VAR` | shell 变量 | **透传**给 shell 规则运行时展开 | `$PPID` → shell 父进程 PID |

### 关键区别

- `$$(QTDIR)` 在 qmake 处理 `.pro` 文件的瞬间读取环境变量，**展开后的绝对路径**直接嵌入生成的 Makefile，之后改环境变量不影响 Makefile。
- `$(QTDIR)` 在 `make` 运行时由 shell 解析，如果 Makefile 规则中有 `$(QTDIR)`，运行时才读取环境变量。
- `$$(VAR)` 语法在 Qt3 qmake 中**只能读**环境变量，不能写。无法通过 `.pro` 设置环境变量供 Makefile 运行时使用。
