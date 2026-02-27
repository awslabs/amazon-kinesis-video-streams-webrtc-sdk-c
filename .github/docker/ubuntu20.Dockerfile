FROM public.ecr.aws/ubuntu/ubuntu:20.04_stable

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y software-properties-common cmake git gdb pkg-config build-essential && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    add-apt-repository 'deb http://archive.ubuntu.com/ubuntu/ trusty main' && \
    add-apt-repository 'deb http://archive.ubuntu.com/ubuntu/ trusty universe' && \
    apt-get -q update && \
    apt-get -y install gcc-4.4 && \
    rm -rf /var/lib/apt/lists/*

# Copy dependency build script and CMake dependency definitions
COPY .github/docker/build-deps.sh /tmp/src/.github/docker/build-deps.sh
COPY CMake/Dependencies/ /tmp/src/CMake/Dependencies/
COPY configs/ /tmp/src/configs/
RUN chmod +x /tmp/src/.github/docker/build-deps.sh

# ── x86_64 old MbedTLS v2.28.8 (gcc-4.4) ──
# Note: CXX defaults to system g++ (needed for gtest C++ compilation)
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/x86_64-mbedtls-old \
    --cc=gcc-4.4 --ssl=mbedtls --old-mbedtls

# Clean up build sources
RUN rm -rf /tmp/src
