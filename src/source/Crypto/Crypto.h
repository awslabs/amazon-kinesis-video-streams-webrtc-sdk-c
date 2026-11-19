#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KVS_USE_OPENSSL
#include <openssl/opensslv.h>

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
 * X509_get_notBefore/X509_get_notAfter are deprecated 1.1.0 aliases that were removed in 4.0.
 * The X509_getm_* variants return the mutable ASN1_TIME* that X509_gmtime_adj() needs and exist since 1.1.0.
 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define KVS_X509_GETM_NOTBEFORE(cert) X509_getm_notBefore(cert)
#define KVS_X509_GETM_NOTAFTER(cert)  X509_getm_notAfter(cert)
#else
#define KVS_X509_GETM_NOTBEFORE(cert) X509_get_notBefore(cert)
#define KVS_X509_GETM_NOTAFTER(cert)  X509_get_notAfter(cert)
#endif

#define KVS_RSA_F4             RSA_F4
#define KVS_MD5_DIGEST_LENGTH  MD5_DIGEST_LENGTH
#define KVS_SHA1_DIGEST_LENGTH SHA_DIGEST_LENGTH

/* MD5 one-shot: MD5() removed in OpenSSL 3.x, use EVP_Q_digest */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_MD5_DIGEST(m, mlen, ob)                                                                                                                  \
    CHK(1 == EVP_Q_digest(NULL, "MD5", NULL, (m), (mlen), (ob), NULL), STATUS_ICE_FAILED_TO_COMPUTE_MD5_FOR_LONG_TERM_CREDENTIAL);
#else
#define KVS_MD5_DIGEST(m, mlen, ob) CHK(NULL != MD5((m), (mlen), (ob)), STATUS_ICE_FAILED_TO_COMPUTE_MD5_FOR_LONG_TERM_CREDENTIAL);
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

/* go through the generic message-digest layer rather than the MD5 module. */
#define KVS_MD5_DIGEST(m, mlen, ob)                                                                                                                  \
    CHK(0 == mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_MD5), (m), (mlen), (ob)), STATUS_ICE_FAILED_TO_COMPUTE_MD5_FOR_LONG_TERM_CREDENTIAL);
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

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CRYPTO__
