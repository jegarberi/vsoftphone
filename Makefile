# make help lists the targets.

SHELL := /bin/bash
VENV  := $(CURDIR)/.venv
JOBS  ?= $(shell nproc)

.DEFAULT_GOAL := help

# Build tools installed into the venv, pinned so a build is repeatable
CONAN_VERSION ?= 2.33.0
ZIG_VERSION   ?= 0.16.0
AQT_VERSION   ?= 3.3.0

##@ Regular build (system libraries, Conan-built PJSIP; see BUILD.md)

.PHONY: build
build: $(VENV)/bin/conan ## Build the app and the test into build/Release
	./build.sh

.PHONY: test
test: build ## Build, then run the loopback call test
	ctest --preset conan-release --output-on-failure

.PHONY: run
run: ## Start the app
	./run_microsip.sh

.PHONY: docker
docker: ## Build and test inside Debian bookworm
	docker build -t microsip-clone .

.PHONY: clean
clean: ## Remove build/
	rm -rf build

$(VENV)/bin/python:
	python3 -m venv $(VENV)

$(VENV)/bin/conan: | $(VENV)/bin/python
	$(VENV)/bin/pip install -q conan==$(CONAN_VERSION)
	$(VENV)/bin/conan profile detect --exist-ok

##@ Portable and cross builds (zig)
#
# One recipe, three platforms. Everything we compile is built with zig and
# linked statically: the app, PJSIP, OpenSSL, Opus and the C++ runtime. Qt
# comes from the official Qt binaries for the target platform and is shipped
# next to the executable, so each build produces a folder:
#
#   make static    build-static/dist    Linux, glibc $(GLIBC) or newer
#   make windows   build-windows/dist   Windows x86_64
#   make macos     build-macos/dist     macOS (needs MACOS_SDK, see below)
#
# On Linux glibc and ALSA also stay dynamic (ALSA loads the sound server's
# plugins). Video is left out of all three: SDL, the capture backends and the
# video codecs would each need the same from-source treatment.

GLIBC           ?= 2.35
QT_VERSION      ?= 6.8.3
PJSIP_VERSION   ?= 2.17
OPENSSL_VERSION ?= 3.5.9
OPUS_VERSION    ?= 1.5.2
# Checked after download; change these together with the versions
PJSIP_SHA256    ?= 065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce
OPENSSL_SHA256  ?= 603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
OPUS_SHA256     ?= 65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
# macOS: zig has no Apple frameworks, so point this at a MacOSX.sdk
MACOS_SDK       ?=
MACOS_ARCH      ?= aarch64
MACOS_MIN       ?= 12.0

PLATFORM ?= linux
CACHE    := $(CURDIR)/build-cache
DL       := $(CACHE)/dl
QT_HOST  := $(CACHE)/qt/$(QT_VERSION)/gcc_64

ifeq ($(PLATFORM),linux)
  S              := $(CURDIR)/build-static
  ZIG_TARGET     := x86_64-linux-gnu.$(GLIBC)
  OPENSSL_TARGET := linux-x86_64
  QT_KIT         := linux desktop $(QT_VERSION) linux_gcc_64
  QT             := $(QT_HOST)
  HOST           := $(S)/host
  HOST_STAMP     := $(S)/.host
  DEP_CFLAGS     := -I$(HOST)/include
  DEP_LDFLAGS    := -L$(HOST)/lib -Wl,--allow-shlib-undefined
  # Qt's OpenGL and xkb lookups must not put /usr/include on the path
  CMAKE_PLATFORM  = $(foreach v,OPENGL OPENGL_EGL OPENGL_GLX OPENGL_GLU OPENGL_GLES2 OPENGL_GLES3 XKB,-D$(v)_INCLUDE_DIR=$(HOST)/include) \
                    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_INSTALL_RPATH='$$ORIGIN/lib'
else ifeq ($(PLATFORM),windows)
  S              := $(CURDIR)/build-windows
  ZIG_TARGET     := x86_64-windows-gnu
  CONF_HOST      := --host=x86_64-w64-mingw32
  OPENSSL_TARGET := mingw64
  # The llvm-mingw kit is built with clang and libc++, as zig's output is
  QT_KIT         := windows desktop $(QT_VERSION) win64_llvm_mingw
  QT             := $(CACHE)/qt/$(QT_VERSION)/llvm-mingw_64
  EXE            := .exe
  # What a static OpenSSL needs on Windows; PJSIP's configure links its SSL
  # test without them and, failing, drops TLS and Opus
  DEP_LIBS       := -lcrypto -lws2_32 -lcrypt32 -lbcrypt -ladvapi32 -luser32 -lgdi32
  # CMake wants a resource compiler for Windows even with no .rc files
  RC             ?= $(shell command -v llvm-windres || command -v x86_64-w64-mingw32-windres || echo /usr/bin/true)
  CMAKE_PLATFORM  = -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=AMD64 \
                    -DCMAKE_RC_COMPILER=$(RC) -DQT_HOST_PATH=$(QT_HOST)
