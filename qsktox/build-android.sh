#!/bin/bash
set -e
export JAVA_HOME=/opt/jdk-17.0.13+11
export ANDROID_NDK=/opt/android-ndk
export ANDROID_SDK_ROOT=/opt/android-sdk

QT_ANDROID=/opt/qt/6.7.3/android_arm64_v8a
QSK_ANDROID=/opt/qt/qskinny-arm64

mkdir -p build-android && cd build-android
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_PREFIX_PATH="$QT_ANDROID;$QSK_ANDROID" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  make -j$(nproc)
  echo "=== native build done ==="
  GRADLE_PROP=build-android/android-build/gradle.properties
  test -f "$GRADLE_PROP" && sed -i 's/-Xmx[0-9]*m/-Xmx386m/' "$GRADLE_PROP"
  echo "Run: cd build-android/android-build && GRADLE_OPTS=\"-Xmx386m\" ./gradlew --no-daemon assembleDebug"
