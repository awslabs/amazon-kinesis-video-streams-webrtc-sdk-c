#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_TLS__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_TLS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TLS_SESSION_STATE_NEW,        /* Tls is just created, but the handshake process has not started */
    TLS_SESSION_STATE_CONNECTING, /* TLS is in the process of negotiating a secure connection and verifying the remote fingerprint. */
    TLS_SESSION_STATE_CONNECTED,  /* TLS has completed negotiation of a secure connection and verified the remote fingerprint. */
    TLS_SESSION_STATE_CLOSED,     /* The transport has been closed intentionally as the result of receipt of a close_notify alert */
} TLS_SESSION_STATE;

/* Callback that is fired when Tls session wishes to send packet */
typedef STATUS (*TlsSessionOutboundPacketFunc)(UINT64, PBYTE, UINT32);

/*  Callback that is fired when Tls state has changed */
typedef VOID (*TlsSessionOnStateChange)(UINT64, TLS_SESSION_STATE);

typedef struct {
    UINT64 outBoundPacketFnCustomData;
    // outBoundPacketFn is a required callback to tell TlsSession how to send outbound packets
    TlsSessionOutboundPacketFunc outboundPacketFn;
    // stateChangeFn is an optional callback to listen to TlsSession state changes
    UINT64 stateChangeFnCustomData;
    TlsSessionOnStateChange stateChangeFn;
} TlsSessionCallbacks, *PTlsSessionCallbacks;

typedef struct __TlsSession TlsSession, *PTlsSession;
struct __TlsSession {
    TlsSessionCallbacks callbacks;
    TLS_SESSION_STATE state;

#ifdef KVS_USE_OPENSSL
    SSL_CTX* pSslCtx;
    SSL* pSsl;
#elif KVS_USE_MBEDTLS
    IOBuffer* pReadBuffer;

    mbedtls_ssl_context sslCtx;
    mbedtls_ssl_config sslCtxConfig;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_x509_crt cacert;
#else
#error "A Crypto implementation is required."
#endif
};

/**
 * Create TLS session.
 * NOT THREAD SAFE.
 * @param PTlsSessionCallbacks - callbacks
 * @param PTlsSession* - pointer to created TlsSession object
 *
 * @return STATUS - status of operation
 */
STATUS createTlsSession(PTlsSessionCallbacks, PTlsSession*);

/**
 * Free TLS session. Not thread safe.
 * @param PTlsSession - TlsSession object to free
 * @return STATUS - status of operation
 */
STATUS freeTlsSession(PTlsSession*);

/**
 * Start TLS handshake.
 * NOT THREAD SAFE.
 * @param PTlsSession - TlsSession object
 * @param BOOL - is server
 * @return STATUS - status of operation
 */
STATUS tlsSessionStart(PTlsSession, BOOL);

/**
 * Decrypt application data up to specified bytes. The decrypted data will be copied back to the original buffer.
 * During handshaking, the return data size should be always 0 since there's no application data yet.
 * NOT THREAD SAFE.
 * @param PTlsSession - TlsSession object
 * @param PBYTE - encrypted data
 * @param UINT32 - the size of buffer that PBYTE is pointing to
 * @param PUINT32 - pointer to the size of encrypted data and will be used to store the size of application data
 */
STATUS tlsSessionProcessPacket(PTlsSession, PBYTE, UINT32, PUINT32);

/**
 * Encrypt application data up to specified bytes. The encrypted data will be sent through specified callback during
 * initialization. If NULL is specified, it'll only check for pending handshake buffer.
 * NOT THREAD SAFE.
 * @param PTlsSession - TlsSession object
 * @param PBYTE - plain data
 * @param UINT32 - the size of encrypted data
 */
STATUS tlsSessionPutApplicationData(PTlsSession, PBYTE, UINT32);

/**
 * Mark Tls session to be closed
 * NOT THREAD SAFE.
 */
STATUS tlsSessionShutdown(PTlsSession);

/* internal functions */
STATUS tlsSessionChangeState(PTlsSession, TLS_SESSION_STATE);

#ifdef KVS_USE_OPENSSL
INT32 tlsSessionCertificateVerifyCallback(INT32, X509_STORE_CTX*);
#elif KVS_USE_MBEDTLS
// following are required callbacks for mbedtls
// NOTE: const is not a pure C qualifier, they're here because there's no way to type cast
//       a callback signature.
INT32 tlsSessionSendCallback(PVOID, const unsigned char*, ULONG);
INT32 tlsSessionReceiveCallback(PVOID, unsigned char*, ULONG);
#else
#error "A Crypto implementation is required."
#endif

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_TLS__