else ifeq ($(PLATFORM),macos)
  S              := $(CURDIR)/build-macos
  ZIG_TARGET     := $(MACOS_ARCH)-macos.$(MACOS_MIN)
  CONF_HOST      := --host=$(MACOS_ARCH)-apple-darwin
  OPENSSL_TARGET := $(if $(filter aarch64,$(MACOS_ARCH)),darwin64-arm64-cc,darwin64-x86_64-cc)
  QT_KIT         := mac desktop $(QT_VERSION) clang_64
  QT             := $(CACHE)/qt/$(QT_VERSION)/macos
  ZIG_SYSROOT    := --sysroot=$(MACOS_SDK) -isystem $(MACOS_SDK)/usr/include \
                    -F$(MACOS_SDK)/System/Library/Frameworks -L$(MACOS_SDK)/usr/lib
  CMAKE_PLATFORM  = -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_SYSTEM_PROCESSOR=$(if $(filter aarch64,$(MACOS_ARCH)),arm64,x86_64) \
                    -DCMAKE_OSX_SYSROOT=$(MACOS_SDK) -DCMAKE_OSX_DEPLOYMENT_TARGET=$(MACOS_MIN) \
                    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_INSTALL_RPATH='@executable_path/lib' \
                    -DQT_HOST_PATH=$(QT_HOST)
else
  $(error PLATFORM must be linux, windows or macos)
endif

# Versioned, so changing a version rebuilds it and never mixes in the
# headers of the one before
PREFIX       := $(S)/deps-openssl$(OPENSSL_VERSION)-opus$(OPUS_VERSION)
PJSIP_PREFIX := $(S)/pjsip-$(PJSIP_VERSION)-install
OPENSSL_STAMP := $(S)/.openssl-$(OPENSSL_VERSION)
OPUS_STAMP    := $(S)/.opus-$(OPUS_VERSION)
PJSIP_STAMP   := $(S)/.pjsip-$(PJSIP_VERSION)-openssl$(OPENSSL_VERSION)-opus$(OPUS_VERSION)
DIST     := $(S)/dist
ZCC      := $(S)/bin/zig-cc
ZCXX     := $(S)/bin/zig-c++
QT_STAMP := $(CACHE)/.qt-$(PLATFORM)-$(QT_VERSION)

# zig from PATH, else the copy pip puts in the venv
ZIG ?= $(shell command -v zig 2>/dev/null || echo "$(VENV)/bin/python -m ziglang")

ZIG_ENV := CC=$(ZCC) CXX=$(ZCXX) AR=$(S)/bin/zig-ar RANLIB=$(S)/bin/zig-ranlib

.PHONY: static
static: ## Linux folder for glibc 2.35 and newer (GLIBC= to change) in build-static/dist
	@$(MAKE) --no-print-directory portable PLATFORM=linux

.PHONY: windows
windows: ## Windows x86_64 folder in build-windows/dist
	@$(MAKE) --no-print-directory portable PLATFORM=windows

.PHONY: macos
macos: ## macOS folder in build-macos/dist; needs MACOS_SDK=/path/to/MacOSX.sdk (untested)
	@test -d "$(MACOS_SDK)/System/Library/Frameworks" || { \
	  echo "zig does not ship Apple's frameworks. Pass the SDK from a Mac with Xcode:"; \
	  echo "  make macos MACOS_SDK=/path/to/MacOSX.sdk"; exit 1; }
	@$(MAKE) --no-print-directory portable PLATFORM=macos

.PHONY: static-check
static-check: ## Show the newest glibc version the Linux folder needs
	@for f in build-static/dist/microsip_clone build-static/dist/call_flow_test; do \
	  printf '%s needs glibc ' "$$(basename $$f)"; \
	  objdump -T $$f | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1; \
	done

# The image only holds the desktop libraries Qt needs, so it is built once
.PHONY: static-test
static-test: ## Run the call test from the Linux folder inside Ubuntu 22.04 (glibc 2.35)
	@printf 'FROM ubuntu:22.04\nRUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends libasound2 libgl1 libegl1 libopengl0 libfontconfig1 libfreetype6 libxkbcommon0 libdbus-1-3 libglib2.0-0 && rm -rf /var/lib/apt/lists/*\n' \
	  | docker build -q -t microsip-static-test - >/dev/null
	docker run --rm -v $(CURDIR)/build-static/dist:/app:ro microsip-static-test \
	  bash -c 'ldd --version | head -1 && timeout 300 /app/call_flow_test'

