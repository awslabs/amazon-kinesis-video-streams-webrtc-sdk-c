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

#undef MBEDTLS_SSL_ALPN

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
