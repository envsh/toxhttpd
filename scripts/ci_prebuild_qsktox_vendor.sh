#!/bin/bash

set -e
set -x

# 下载 qsktox Android 构建所需的 vendor 预编译库并归位到 qsktox/vendor/
# 来源与 SHA256 记录见 qsktox/vendor/vendorinfos.md

pwd
PROJDIR=$PWD
VENDOR_DIR="$PROJDIR/qsktox/vendor"

mkdir -p "$VENDOR_DIR/include" "$VENDOR_DIR/lib/arm64-v8a"

# ── curl 8.19.0 + OpenSSL 3.6.x (XDcobra prebuilt) ──
# 下载到 /tmp，避免污染 workspace
wget -q -O /tmp/libcurl-android.zip \
    https://github.com/XDcobra/libcurl-ios-android-prebuilt-and-buildscripts/releases/download/v8.19.0-1/libcurl-android.zip

rm -rf /tmp/libcurl-android && mkdir -p /tmp/libcurl-android
unzip -q /tmp/libcurl-android.zip -d /tmp/libcurl-android

# 按文件名归位头文件。
# 注意：必须先对 find 结果判空再 dirname —— dirname '' 返回 '.'，
# 会触发 cp -r . 自拷贝事故（v8.19.0-1 CI 实踩）
CURL_H=$(find /tmp/libcurl-android -name curl.h -path '*curl*' | head -1)
if [ -z "$CURL_H" ]; then
    echo "FATAL: curl.h not found in libcurl-android.zip"
    exit 1
fi
cp -r "$(dirname "$CURL_H")" "$VENDOR_DIR/include/curl"

# 该 release 包不含 OpenSSL 头（v8.19.0-1 实测无 ssl.h）。
# qsktox 无人直接 #include <openssl/>，链接仅需 .so，缺失无影响 → 不安装 openssl 头

# arm64 .so 归位 —— 锚定 openssl 变体目录（zip 内可能有多种 SSL 后端变体，
# 无限定会被 find 遍历顺序随机命中其它变体，v8.19.0-1 CI 实踩）
for so in libcurl libssl libcrypto; do
    SRC=$(find /tmp/libcurl-android -name "${so}.so" \
        -path '*libcurl-openssl*' | grep -iE 'arm64|aarch64' | head -1)
    cp "$SRC" "$VENDOR_DIR/lib/arm64-v8a/"
done

# ── SQLite 3.53.3 (sqlite3.dart prebuilt) ──
wget -q -O "$VENDOR_DIR/lib/arm64-v8a/libsqlite3.so" \
    https://github.com/simolus3/sqlite3.dart/releases/download/sqlite3-3.4.0/libsqlite3.arm64.android.so

# ── SHA256 校验（摆错文件此处即失败）──
cd "$VENDOR_DIR/lib/arm64-v8a"
cat > /tmp/vendor.sha256 <<'EOF'
5ceb34ff92d9f6cd6b28901cc220bc2917a53e2614e8c9f6764af18c89063b88  libcurl.so
96d844acd9b264face6529b3502269577c1c14843cef4b55031deb114db8f0a7  libssl.so
953e2c534771b09e022bf4ef3d7d3ff4c18ba10241fb430036d1147399a28a90  libcrypto.so
e99515af1d7119fb61843ae5e597344e7f258563de3a7e5a3869f627aab2887b  libsqlite3.so
EOF
if ! sha256sum -c /tmp/vendor.sha256; then
    echo "=== DIAGNOSIS: actual hashes of downloaded files ==="
    sha256sum libcurl.so libssl.so libcrypto.so libsqlite3.so
    echo "=== DIAGNOSIS: all libcurl*.so candidates in zip ==="
    find /tmp/libcurl-android -name "libcurl*.so" -exec sha256sum {} \;
    exit 1
fi
