# make help lists the targets.

SHELL := /bin/bash
VENV  := $(CURDIR)/.venv
JOBS  ?= $(shell nproc)

.DEFAULT_GOAL := help

##@ Regular build (system libraries, Conan-built PJSIP; see BUILD.md)

.PHONY: build
build: $(VENV)/bin/conan ## Build the app and the test into build/Release
	./build.sh

.PHONY: test
test: ## Run the loopback call test (needs `make build`)
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
	$(VENV)/bin/pip install -q conan
	$(VENV)/bin/conan profile detect --exist-ok

##@ Portable build (zig, old glibc)
#
# Everything we compile is built with zig for glibc $(GLIBC) and linked
# statically: the app, PJSIP, OpenSSL, Opus and the C++ runtime. What cannot
# be static stays dynamic: glibc itself, ALSA (it loads the sound server's
# plugins) and Qt, which is taken from the official Qt binaries (built for an
# even older glibc) and shipped next to the executable. The result is a
# folder, $(DIST), that runs on any x86_64 Linux with glibc >= $(GLIBC).
#
# Video is left out of this build: SDL, V4L2 and the video codecs would each
# need the same treatment.

GLIBC           ?= 2.35
ZIG_TARGET      := x86_64-linux-gnu.$(GLIBC)
QT_VERSION      ?= 6.8.3
PJSIP_VERSION   ?= 2.14.1
OPENSSL_VERSION ?= 3.0.16
OPUS_VERSION    ?= 1.5.2

S      := $(CURDIR)/build-static
PREFIX := $(S)/prefix
HOST   := $(S)/host
QT     := $(S)/qt/$(QT_VERSION)/gcc_64
DIST   := $(S)/dist
ZCC    := $(S)/bin/zig-cc
ZCXX   := $(S)/bin/zig-c++

# zig from PATH, else the copy pip puts in the venv
ZIG ?= $(shell command -v zig 2>/dev/null || echo "$(VENV)/bin/python -m ziglang")

ZIG_ENV := CC=$(ZCC) CXX=$(ZCXX) AR=$(S)/bin/zig-ar RANLIB=$(S)/bin/zig-ranlib

.PHONY: static
static: $(DIST)/microsip_clone ## Portable build (default glibc 2.35, set GLIBC=) into build-static/dist
	@echo "Portable build ready: $(DIST)  (run $(DIST)/microsip_clone)"

.PHONY: static-check
static-check: $(DIST)/microsip_clone ## Show the newest glibc symbol version the portable build needs
	@for f in $(DIST)/microsip_clone $(DIST)/call_flow_test; do \
	  printf '%s needs glibc ' "$$(basename $$f)"; \
	  objdump -T $$f | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1; \
	done

.PHONY: static-test
static-test: $(DIST)/microsip_clone ## Run the call test from the portable build inside Ubuntu 22.04 (glibc 2.35)
	docker run --rm -v $(DIST):/app:ro ubuntu:22.04 bash -c '\
	  apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
	    libasound2 libgl1 libegl1 libopengl0 libfontconfig1 libfreetype6 libxkbcommon0 libdbus-1-3 libglib2.0-0 >/dev/null && \
	  ldd --version | head -1 && /app/call_flow_test'

.PHONY: static-clean
static-clean: ## Remove build-static/
	rm -rf $(S)

# --- toolchain -------------------------------------------------------------

