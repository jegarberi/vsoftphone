FROM debian:bookworm

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qt6-tools-dev \
    qt6-l10n-tools \
    libpjproject-dev \
    libssl-dev \
    libopus-dev \
    libv4l-dev \
    libasound2-dev \
    libpulse-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libavutil-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Placeholder for Android/Windows/iOS toolchains
# In a real scenario, we would add Android NDK, MinGW, etc. here.
