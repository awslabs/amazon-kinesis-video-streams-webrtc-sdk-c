# AWS-LC Crypto Backend

[AWS-LC](https://github.com/aws/aws-lc) is an optional crypto backend for the SDK. It provides:

- Post-quantum key exchange on TLS 1.3, using the `X25519MLKEM768` hybrid group ([RFC 10024](https://www.rfc-editor.org/rfc/rfc10024.txt)).
- FIPS 140-3 validated cryptography, when built in FIPS mode.

AWS-LC is off by default. Builds that do not enable it are unchanged.

## Requirements

| Tool | Non-FIPS build | FIPS build |
|---|---|---|
| CMake | required | required |
| C and C++ compiler | required | required |
| Perl | optional | required |
| Go 1.20 or later | optional | required |
| NASM | Windows x86_64 only | Windows x86_64 only |

### Supported platforms

| Build | Linux | macOS | Windows |
|---|---|---|---|
| Non-FIPS, shared or static | yes | yes | not tested |
| FIPS, shared | yes | yes | not tested, no Debug builds |
| FIPS, static | yes | no | no |

## Building

```bash
mkdir build && cd build

# Post-quantum key exchange
cmake .. -DUSE_AWS_LC=ON
make -j$(nproc)

# Post-quantum key exchange and FIPS 140-3
cmake .. -DUSE_AWS_LC_FIPS=ON
make -j$(nproc)
```

### Options

| Option | Default | Description |
|---|---|---|
| `USE_AWS_LC` | `OFF` | Use AWS-LC as the crypto backend. Replaces the default OpenSSL. |
| `USE_AWS_LC_FIPS` | `OFF` | Build AWS-LC in FIPS mode. Also turns on `USE_AWS_LC`. |
| `AWS_LC_GIT_TAG` | *(empty)* | Build a different AWS-LC tag. The default is `v5.9.0`, or `AWS-LC-FIPS-3.1.0` in FIPS mode. |

AWS-LC cannot be combined with `-DUSE_MBEDTLS=ON` or `-DUSE_OPENSSL3=ON`. FIPS mode cannot be combined with `-DADDRESS_SANITIZER=ON` or `-DMEMORY_SANITIZER=ON`.

Use a separate dependency folder for each crypto backend. AWS-LC and OpenSSL install libraries with the same names.

```bash
cmake .. -DUSE_AWS_LC=ON -DOPEN_SRC_INSTALL_PREFIX=$PWD/../open-source-awslc
```

## FIPS 140-3

FIPS mode builds the `AWS-LC-FIPS-3.1.0` release of the AWS-LC FIPS module.

| Library | Certificate | Security policy |
|---|---|---|
| Shared | [#5298](https://csrc.nist.gov/projects/cryptographic-module-validation-program/certificate/5298) | [140sp5298](https://csrc.nist.gov/CSRC/media/projects/cryptographic-module-validation-program/documents/security-policies/140sp5298.pdf) |
| Static | [#5314](https://csrc.nist.gov/projects/cryptographic-module-validation-program/certificate/5314) | [140sp5314](https://csrc.nist.gov/CSRC/media/projects/cryptographic-module-validation-program/documents/security-policies/140sp5314.pdf) |

The certificates list these tested operating environments:

- Amazon Linux 2023 on AWS Graviton4
- Amazon Linux 2023 on Intel Xeon Platinum 8375C

Other platforms are not listed on the certificates. Check with your compliance team before you rely on a FIPS build there.

To build a different AWS-LC FIPS release, set `-DAWS_LC_GIT_TAG`. Use only a release with an active FIPS 140-3 certificate.

### TURN uses MD5

TURN long-term credentials use an MD5 key, as the TURN specification requires ([RFC 8489](https://www.rfc-editor.org/rfc/rfc8489)). MD5 is not a FIPS-approved algorithm. In FIPS mode, AWS-LC still allows MD5, so TURN keeps working. Include this in your compliance review if your application uses TURN.

### Checking crypto calls in your application

To check whether a crypto call in your own code used a FIPS-approved algorithm, wrap it in `CALL_SERVICE_AND_CHECK_APPROVED`.

```c
#include <assert.h>
#include <openssl/crypto.h>
#include <openssl/md5.h>
#include <openssl/service_indicator.h>

enum FIPSStatus approved = AWSLC_NOT_APPROVED;
if (FIPS_mode() != 0) {
    CALL_SERVICE_AND_CHECK_APPROVED(approved, MD5(data, len, out));
}
/* approved == AWSLC_NOT_APPROVED: MD5 is not FIPS approved */
```

Compile this code with `-DBORINGSSL_FIPS`. Without it, every call reports as approved.

## Post-quantum key exchange

With AWS-LC, the SDK offers `X25519MLKEM768` first on these connections:

| Connection | Offers ML-KEM |
|---|---|
| Control plane HTTPS (for example `DescribeSignalingChannel`, `GetSignalingChannelEndpoint`) | yes |
| `GetIceServerConfig` and `JoinStorageSession` | yes |
| Signaling WebSocket (`ConnectAsMaster`, `ConnectAsViewer`) | yes |
| TURN over TLS (`turns:`) | yes |
| Peer-to-peer media (DTLS-SRTP) | no |

A connection uses ML-KEM only when the AWS endpoint also supports it. Otherwise it uses a classical group.

Peer-to-peer media uses DTLS 1.2, which does not support ML-KEM. Media key exchange is not post-quantum with any configuration.

## Verifying the build

At startup, `initKvsWebRtc()` logs the crypto backend and its FIPS state. Set `AWS_KVS_LOG_LEVEL=3` or lower to see these lines. A FIPS build prints:

```
Crypto backend: AWS-LC 3.1.0 (headers), loaded library reports: AWS-LC FIPS 3.1.0
AWS-LC FIPS mode: ENABLED (AWS-LC FIPS)
```

A non-FIPS build prints `AWS-LC FIPS mode: disabled (non-FIPS build)` on the second line.

## Troubleshooting

### `Could not find AWS-LC` or `already contains a non-AWS-LC libssl`

The dependency folder, `open-source/` by default, holds an earlier OpenSSL build. Remove it and configure again, or use a separate folder:

```bash
rm -rf open-source
cmake .. -DUSE_AWS_LC=ON
```

Removing the `build` folder alone does not fix this.

### The two versions in the startup log differ

The application loaded a different libcrypto at runtime, usually a system OpenSSL. Check `LD_LIBRARY_PATH` and `ldd` on Linux, or `DYLD_LIBRARY_PATH` and `otool -L` on macOS.

### `AWS-LC FIPS MISMATCH` in the startup log

The SDK was built with `-DUSE_AWS_LC_FIPS=ON` but linked a non-FIPS AWS-LC. Remove the dependency folder and configure again.