# Wrappers, because build systems want a compiler that is one word.
# -fno-sanitize=undefined: zig turns UBSan traps on by default.
$(S)/.tools: | $(VENV)/bin/python
	@mkdir -p $(S)/bin $(S)/dl
	@command -v zig >/dev/null 2>&1 || $(VENV)/bin/python -c 'import ziglang' 2>/dev/null || $(VENV)/bin/pip install -q ziglang
	@printf '#!/bin/sh\nexec $(ZIG) cc -target $(ZIG_TARGET) -fno-sanitize=undefined "$$@"\n'  > $(ZCC)
	@printf '#!/bin/sh\nexec $(ZIG) c++ -target $(ZIG_TARGET) -fno-sanitize=undefined "$$@"\n' > $(ZCXX)
	@printf '#!/bin/sh\nexec $(ZIG) ar "$$@"\n'     > $(S)/bin/zig-ar
	@printf '#!/bin/sh\nexec $(ZIG) ranlib "$$@"\n' > $(S)/bin/zig-ranlib
	@chmod +x $(S)/bin/*
	@$(ZCC) --version | head -1
	@touch $@

# The few system pieces we link dynamically. Only their own headers are
# exposed: putting /usr/include on the path would replace zig's glibc
# $(GLIBC) headers with the system's newer ones.
$(S)/.host: $(S)/.tools
	@mkdir -p $(HOST)/include $(HOST)/lib
	@for d in alsa GL KHR EGL GLES2 GLES3 xkbcommon; do \
	  test -e /usr/include/$$d || { echo "missing /usr/include/$$d (install the ALSA, OpenGL and xkbcommon development packages)"; exit 1; }; \
	  ln -sfn /usr/include/$$d $(HOST)/include/$$d; \
	done
	@ln -sfn $$(pkg-config --variable=libdir alsa)/libasound.so $(HOST)/lib/libasound.so
	@touch $@

# --- dependencies ----------------------------------------------------------

$(S)/dl/openssl-$(OPENSSL_VERSION).tar.gz: | $(S)/.tools
	curl -fL -o $@ https://github.com/openssl/openssl/releases/download/openssl-$(OPENSSL_VERSION)/openssl-$(OPENSSL_VERSION).tar.gz

$(S)/dl/opus-$(OPUS_VERSION).tar.gz: | $(S)/.tools
	curl -fL -o $@ https://downloads.xiph.org/releases/opus/opus-$(OPUS_VERSION).tar.gz

$(S)/dl/pjproject-$(PJSIP_VERSION).tar.gz: | $(S)/.tools
	curl -fL -o $@ https://github.com/pjsip/pjproject/archive/refs/tags/$(PJSIP_VERSION).tar.gz

$(S)/.openssl: $(S)/dl/openssl-$(OPENSSL_VERSION).tar.gz $(S)/.tools
	rm -rf $(S)/openssl-$(OPENSSL_VERSION) && tar -xzf $< -C $(S)
	cd $(S)/openssl-$(OPENSSL_VERSION) && $(ZIG_ENV) ./Configure linux-x86_64 \
	  no-shared no-tests no-engine no-dso \
	  --prefix=$(PREFIX) --libdir=lib > $(S)/openssl.log 2>&1
	$(MAKE) -C $(S)/openssl-$(OPENSSL_VERSION) -j$(JOBS) build_libs >> $(S)/openssl.log 2>&1
	$(MAKE) -C $(S)/openssl-$(OPENSSL_VERSION) install_dev >> $(S)/openssl.log 2>&1
	@touch $@

$(S)/.opus: $(S)/dl/opus-$(OPUS_VERSION).tar.gz $(S)/.tools
	rm -rf $(S)/opus-$(OPUS_VERSION) && tar -xzf $< -C $(S)
	cd $(S)/opus-$(OPUS_VERSION) && $(ZIG_ENV) ./configure --prefix=$(PREFIX) \
	  --disable-shared --disable-doc --disable-extra-programs > $(S)/opus.log 2>&1
	$(MAKE) -C $(S)/opus-$(OPUS_VERSION) -j$(JOBS) install >> $(S)/opus.log 2>&1
	@touch $@

# Audio only; see the note at the top of this section.
$(S)/.pjsip: $(S)/dl/pjproject-$(PJSIP_VERSION).tar.gz $(S)/.openssl $(S)/.opus $(S)/.host
	rm -rf $(S)/pjproject-$(PJSIP_VERSION) && tar -xzf $< -C $(S)
	cd $(S)/pjproject-$(PJSIP_VERSION) && $(ZIG_ENV) \
	  CFLAGS="-O2 -DNDEBUG -I$(HOST)/include" CXXFLAGS="-O2 -DNDEBUG -I$(HOST)/include" \
	  LDFLAGS="-L$(HOST)/lib -Wl,--allow-shlib-undefined" \
	  ./aconfigure --prefix=$(PREFIX) --disable-shared \
	  --with-ssl=$(PREFIX) --with-opus=$(PREFIX) \
	  --disable-video --disable-sdl --disable-v4l2 --disable-ffmpeg \
	  --disable-openh264 --disable-vpx --disable-libyuv --disable-libwebrtc \
	  --disable-opencore-amr --disable-bcg729 --disable-silk --disable-libuuid \
	  > $(S)/pjsip.log 2>&1
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) dep >> $(S)/pjsip.log 2>&1
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) -j$(JOBS) lib >> $(S)/pjsip.log 2>&1
	$(MAKE) -C $(S)/pjproject-$(PJSIP_VERSION) install >> $(S)/pjsip.log 2>&1
	@touch $@

# Official Qt binaries: shared libraries built for an older glibc than ours
$(S)/.qt: | $(S)/.tools
	@$(VENV)/bin/python -c 'import aqt' 2>/dev/null || $(VENV)/bin/pip install -q aqtinstall
	$(VENV)/bin/aqt install-qt linux desktop $(QT_VERSION) linux_gcc_64 -m qt5compat qtshadertools -O $(S)/qt > $(S)/qt.log 2>&1
	@touch $@

# --- the app ---------------------------------------------------------------

SOURCES := $(shell find src tests -type f) CMakeLists.txt

$(S)/app/microsip_clone: $(SOURCES) $(S)/.pjsip $(S)/.qt $(S)/.host
	PKG_CONFIG_LIBDIR=$(PREFIX)/lib/pkgconfig cmake -S . -B $(S)/app \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_C_COMPILER=$(ZCC) -DCMAKE_CXX_COMPILER=$(ZCXX) \
	  -DCMAKE_AR=$(S)/bin/zig-ar -DCMAKE_RANLIB=$(S)/bin/zig-ranlib \
	  -DCMAKE_PREFIX_PATH=$(QT) \
	  -DMICROSIP_PJSIP_PKGCONFIG=ON -DMICROSIP_KEYRING=OFF \
	  $(foreach v,OPENGL OPENGL_EGL OPENGL_GLX OPENGL_GLU OPENGL_GLES2 OPENGL_GLES3 XKB,-D$(v)_INCLUDE_DIR=$(HOST)/include) \
	  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_INSTALL_RPATH='$$ORIGIN/lib' \
	  -DCMAKE_EXE_LINKER_FLAGS="-L$(HOST)/lib -Wl,--allow-shlib-undefined" \
	  > $(S)/app.log 2>&1
	@! grep -q -- '-isystem /usr/include\|-I/usr/include' $(S)/app/CMakeFiles/*/flags.make || \
	  { echo "system headers leaked into the build, see $(S)/app/CMakeFiles/*/flags.make"; exit 1; }
	cmake --build $(S)/app -j$(JOBS) >> $(S)/app.log 2>&1

# The executable plus the Qt runtime it loads, found through qt.conf
$(DIST)/microsip_clone: $(S)/app/microsip_clone
	rm -rf $(DIST) && mkdir -p $(DIST)/lib
	cp $(S)/app/microsip_clone $(S)/app/call_flow_test $(DIST)/
	cp -a $(QT)/lib/*.so.* $(DIST)/lib/
	cp -a $(QT)/plugins $(QT)/qml $(DIST)/
	find $(DIST) -name '*.debug' -delete
	printf '[Paths]\nPrefix = .\nPlugins = plugins\nQmlImports = qml\n' > $(DIST)/qt.conf

##@ Help

.PHONY: help
help: ## Show this list
	@awk 'BEGIN {FS = ":.*## "} \
	  /^##@/ {printf "\n%s\n", substr($$0, 5)} \
	  /^[a-zA-Z_-]+:.*## / {printf "  make %-14s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
