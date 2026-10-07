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
            # ponytail: PJSIP (checked up to 2.16) does not compile against
            # FFmpeg >= 8, so the ffmpeg codecs (H.264/H.263) are off.
            # Drop this and restore the av* system_libs once upstream catches up.
            "--disable-ffmpeg",
            "--with-sdl",
            "--with-v4l2",
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
