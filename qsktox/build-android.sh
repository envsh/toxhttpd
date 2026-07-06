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
mkdir -p "$APK_DIR/libs/arm64-v8a"
cp "$BUILD_DIR/libqsktox_arm64-v8a.so" "$APK_DIR/libs/arm64-v8a/"

"$QT_HOST/bin/androiddeployqt" \
    --input "$BUILD_DIR/android-qsktox-deployment-settings.json" \
    --output "$APK_DIR" \
    --deployment bundled \
    --builddir "$BUILD_DIR" \
    --aux-mode

# 4. append Qt-specific properties (missing from --aux-mode)
cat >> "$APK_DIR/gradle.properties" <<PROPS
androidBuildToolsVersion=34.0.0
androidCompileSdkVersion=android-33
androidNdkVersion=26.1.10909125
androidPackageName=org.qtproject.example.qsktox
buildDir=build
qtAndroidDir=${QT_ANDROID}/src/android/java
qtMinSdkVersion=23
qtTargetAbiList=arm64-v8a
qtTargetSdkVersion=34
PROPS

# 5. patch Gradle heap (low-memory device)
sed -i 's/-Xmx[0-9]*m/-Xmx386m/' "$APK_DIR/gradle.properties"

# 6. build debug APK (auto-signed with Android debug key)
GRADLE_OPTS="-Xmx386m" "$APK_DIR/gradlew" --no-daemon -p "$APK_DIR" assembleDebug

# 6. verify output
APK=$(find "$APK_DIR/build/outputs" -name "*.apk" 2>/dev/null | head -1)
if [ -n "$APK" ]; then
    echo "=== APK built: $APK ==="
    ls -lh "$APK"
fi
