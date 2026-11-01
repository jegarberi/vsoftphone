#!/bin/sh
# Builds what the Android app links, from checksummed source archives:
#
#   android/build-deps.sh <prefix> [--qt]
#
# OpenSSL and Opus go to <prefix>/deps, PJSIP to <prefix>/pjsip. With --qt,
# Qt itself is built too: <prefix>/qt-host holds the build tools (moc, rcc,
# androiddeployqt, ...) and <prefix>/qt-android the libraries for the phone.
# Dockerfile.android uses Qt's downloadable kits and skips --qt; F-Droid only
# accepts what it builds from source, so its recipe passes --qt.
#
# Needs ANDROID_SDK_ROOT and ANDROID_NDK_ROOT. ANDROID_ABI (arm64-v8a,
# armeabi-v7a, x86_64 or x86) defaults to arm64-v8a, ANDROID_API to 23.
set -eu

PREFIX=$(realpath -m "$1")
WITH_QT=${2:-}
ANDROID_ABI=${ANDROID_ABI:-arm64-v8a}
ANDROID_API=${ANDROID_API:-23}
JOBS=$(nproc)

QT_VERSION=6.4.3
QTBASE_SHA256=5087c9e5b0165e7bc3c1a4ab176b35d0cd8f52636aea903fa377bdba00891a60
QTDECLARATIVE_SHA256=67ded1b3b988135e3600916b34bb156fe3986c88420c6f87a6d0fd53521ba795
QTSHADERTOOLS_SHA256=c648b74ce3e769a20bd62792a589c540bceacfedf2f38fc7273d2c39782768f8
PJSIP_VERSION=2.17
PJSIP_SHA256=065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce
OPENSSL_VERSION=3.5.9
OPENSSL_SHA256=603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
OPUS_VERSION=1.5.2
OPUS_SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1

WORK=$PREFIX/src
mkdir -p "$WORK"
cd "$WORK"

# fetch <url> <sha256>: download once, check, unpack next to it
fetch() {
    file=$(basename "$1")
    [ -f "$file" ] || curl -fsSL -o "$file.part" "$1"
    [ -f "$file" ] || mv "$file.part" "$file"
    echo "$2  $file" | sha256sum -c -
    tar -xf "$file"
}

# --- What the NDK calls this CPU ---------------------------------------------
case "$ANDROID_ABI" in
    arm64-v8a) TRIPLE=aarch64-linux-android; OPENSSL_TARGET=android-arm64 ;;
    armeabi-v7a) TRIPLE=armv7a-linux-androideabi; OPENSSL_TARGET=android-arm ;;
    x86_64) TRIPLE=x86_64-linux-android; OPENSSL_TARGET=android-x86_64 ;;
    x86) TRIPLE=i686-linux-android; OPENSSL_TARGET=android-x86 ;;
    *) echo "unknown ANDROID_ABI $ANDROID_ABI"; exit 1 ;;
esac
TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64
NDK_PATH=$TOOLCHAIN/bin:$PATH
NDK_CC=$TOOLCHAIN/bin/$TRIPLE$ANDROID_API-clang
NDK_CXX=$TOOLCHAIN/bin/$TRIPLE$ANDROID_API-clang++

# --- OpenSSL (TLS and SRTP for PJSIP), static --------------------------------
if [ ! -f "$PREFIX/deps/lib/libssl.a" ]; then
    fetch https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz $OPENSSL_SHA256
    (cd openssl-$OPENSSL_VERSION \
        && export PATH="$NDK_PATH" \
        && ./Configure $OPENSSL_TARGET -D__ANDROID_API__=$ANDROID_API \
           no-shared no-tests no-engine no-dso --prefix="$PREFIX/deps" --libdir=lib > /dev/null \
        && make -j"$JOBS" build_libs > /dev/null && make install_dev > /dev/null)
fi

# --- Opus, static -------------------------------------------------------------
if [ ! -f "$PREFIX/deps/lib/libopus.a" ]; then
    fetch https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz $OPUS_SHA256
    (cd opus-$OPUS_VERSION \
        && PATH="$NDK_PATH" CC="$NDK_CC" CXX="$NDK_CXX" AR=$TOOLCHAIN/bin/llvm-ar \
           RANLIB=$TOOLCHAIN/bin/llvm-ranlib \
           ./configure --host=$TRIPLE --prefix="$PREFIX/deps" --disable-shared \
           --disable-doc --disable-extra-programs > /dev/null \
        && PATH="$NDK_PATH" make -j"$JOBS" install > /dev/null)
fi

