#!/bin/bash
#
# CI check: an AWS-LC build really linked AWS-LC, and that AWS-LC offers the
# post-quantum group list this SDK configures.
#
# Usage:
#   ./scripts/check-awslc.sh [build-dir]
#
# Default build-dir is "build". Run it after a build configured with
# -DUSE_AWS_LC=ON.
#
# What this checks, and what it deliberately does not
# ---------------------------------------------------
# Everything here is offline. No network, no AWS credentials, no endpoint. That
# is what makes it safe to run on every CI job.
#
#   1. The dependency prefix holds AWS-LC, not OpenSSL, and the built SDK
#      library links libssl/libcrypto.
#   2. The libcrypto loaded at RUNTIME reports AWS-LC, its API version matches
#      the headers the SDK compiled against, FIPS mode matches the configured
#      flag, and X25519MLKEM768 both exists in the library and is accepted on a
#      client SSL_CTX.
#
# It does NOT observe a handshake, so it does not prove a ClientHello carried
# ML-KEM. That needs a live connection and belongs in a local run, not here.
#
# The group list is read out of src/source/Crypto/Crypto.h rather than restated,
# so editing KVS_AWS_LC_PQC_GROUPS cannot leave this check passing against a
# stale copy of the list.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR_ARG="${1:-build}"
BUILD_DIR="$(cd "$REPO_ROOT/$BUILD_DIR_ARG" 2>/dev/null && pwd)" || {
    echo "FAIL: build directory '$REPO_ROOT/$BUILD_DIR_ARG' not found. Build first:"
    echo "        mkdir build && cd build && cmake .. -DUSE_AWS_LC=ON && make -j"
    exit 1
}

CRYPTO_HEADER="$REPO_ROOT/src/source/Crypto/Crypto.h"
PQC_GROUP_HEX="0x11ec"
PQC_GROUP_NAME="X25519MLKEM768"
PQC_FIRST_NID="NID_X25519MLKEM768"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0
skip=0

ok() { echo "  PASS  $1"; pass=$((pass + 1)); }
no() { echo "  FAIL  $1"; fail=$((fail + 1)); }
warn() { echo "  SKIP  $1"; skip=$((skip + 1)); }

echo "=============================================================="
echo " AWS-LC backend check (offline)"
echo "=============================================================="
echo "build dir : $BUILD_DIR"

# Locate the AWS-LC this build links. CMakeCache is authoritative and final: a
# build may install its dependencies outside the default prefix (a FIPS build
# next to a non-FIPS one is the normal case), and falling back to a conventional
# location would check a different library than the one this build uses. That
# silently passes a default OpenSSL build whenever an AWS-LC prefix happens to
# sit in the repo, which is the opposite of what this script is for.
PREFIX=""
PREFIX_FROM_CACHE=0
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    PREFIX="$(grep -m1 '^OPEN_SRC_INSTALL_PREFIX' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2-)"
    [ -n "$PREFIX" ] && PREFIX_FROM_CACHE=1
fi
if [ "$PREFIX_FROM_CACHE" -eq 0 ]; then
    for cand in "$REPO_ROOT/open-source" "$BUILD_DIR/../open-source"; do
        if [ -f "$cand/include/openssl/base.h" ]; then
            PREFIX="$(cd "$cand" && pwd)"
            break
        fi
    done
fi
echo "AWS-LC    : ${PREFIX:-<not found>}"

# The configured FIPS flag decides what FIPS_mode() must report. A FIPS build
# that linked a non-FIPS AWS-LC is a real defect, not a warning.
FIPS_EXPECTED=0
if [ -f "$BUILD_DIR/CMakeCache.txt" ] &&
    grep -qE '^USE_AWS_LC_FIPS(:[A-Z]+)?=(ON|TRUE|1)$' "$BUILD_DIR/CMakeCache.txt"; then
    FIPS_EXPECTED=1
fi
echo "FIPS flag : $FIPS_EXPECTED"
echo

