# Build Instructions

This guide provides step-by-step instructions to clear and build the Microsip Clone project.

## Prerequisites

Ensure you have the following installed on your system:
- **C++ Compiler**: GCC (Linux), MSVC (Windows), or Clang (macOS) with C++17 support.
- **CMake**: Version 3.16 or higher.
- **Conan**: Version 2.x (Package Manager).
- **Qt6**: Development libraries (Core, Quick, Multimedia, Network).
- **Python 3**: Required for Conan and virtual environment setup.

## Quick Start (Linux)

### 1. Project Setup
Ideally, set up a Python virtual environment to manage Conan isolated from your system packages.

```bash
# Create and activate virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install Conan
pip install conan
```

### 2. Install Dependencies
This project uses Conan to manage PJSIP. We use a local recipe for PJSIP to ensure correct configuration.

```bash
# Detect your profile (first time only)
conan profile detect --force

# Export the local PJSIP recipe
conan export conan/pjsip --version=2.14.1

# Install dependencies (this might take a while as it builds PJSIP from source)
conan install . --build=missing
```

### 3. Build the Application
Once dependencies are installed, use CMake to configure and build the project.

```bash
# Configure and build with the presets Conan generated
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc)
```

`./build.sh` runs steps 2 and 3 in one go (it expects Conan in `.venv`).

### 4. Run
The executable is `build/Release/microsip_clone`. Run it through the wrapper, which sets up the Conan runtime environment:

```bash
./run_microsip.sh
```

## Troubleshooting

### PJSIP Build Failures
If PJSIP fails to build via Conan:
- Ensure you have `autoconf`, `automake`, and `libtool` installed on Linux.
- Check the Conan output log for specific compiler errors.
- Verify that your Python virtual environment is active.

### Qt Issues
- Make sure `Qt6_DIR` is set correctly if CMake cannot find Qt6.
- Ensure `qmake` is in your PATH or specify `-DCMAKE_PREFIX_PATH=/path/to/qt6`.
