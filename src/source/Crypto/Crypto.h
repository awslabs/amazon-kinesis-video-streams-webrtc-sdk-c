#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KVS_USE_OPENSSL
#include <openssl/opensslv.h>

#ifdef KVS_USE_AWS_LC
/*
 * ---------------------------------------------------------------------------
 * AWS-LC additions.
 *
 * AWS-LC advertises OPENSSL_VERSION_NUMBER 0x1010107f (OpenSSL 1.1.1
 * compatible), so every version gate in this header and in the *_openssl.c
 * sources resolves to the 1.1.x branch. That is correct and intentional: the
 * legacy RSA/EC_KEY/HMAC/MD5 APIs those branches use are all present in AWS-LC.
 * Nothing below overrides them; this block only adds what AWS-LC uniquely
 * offers.
 *
 * OPENSSL_IS_AWSLC is AWS-LC's own marker, defined in <openssl/base.h>. We
 * assert on it so a build configured with -DUSE_AWS_LC=ON that accidentally
 * resolves real OpenSSL headers fails at compile time rather than silently
 * producing a non-PQC, non-FIPS binary.
 * ---------------------------------------------------------------------------
 */
/* <openssl/base.h> is an AWS-LC/BoringSSL header; real OpenSSL has no such file
 * in any version. Probe for it first so a misconfigured include path produces an
 * actionable message rather than a bare "file not found". The #ifdef wrapper
 * around __has_include is required because a bare __has_include in an #if is a
 * syntax error on pre-C23 compilers - the && does not short-circuit the parse.
 * Same pattern as the mbedTLS 4 probe in Include_i.h. */
#ifdef __has_include
#if !__has_include(<openssl/base.h>)
#error                                                                                                                                               \
    "USE_AWS_LC=ON but <openssl/base.h> is missing, so the include path resolved real OpenSSL instead of AWS-LC. A stale libssl in OPEN_SRC_INSTALL_PREFIX from a previous non-AWS-LC build is the usual cause: remove that directory and reconfigure."
#endif
#endif
#include <openssl/base.h>
#ifndef OPENSSL_IS_AWSLC
#error                                                                                                                                               \
    "USE_AWS_LC=ON but OPENSSL_IS_AWSLC is not defined, so the resolved library is not AWS-LC. Continuing would silently produce a binary with neither post-quantum key exchange nor FIPS. Check AWSLC_INCLUDE_DIR / OPEN_SRC_INSTALL_PREFIX."
#endif
#include <openssl/crypto.h>
#include <openssl/nid.h>
#include <openssl/service_indicator.h>

/*
 * Post-quantum key exchange groups, in client preference order.
 *
 * X25519MLKEM768 (RFC 10024, IANA codepoint 0x11EC) is the hybrid group the
 * PQC Confidentiality Campaign targets. It is already first in AWS-LC's own
 * default group list, so PQC is negotiated even without this configuration.
 * We set it explicitly anyway so the SDK's posture is deterministic and
 * auditable rather than dependent on a library default that may be reordered.
 *
 * NIDs (not SSL_GROUP_* codepoints) are used deliberately: SSL_CTX_set1_groups
 * takes NIDs and exists on both AWS-LC mainline and the fips-* branches, whereas
 * the codepoint-based SSL_CTX_set1_group_ids is mainline-only.
 *
 * The classical groups are retained as fallbacks so a server that does not yet
 * offer ML-KEM still completes a handshake instead of failing closed.
 */
#define KVS_AWS_LC_PQC_GROUPS {NID_X25519MLKEM768, NID_X25519, NID_X9_62_prime256v1, NID_secp384r1}

/* Groups used when PQC is explicitly disabled at runtime. */
#define KVS_AWS_LC_CLASSICAL_GROUPS {NID_X25519, NID_X9_62_prime256v1, NID_secp384r1}

