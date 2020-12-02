//
// Dtls
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SRTP_MASTER_KEY_LEN   16
#define MAX_SRTP_SALT_KEY_LEN     14
#define MAX_DTLS_RANDOM_BYTES_LEN 32
#define MAX_DTLS_MASTER_KEY_LEN   48

#define GENERATED_CERTIFICATE_MAX_SIZE 4096
#define GENERATED_CERTIFICATE_BITS     2048
#define GENERATED_CERTIFICATE_SERIAL   1
#define GENERATED_CERTIFICATE_DAYS     365
#define GENERATED_CERTIFICATE_NAME     "KVS-WebRTC-Client"
#define KEYING_EXTRACTOR_LABEL         "EXTRACTOR-dtls_srtp"

/*
 * DTLS transmission interval timer (in 100ns)
 */
#define DTLS_TRANSMISSION_INTERVAL (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define DTLS_SESSION_TIMER_START_DELAY (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define SECONDS_IN_A_DAY (24 * 60 * 60LL)

#define HUNDREDS_OF_NANOS_IN_A_DAY (HUNDREDS_OF_NANOS_IN_AN_HOUR * 24LL)

typedef enum {
    RTC_DTLS_TRANSPORT_STATE_NEW,
    RTC_DTLS_TRANSPORT_STATE_CONNECTING, /* DTLS is in the process of negotiating a secure connection and verifying the remote fingerprint. */
    RTC_DTLS_TRANSPORT_STATE_CONNECTED,  /* DTLS has completed negotiation of a secure connection and verified the remote fingerprint. */
    RTC_DTLS_TRANSPORT_STATE_CLOSED,     /* The transport has been closed intentionally as the result of receipt of a close_notify alert */
    RTC_DTLS_TRANSPORT_STATE_FAILED,     /* The transport has failed as the result of an error */
} RTC_DTLS_TRANSPORT_STATE;

/* Callback that is fired when Dtls Server wishes to send packet */
typedef VOID (*DtlsSessionOutboundPacketFunc)(UINT64, PBYTE, UINT32);

/*  Callback that is fired when Dtls state has changed */
typedef VOID (*DtlsSessionOnStateChange)(UINT64, RTC_DTLS_TRANSPORT_STATE);

typedef struct {
    UINT64 outBoundPacketFnCustomData;
    DtlsSessionOutboundPacketFunc outboundPacketFn;
    UINT64 stateChangeFnCustomData;
    DtlsSessionOnStateChange stateChangeFn;
} DtlsSessionCallbacks, *PDtlsSessionCallbacks;

// DtlsKeyingMaterial is information extracted via https://tools.ietf.org/html/rfc5705
// also includes the use_srtp value from Handshake
typedef struct {
    BYTE clientWriteKey[MAX_SRTP_MASTER_KEY_LEN + MAX_SRTP_SALT_KEY_LEN];
    BYTE serverWriteKey[MAX_SRTP_MASTER_KEY_LEN + MAX_SRTP_SALT_KEY_LEN];
    UINT8 key_length;

    KVS_SRTP_PROFILE srtpProfile;
} DtlsKeyingMaterial, *PDtlsKeyingMaterial;

#ifdef KVS_USE_OPENSSL
typedef struct {
    BOOL created;
    X509* pCert;
    EVP_PKEY* pKey;
} DtlsSessionCertificateInfo, *PDtlsSessionCertificateInfo;

#elif KVS_USE_MBEDTLS
typedef struct {
    mbedtls_x509_crt cert;
    mbedtls_pk_context privateKey;
    CHAR fingerprint[CERTIFICATE_FINGERPRINT_LENGTH + 1];
} DtlsSessionCertificateInfo, *PDtlsSessionCertificateInfo;

typedef struct {
    UINT64 updatedTime;
    UINT32 intermediateDelay, finalDelay;
} DtlsSessionTimer, *PDtlsSessionTimer;

typedef struct {
    BYTE masterSecret[MAX_DTLS_MASTER_KEY_LEN];
    // client random bytes + server random bytes
    BYTE randBytes[2 * MAX_DTLS_RANDOM_BYTES_LEN];
    mbedtls_tls_prf_types tlsProfile;
} TlsKeys, *PTlsKeys;
#else
#error "A Crypto implementation is required."
#endif

typedef struct __DtlsSession DtlsSession, *PDtlsSession;
struct __DtlsSession {
    volatile ATOMIC_BOOL isStarted;
    volatile ATOMIC_BOOL shutdown;
    UINT32 certificateCount;
    DtlsSessionCallbacks dtlsSessionCallbacks;
    TIMER_QUEUE_HANDLE timerQueueHandle;
    UINT32 timerId;
    UINT64 dtlsSessionStartTime;
    RTC_DTLS_TRANSPORT_STATE state;
    MUTEX sslLock;

#ifdef KVS_USE_OPENSSL
    volatile ATOMIC_BOOL sslInitFinished;
    // dtls message must fit into a UDP packet
    BYTE outgoingDataBuffer[MAX_UDP_PACKET_SIZE];
    UINT32 outgoingDataLen;
    CHAR certFingerprints[MAX_RTCCONFIGURATION_CERTIFICATES][CERTIFICATE_FINGERPRINT_LENGTH + 1];
    SSL_CTX* pSslCtx;
    SSL* pSsl;
#elif KVS_USE_MBEDTLS
    DtlsSessionTimer transmissionTimer;
    TlsKeys tlsKeys;
    PIOBuffer pReadBuffer;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_ssl_config sslCtxConfig;
    mbedtls_ssl_context sslCtx;
    DtlsSessionCertificateInfo certificates[MAX_RTCCONFIGURATION_CERTIFICATES];
#else
#error "A Crypto implementation is required."
#endif
};

