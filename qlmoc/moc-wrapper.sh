#!/bin/bash
# moc-wrapper.sh — 替换 $(MOC) 调用，支持 .cppm 模块
# 对含 export module 的输入走预处理→moc→后处理管道
# 对普通 .h 透传原始 moc
#
# Usage: moc-wrapper.sh [options] <input> -o <output>

set -e
TOP=$(dirname "$0")

# 保存原始参数，用于透传
ORIG_ARGS=("$@")

# 解析参数
INPUT=""; OUTPUT=""; ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        -o) OUTPUT="$2"; shift 2 ;;
        -p) shift 2 ;;   # -p path prefix, skip
        -f) shift 2 ;;   # -f force include, skip
        -i) ARGS+=("$1"); shift ;;
        -k) ARGS+=("$1"); shift ;;
        -nw) ARGS+=("$1"); shift ;;
        -v)
            # 透传 -v 给真实 moc
            if [ -n "$REAL_MOC" ]; then
                exec "$REAL_MOC" "$@"
            else
                echo "moc-wrapper: -v needs REAL_MOC set" >&2
                exit 1
            fi
            ;;
        *)
            if [ -f "$1" ] && [ -z "$INPUT" ]; then
                INPUT="$1"
            fi
            shift ;;
    esac
done

if [ -z "$INPUT" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 [options] <input> -o <output>" >&2
    exit 1
fi

# 检测是否包含 export module（模块接口单元标记）
MODULE_NAME=$(grep -m1 'export module ' "$INPUT" 2>/dev/null | \
              sed 's/.*export module //;s/;.*//;s/ //g')

if [ -n "$MODULE_NAME" ]; then
    # 模块文件：走管道
    TEMP_INPUT=$(mktemp /tmp/mocwrap_XXXXX.moc_input.h)
    TEMP_RAW=$(mktemp /tmp/mocwrap_XXXXX.moc_raw.cpp)

    # 1. 预处理：.cppm → .moc_input.h（去掉模块关键字，留 Q_OBJECT）
    python3 "$TOP/preprocess_cppm.py" "$INPUT" "$TEMP_INPUT" \
        --module-name "$MODULE_NAME"

    # 2. 原始 moc
    if [ -n "$REAL_MOC" ]; then
        "$REAL_MOC" "${ARGS[@]}" "$TEMP_INPUT" -o "$TEMP_RAW"
    else
        echo "moc-wrapper: REAL_MOC not set for module input" >&2
        exit 1
    fi

    # 3. 后处理：moc_raw → 模块实现单元
    python3 "$TOP/postprocess_moc.py" "$TEMP_RAW" "$OUTPUT" \
        --module "$MODULE_NAME" \
        --input-file "$INPUT"

    rm -f "$TEMP_INPUT" "$TEMP_RAW"
else
    # 普通 .h 文件：透传原始 moc
    if [ -n "$REAL_MOC" ]; then
        exec "$REAL_MOC" "${ORIG_ARGS[@]}"
    else
        echo "moc-wrapper: REAL_MOC not set" >&2
        exit 1
    fi
fi
