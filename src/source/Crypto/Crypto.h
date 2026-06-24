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

#define KVS_RSA_F4             RSA_F4
#define KVS_MD5_DIGEST_LENGTH  MD5_DIGEST_LENGTH
#define KVS_SHA1_DIGEST_LENGTH SHA_DIGEST_LENGTH

/* MD5 one-shot: MD5() removed in OpenSSL 3.x, use EVP_Q_digest */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_MD5_DIGEST(m, mlen, ob) EVP_Q_digest(NULL, "MD5", NULL, (m), (mlen), (ob), NULL);
#else
#define KVS_MD5_DIGEST(m, mlen, ob) MD5((m), (mlen), (ob));
#endif

/* HMAC one-shot: HMAC() removed in OpenSSL 3.x, use EVP_MAC API */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    do {                                                                                                                                             \
        EVP_MAC* _mac = EVP_MAC_fetch(NULL, "HMAC", NULL);                                                                                           \
        EVP_MAC_CTX* _ctx = (_mac != NULL) ? EVP_MAC_CTX_new(_mac) : NULL;                                                                           \
        OSSL_PARAM _params[2];                                                                                                                       \
        size_t _outlen = 0;                                                                                                                          \
        _params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA1", 0);                                                                          \
        _params[1] = OSSL_PARAM_construct_end();                                                                                                     \
        CHK(_mac != NULL && _ctx != NULL, STATUS_HMAC_GENERATION_ERROR);                                                                             \
        CHK(EVP_MAC_init(_ctx, (k), (klen), _params) == 1, STATUS_HMAC_GENERATION_ERROR);                                                            \
        CHK(EVP_MAC_update(_ctx, (m), (mlen)) == 1, STATUS_HMAC_GENERATION_ERROR);                                                                   \
        CHK(EVP_MAC_final(_ctx, (ob), &_outlen, EVP_MAX_MD_SIZE) == 1, STATUS_HMAC_GENERATION_ERROR);                                                \
        *(plen) = (UINT32) _outlen;                                                                                                                  \
        EVP_MAC_CTX_free(_ctx);                                                                                                                      \
        EVP_MAC_free(_mac);                                                                                                                          \
    } while (0);
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
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define KVS_MD5_DIGEST(m, mlen, ob) mbedtls_md5((m), (mlen), (ob));
#else
#define KVS_MD5_DIGEST(m, mlen, ob) mbedtls_md5_ret((m), (mlen), (ob));
#endif
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    CHK(0 == mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), (k), (klen), (m), (mlen), (ob)), STATUS_HMAC_GENERATION_ERROR);             \
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1));
#define KVS_CRYPTO_INIT()                                                                                                                            \
    do {                                                                                                                                             \
    } while (0)
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
