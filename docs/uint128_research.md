# uint128 C/C++ 库选型调研

## 需求

- UUID/Hash 128-bit 固定宽度整数存储
- 从两个 64-bit 构造、比较、序列化（hex/字符串）
- 不用 Boost、不自己写

## 候选库对比

| 库 | Stars | 提交 | C++ 标准 | 32-bit | 字符串转换 | CI | 最后更新 | License |
|---|---|---|---|---|---|---|---|---|
| **wide-integer** | 221 | 1701 | C++14 | ✅ | ✅ `to_string()` | 6 workflows | 2026-07 | BSL-1.0 |
| **cppalliance/int128** | 12 | 1758 | C++14 | ✅ | ✅ | 5 workflows | 2026-07 | BSL-1.0（Boost 候选） |
| **SzigetiJ/biguint** | 12 | 137 | C++? | ✅ | ✅ | 2 workflows | 2024-03 | GPL-3.0 |
| **jibsen/wideint** | 7 | 73 | **C++20** | ✅ | ✅ `to_string()` | 1 workflow | 2024-03 | Apache-2.0 |
| **zkint.h** | 1 | 10 | C++11 | ✅ | ✅ `to_string()` | 1 workflow | 2026-06 | Apache-2.0 |
| **larkmjc/c128** | 7 | 3 | 纯 C | ✅ | ❌ | 无 | 2024-03 | 未标注 |
| **Abseil** | 18000+ | 大量 | C++11 | ✅ | ✅ `ToString()` | CI | 活跃 | Apache-2.0 |

## 结论

### 首选：wide-integer（ckormanyos/wide-integer）

- 8 年历史，221 stars，1701 commits
- 6 CI workflow（含 fuzzing/codecov/Sonar/Coverity）
- 单文件头库 `uintwide_t.h`，无依赖
- 支持 `to_string()`、运算符重载
- 明确支持嵌入式 bare-metal 环境
- BSL-1.0 许可

### 排除原因

| 库 | 排除原因 |
|---|---|
| cppalliance/int128 | Boost 候选库，用户不用 Boost |
| SzigetiJ/biguint | GPL-3.0（传染性许可） |
| jibsen/wideint | 要求 C++20 |
| zkint.h | 极度不成熟（1.5 月、1 star、10 commits） |
| c128 | 无字符串转换、无 CI、3 commits、2 年未更新 |
| Abseil | 太重（整个 abseil-cpp 依赖） |

## C++11 兼容性问题

wide-integer **不支持 C++11**，必须 **C++14**。

qltox 当前使用 `-std=c++11`（qltox.pro:75）。

测试结果：C++11 编译 wide-integer 产生 **617 个错误**，主要是 C++14 `constexpr` 成员函数重载（non-const 与 const 版本）在 C++11 中不允许。

### 解决方案

**方案 A（推荐）：改 `-std=c++11` 为 `-std=c++14`**
- C++14 是 C++11 超集，语法完全向后兼容
- Qt3 moc 只处理类声明语法，不受 C++ 标准版本影响
- 最简单

**方案 B：用 C++11 兼容的库**
- 仅 zkint.h 支持 C++11，但极度不成熟（1.5 月、1 star）

**方案 C：不用库，手写 struct**
- `struct { uint64_t hi, lo; }` + toHex() + 比较运算
- 对 UUID/hash 场景够用，零依赖

## 暂不实施

以上调研记录在案，待确认后再实施。
