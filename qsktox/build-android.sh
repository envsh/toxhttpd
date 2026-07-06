#!/bin/bash
set -e
export JAVA_HOME=/opt/jdk-17.0.13+11
export ANDROID_SDK_ROOT=/opt/android-sdk
export ANDROID_NDK_ROOT=/opt/android-ndk-r26b

QT_ANDROID=/opt/qt/6.7.3/android_arm64_v8a
QSK_ANDROID=/opt/qt/qskinny-arm64
QT_HOST=/opt/qt/6.7.3/gcc_64

BUILD_DIR=build-android
rm -rf "$BUILD_DIR"

# 1. cmake configure via qt-cmake
"$QT_ANDROID/bin/qt-cmake" -S . -B "$BUILD_DIR" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_PREFIX_PATH="$QSK_ANDROID" \
    -DQSkinny_DIR="$QSK_ANDROID/lib/cmake/QSkinny" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. build native .so only (not apk target)
cmake --build "$BUILD_DIR" -j$(nproc) --target qsktox

# 3. prepare android-build dir (copy .so + Qt deps, no Gradle)
APK_DIR="$BUILD_DIR/android-build"
LIB_DIR="$APK_DIR/libs/arm64-v8a"
mkdir -p "$LIB_DIR"
cp "$BUILD_DIR/libqsktox_arm64-v8a.so" "$LIB_DIR/"

# 平铺复制 qskinny 皮肤插件到 JNI lib 根目录（Android 无子目录）
cp "$QSK_ANDROID/lib/qskinny/plugins/skins/"*.so "$LIB_DIR/"
cp "$QSK_ANDROID/lib/qskinny/plugins/platforminputcontexts/"*.so "$LIB_DIR/"

"$QT_HOST/bin/androiddeployqt" \
    --input "$BUILD_DIR/android-qsktox-deployment-settings.json" \
    --output "$APK_DIR" \
    --deployment bundled \
    --builddir "$BUILD_DIR" \
    --aux-mode

# 复制自定义 Java 源码到 Android 构建目录
mkdir -p "$APK_DIR/src/main/java"
cp -R android/src/java/* "$APK_DIR/src/main/java/"
# 移除 renderscript.srcDirs 配置（build-tools 34+ 不再支持，且会干扰自定义 Java 源码）
sed -i '/renderscript\.srcDirs/d' "$APK_DIR/build.gradle"

# 替换包名为 qsktox.fedlet.io
sed -i 's/package="org\.qtproject\.example\.qsktox"/package="io.fedlet.qsktox"/' \
    "$APK_DIR/AndroidManifest.xml"

# Android 13+ gesture back bypasses Qt key handling
sed -i '/android:fullBackupOnly/a\        android:enableOnBackInvokedCallback="false"' \
    "$APK_DIR/AndroidManifest.xml"

# KeepAlive FG Service: permissions
sed -i '/<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" \/>/a\
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" \/>\n\
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_DATA_SYNC" \/>\n\
    <uses-permission android:name="android.permission.POST_NOTIFICATIONS" \/>\n\
    <uses-permission android:name="android.permission.READ_MEDIA_IMAGES" \/>' \
    "$APK_DIR/AndroidManifest.xml"

# KeepAlive FG Service: service declaration (before </application>)
sed -i '/<\/application>/ i\    <service\n\
        android:name="io.fedlet.mobutil.KeepAliveService"\n\
        android:foregroundServiceType="dataSync"\n\
        android:exported="false"\/>' \
    "$APK_DIR/AndroidManifest.xml"

# 4. append Qt-specific properties (missing from --aux-mode)
cat >> "$APK_DIR/gradle.properties" <<PROPS
androidBuildToolsVersion=34.0.0
androidCompileSdkVersion=android-33
androidNdkVersion=26.1.10909125
androidPackageName=io.fedlet.qsktox
buildDir=build
qtAndroidDir=${QT_ANDROID}/src/android/java
qtMinSdkVersion=23
qtTargetAbiList=arm64-v8a
qtTargetSdkVersion=34
PROPS

# 5. patch Gradle heap (low-memory device)
sed -i 's/-Xmx[0-9]*m/-Xmx386m/' "$APK_DIR/gradle.properties"

# 6. build debug APK (auto-signed with Android debug key)
GRADLE_OPTS="-Xmx386m" "$APK_DIR/gradlew" --no-daemon --max-workers=2 -p "$APK_DIR" assembleDebug

# 6. verify output
APK=$(find "$APK_DIR/build/outputs" -name "*.apk" 2>/dev/null | head -1)
if [ -n "$APK" ]; then
    echo "=== APK built: $APK ==="
    ls -lh "$APK"
fi

find build-android -name *qsktox*.so | xargs ls -lh

### todo kill java of this process forked
