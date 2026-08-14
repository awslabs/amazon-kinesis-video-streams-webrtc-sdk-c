#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// enable DTLS-SRTP extension
#define MBEDTLS_SSL_DTLS_SRTP

// disable TLS 1.0 and 1.1 as they should be deprecated in major browser vendors
#undef MBEDTLS_SSL_CBC_RECORD_SPLITTING
#undef MBEDTLS_SSL_PROTO_TLS1
#undef MBEDTLS_SSL_PROTO_TLS1_1

// disable because they don't comply with AWS security standard
#undef MBEDTLS_ECP_DP_SECP224K1_ENABLED
#undef MBEDTLS_ECP_DP_SECP256K1_ENABLED

// The mbedTLS 4 build links a mainline libwebsockets whose TLS layer calls
// mbedtls_ssl_get_alpn_protocol() unconditionally. This user-config is applied
// to the mbedTLS build itself, so undef'ing ALPN there breaks the lws link.
// Keep it stripped only on mbedTLS < 4 (paired with the v4.3.10 lws that
// doesn't reference ALPN). Gate on the version macro, which build_info.h
// defines before including this user config.
#if MBEDTLS_VERSION_MAJOR < 4
#undef MBEDTLS_SSL_ALPN
#else
// mbedTLS 4 drops PEM/Base64 from its defaults; PEM CA bundles (the AWS root
// cert lws parses via client_ssl_ca_mem) need them, or x509 parse returns
// MBEDTLS_ERR_X509_INVALID_FORMAT. mbedTLS 3.x already enables these.
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#endif

/**
 * \def MBEDTLS_ENTROPY_HARDWARE_ALT
 *
 * Uncomment this macro to let mbed TLS use your own implementation of a
 * hardware entropy collector.
 *
 * Your function must be called \c mbedtls_hardware_poll(), have the same
 * prototype as declared in entropy_poll.h, and accept NULL as first argument.
 *
 * Uncomment to use your own hardware entropy collector.
 */
// #define MBEDTLS_ENTROPY_HARDWARE_ALT

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