/**
 * Create DTLS session. Not thread safe.
 * @param PDtlsSessionCallbacks - callbacks
 * @param TIMER_QUEUE_HANDLE - timer handle to schedule timer task with
 * @param INT32 - size of generated certificate
 * @param BOOL - whether to generate certificate or not
 * @param PRtcCertificate - user provided certificate
 * @param PDtlsSession* - pointer to created DtlsSession object
 *
 * @return STATUS - status of operation
 */
STATUS createDtlsSession(PDtlsSessionCallbacks, TIMER_QUEUE_HANDLE, INT32, BOOL, PRtcCertificate, PDtlsSession*);

/**
 * Free DTLS session. Not thread safe.
 * @param PDtlsSession - DtlsSession object to free
 * @return STATUS - status of operation
 */
STATUS freeDtlsSession(PDtlsSession*);

/**
 * Start DTLS handshake. Not thread safe.
 * @param PDtlsSession - DtlsSession object
 * @param BOOL - is server
 * @return STATUS - status of operation
 */
STATUS dtlsSessionStart(PDtlsSession, BOOL);
STATUS dtlsSessionProcessPacket(PDtlsSession, PBYTE, PINT32);
STATUS dtlsSessionIsInitFinished(PDtlsSession, PBOOL);
STATUS dtlsSessionPopulateKeyingMaterial(PDtlsSession, PDtlsKeyingMaterial);
STATUS dtlsSessionGetLocalCertificateFingerprint(PDtlsSession, PCHAR, UINT32);
STATUS dtlsSessionVerifyRemoteCertificateFingerprint(PDtlsSession, PCHAR);
STATUS dtlsSessionPutApplicationData(PDtlsSession, PBYTE, INT32);
STATUS dtlsSessionShutdown(PDtlsSession);

STATUS dtlsSessionOnOutBoundData(PDtlsSession, UINT64, DtlsSessionOutboundPacketFunc);
STATUS dtlsSessionOnStateChange(PDtlsSession, UINT64, DtlsSessionOnStateChange);

/******** Internal Functions **********/
STATUS dtlsValidateRtcCertificates(PRtcCertificate, PUINT32);
STATUS dtlsSessionChangeState(PDtlsSession, RTC_DTLS_TRANSPORT_STATE);

#ifdef KVS_USE_OPENSSL
STATUS dtlsCheckOutgoingDataBuffer(PDtlsSession);
STATUS dtlsCertificateFingerprint(X509*, PCHAR);
STATUS dtlsGenerateCertificateFingerprints(PDtlsSession, PDtlsSessionCertificateInfo);
STATUS createCertificateAndKey(INT32, BOOL, X509** ppCert, EVP_PKEY** ppPkey);
STATUS freeCertificateAndKey(X509** ppCert, EVP_PKEY** ppPkey);
STATUS dtlsValidateRtcCertificates(PRtcCertificate, PUINT32);
STATUS createSslCtx(PDtlsSessionCertificateInfo, UINT32, SSL_CTX**);
#elif KVS_USE_MBEDTLS
STATUS dtlsCertificateFingerprint(mbedtls_x509_crt*, PCHAR);
STATUS copyCertificateAndKey(mbedtls_x509_crt*, mbedtls_pk_context*, PDtlsSessionCertificateInfo);
STATUS createCertificateAndKey(INT32, BOOL, mbedtls_x509_crt*, mbedtls_pk_context*);
STATUS freeCertificateAndKey(mbedtls_x509_crt*, mbedtls_pk_context*);

// following are required callbacks for mbedtls
// NOTE: const is not a pure C qualifier, they're here because there's no way to type cast
//       a callback signature.
INT32 dtlsSessionSendCallback(PVOID, const unsigned char*, ULONG);
INT32 dtlsSessionReceiveCallback(PVOID, unsigned char*, ULONG);
VOID dtlsSessionSetTimerCallback(PVOID, UINT32, UINT32);
INT32 dtlsSessionGetTimerCallback(PVOID);
INT32 dtlsSessionKeyDerivationCallback(PVOID, const unsigned char*, const unsigned char*, ULONG, ULONG, ULONG,
                                       const unsigned char[MAX_DTLS_RANDOM_BYTES_LEN], const unsigned char[MAX_DTLS_RANDOM_BYTES_LEN],
                                       mbedtls_tls_prf_types);
#else
#error "A Crypto implementation is required."
#endif

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__
