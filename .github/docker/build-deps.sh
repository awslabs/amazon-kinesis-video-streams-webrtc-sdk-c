#!/usr/bin/env bash
# build-deps.sh — Build C/C++ dependencies into a given prefix.
#
# Reuses the ExternalProject CMakeLists.txt files from CMake/Dependencies/
# exactly as build_dependency() in CMake/Utilities.cmake does.
#
# Usage:
#   build-deps.sh <prefix> [options...]
#
# Options:
#   --cc=<compiler>            C compiler   (default: gcc)
#   --cxx=<compiler>           C++ compiler (default: g++)
#   --ssl=openssl|mbedtls      TLS library  (required)
#   --static                   Build static libraries
#   --openssl-platform=P       OpenSSL Configure platform (cross-compile)
#   --srtp-host=H              libsrtp --build= value (cross-compile)
#   --srtp-dest=D              libsrtp --host= value  (cross-compile)
#   --old-mbedtls              Use MbedTLS v2.28.8 instead of v3.6.3
#   --with-signaling           Also build lws, jsmn, kvsCommonLws+kvspic
#   --cmake-c-flags=F          Extra CMAKE_C_FLAGS
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <prefix> --ssl=openssl|mbedtls [options...]" >&2
  exit 1
fi

PREFIX="$1"; shift

# Locate the repo root (two levels up from .github/docker/)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Defaults
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
USE_OPENSSL=OFF
USE_MBEDTLS=OFF
BUILD_STATIC=OFF
OPENSSL_PLATFORM=""
SRTP_HOST=""
SRTP_DEST=""
OLD_MBEDTLS=OFF
WITH_SIGNALING=OFF
EXTRA_C_FLAGS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cc=*)               CC="${1#*=}" ;;
    --cxx=*)              CXX="${1#*=}" ;;
    --ssl=openssl)        USE_OPENSSL=ON; USE_MBEDTLS=OFF ;;
    --ssl=mbedtls)        USE_OPENSSL=OFF; USE_MBEDTLS=ON ;;
    --static)             BUILD_STATIC=ON ;;
    --openssl-platform=*) OPENSSL_PLATFORM="${1#*=}" ;;
    --srtp-host=*)        SRTP_HOST="${1#*=}" ;;
    --srtp-dest=*)        SRTP_DEST="${1#*=}" ;;
    --old-mbedtls)        OLD_MBEDTLS=ON ;;
    --with-signaling)     WITH_SIGNALING=ON ;;
    --cmake-c-flags=*)    EXTRA_C_FLAGS="${1#*=}" ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

export CC CXX
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
mkdir -p "$PREFIX"
NPROC=$(nproc)

echo "===== build-deps.sh ====="
echo "PREFIX=$PREFIX  CC=$CC  CXX=$CXX"
echo "USE_OPENSSL=$USE_OPENSSL  USE_MBEDTLS=$USE_MBEDTLS  BUILD_STATIC=$BUILD_STATIC"
echo "OPENSSL_PLATFORM=$OPENSSL_PLATFORM  SRTP_HOST=$SRTP_HOST  SRTP_DEST=$SRTP_DEST"
echo "OLD_MBEDTLS=$OLD_MBEDTLS  WITH_SIGNALING=$WITH_SIGNALING"
echo "========================="

# Helper: build a single dep via its ExternalProject CMakeLists.txt.
# This mirrors what build_dependency() in CMake/Utilities.cmake does:
#   1. Copy the CMakeLists.txt (and any patches) to a temp dir
#   2. Run cmake configure + build
#   3. Clean up
build_dep() {
  local name="$1"; shift
  local cmake_file="$SRC_DIR/CMake/Dependencies/lib${name}-CMakeLists.txt"

  if [[ ! -f "$cmake_file" ]]; then
    echo "ERROR: $cmake_file not found" >&2
    exit 1
  fi

  echo "--- Building ${name} ---"
  local build_dir="$PREFIX/_build-${name}"
  rm -rf "$build_dir"
  mkdir -p "$build_dir"
  cp "$cmake_file" "$build_dir/CMakeLists.txt"

  # Copy any patches (e.g., libjsmn-add-cmakelists.patch)
  for patch in "$SRC_DIR/CMake/Dependencies/lib${name}-"*.patch; do
    [[ -f "$patch" ]] && cp "$patch" "$build_dir/"
  done

  cmake -S "$build_dir" -B "$build_dir" \
    -DOPEN_SRC_INSTALL_PREFIX="$PREFIX" \
    "$@"
  cmake --build "$build_dir" -- -j"$NPROC"
  rm -rf "$build_dir"
  echo "--- ${name} done ---"
}