.PHONY: windows-test
windows-test: ## Run the call test from the Windows folder under wine
	cd build-windows/dist && WINEDEBUG=-all QT_QPA_PLATFORM=offscreen wine call_flow_test.exe

.PHONY: static-clean
static-clean: ## Remove build-static/, build-windows/ and build-macos/
	rm -rf build-static build-windows build-macos

.PHONY: cache-clean
cache-clean: ## Remove build-cache/ (the downloaded sources and Qt kits, about 3 GB)
	rm -rf build-cache

.PHONY: portable
portable: $(DIST)/microsip_clone$(EXE)
	@echo "$(PLATFORM) build ready: $(DIST)"

# --- toolchain -------------------------------------------------------------

# Wrappers, because build systems want a compiler that is one word.
# -fno-sanitize=undefined: zig turns UBSan traps on by default.
$(S)/.tools: | $(VENV)/bin/python
	@mkdir -p $(S)/bin $(DL)
	@command -v zig >/dev/null 2>&1 || $(VENV)/bin/python -c 'import ziglang' 2>/dev/null || $(VENV)/bin/pip install -q ziglang==$(ZIG_VERSION)
	@printf '#!/bin/sh\nexec $(ZIG) cc -target $(ZIG_TARGET) $(ZIG_SYSROOT) -fno-sanitize=undefined "$$@"\n'  > $(ZCC)
	@printf '#!/bin/sh\nexec $(ZIG) c++ -target $(ZIG_TARGET) $(ZIG_SYSROOT) -fno-sanitize=undefined "$$@"\n' > $(ZCXX)
	@printf '#!/bin/sh\nexec $(ZIG) ar "$$@"\n'     > $(S)/bin/zig-ar
	@printf '#!/bin/sh\nexec $(ZIG) ranlib "$$@"\n' > $(S)/bin/zig-ranlib
	@chmod +x $(S)/bin/*
	@$(ZCC) --version | head -1
	@touch $@

# Linux only: the few system pieces linked dynamically. Just their own
# headers are exposed: putting /usr/include on the path would replace zig's
# glibc $(GLIBC) headers with the system's newer ones.
$(S)/.host: $(S)/.tools
	@mkdir -p $(HOST)/include $(HOST)/lib
	@for d in alsa GL KHR EGL GLES2 GLES3 xkbcommon; do \
	  test -e /usr/include/$$d || { echo "missing /usr/include/$$d (install the ALSA, OpenGL and xkbcommon development packages)"; exit 1; }; \
	  ln -sfn /usr/include/$$d $(HOST)/include/$$d; \
	done
	@ln -sfn $$(pkg-config --variable=libdir alsa)/libasound.so $(HOST)/lib/libasound.so
	@touch $@

# --- dependencies ----------------------------------------------------------

# $(call fetch,url,sha256): download to a temporary name and keep the file
# only if its checksum matches
define fetch
	curl -fL -o $@.part $(1)
	echo "$(2)  $@.part" | sha256sum -c - || { rm -f $@.part; exit 1; }
	mv $@.part $@
endef

$(DL)/openssl-$(OPENSSL_VERSION).tar.gz: | $(S)/.tools
	$(call fetch,https://github.com/openssl/openssl/releases/download/openssl-$(OPENSSL_VERSION)/openssl-$(OPENSSL_VERSION).tar.gz,$(OPENSSL_SHA256))

$(DL)/opus-$(OPUS_VERSION).tar.gz: | $(S)/.tools
	$(call fetch,https://downloads.xiph.org/releases/opus/opus-$(OPUS_VERSION).tar.gz,$(OPUS_SHA256))

$(DL)/pjproject-$(PJSIP_VERSION).tar.gz: | $(S)/.tools
	$(call fetch,https://github.com/pjsip/pjproject/archive/refs/tags/$(PJSIP_VERSION).tar.gz,$(PJSIP_SHA256))

$(OPENSSL_STAMP): $(DL)/openssl-$(OPENSSL_VERSION).tar.gz $(S)/.tools
	rm -rf $(S)/openssl-$(OPENSSL_VERSION) && tar -xzf $< -C $(S)
	cd $(S)/openssl-$(OPENSSL_VERSION) && $(ZIG_ENV) ./Configure $(OPENSSL_TARGET) \
	  no-shared no-tests no-engine no-dso \
	  --prefix=$(PREFIX) --libdir=lib > $(S)/openssl.log 2>&1
	$(MAKE) -C $(S)/openssl-$(OPENSSL_VERSION) -j$(JOBS) build_libs >> $(S)/openssl.log 2>&1
	$(MAKE) -C $(S)/openssl-$(OPENSSL_VERSION) install_dev >> $(S)/openssl.log 2>&1
	@touch $@

$(OPUS_STAMP): $(DL)/opus-$(OPUS_VERSION).tar.gz $(S)/.tools
	rm -rf $(S)/opus-$(OPUS_VERSION) && tar -xzf $< -C $(S)
	cd $(S)/opus-$(OPUS_VERSION) && $(ZIG_ENV) ./configure $(CONF_HOST) --prefix=$(PREFIX) \
	  --disable-shared --disable-doc --disable-extra-programs > $(S)/opus.log 2>&1
	$(MAKE) -C $(S)/opus-$(OPUS_VERSION) -j$(JOBS) install >> $(S)/opus.log 2>&1
	@touch $@

# Audio only; see the note at the top of this section.
# OBJEXT: configure sees zig name Windows objects .obj, but PJSIP's makefiles
# list every object as .o and compile with an explicit -o
$(PJSIP_STAMP): $(DL)/pjproject-$(PJSIP_VERSION).tar.gz $(OPENSSL_STAMP) $(OPUS_STAMP) $(HOST_STAMP)
	rm -rf $(S)/pjproject-$(PJSIP_VERSION) $(PJSIP_PREFIX) && tar -xzf $< -C $(S)
	@# IPv6 is off by default; in config_site.h so the installed headers agree
	echo '#define PJ_HAS_IPV6 1' >> $(S)/pjproject-$(PJSIP_VERSION)/pjlib/include/pj/config_site.h
	@# Windows headers are lower case; it only matters on a case-sensitive disk
	sed -i -E 's/^(#[[:space:]]*include <)([A-Za-z_]+\.h>)/\1\L\2/' $(S)/pjproject-$(PJSIP_VERSION)/pjmedia/src/pjmedia-audiodev/wmme_dev.c
	cd $(S)/pjproject-$(PJSIP_VERSION) && $(ZIG_ENV) \
	  CFLAGS="-O2 -DNDEBUG $(DEP_CFLAGS)" CXXFLAGS="-O2 -DNDEBUG $(DEP_CFLAGS)" \
	  LDFLAGS="-L$(PREFIX)/lib $(DEP_LDFLAGS)" LIBS="$(DEP_LIBS)" \
	  ./aconfigure $(CONF_HOST) --prefix=$(PJSIP_PREFIX) --disable-shared \
	  --with-ssl=$(PREFIX) --with-opus=$(PREFIX) \
	  --disable-video --disable-sdl --disable-v4l2 --disable-ffmpeg \
	  --disable-openh264 --disable-vpx --disable-libyuv --disable-libwebrtc \
	  --disable-opencore-amr --disable-bcg729 --disable-silk --disable-libuuid \
	  > $(S)/pjsip.log 2>&1
	sed -i 's/^export OBJEXT := .*/export OBJEXT := .o/' $(S)/pjproject-$(PJSIP_VERSION)/build/cc-auto.mak
	@grep -q "SSL support enabled" $(S)/pjsip.log && grep -q "opus_repacketizer_get_size in -lopus... yes" $(S)/pjsip.log || \
	  { echo "PJSIP's configure did not pick up OpenSSL or Opus, see $(S)/pjsip.log"; exit 1; }
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) dep >> $(S)/pjsip.log 2>&1
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) -j$(JOBS) lib >> $(S)/pjsip.log 2>&1
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) install >> $(S)/pjsip.log 2>&1
	@touch $@

