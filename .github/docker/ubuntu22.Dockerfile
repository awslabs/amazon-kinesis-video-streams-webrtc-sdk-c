FROM public.ecr.aws/ubuntu/ubuntu:22.04_stable

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    clang \
    clang-format \
    git \
    libcurl4-openssl-dev \
    pkg-config \
    file \
    gdb \
    perl \
    gcc-mips-linux-gnu \
    g++-mips-linux-gnu \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# Copy dependency build script and CMake dependency definitions
COPY .github/docker/build-deps.sh /tmp/src/.github/docker/build-deps.sh
COPY CMake/Dependencies/ /tmp/src/CMake/Dependencies/
COPY configs/ /tmp/src/configs/
RUN chmod +x /tmp/src/.github/docker/build-deps.sh

# ── x86_64 OpenSSL (shared) + signaling deps ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/x86_64-openssl \
    --cc=gcc --cxx=g++ --ssl=openssl --with-signaling

# ── x86_64 MbedTLS v3.6.3 (shared) ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/x86_64-mbedtls \
    --cc=gcc --cxx=g++ --ssl=mbedtls

# ── MIPS-32 static OpenSSL (cross-compile) ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/mips-openssl \
    --cc=mips-linux-gnu-gcc --cxx=mips-linux-gnu-g++ \
    --ssl=openssl --static \
    --openssl-platform=linux-mips32 \
    --srtp-host=x86_64-unknown-linux-gnu \
    --srtp-dest=mips-unknown-linux-gnu

# ── ARM-32 static OpenSSL (cross-compile) ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/arm32-openssl \
    --cc=arm-linux-gnueabihf-gcc --cxx=arm-linux-gnueabihf-g++ \
    --ssl=openssl --static \
    --openssl-platform=linux-generic32 \
    --srtp-host=x86_64-unknown-linux-gnu \
    --srtp-dest=arm-unknown-linux-uclibcgnueabi

# ── AArch64 static OpenSSL (cross-compile) ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/aarch64-openssl \
    --cc=aarch64-linux-gnu-gcc --cxx=aarch64-linux-gnu-g++ \
    --ssl=openssl --static \
    --openssl-platform=linux-aarch64 \
    --srtp-host=x86_64-unknown-linux-gnu \
    --srtp-dest=arm-unknown-linux-uclibcgnueabi

# ── AArch64 static MbedTLS (cross-compile) ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/aarch64-mbedtls \
    --cc=aarch64-linux-gnu-gcc --cxx=aarch64-linux-gnu-g++ \
    --ssl=mbedtls --static \
    --srtp-host=x86_64-unknown-linux-gnu \
    --srtp-dest=arm-unknown-linux-uclibcgnueabi

# Clean up build sources
RUN rm -rf /tmp/src
