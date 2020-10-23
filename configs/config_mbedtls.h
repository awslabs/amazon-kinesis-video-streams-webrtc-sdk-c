//
// mbedTLS config
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Minimal Suite-B configuration
 *
 * Inherited from https://github.com/ARMmbed/mbedtls/blob/1e14827bebc8480786498f007304bd852ef953c6/configs/config-suite-b.h.
 * Some of the configurations are disabled due to incompatibility with what we need. The configs that are disabled are still
 * maintained here for documentation purpose only. Enabling them will break some parts of the SDK. Each disabled config has
 * a "note" that explains why it needs to be disabled.
 *
 */

/* System support */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME

/* mbed TLS feature support */
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_SSL_PROTO_TLS1_2

/* mbed TLS modules */
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_NET_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C

/* For test certificates */
#define MBEDTLS_BASE64_C
#define MBEDTLS_CERTS_C
#define MBEDTLS_PEM_PARSE_C

/* Save RAM at the expense of ROM */
#define MBEDTLS_AES_ROM_TABLES

/* Save RAM by adjusting to our exact needs
 *
 * Note: disabled because we want RSA as well, which is much bigger that ECP.
 */
// #define MBEDTLS_ECP_MAX_BITS 384
// #define MBEDTLS_MPI_MAX_SIZE 48 // 384 bits is 48 bytes

/* Save RAM at the expense of speed, see ecp.h */
#define MBEDTLS_ECP_WINDOW_SIZE       2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0

/* Significant speed benefit at the expense of some ROM */
#define MBEDTLS_ECP_NIST_OPTIM

/*
 * You should adjust this to the exact number of sources you're using: default
 * is the "mbedtls_platform_entropy_poll" source, but you may want to add other ones.
 * Minimum is 2 for the entropy test suite.
 */
#define MBEDTLS_ENTROPY_MAX_SOURCES 2

/* Save ROM and a few bytes of RAM by specifying our own ciphersuite list
 *
 * Note: disabled because we want high interoperability.
 */
// #define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256

/*
 * Save RAM at the expense of interoperability: do this only if you control
 * both ends of the connection!  (See coments in "mbedtls/ssl.h".)
 * The minimum size here depends on the certificate chain used as well as the
 * typical size of records.
 *
 * Note: disabled because we don't control the other peer.
 */
// #define MBEDTLS_SSL_MAX_CONTENT_LEN 1024

/**
 * Minimal Suite-B configuration end here
 */

/**
 * mbedTLS configuration extensions for KVS
 */

// The OpenSSL version still supports RSA key exchange, so we need to enable this as well for compatibility.
// Also, libwebsocket requires RSA key exchange.
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED

// Enable X509 certificate creation. This is being used for self-signed certificate generation within DTLS
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_WRITE_C

// DTLS is required as the foundation for data and media channels
#define MBEDTLS_SSL_PROTO_DTLS
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY

// Enable DTLS-SRTP extension which reuses the DTLS negotiated ciphersuite for SRTP
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_DTLS_SRTP

// MD5 is considered a weak hashing algorithm. But, we still use it, we have to enable it for compatibility
#define MBEDTLS_MD5_C

// Enable reading X509 certificate from the file system. This is used for the TLS module to load the client certificate
// to talk to our turn and signaling
#define MBEDTLS_FS_IO
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C

// Enable big prime number generation, primarily used in public-private key pair generation
#define MBEDTLS_GENPRIME

// Enable mbedTLS error code to string conversion
#define MBEDTLS_ERROR_C

// Enable support for RFC 6066 server name indication (SNI) in SSL. This is necessary because our backend service uses SNI
// in the certificate for verification.
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

// Enable SHA1 message digest algorithm. This is used in self-signed certificate generation in DTLS.
#define MBEDTLS_SHA1_C

// Enable this because we need to validate the peer certificate by recalculating the signature and match it with the
// given signature from SDP
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE

// mbedTLS somehow fails to find some string functions if we don't include string.h ourselves
#include <string.h>

// utility from mbedTLS to check all the config requirements are fulfilled
#include <mbedtls/check_config.h>

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CONFIG__