/*
 * FIPS builds are selected at compile time (-DFIPS=1 when building AWS-LC);
 * there is no runtime switch. FIPS_mode_set() only succeeds when its argument
 * already matches how the library was compiled, so never call it to "turn FIPS
 * on". AWSLC_FIPS is AWS-LC's own define for a FIPS build; KVS_USE_AWS_LC_FIPS
 * is ours, recording what the SDK was configured to expect. AwsLc_awslc.c
 * cross-checks the two and warns on mismatch.
 */

/* Emit a one-time log line describing the AWS-LC build (version, FIPS state,
 * PQC availability). Called from initKvsWebRtc(). Lives in Crypto_awslc.c,
 * which only kvsWebrtcClient compiles. */
VOID awsLcLogCryptoBackend(VOID);

/* awsLcConfigureTlsGroups() and awsLcRecordNegotiatedGroup() are static inline
 * in AwsLcPqc.h, included below: both SDK libraries need them and they are
 * separate link targets. */
#endif /* KVS_USE_AWS_LC */

/*
 * OpenSSL 3.x compatibility macros.
 * The deprecated 1.1.x APIs still work in 3.x but will be removed in 4.0.
 * When OPENSSL_VERSION_NUMBER >= 0x30000000L, use the modern EVP-based APIs.
 */

/* SSL_get_peer_certificate was renamed to SSL_get1_peer_certificate in 3.0 */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_SSL_GET_PEER_CERTIFICATE(ssl) SSL_get1_peer_certificate(ssl)
#else
#define KVS_SSL_GET_PEER_CERTIFICATE(ssl) SSL_get_peer_certificate(ssl)
#endif

/*
 * X509_get_notBefore/X509_get_notAfter were removed in OpenSSL 4.0. The mutable
 * X509_getm_notBefore/X509_getm_notAfter variants (returning ASN1_TIME* as
 * X509_gmtime_adj requires) exist since 1.1.0; fall back to the old names on 1.0.x.
 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define KVS_X509_GET_NOT_BEFORE(x) X509_getm_notBefore(x)
#define KVS_X509_GET_NOT_AFTER(x)  X509_getm_notAfter(x)
#else
#define KVS_X509_GET_NOT_BEFORE(x) X509_get_notBefore(x)
#define KVS_X509_GET_NOT_AFTER(x)  X509_get_notAfter(x)
#endif

#define KVS_RSA_F4             RSA_F4
#define KVS_MD5_DIGEST_LENGTH  MD5_DIGEST_LENGTH
#define KVS_SHA1_DIGEST_LENGTH SHA_DIGEST_LENGTH

/* MD5 one-shot: MD5() removed in OpenSSL 3.x, use EVP_Q_digest */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_MD5_DIGEST(m, mlen, ob) EVP_Q_digest(NULL, "MD5", NULL, (m), (mlen), (ob), NULL);
#else
#define KVS_MD5_DIGEST(m, mlen, ob) MD5((m), (mlen), (ob));
#endif

/* HMAC one-shot: HMAC() deprecated in OpenSSL 3.x, use kvsSha1Hmac() function with proper cleanup */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
STATUS kvsSha1Hmac(const PBYTE pKey, size_t keyLen, const PBYTE pMessage, size_t messageLen, PBYTE pOutput, PUINT32 pOutputLen);
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    CHK_STATUS(kvsSha1Hmac((const PBYTE)(k), (size_t) (klen), (const PBYTE)(m), (size_t) (mlen), (ob), (plen)));
#else
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    CHK(NULL != HMAC(EVP_sha1(), (k), (INT32) (klen), (m), (mlen), (ob), (plen)), STATUS_HMAC_GENERATION_ERROR);
#endif

/*
 * KVS_CRYPTO_INIT: On OpenSSL 1.1.0+, the library auto-initializes so the legacy
 * init calls are no-ops. On 3.x, OPENSSL_INIT_NO_ATEXIT prevents OPENSSL_cleanup()
 * from being called by libwebsockets on context destroy, avoiding state corruption.
 */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_CRYPTO_INIT()                                                                                                                            \
    do {                                                                                                                                             \
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_NO_ATEXIT, NULL);                                                              \
    } while (0)