# Official Qt binaries for the target. A cross build also needs the Linux
# kit: moc, rcc and the QML tools run on this machine.
$(QT_STAMP): | $(S)/.tools
	@$(VENV)/bin/python -c 'import aqt' 2>/dev/null || $(VENV)/bin/pip install -q aqtinstall==$(AQT_VERSION)
	@test -d $(QT_HOST) || $(VENV)/bin/aqt install-qt linux desktop $(QT_VERSION) linux_gcc_64 -O $(CACHE)/qt > $(CACHE)/qt-linux.log 2>&1
	@test -d $(QT) || $(VENV)/bin/aqt install-qt $(QT_KIT) -O $(CACHE)/qt > $(CACHE)/qt-$(PLATFORM).log 2>&1
	@touch $@

# --- the app ---------------------------------------------------------------

SOURCES := $(shell find src tests -type f) CMakeLists.txt

$(S)/app/microsip_clone$(EXE): $(SOURCES) $(PJSIP_STAMP) $(QT_STAMP) $(HOST_STAMP)
	@# CMake caches what pkg-config said about PJSIP; forget it in case PJSIP changed
	@rm -f $(S)/app/CMakeCache.txt
	PKG_CONFIG_LIBDIR=$(PJSIP_PREFIX)/lib/pkgconfig cmake -S . -B $(S)/app \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_C_COMPILER=$(ZCC) -DCMAKE_CXX_COMPILER=$(ZCXX) \
	  -DCMAKE_AR=$(S)/bin/zig-ar -DCMAKE_RANLIB=$(S)/bin/zig-ranlib \
	  -DCMAKE_PREFIX_PATH=$(QT) \
	  -DMICROSIP_PJSIP_PKGCONFIG=ON \
	  -DCMAKE_EXE_LINKER_FLAGS="-L$(PREFIX)/lib $(DEP_LDFLAGS)" \
	  $(CMAKE_PLATFORM) > $(S)/app.log 2>&1
	@! grep -q -- '-isystem /usr/include\|-I/usr/include' $(S)/app/CMakeFiles/*/flags.make || \
	  { echo "system headers leaked into the build, see $(S)/app/CMakeFiles/*/flags.make"; exit 1; }
	cmake --build $(S)/app -j$(JOBS) >> $(S)/app.log 2>&1

