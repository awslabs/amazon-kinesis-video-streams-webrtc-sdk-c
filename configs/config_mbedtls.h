#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// enable DTLS-SRTP extension
#define MBEDTLS_SSL_DTLS_SRTP

// disable TLS 1.0 and 1.1 as they should be deprecated in major browser vendors
#undef MBEDTLS_SSL_PROTO_TLS1
#undef MBEDTLS_SSL_PROTO_TLS1_1
#undef MBEDTLS_SSL_CBC_RECORD_SPLITTING

// disable because they don't comply with AWS security standard
#undef MBEDTLS_ECP_DP_SECP224K1_ENABLED
#undef MBEDTLS_ECP_DP_SECP256K1_ENABLED

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
