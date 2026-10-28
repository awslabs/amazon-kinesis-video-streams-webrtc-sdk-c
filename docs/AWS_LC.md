# AWS-LC Backend (Post-Quantum and FIPS)

AWS-LC is a third crypto backend, next to OpenSSL and mbedTLS. It adds two things the other two do not provide in this SDK.

- Post-quantum TLS 1.3 key exchange, using the `X25519MLKEM768` hybrid group ([RFC 10024](https://www.rfc-editor.org/rfc/rfc10024.txt), IANA codepoint `0x11EC`). The group combines X25519 ECDHE with ML-KEM-768 ([FIPS 203](https://csrc.nist.gov/pubs/fips/203/final)).
- FIPS 140-3 validated cryptography, when you build with `USE_AWS_LC_FIPS=ON`.

The backend is opt-in. Both options default to `OFF`, so a build that does not pass them links the same OpenSSL as before.

## Building

```bash
mkdir build && cd build

# Post-quantum, non-FIPS (recommended starting point)
cmake .. -DUSE_AWS_LC=ON
make -j$(nproc)

# Post-quantum + FIPS 140-3
cmake .. -DUSE_AWS_LC=ON -DUSE_AWS_LC_FIPS=ON
make -j$(nproc)
```

`-DUSE_AWS_LC=ON` supersedes the default `USE_OPENSSL=ON`. You do not need to pass `-DUSE_OPENSSL=OFF`. `-DUSE_AWS_LC_FIPS=ON` turns on `USE_AWS_LC` as well. CMake rejects AWS-LC together with mbedTLS, and together with `USE_OPENSSL3`.

### Options

| Option | Default | Notes |
|---|---|---|
| `USE_AWS_LC` | `OFF` | Select the AWS-LC backend |
| `USE_AWS_LC_FIPS` | `OFF` | Build AWS-LC with `-DFIPS=1`. Turns on `USE_AWS_LC` |
| `AWS_LC_GIT_TAG` | *(empty)* | Override the AWS-LC ref. Defaults to `v5.9.0`, or the `fips-<date>-lts` branch when FIPS is on |

### Build tooling

| Tool | Non-FIPS | FIPS |
|---|---|---|
| CMake | required | required |
| C/C++ compiler | required | required |
| Perl | optional | required |
| Go >= 1.20 | optional | required |
| NASM | optional, x86_64 only | optional, x86_64 only |

Without Perl, AWS-LC compiles portable C instead of its assembly backends. That is slower for AES, SHA and EC, which matters on embedded targets. The SDK detects both tools, logs which fallback it took, and continues.

FIPS behaves differently. AWS-LC refuses to configure a FIPS build without Go and Perl, so the SDK checks for both first and stops with `USE_AWS_LC_FIPS=ON requires Go on PATH` or the matching Perl message.

### FIPS platform limitations

AWS-LC imposes these, not this SDK.

| Build | Linux | macOS | Windows |
|---|---|---|---|
| Static FIPS (`-DBUILD_STATIC_LIBS=ON`) | yes | no | no |
| Shared FIPS | yes | yes | yes (no Debug) |

The SDK checks the combination at configure time. FIPS also conflicts with `-DADDRESS_SANITIZER=ON` and `-DMEMORY_SANITIZER=ON`, because AWS-LC disables its module integrity check under a sanitizer, and that voids the validation.

## Confirming the build

### Is AWS-LC linked at all?

```bash
nm -gU libkvsWebrtcClient.dylib 2>/dev/null | grep -i awslc   # macOS
nm -D  libkvsWebrtcClient.so    2>/dev/null | grep -i awslc   # Linux
strings open-source/lib/libcrypto.* | grep 'AWS-LC'
```

### The startup banner

`initKvsWebRtc()` logs the backend, its FIPS state and its post-quantum groups once, at `INFO` level.

```
Crypto backend: AWS-LC 5.9.0 (headers), loaded library reports: AWS-LC 5.9.0
AWS-LC FIPS mode: ENABLED (AWS-LC FIPS, module version 5)
Post-quantum key exchange: offered (X25519MLKEM768, 0x11ec) on the TLS 1.3 connections to AWS (control plane HTTPS, signaling WSS, TURNS); DTLS-SRTP media path is DTLS 1.2 and remains classical
```

Set `AWS_KVS_LOG_LEVEL=3` or lower to see these lines. The default is `4`, which is `WARN`.

The first line reports two versions. "headers" is the AWS-LC version the SDK compiled against. "loaded library reports" is the version string inside the libcrypto the process loaded. The two differ when the loader finds a stale or system libcrypto ahead of the intended AWS-LC.

### Did a given connection negotiate ML-KEM?

Offering the group is not the same as negotiating it. The peer has to support the group too. The SDK logs the outcome of every TLS 1.3 handshake it completes, labelled by connection.

```
PQC[signaling WSS]: TLSv1.3 offered=X25519MLKEM768(configured) negotiated=X25519MLKEM768 (0x11ec) POST-QUANTUM
PQC[control-plane DescribeSignalingChannel]: TLSv1.3 offered=X25519MLKEM768(configured) negotiated=X25519MLKEM768 (0x11ec) POST-QUANTUM
PQC[TURNS]: TLSv1.3 offered=X25519MLKEM768(configured) negotiated=X25519 (0x001d) CLASSICAL - peer did not accept ML-KEM
```

A `CLASSICAL` line means the peer has not enabled ML-KEM. The SDK offered the group and the peer declined it. That is not a misconfiguration. `POST-QUANTUM` lines log at `INFO` and `CLASSICAL` lines at `WARN`.

To inspect an endpoint's server side without the SDK, build the `bssl` tool with `-DBUILD_TOOL=ON` in the AWS-LC build, or run `openssl s_client` from OpenSSL 3.5 or later.

### Confirming a FIPS operation ran inside the boundary

A FIPS build means the library carries the validated module. It says nothing about whether one operation ran inside the boundary. AWS-LC reports that through a per-thread service indicator.

```c
#include <openssl/service_indicator.h>

FIPSStatus approved = AWSLC_NOT_APPROVED;
CALL_SERVICE_AND_CHECK_APPROVED(approved, MD5(data, len, out));
/* approved == AWSLC_NOT_APPROVED: that call was outside the boundary */
```

One trap. In a non-FIPS build, `CALL_SERVICE_AND_CHECK_APPROVED` always sets `AWSLC_APPROVED`, by design, for source compatibility. Gate the check on `FIPS_mode() != 0` first. Do not gate it on `FIPS_version()`, which the `fips-*` branches do not export publicly. Without that gate, a non-FIPS build reports every operation as approved and the check passes for the wrong reason.

## Coverage and limits

### What post-quantum key exchange covers

Every TLS 1.3 connection the SDK opens to AWS. Two TLS stacks carry them.

| Path | TLS stack | Covered |
|---|---|---|
| Control plane (`DescribeSignalingChannel`, `CreateSignalingChannel`, `GetSignalingChannelEndpoint`, `DeleteSignalingChannel`, `DescribeMediaStorageConfiguration`) | libwebsockets | yes |
| `GetIceServerConfig`, `JoinStorageSession` | libwebsockets | yes |
| Signaling WSS (`ConnectAsMaster` / `ConnectAsViewer`) | libwebsockets | yes |
| TURN over TLS (`turns:`) | SDK TLS session | yes |
| DTLS-SRTP peer-to-peer media | SDK DTLS session | no, see below |

### What it cannot cover

The peer-to-peer media path uses DTLS-SRTP. AWS-LC caps DTLS at 1.2. ML-KEM key exchange exists only in TLS 1.3. So this backend cannot make media stream confidentiality post-quantum, and no configuration changes that. Do not read "the WebRTC SDK is PQC-enabled" as covering the media stream.

### FIPS and TURN conflict at the protocol level

The STUN and TURN RFCs mandate two algorithms the SDK cannot choose.

- MD5. The TURN long-term credential key is `MD5(username:realm:password)`, per [RFC 5389](https://www.rfc-editor.org/rfc/rfc5389) and [RFC 8489](https://www.rfc-editor.org/rfc/rfc8489). MD5 is not FIPS-approved.
- HMAC-SHA1. The STUN `MESSAGE-INTEGRITY` attribute uses HMAC-SHA1, also RFC-mandated. SP 800-131A still permits HMAC-SHA-1 in general. Confirm it against the security policy for the AWS-LC-FIPS version you pin.

AWS-LC derives from BoringSSL, so it does not abort on a non-approved algorithm. RHEL system-FIPS OpenSSL and the OpenSSL 3 FIPS provider do abort. In AWS-LC, MD5 stays callable and TURN keeps working. The service indicator simply does not increment.

So a FIPS build that uses TURN long-term authentication performs non-approved operations. The SDK cannot fix this, because the protocol specifies MD5. Account for it in any compliance attestation.

## Troubleshooting

### `Could NOT find AWSLC (missing: AWSLC_INCLUDE_DIR)`, usually right after `awslc already built`

The dependency prefix, `open-source/` by default, still holds `libssl` and `libcrypto` from an earlier OpenSSL build. AWS-LC and OpenSSL install under identical library names, so the generic "is it already built?" probe cannot tell them apart. The probe reports AWS-LC as present, the build skips AWS-LC, and the AWS-LC header lookup then fails.

The build catches this before configuring AWS-LC and stops with `USE_AWS_LC=ON, but <prefix> already contains a non-AWS-LC libssl`. Either way, stop sharing one prefix between backends.

```bash
rm -rf open-source
cmake .. -DUSE_AWS_LC=ON
```

Or give each backend its own prefix, which saves time if you switch between them often.

```bash
cmake .. -DUSE_AWS_LC=ON -DOPEN_SRC_INSTALL_PREFIX=$PWD/../open-source-awslc
```

The same guard fires in reverse. CMake rejects `-DUSE_OPENSSL=ON` against a prefix holding AWS-LC. That direction would otherwise appear to succeed, because AWS-LC presents the OpenSSL 1.1.1 API, while the build actually links AWS-LC.

`rm -rf build` does not fix this. The stale artifacts live in the dependency prefix, not the build directory.

### The banner reports different header and loaded versions

The loader picked a different libcrypto at runtime. On Linux, check `LD_LIBRARY_PATH` and run `ldd` on the built library. On macOS, check `DYLD_LIBRARY_PATH` and run `otool -L`. A system OpenSSL earlier on the search path is the usual cause.

### A FIPS build logs `AWS-LC FIPS MISMATCH`

The build used `-DUSE_AWS_LC_FIPS=ON` but linked a non-FIPS AWS-LC. The usual cause is a stale artifact in the dependency prefix from an earlier non-FIPS configure, because the dependency step skips the rebuild when it finds an existing `libssl`. Remove the prefix and reconfigure.

## How it integrates

AWS-LC reports `OPENSSL_VERSION_NUMBER` as `0x1010107f`, which is OpenSSL 1.1.1 compatible. It installs `include/openssl/*.h` plus `lib/libssl` and `lib/libcrypto`. The SDK therefore reuses its existing OpenSSL crypto sources and defines two macros together.

- `KVS_USE_OPENSSL` means compile against the OpenSSL API surface.
- `KVS_USE_AWS_LC` means that surface is AWS-LC, so enable the ML-KEM and FIPS additions.

The build compiles AWS-LC-specific code into its own file, and only when the flag is on. A build that sets `KVS_USE_AWS_LC` while resolving real OpenSSL headers fails at compile time. It does not ship silently without PQC or FIPS.

Group selection calls `SSL_CTX_set1_groups()` with NIDs. It does not call `SSL_CTX_set1_group_ids()` with codepoints, because the AWS-LC `fips-*` branches do not export that function. `X25519MLKEM768` already leads AWS-LC's default group list, so a connection would negotiate it even without this call. The SDK sets the list explicitly so its posture does not depend on a library default that AWS-LC may reorder.

`libwebsockets` and `libsrtp` need no AWS-LC awareness. They receive `USE_OPENSSL=ON`, meaning the backend presents the OpenSSL API, and they resolve AWS-LC through the shared install prefix. The AWS-LC build sets `ENABLE_DIST_PKG=OFF` so the install layout stays byte-for-byte OpenSSL-compatible. Turning it on would rename the libraries and move headers to `include/aws-lc/`, and every downstream consumer here would then fail to find them.