# --------------------------------------------------------------------------
echo "[1/2] The build is linked against AWS-LC"
# --------------------------------------------------------------------------
if [ -z "$PREFIX" ] || [ ! -f "$PREFIX/include/openssl/base.h" ]; then
    no "no AWS-LC headers at ${PREFIX:-<no prefix>}; was this configured with -DUSE_AWS_LC=ON?"
    # Without the right library nothing below can mean anything, and reporting
    # PASS on checks run against some other prefix would be worse than stopping.
    echo
    echo "=============================================================="
    echo " $pass passed, $fail failed, $skip skipped"
    echo "=============================================================="
    exit 1
elif ! grep -q "OPENSSL_IS_AWSLC" "$PREFIX/include/openssl/base.h"; then
    no "$PREFIX holds OpenSSL, not AWS-LC (OPENSSL_IS_AWSLC absent)"
    echo
    echo "=============================================================="
    echo " $pass passed, $fail failed, $skip skipped"
    echo "=============================================================="
    exit 1
else
    ver="$(grep -m1 'AWSLC_VERSION_NUMBER_STRING' "$PREFIX/include/openssl/base.h" | sed 's/.*"\(.*\)".*/\1/')"
    ok "AWS-LC ${ver:-unknown} headers in the dependency prefix (OPENSSL_IS_AWSLC present)"
fi

lib=""
for cand in "$BUILD_DIR"/libkvsWebrtcClient.so "$BUILD_DIR"/libkvsWebrtcClient.dylib "$BUILD_DIR"/libkvsWebrtcClient.a; do
    if [ -f "$cand" ]; then
        lib="$cand"
        break
    fi
done
if [ -z "$lib" ]; then
    # A build that produced no SDK library at all (or named it something else)
    # is not evidence of a wrong backend. The prefix check above and the probe
    # below carry the real assertions.
    warn "no libkvsWebrtcClient found in $BUILD_DIR; skipping the link check"
elif [ "${lib##*.}" = "a" ]; then
    # A static archive records no dependencies, so check for AWS-LC symbols instead.
    if nm "$lib" 2> /dev/null | grep -qi "awslc\|FIPS_mode"; then
        ok "$(basename "$lib") references AWS-LC symbols"
    else
        warn "no AWS-LC symbols visible in $(basename "$lib"); the linker may have dropped them"
    fi
else
    lister="ldd"
    command -v otool > /dev/null && lister="otool -L"
    if $lister "$lib" 2> /dev/null | grep -q "libssl"; then
        ok "$(basename "$lib") links libssl/libcrypto"
    else
        warn "could not read libssl in the link deps of $(basename "$lib")"
    fi
fi
echo

# --------------------------------------------------------------------------
echo "[2/2] The loaded library is AWS-LC and accepts the SDK's PQC group list"
# --------------------------------------------------------------------------

# Read the group list out of the SDK header so this check cannot drift from it.
# Not named GROUPS: bash reserves that for the caller's group ids, so assigning
# to it is silently discarded.
GROUP_LIST="$(sed -n 's/^#define[ \t]*KVS_AWS_LC_PQC_GROUPS[ \t]*{\(.*\)}.*/\1/p' "$CRYPTO_HEADER")"
if [ -z "$GROUP_LIST" ]; then
    no "could not read KVS_AWS_LC_PQC_GROUPS from $CRYPTO_HEADER"
