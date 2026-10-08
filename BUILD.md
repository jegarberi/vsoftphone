# Build Instructions

Linux is the only platform this has been built and tested on.

## Prerequisites

- GCC or Clang with C++17, CMake 3.23+, Python 3 (for Conan 2)
- Qt 6.4+ development packages: Core, Gui, Qml, Quick, QuickControls2,
  Network, Sql, plus the QML modules for Controls (with the Material style),
  Layouts and Qt.labs.platform (the tray icon)
- Libraries PJSIP is compiled against: OpenSSL, Opus, libvpx, V4L2, ALSA,
  SDL2, libuuid (openh264, opencore-amr and bcg729 are picked up if present)
- autoconf, automake, libtool (PJSIP is built from source by Conan)

The `Dockerfile` lists the exact Debian package names.

## Build

```bash
python3 -m venv .venv
.venv/bin/pip install conan
.venv/bin/conan profile detect   # first time only

./build.sh
```

`build.sh` exports the local PJSIP recipe (`conan/pjsip`), builds PJSIP, then
configures and builds the app with the presets Conan generates. The first run
takes several minutes because PJSIP is compiled from source.

PJSIP's FFmpeg codec is disabled in the recipe: PJSIP does not compile against
FFmpeg 8 or newer. H.264 is still available through openh264, and VP8 through
libvpx.

## Run

```bash
./run_microsip.sh
```

The binary is `build/Release/microsip_clone`; the wrapper sets up the Conan
runtime environment and makes ALSA use the system configuration.

To see the SIP messages on the console, run with `MICROSIP_SIP_TRACE=1`. It is
off by default because the trace contains `Authorization` headers.

## Test

```bash
ctest --preset conan-release
```

`tests/call_flow_test.cpp` makes the SIP endpoint call and message its own UDP
port with the null audio device, then loads every QML window. It needs no SIP
server, network or sound card, and uses a throwaway profile under `~/.qttest`.

To also check media relaying, start a TURN server and name it:

```bash
MICROSIP_TEST_TURN=host:port MICROSIP_TEST_TURN_USER=user \
MICROSIP_TEST_TURN_PASS=pass build/Release/call_flow_test
```

## Android

```bash
make android                           # 64-bit ARM, most phones
make android ANDROID_ABI=armeabi-v7a   # old 32-bit phones
```

`Dockerfile.android` installs the Android SDK and NDK and Qt for Android,
cross-builds OpenSSL, Opus and PJSIP, and packages the app as an APK in
`build-android/`. It targets Android 6.0 (API 23): that is the oldest version
Qt 6.4 supports, and 6.4 is the oldest Qt this app builds with. The first
build downloads several gigabytes and accepts the Android SDK licence.

The APK is signed with Android's debug key, so it installs for testing
(`adb install build-android/microsip-clone-arm64-v8a.apk`); sign a release
with your own key. **The APK has been built but never run**: there was no
device or emulator to try it on, and the interface was designed for a
desktop, with separate windows for calls and settings. Audio only, like the
other cross builds.

## A SIP server to try it against

```bash
make asterisk        # starts Asterisk in Docker and prints the account settings
make asterisk-down
make asterisk-test   # runs the call test against it too
```

`asterisk/compose.yaml` runs an Asterisk with two accounts, 1001 and 1002,
that can call and message each other, an echo test on 600, a tone on 601, and
a busy-lamp list for each account. It uses the host's network (Linux only)
and listens on port 5080, leaving 5060 to the softphone. Passwords are random
on every start and are printed with the rest of the settings; run two copies
of the softphone on different machines, or one here and one elsewhere on the
network, and enter one account in each.

## A TURN relay

```bash
make turn        # starts coturn in Docker and prints the settings to enter
make turn-down
make turn-test   # runs the call test with media forced through it
```

`coturn/` holds a coturn configuration for the softphone
(`turnserver.conf`) and a compose file that runs it on the host's network
(Linux only): TURN over UDP on port 3478, relay ports 49160 to 49200. The
user is `softphone`; the password is random on every start, or the one in
`TURN_PASSWORD`, and is printed with the rest of the settings. Enter them
in Settings > Network, and tick "Always send media through the TURN relay"
to force every call through it.

A relay only helps against NAT from a machine that both the softphone and
the SIP server can reach, which usually means one with a public address;
open UDP 3478 and 49160 to 49200 there. If that machine has a private
address itself (most cloud servers), start it with
`EXTERNAL_IP=<its public address> make turn`.

## Screenshots

The test can save a picture of every window and of each tab:

```bash
MICROSIP_TEST_SCREENSHOTS=/tmp/shots QT_QUICK_BACKEND=software \
  MICROSIP_TEST_THEME="Solarized Light" build/Release/call_flow_test
```

`MICROSIP_TEST_THEME` is optional and names one of the themes in Settings.

## Makefile

`make help` lists the targets; `make build`, `make test`, `make run` and
`make docker` wrap the steps above.

## Portable and cross builds

```bash
make static        # Linux:   build-static/dist, for glibc 2.35 and newer
make windows       # Windows: build-windows/dist, x86_64
make macos MACOS_SDK=/path/to/MacOSX.sdk   # macOS: build-macos/dist

make static-check  # newest glibc version the Linux binaries need
make static-test   # runs the call test from the Linux folder in Ubuntu 22.04
make windows-test  # runs the call test from the Windows folder under wine
```

All three compile everything with [zig](https://ziglang.org) and link PJSIP,
OpenSSL, Opus and the C++ runtime statically. Qt comes from the official Qt
binaries for the target platform and is shipped next to the executable, so
each build produces a folder to copy to the target machine, not a single
file. zig and the Qt downloader are installed into `.venv` when missing, at
the versions pinned in the Makefile;
downloads are kept in `build-cache/` and shared between the builds.

Per platform:

- **Linux**: targets an older glibc (`GLIBC=2.35` by default; pass
  `GLIBC=2.31` for older). glibc and ALSA (`libasound.so.2`, which loads the
  sound server's plugins) stay dynamic. The target needs the usual desktop
  libraries Qt depends on (OpenGL, fontconfig, xkbcommon, D-Bus). The host
  needs the ALSA, OpenGL and xkbcommon development headers; only those are
  exposed to the compiler, so the system's own glibc headers never get in.
- **Windows**: uses Qt's `llvm-mingw` kit, which is built with the same C++
  runtime zig uses. Run `dist\microsip_clone.exe`.
- **macOS**: zig does not include Apple's frameworks, so this needs a
  `MacOSX.sdk` from a Mac with Xcode. `MACOS_ARCH` (`aarch64` or `x86_64`)
  and `MACOS_MIN` select the target. **This target has not been run**: there
  was no SDK to build against.

These builds have no video: SDL, the capture backends and the video codecs
would each need to be built the same way. The video controls are hidden in
them.

Source downloads are checked against the SHA-256 values in the Makefile, so
changing `PJSIP_VERSION`, `OPENSSL_VERSION` or `OPUS_VERSION` means changing
the matching `*_SHA256` too. `make static-clean` removes the build folders;
`make cache-clean` removes the downloads and Qt kits as well.

## Docker

```bash
docker build -t microsip-clone .
```

Builds the Linux binary and runs the test inside Debian bookworm.

## Troubleshooting

- **PJSIP fails to build**: read the Conan output; a missing system library or
  an incompatible FFmpeg are the usual causes.
- **CMake cannot find Qt6**: pass `-DCMAKE_PREFIX_PATH=/path/to/qt6`.
- **Stale CMake cache after moving the checkout**: `cmake --fresh --preset conan-release`.