#else
#define KVS_CRYPTO_INIT()                                                                                                                            \
    do {                                                                                                                                             \
        OpenSSL_add_ssl_algorithms();                                                                                                                \
        SSL_load_error_strings();                                                                                                                    \
        SSL_library_init();                                                                                                                          \
    } while (0)
#endif

#define LOG_OPENSSL_ERROR(s)                                                                                                                         \
    while ((sslErr = ERR_get_error()) != 0) {                                                                                                        \
        if (sslErr != SSL_ERROR_WANT_WRITE && sslErr != SSL_ERROR_WANT_READ) {                                                                       \
            DLOGW("%s failed with %s", (s), ERR_error_string(sslErr, NULL));                                                                         \
        }                                                                                                                                            \
    }

typedef enum {
    KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80 = SRTP_AES128_CM_SHA1_80,
    KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_32 = SRTP_AES128_CM_SHA1_32,
} KVS_SRTP_PROFILE;
#elif KVS_USE_MBEDTLS
#define KVS_RSA_F4             0x10001L
#define KVS_MD5_DIGEST_LENGTH  16
#define KVS_SHA1_DIGEST_LENGTH 20
#if MBEDTLS_VERSION_MAJOR >= 4
/* TF-PSA-Crypto 1.0 (mbedTLS 4) removed the legacy MBEDTLS_MD5_C module and the
 * one-shot mbedtls_md5(); MD5 is reachable only through PSA (PSA_WANT_ALG_MD5). */
#define KVS_MD5_DIGEST(m, mlen, ob)                                                                                                                  \
    do {                                                                                                                                             \
        size_t _kvsMd5Len = 0;                                                                                                                       \
        (void) psa_crypto_init();                                                                                                                    \
        (void) psa_hash_compute(PSA_ALG_MD5, (const unsigned char*) (m), (mlen), (unsigned char*) (ob), KVS_MD5_DIGEST_LENGTH, &_kvsMd5Len);         \
    } while (0);
#elif MBEDTLS_VERSION_NUMBER >= 0x03000000
#define KVS_MD5_DIGEST(m, mlen, ob) mbedtls_md5((m), (mlen), (ob));
#else
#define KVS_MD5_DIGEST(m, mlen, ob) mbedtls_md5_ret((m), (mlen), (ob));
#endif
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    CHK(0 == mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), (k), (klen), (m), (mlen), (ob)), STATUS_HMAC_GENERATION_ERROR);             \
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1));
#if MBEDTLS_VERSION_MAJOR >= 4
/* PSA Crypto must be initialised before any psa_* call. psa_crypto_init() is
 * idempotent; calling it once from the SDK init path avoids the per-session
 * race on builds without MBEDTLS_THREADING_C. */
#define KVS_CRYPTO_INIT()                                                                                                                            \
    do {                                                                                                                                             \
        (void) psa_crypto_init();                                                                                                                    \
    } while (0)
#else
#define KVS_CRYPTO_INIT()                                                                                                                            \
    do {                                                                                                                                             \
    } while (0)
#endif
#define LOG_MBEDTLS_ERROR(s, ret)                                                                                                                    \
    do {                                                                                                                                             \
        CHAR __mbedtlsErr[1024];                                                                                                                     \
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {                                                                 \
            mbedtls_strerror(ret, __mbedtlsErr, SIZEOF(__mbedtlsErr));                                                                               \
            DLOGW("%s failed with %s", (s), __mbedtlsErr);                                                                                           \
        }                                                                                                                                            \
    } while (0)

typedef enum {
    KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80 = MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
    KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_32 = MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32,
} KVS_SRTP_PROFILE;
#else
#error "A Crypto implementation is required."
#endif

/* Must come after the KVS_AWS_LC_* group macros and the SRTP profile enum above,
 * which its static inline definitions reference. */
#include "AwsLcPqc.h"

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__
