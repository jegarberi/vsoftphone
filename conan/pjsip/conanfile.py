from conan import ConanFile
from conan.tools.gnu import Autotools, AutotoolsToolchain, PkgConfigDeps
from conan.tools.files import get, rmdir, copy
from conan.tools.layout import basic_layout
import os

class Pjsip(ConanFile):
    name = "pjsip"
    version = "2.14.1"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def layout(self):
        basic_layout(self, src_folder="src")
        self.folders.build = self.folders.source

    def requirements(self):
        # Use system libraries for everything to match softphone configuration
        pass

    def build_requirements(self):
        self.tool_requires("pkgconf/2.2.0")

    def source(self):
        # Downloading PJSIP 2.14.1 source
        get(self, "https://github.com/pjsip/pjproject/archive/refs/tags/2.14.1.tar.gz", strip_root=True)
        # PJSIP's configure script is a wrapper that doesn't support out-of-source builds well.
        # We replace it with 'aconfigure' which is the actual autoconf script.
        os.rename("configure", "configure_wrapper")
        os.rename("aconfigure", "configure")
        # Fix permissions immediately
        self.run("chmod +x configure")

    def generate(self):
        from conan.tools.env import VirtualBuildEnv
        ve = VirtualBuildEnv(self)
        ve.generate()

        tc = AutotoolsToolchain(self)
        # Configure options can be added here, e.g. --enable-shared, --with-ssl
        tc.configure_args.extend([
            "--disable-libwebrtc",
            "--enable-video",
            "--with-ffmpeg",
            "--with-sdl",
            "--with-v4l2",
            "ac_has_ffmpeg=1",
            "ac_pjmedia_video=1",
            "ac_sdl_ldflags=-lSDL2",
            "ac_v4l2_ldflags=-lv4l2"
        ])
        
        # Ensure system headers are found and macros are set
        tc.extra_cflags.extend([
            "-DPJ_AUTOCONF=1",
            "-DPJMEDIA_HAS_VIDEO=1",
            "-DPJMEDIA_VIDEO_DEV_HAS_V4L2=1",
            "-DPJMEDIA_VIDEO_DEV_HAS_SDL=1",
            "-I/usr/include/SDL2" # Explicitly help it find SDL2 headers
        ])

        # Gentoo-specific: Ensure /usr/lib64 is searched and libs are linked
        tc.extra_ldflags.extend([
            "-L/usr/lib64",
            "-lSDL2",
            "-lv4l2"
        ])
        
        tc.generate()
        
        deps = PkgConfigDeps(self)
        deps.generate()

    def build(self):
        autotools = Autotools(self)
        try:
            autotools.configure()
        except:
            self.run("cat config.log")
            raise

        # Always dump config.log for inspection if pjsip_config.log doesn't exist or to overwrite it
        self.run("cp config.log /home/jon/antigravity_microsip/pjsip_config.log")
        
        autotools.make(target="dep")
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        suffix = "-x86_64-unknown-linux-gnu"
        # Order is important for static linking!
        libs = [
            "pjsua2", "pjsua", "pjsip-ua", "pjsip-simple", "pjsip", 
            "pjmedia-codec", "pjmedia-videodev", "pjmedia-audiodev", "pjmedia", 
            "pjnath", "pjlib-util", "pj",
            # Third party libs bundled with PJSIP
            "srtp", "resample", "speex", "ilbccodec", "g7221codec", "gsmcodec", "yuv"
        ]
        self.cpp_info.libs = [lib + suffix for lib in libs]
        
        # System libs needed by PJSIP
        self.cpp_info.system_libs = ["m", "pthread", "rt", "uuid"]
        
        if self.settings.os == "Linux":
            # Add system libraries that PJSIP was compiled against
            # These replace the Conan dependencies
            self.cpp_info.system_libs.extend([
                "asound",   # ALSA
                "SDL2",     # SDL2
                "v4l2",     # V4L2
                "opus",     # Opus
                "vpx",      # VPX
                "avdevice", "avformat", "avcodec", "avutil", "swscale", # FFmpeg
                "ssl", "crypto" # OpenSSL
            ])
