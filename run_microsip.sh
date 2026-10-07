#!/bin/bash
cd "$(dirname "$0")" || exit 1
# Source the Conan environment (for Qt, ffmpeg, etc.)
source build/Release/generators/conanrun.sh

# FORCE SYSTEM ALSA CONFIG
# Compiling PJSIP against system ALSA means we must use system config.
# Conan dependencies (like SDL) might still inject ALSA_CONFIG_DIR pointing to Conan's package.
# We must unset this to let libasound find /usr/share/alsa/alsa.conf
echo "Unsetting ALSA_CONFIG_DIR (was $ALSA_CONFIG_DIR)"
unset ALSA_CONFIG_DIR
unset ALSA_CONFIG_PATH

# Run the application
./build/Release/microsip_clone
