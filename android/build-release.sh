#!/bin/sh
# The release APK, built entirely from source the way F-Droid builds it:
#
#   ANDROID_SDK_ROOT=... ANDROID_NDK_ROOT=... android/build-release.sh
#
# The APK is unsigned (F-Droid signs it with its own key):
#   build-release/app/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
# ANDROID_ABI picks the CPU (default arm64-v8a). The first run builds Qt and
# takes a while; later runs reuse build-release/deps.
set -eu
cd "$(dirname "$0")/.."

ANDROID_API=${ANDROID_API:-23}
DEPS=$PWD/build-release/deps
android/build-deps.sh "$DEPS" --qt

PKG_CONFIG_LIBDIR=$DEPS/pjsip/lib/pkgconfig "$DEPS/qt-android/bin/qt-cmake" \
    -S . -B build-release/app -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="$DEPS/qt-host" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_PLATFORM=android-$ANDROID_API \
    -DVSOFTPHONE_PJSIP_PKGCONFIG=ON \
    -DVSOFTPHONE_EXTRA_LIBRARY_DIRS="$DEPS/deps/lib"
cmake --build build-release/app

# Qt 6.4's apk target only makes debug APKs
"$DEPS/qt-host/bin/androiddeployqt" \
    --input build-release/app/android-vsoftphone-deployment-settings.json \
    --output build-release/app/android-build --release --gradle
