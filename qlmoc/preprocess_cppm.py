#!/usr/bin/env python3
"""
preprocess_cppm.py — 将 .cppm 模块接口转换为 moc 可解析的传统头文件

变换规则：
  - module;              → 保留（GMF）
  - #include ...         → 保留
  - export module NAME;  → 注释掉，记录模块名
  - import ...;          → 注释掉
  - export class ...     → class ...（去掉 export）
  - export template ...  → template ...（去掉 export）
  - export { ... }       → { ... }（去掉 export）
  - 其他                  → 原样保留
"""

import re
import sys

def preprocess(text):
    lines = text.splitlines()
    out = []
    module_name = ""
    state = "gmf"  # gmf | module_body

    for line in lines:
        s = line.strip()

        # 保留 GMF 中的所有行（module;, #include, 预处理指令, 空白）
        if state == "gmf":
            out.append(line)
            if s == "module;":
                # module; 之后仍然是 GMF，直到遇到 export module
                continue
            if s.startswith("export module ") or s.startswith("module "):
                # export module NAME; — 记录模块名
                m = re.match(r'(?:export\s+)?module\s+(\S+);', s)
                if m:
                    module_name = m.group(1)
                out.append(f"// {line}")
                state = "module_body"
                continue
            if s.startswith("import "):
                out.append(f"// {line}")
                continue
            continue  # 其他行（#include, 宏定义, 空白）已追加

        if state == "module_body":
            # export class → class
            m = re.match(r'^(\s*)export\s+(class\s+)', line)
            if m:
                out.append(m.group(1) + m.group(2) + line[m.end():])
                continue

            # export template → template
            m = re.match(r'^(\s*)export\s+(template\s+)', line)
            if m:
                out.append(m.group(1) + m.group(2) + line[m.end():])
                continue

            # export { → { (去掉 export, 加注释)
            m = re.match(r'^(\s*)export\s*(\{)', line)
            if m:
                out.append(m.group(1) + m.group(2) + " // export")
                continue

            # export struct/union/enum → 同上
            m = re.match(r'^(\s*)export\s+(struct|union|enum\s+)', line)
            if m:
                out.append(m.group(1) + m.group(2) + line[m.end():])
                continue

            # import ...; → 注释掉
            if s.startswith("import ") and s.endswith(";"):
                out.append(f"// {line}")
                continue

            out.append(line)
            continue

    return "\n".join(out), module_name


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Preprocess .cppm for moc")
    parser.add_argument("input", help="Input .cppm file")
    parser.add_argument("output", help="Output header file for moc")
    parser.add_argument("--module-name", help="Module name (overrides auto-detect)")
    args = parser.parse_args()

    with open(args.input, "r") as f:
        text = f.read()

    result, detected_name = preprocess(text)
    module_name = args.module_name or detected_name

    with open(args.output, "w") as f:
        f.write("// Preprocessed from " + args.input + "\n")
        f.write("// Module: " + module_name + "\n")
        if module_name:
            f.write("// See also: " + module_name + ".cppm\n")
        f.write("\n")
        f.write(result)

    if not module_name:
        print("Warning: no module name found in " + args.input, file=sys.stderr)


if __name__ == "__main__":
    main()
