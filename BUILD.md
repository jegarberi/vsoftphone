# Build Instructions

Linux is the only platform this has been built and tested on.

## Prerequisites

- GCC or Clang with C++17, CMake 3.23+, Python 3 (for Conan 2)
- Qt 6.4+ development packages: Core, Gui, Qml, Quick, QuickControls2,
  Network, Multimedia, Sql, plus the QML modules for Controls, Layouts,
  Dialogs and Qt5Compat.GraphicalEffects
- Libraries PJSIP is compiled against: OpenSSL, Opus, libvpx, V4L2, ALSA,
  SDL2, libuuid (openh264, opencore-amr and bcg729 are picked up if present)
- Optional: libsecret, to keep SIP passwords in the desktop keyring
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

## Makefile

`make help` lists the targets; `make build`, `make test`, `make run` and
`make docker` wrap the steps above.

## Portable build

```bash
make static        # build-static/dist, for glibc 2.35 and newer
make static-check  # prints the newest glibc version the binaries need
make static-test   # runs the call test from dist/ in an Ubuntu 22.04 container
```

`make static` compiles everything with [zig](https://ziglang.org) targeting an
older glibc (`GLIBC=2.35` by default; `make static GLIBC=2.31` for older) and
links PJSIP, OpenSSL, Opus and the C++ runtime statically. zig and the Qt
downloader are installed into `.venv` if they are not already available.

Three things stay dynamic, because they cannot sensibly be static:

- glibc, at the chosen version or newer
- ALSA (`libasound.so.2`), which loads the sound server's plugins at run time
- Qt, taken from the official Qt binaries and shipped in `dist/` next to the
  executable

So the result is a folder, not a single file. Copy `build-static/dist` to the
target machine and run `dist/microsip_clone`. The target needs the usual
desktop libraries Qt depends on (OpenGL, fontconfig, xkbcommon, D-Bus).

The portable build has no video and no keyring support: SDL, V4L2, the video
codecs and libsecret would each need to be built the same way. The video
controls are hidden in that build.

The host needs the ALSA, OpenGL and xkbcommon development headers; only those
are exposed to the compiler, so the system's own glibc headers never get in.

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
