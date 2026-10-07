from conan import ConanFile
from conan.tools.gnu import Autotools, AutotoolsToolchain, PkgConfigDeps
from conan.tools.files import get
from conan.tools.layout import basic_layout
import os
import subprocess

class Pjsip(ConanFile):
    name = "pjsip"
    version = "2.17"
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
        get(self, f"https://github.com/pjsip/pjproject/archive/refs/tags/{self.version}.tar.gz",
            sha256="065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce",
            strip_root=True)
        # PJSIP's configure script is a wrapper that doesn't support out-of-source builds well.
        # We replace it with 'aconfigure' which is the actual autoconf script.
        os.rename("configure", "configure_wrapper")
        os.rename("aconfigure", "configure")
        # Fix permissions immediately
        self.run("chmod +x configure")
        # IPv6 is off by default in PJSIP. Set here rather than on the command
        # line so the installed headers say the same thing to the application.
        with open("pjlib/include/pj/config_site.h", "a") as f:
            f.write("#define PJ_HAS_IPV6 1\n")
            # Same for video: built with it on the command line only, the
            # application would compile against headers that say it is off
            f.write("#define PJMEDIA_HAS_VIDEO 1\n")
            f.write("#define PJMEDIA_VIDEO_DEV_HAS_V4L2 1\n")
            f.write("#define PJMEDIA_VIDEO_DEV_HAS_SDL 1\n")
            # VP9 is in PJSIP's libvpx codec but switched off by default
            f.write("#define PJMEDIA_HAS_VPX_CODEC_VP9 1\n")

    def generate(self):
        from conan.tools.env import VirtualBuildEnv
        ve = VirtualBuildEnv(self)
        ve.generate()

        tc = AutotoolsToolchain(self)
        # Configure options can be added here, e.g. --enable-shared, --with-ssl
        tc.configure_args.extend([
            "--disable-libwebrtc",
            "--enable-video",
            # ponytail: PJSIP (checked up to 2.17) does not compile against
            # FFmpeg >= 8, so the ffmpeg codecs (H.264/H.263) are off.
            # Drop this and restore the av* system_libs once upstream catches up.
            "--disable-ffmpeg",
            "--with-sdl",
            "--with-v4l2",
            "ac_pjmedia_video=1",
            "ac_sdl_ldflags=-lSDL2",
            "ac_v4l2_ldflags=-lv4l2"
        ])
        
        # SDL2 and V4L2 come from the system; ask pkg-config where they are
        def pkg_config(*args):
            return subprocess.check_output(["pkg-config", *args], text=True).split()

        tc.extra_cflags.extend([
            "-DPJ_AUTOCONF=1",
        ] + pkg_config("--cflags", "sdl2", "libv4l2"))
        tc.extra_ldflags.extend(pkg_config("--libs", "sdl2", "libv4l2"))

        tc.generate()
        
        deps = PkgConfigDeps(self)
        deps.generate()

    def build(self):
        autotools = Autotools(self)
        try:
            autotools.configure()
        except Exception:
            self.run("cat config.log")
            raise

        autotools.make(target="dep")
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install()

    def package_info(self):
        # libpjproject.pc lists, in link order, the PJSIP libraries plus every
        # system library configure detected on this machine (codecs, TLS, ...).
        pc = os.path.join(self.package_folder, "lib", "pkgconfig", "libpjproject.pc")
        libs, system_libs = [], []
        with open(pc) as f:
            for line in f:
                if not line.startswith(("Libs:", "Libs.private:")):
                    continue
                for flag in line.split():
                    if not flag.startswith("-l"):
                        continue
                    name = flag[2:]
                    bundled = os.path.exists(
                        os.path.join(self.package_folder, "lib", f"lib{name}.a"))
                    target = libs if bundled else system_libs
                    if name not in target:
                        target.append(name)
        # SDL is forced on through configure_args above, so configure never
        # records it in the .pc file.
        if "SDL2" not in system_libs:
            system_libs.append("SDL2")
        self.cpp_info.libs = libs
        self.cpp_info.system_libs = system_libs