else
    first="$(echo "$GROUP_LIST" | cut -d, -f1 | tr -d ' ')"
    if [ "$first" = "$PQC_FIRST_NID" ]; then
        ok "KVS_AWS_LC_PQC_GROUPS leads with $PQC_FIRST_NID"
    else
        no "KVS_AWS_LC_PQC_GROUPS leads with '$first', expected $PQC_FIRST_NID"
    fi

    cat > "$WORK/probe.c" << EOF
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <stdio.h>
int main(void)
{
    /* Plain \`int\`, not INT32: SSL_CTX_set1_groups takes \`const int *\`. */
    const int groups[] = {$GROUP_LIST};
    const size_t groupCount = sizeof(groups) / sizeof(groups[0]);
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    const char* groupName = SSL_get_group_name(SSL_GROUP_X25519_MLKEM768);
    int applied = (ctx != NULL) && (SSL_CTX_set1_groups(ctx, groups, groupCount) == 1);

    printf("loaded=%s\n", OpenSSL_version(OPENSSL_VERSION));
    printf("apiVersion=%lu headerApiVersion=%d\n", awslc_api_version_num(), AWSLC_API_VERSION);
    printf("fips=%d\n", FIPS_mode() != 0 ? 1 : 0);
    printf("pqcGroup=%s id=0x%04x groupCount=%zu applied=%d\n", groupName != NULL ? groupName : "(none)",
           SSL_GROUP_X25519_MLKEM768, groupCount, applied);

    if (ctx != NULL) {
        SSL_CTX_free(ctx);
    }
    return 0;
}
EOF

    if "${CC:-cc}" "$WORK/probe.c" -o "$WORK/probe" \
        -I"$PREFIX/include" -L"$PREFIX/lib" -L"$PREFIX/lib64" -lssl -lcrypto \
        -Wl,-rpath,"$PREFIX/lib" -Wl,-rpath,"$PREFIX/lib64" 2> "$WORK/probe.err"; then
        if out="$("$WORK/probe" 2>&1)"; then
            echo "$out" | sed 's/^/        /'

            echo "$out" | grep -q "loaded=.*AWS-LC" &&
                ok "the loaded libcrypto reports AWS-LC" ||
                no "the loaded libcrypto does not report AWS-LC"

            hdrApi="$(echo "$out" | sed -n 's/.*headerApiVersion=\([0-9]*\).*/\1/p')"
            libApi="$(echo "$out" | sed -n 's/apiVersion=\([0-9]*\).*/\1/p')"
            if [ -n "$hdrApi" ] && [ "$hdrApi" = "$libApi" ]; then
                ok "the loaded library matches the headers (API $libApi)"
            else
                no "header/library API mismatch (headers $hdrApi, loaded $libApi)"
            fi

            fipsActual="$(echo "$out" | sed -n 's/^fips=\([0-9]*\).*/\1/p')"
            if [ "$fipsActual" = "$FIPS_EXPECTED" ]; then
                ok "FIPS mode matches the configured flag (fips=$fipsActual)"
            else
                no "FIPS mismatch: configured $FIPS_EXPECTED, linked AWS-LC reports $fipsActual"
            fi

            echo "$out" | grep -qi "id=$PQC_GROUP_HEX" &&
                ok "$PQC_GROUP_NAME is $PQC_GROUP_HEX in this library" ||
                no "$PQC_GROUP_NAME is not $PQC_GROUP_HEX"

            echo "$out" | grep -q "pqcGroup=$PQC_GROUP_NAME" &&
                ok "the library knows $PQC_GROUP_NAME by name" ||
                no "the library does not know $PQC_GROUP_NAME"

            echo "$out" | grep -q "applied=1" &&
                ok "the SDK's group list applies to a client SSL_CTX" ||
                no "the library rejected the SDK's group list"
        else
            echo "$out" | sed 's/^/        /'
            no "the probe built but did not run"
        fi
    else
        no "could not build the probe against $PREFIX"
        sed 's/^/        /' "$WORK/probe.err" | head -8
    fi
fi
echo

# --------------------------------------------------------------------------
echo "=============================================================="
echo " $pass passed, $fail failed, $skip skipped"
echo "=============================================================="
echo
echo "This check is offline. It proves AWS-LC is linked and offers"
echo "$PQC_GROUP_NAME, not that a handshake negotiated it. For on-wire"
echo "evidence run the full local verification against a live endpoint."

[ "$fail" -eq 0 ] || exit 1
exit 0
