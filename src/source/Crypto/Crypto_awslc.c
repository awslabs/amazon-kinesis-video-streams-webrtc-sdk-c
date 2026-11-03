/**
 * AWS-LC specific crypto support: post-quantum group configuration and
 * FIPS/PQC status reporting.
 *
 * This file is compiled only when the AWS-LC backend is selected
 * (-DUSE_AWS_LC=ON). Everything else in src/source/Crypto/ is shared verbatim
 * with the OpenSSL backend, because AWS-LC presents the same API.
 *
 * Scope note on what PQC can and cannot cover in this SDK:
 *
 *   IN SCOPE  - TLS 1.3 connections to AWS endpoints. Two independent stacks
 *               carry these: libwebsockets owns the signaling control-plane
 *               HTTPS calls and the signaling WSS connection, while this SDK's
 *               own Tls_openssl.c owns TURN-over-TLS (turns:). ML-KEM applies
 *               to both.
 *
 *   OUT OF    - The DTLS-SRTP media path. AWS-LC caps DTLS at 1.2 and ML-KEM
 *   SCOPE       key exchange exists only in TLS 1.3, so peer-to-peer media
 *               confidentiality cannot be made post-quantum by this work. Do
 *               not configure PQC groups on a DTLS SSL_CTX; it would be
 *               ignored at best.
 */
#define LOG_CLASS "CryptoAwsLc"
#include "../Include_i.h"

#ifdef KVS_USE_AWS_LC

/* Set once by awsLcLogCryptoBackend() so the banner is emitted a single time
 * even though initKvsWebRtc() may be called repeatedly by an embedder. */
static volatile SIZE_T gAwsLcBannerLogged = 0;

/*
 * FIPS_version() and FIPS_module_name() are public on AWS-LC mainline from
 * API version 37 (v5.x). On the fips-* branches (AWS-LC-FIPS 4.2.0 is API 35)
 * they are declared only under the library-private BORINGSSL_FIPS_140_3 macro
 * and are not exported from libcrypto at all, so a build against the very
 * library that is actually FIPS-validated fails at compile time if it calls
 * them. FIPS_mode() exists and is exported on both lines; it is the portable
 * probe. The richer identifiers are reported when the headers offer them.
 */
#if AWSLC_API_VERSION >= 37
#define KVS_AWS_LC_HAS_FIPS_MODULE_INFO 1
#endif

/**
 * TRUE when the linked AWS-LC is a FIPS build.
 *
 * FIPS_mode() returns 1 for a library compiled with -DFIPS=1 and 0 otherwise.
 * This is a property of how AWS-LC was compiled, not something the SDK can
 * toggle; FIPS_mode_set() only succeeds when its argument already matches.
 */
static BOOL awsLcIsFipsBuild(VOID)
{
    return (FIPS_mode() != 0) ? TRUE : FALSE;
}

VOID awsLcLogCryptoBackend(VOID)
{
    BOOL fipsBuild;
    BOOL fipsExpected = FALSE;
    UINT32 fipsModuleVersion = 0;
    const CHAR* pModuleName = NULL;
    const CHAR* pLoadedVersion = NULL;

    if (ATOMIC_EXCHANGE(&gAwsLcBannerLogged, 1) != 0) {
        return;
    }

    /*
     * OpenSSL_version() returns a string baked into libcrypto itself, so it
     * identifies the library actually loaded rather than the headers this file
     * was compiled against. The two differ when a stale or system libcrypto is
     * picked up ahead of the intended AWS-LC, which is precisely the case worth
     * surfacing in a diagnostic banner. AWSLC_VERSION_NUMBER_STRING (the header
     * value) is logged next to it so a mismatch is visible.
     */
    pLoadedVersion = OpenSSL_version(OPENSSL_VERSION);
    DLOGI("Crypto backend: AWS-LC %s (headers), loaded library reports: %s", AWSLC_VERSION_NUMBER_STRING,
          pLoadedVersion != NULL ? pLoadedVersion : "unknown");

    fipsBuild = awsLcIsFipsBuild();
#ifdef KVS_AWS_LC_HAS_FIPS_MODULE_INFO
    fipsModuleVersion = (UINT32) FIPS_version();
    pModuleName = FIPS_module_name();
#else
    // The fips-* branches do not expose a module version through the public
    // API. The name is still known: FIPS_mode() above settles it at runtime.
    pModuleName = fipsBuild ? "AWS-LC FIPS" : "AWS-LC";
#endif
    if (pModuleName == NULL) {
        pModuleName = "AWS-LC";
    }

#ifdef KVS_USE_AWS_LC_FIPS
    fipsExpected = TRUE;
#endif

    if (fipsBuild && fipsModuleVersion != 0) {
        DLOGI("AWS-LC FIPS mode: ENABLED (%s, module version %u)", pModuleName, fipsModuleVersion);
    } else if (fipsBuild) {
        // fips-* branch headers do not expose the module version publicly
        DLOGI("AWS-LC FIPS mode: ENABLED (%s)", pModuleName);
    } else {
        DLOGI("AWS-LC FIPS mode: disabled (non-FIPS build)");
    }

    /*
     * A mismatch here is a real build defect, not cosmetic. It means the SDK was
     * configured with -DUSE_AWS_LC_FIPS=ON but linked against a non-FIPS AWS-LC
     * (or the reverse) - typically a stale artifact left in OPEN_SRC_INSTALL_PREFIX
     * from an earlier non-FIPS configure, since build_dependency() skips the
     * rebuild when it finds an existing libssl.
     */
    if (fipsExpected && !fipsBuild) {
        DLOGE("AWS-LC FIPS MISMATCH: built with -DUSE_AWS_LC_FIPS=ON but the linked AWS-LC is NOT a FIPS build. "
              "This binary is not FIPS-validated. Remove the stale dependency install prefix and reconfigure.");
    } else if (!fipsExpected && fipsBuild) {
        DLOGW("Linked AWS-LC is a FIPS build although -DUSE_AWS_LC_FIPS was not set.");
    }

    /*
     * ML-KEM is compiled into every AWS-LC build we support, both mainline and
     * the FIPS branches (AWS-LC-FIPS 3.0 was the first FIPS 140-3 validation to
     * include ML-KEM), and awsLcConfigureTlsGroups() puts it first on every
     * client context the SDK creates. This line reports that the group is
     * available and offered, not that any handshake negotiated it: the DTLS
     * media path is DTLS 1.2, which has no ML-KEM at all, and a peer without
     * ML-KEM still negotiates a classical group.
     */
    DLOGD("Post-quantum key exchange: offered (X25519MLKEM768, 0x%04x) on the TLS 1.3 connections to AWS "
          "(control plane HTTPS, signaling WSS, TURNS); DTLS-SRTP media path is DTLS 1.2 and remains classical",
          SSL_GROUP_X25519_MLKEM768);
}

#endif /* KVS_USE_AWS_LC */