# ──────────────────────────────────────────────
# 1. TLS library
# ──────────────────────────────────────────────
if [[ "$USE_OPENSSL" == "ON" ]]; then
  build_dep openssl \
    -DBUILD_STATIC_LIBS="$BUILD_STATIC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_OPENSSL_PLATFORM="${OPENSSL_PLATFORM:-OFF}" \
    -DOPENSSL_EXTRA=""

elif [[ "$USE_MBEDTLS" == "ON" ]]; then
  # Pass the same MBEDTLS_USER_CONFIG_FILE that CMakeLists.txt uses (line 163)
  # so MbedTLS is compiled with MBEDTLS_SSL_DTLS_SRTP enabled.
  build_dep mbedtls \
    -DBUILD_STATIC_LIBS="$BUILD_STATIC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_OLD_MBEDTLS_VERSION="$OLD_MBEDTLS" \
    "-DCMAKE_C_FLAGS=-I${SRC_DIR}/configs -DMBEDTLS_USER_CONFIG_FILE=\"<config_mbedtls.h>\" ${EXTRA_C_FLAGS} -std=c99"
fi

# ──────────────────────────────────────────────
# 2. libsrtp
# ──────────────────────────────────────────────
build_dep srtp \
  -DBUILD_STATIC_LIBS="$BUILD_STATIC" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_DIR="$PREFIX" \
  -DBUILD_LIBSRTP_HOST_PLATFORM="${SRTP_HOST:-OFF}" \
  -DBUILD_LIBSRTP_DESTINATION_PLATFORM="${SRTP_DEST:-OFF}" \
  -DUSE_OPENSSL="$USE_OPENSSL" \
  -DUSE_MBEDTLS="$USE_MBEDTLS" \
  "-DCMAKE_C_FLAGS=${EXTRA_C_FLAGS}"

# ──────────────────────────────────────────────
# 3. usrsctp
# ──────────────────────────────────────────────
build_dep usrsctp \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_C_FLAGS=${EXTRA_C_FLAGS}"

# ──────────────────────────────────────────────
# 4. gtest
# ──────────────────────────────────────────────
build_dep gtest

# ──────────────────────────────────────────────
# 5. Signaling deps (optional)
# ──────────────────────────────────────────────
if [[ "$WITH_SIGNALING" == "ON" ]]; then
  # 5a. jsmn (needed by kvsCommonLws / producer-c)
  build_dep jsmn

  # 5b. libwebsockets (needs OpenSSL or MbedTLS already installed)
  build_dep websockets \
    -DBUILD_STATIC_LIBS="$BUILD_STATIC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_DIR="$PREFIX" \
    -DOPENSSL_ROOT_DIR="$PREFIX" \
    -DUSE_OPENSSL="$USE_OPENSSL" \
    -DUSE_MBEDTLS="$USE_MBEDTLS" \
    "-DCMAKE_C_FLAGS=${EXTRA_C_FLAGS}"

  # 5c. kvsCommonLws (includes kvspic)
  build_dep kvsCommonLws \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC="$BUILD_STATIC" \
    -DUSE_OPENSSL="$USE_OPENSSL" \
    -DUSE_MBEDTLS="$USE_MBEDTLS" \
    "-DCMAKE_C_FLAGS=-D_GNU_SOURCE ${EXTRA_C_FLAGS}"
fi

echo "===== All dependencies installed to $PREFIX ====="