# --- PJSIP, static: Android's own audio, camera and video codecs (MediaCodec),
# pictures drawn with OpenGL ES; libyuv is PJSIP's bundled copy. The phone's
# hardware video codecs are preferred to Android's software ones (PJSIP's
# default), which lag ----------------------------------------------------------
if [ ! -f "$PREFIX/pjsip/lib/pkgconfig/libpjproject.pc" ]; then
    fetch https://github.com/pjsip/pjproject/archive/refs/tags/$PJSIP_VERSION.tar.gz $PJSIP_SHA256
    (cd pjproject-$PJSIP_VERSION \
        && printf '#define PJ_CONFIG_ANDROID 1\n#include <pj/config_site_sample.h>\n#undef PJ_HAS_IPV6\n#define PJ_HAS_IPV6 1\n#undef PJMEDIA_HAS_VIDEO\n#define PJMEDIA_HAS_VIDEO 1\n#define PJMEDIA_AND_MEDIA_PRIO_SW_VID_ENC 0\n#define PJMEDIA_AND_MEDIA_PRIO_SW_VID_DEC 0\n' \
           > pjlib/include/pj/config_site.h \
        && { TARGET_ABI=$ANDROID_ABI APP_PLATFORM=android-$ANDROID_API \
             ./configure-android --use-ndk-cflags --prefix="$PREFIX/pjsip" \
             --with-ssl="$PREFIX/deps" --with-opus="$PREFIX/deps" \
             --disable-libwebrtc \
             --disable-opencore-amr --disable-bcg729 --disable-silk > configure.log 2>&1 \
             || { tail -40 configure.log; exit 1; }; } \
        && grep -E "SSL support|opus_repacketizer|sound device backend|OpenGL|MediaCodec|libyuv" configure.log \
        && make dep > /dev/null 2>&1 \
        && { make -j"$JOBS" lib > build.log 2>&1 || { grep -E "error|Error" build.log | head -30; exit 1; }; } \
        && make install > /dev/null)
fi

[ "$WITH_QT" = "--qt" ] || exit 0

# --- Qt from source: qtbase and qtdeclarative are what the app uses; Qt Quick
# is only built where qtshadertools' qsb tool is there -------------------------
QT_URL=https://download.qt.io/archive/qt/${QT_VERSION%.*}/$QT_VERSION/submodules
fetch $QT_URL/qtbase-everywhere-src-$QT_VERSION.tar.xz $QTBASE_SHA256
fetch $QT_URL/qtshadertools-everywhere-src-$QT_VERSION.tar.xz $QTSHADERTOOLS_SHA256
fetch $QT_URL/qtdeclarative-everywhere-src-$QT_VERSION.tar.xz $QTDECLARATIVE_SHA256
QTBASE=$WORK/qtbase-everywhere-src-$QT_VERSION
QT_COMMON="-release -nomake examples -nomake tests -no-warnings-are-errors"

# build_qt <host|android> <module> <configure command...>: once per module,
# remembered with a stamp file in the install directory
build_qt() {
    target=$1 module=$2
    shift 2
    stamp=$PREFIX/qt-$target/.built-$module
    [ -f "$stamp" ] && return 0
    rm -rf "build-$target-$module" && mkdir "build-$target-$module"
    (cd "build-$target-$module" && "$@" && cmake --build . --parallel "$JOBS" && cmake --install .)
    touch "$stamp"
}

# The tools that run on this machine while building for the phone. Headless,
# with Qt's bundled copies of its third-party libraries
build_qt host qtbase "$QTBASE/configure" -prefix "$PREFIX/qt-host" $QT_COMMON \
    -no-opengl -no-xcb -no-dbus -no-icu -no-glib -no-fontconfig \
    -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-freetype -qt-harfbuzz
for module in qtshadertools qtdeclarative; do
    build_qt host $module "$PREFIX/qt-host/bin/qt-configure-module" \
        "$WORK/$module-everywhere-src-$QT_VERSION"
done

build_qt android qtbase "$QTBASE/configure" -prefix "$PREFIX/qt-android" $QT_COMMON \
    -platform android-clang -android-abis "$ANDROID_ABI" \
    -android-ndk "$ANDROID_NDK_ROOT" -android-sdk "$ANDROID_SDK_ROOT" \
    -android-ndk-platform android-$ANDROID_API \
    -qt-host-path "$PREFIX/qt-host" \
    -openssl-runtime -- -DOPENSSL_ROOT_DIR="$PREFIX/deps"
for module in qtshadertools qtdeclarative; do
    build_qt android $module "$PREFIX/qt-android/bin/qt-configure-module" \
        "$WORK/$module-everywhere-src-$QT_VERSION"
done