# The executable plus the Qt runtime it loads, found through qt.conf
$(DIST)/microsip_clone$(EXE): $(S)/app/microsip_clone$(EXE)
	rm -rf $(DIST) && mkdir -p $(DIST)
	cp $(S)/app/microsip_clone$(EXE) $(S)/app/call_flow_test$(EXE) $(DIST)/
ifeq ($(PLATFORM),linux)
	mkdir -p $(DIST)/lib && cp -a $(QT)/lib/*.so.* $(DIST)/lib/
else ifeq ($(PLATFORM),windows)
	cp $(QT)/bin/*.dll $(DIST)/
else
	mkdir -p $(DIST)/lib && cp -a $(QT)/lib/*.framework $(DIST)/lib/
endif
	cp -a $(QT)/plugins $(QT)/qml $(DIST)/
	find $(DIST) -name '*.debug' -delete
	printf '[Paths]\nPrefix = .\nPlugins = plugins\nQmlImports = qml\n' > $(DIST)/qt.conf

##@ Android (in Docker)

ANDROID_ABI ?= arm64-v8a

.PHONY: android
android: ## Build an APK for Android 6.0 and newer into build-android/ (ANDROID_ABI=armeabi-v7a for old 32-bit phones)
	docker build -f Dockerfile.android --build-arg ANDROID_ABI=$(ANDROID_ABI) -t microsip-android-$(ANDROID_ABI) .
	mkdir -p build-android
	docker run --rm --user $$(id -u):$$(id -g) -v $(CURDIR)/build-android:/out microsip-android-$(ANDROID_ABI)

##@ Test server (Asterisk in Docker, Linux)

ASTERISK := docker compose -f asterisk/compose.yaml

.PHONY: asterisk
asterisk: ## Start an Asterisk with two accounts and print the settings for each softphone
	$(ASTERISK) up -d --build --quiet-pull
	@sleep 3
	@$(ASTERISK) logs --no-log-prefix asterisk | sed -n '/=== Softphone settings/,/^====*$$/p' | tail -n 14

.PHONY: asterisk-down
asterisk-down: ## Stop it
	$(ASTERISK) down

# Fixed passwords here: the test has to know them
.PHONY: asterisk-test
asterisk-test: ## Run the call test against that Asterisk as well (registration, calls, messages, busy lamps)
	PASSWORD_1001=test-pass-1001 PASSWORD_1002=test-pass-1002 $(ASTERISK) up -d --build --quiet-pull --force-recreate
	@sleep 5
	MICROSIP_TEST_ASTERISK=$$(ip -4 route get 1.1.1.1 | sed -n 's/.* src \([0-9.]*\).*/\1/p'):5080 \
	MICROSIP_TEST_PASSWORD_1001=test-pass-1001 MICROSIP_TEST_PASSWORD_1002=test-pass-1002 \
	  build/Release/call_flow_test 2>&1 | grep -a "call_flow_test:"
	$(ASTERISK) down

##@ Help

.PHONY: help
help: ## Show this list
	@awk 'BEGIN {FS = ":.*## "} \
	  /^##@/ {printf "\n%s\n", substr($$0, 5)} \
	  /^[a-zA-Z_-]+:.*## / {printf "  make %-14s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
