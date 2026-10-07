# Builds and tests the Linux binary:  docker build -t microsip-clone .
FROM debian:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config ca-certificates \
    python3 python3-venv autoconf automake libtool \
    qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-templates \
    qml6-module-qtqml-workerscript qml6-module-qt-labs-platform \
    libssl-dev libopus-dev libvpx-dev libv4l-dev libasound2-dev libsdl2-dev \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN python3 -m venv .venv \
    && .venv/bin/pip install --no-cache-dir conan \
    && .venv/bin/conan profile detect \
    && ./build.sh

# Loopback call test; needs no network or sound card
RUN ctest --preset conan-release --output-on-failure

CMD ["./run_microsip.sh"]
