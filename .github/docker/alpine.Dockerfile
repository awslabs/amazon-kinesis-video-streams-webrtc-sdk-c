FROM alpine:3.15.4

RUN apk update && apk upgrade && apk add --no-cache \
    alpine-sdk \
    cmake \
    clang \
    linux-headers \
    perl \
    bash \
    openssl-dev \
    zlib-dev \
    curl-dev \
    gdb

# Copy dependency build script and CMake dependency definitions
COPY .github/docker/build-deps.sh /tmp/src/.github/docker/build-deps.sh
COPY CMake/Dependencies/ /tmp/src/CMake/Dependencies/
RUN chmod +x /tmp/src/.github/docker/build-deps.sh

# ── Alpine static OpenSSL ──
RUN /tmp/src/.github/docker/build-deps.sh /opt/deps/alpine-openssl \
    --cc=gcc --cxx=g++ --ssl=openssl --static

# Clean up build sources
RUN rm -rf /tmp/src
