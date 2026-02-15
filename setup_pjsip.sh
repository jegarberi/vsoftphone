#!/bin/bash
set -e

# Directory where to build PJSIP
BUILD_DIR="3rdparty"
PJSIP_VERSION="2.14" # Check for latest version or use git master
INSTALL_DIR="$(pwd)/$BUILD_DIR/pjsip_install"

mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ ! -d "pjproject" ]; then
    echo "Cloning PJSIP..."
    git clone https://github.com/pjsip/pjproject.git
    cd pjproject
    # checkout a stable version if needed, or stick with master
    # git checkout $PJSIP_VERSION
else
    cd pjproject
    echo "PJSIP already cloned. pulling latest..."
    git pull
fi

# Configure PJSIP
echo "Configuring PJSIP..."
# Add flags for codecs and features as requested
# Note: dependencies must be present on the system for these to initiate
./configure --prefix=$INSTALL_DIR --enable-shared --with-external-pa --disable-libwebrtc

# Build
echo "Building PJSIP..."
make dep || true
make -j$(nproc)

# Install
echo "Installing PJSIP to $INSTALL_DIR..."
make install

echo "PJSIP build complete."
