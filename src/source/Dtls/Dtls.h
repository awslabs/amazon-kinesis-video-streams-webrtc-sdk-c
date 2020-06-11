//
// Dtls
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define MAX_SRTP_MASTER_KEY_LEN 16
#define MAX_SRTP_SALT_KEY_LEN 14

#define GENERATED_CERTIFICATE_BITS 2048
#define GENERATED_CERTIFICATE_SERIAL 0
#define GENERATED_CERTIFICATE_DAYS 365
#define GENERATED_CERTIFICATE_NAME (PUINT8) "KVS-WebRTC-Client"
#define KEYING_EXTRACTOR_LABEL "EXTRACTOR-dtls_srtp"

/*
 * DTLS transmission interval timer (in 100ns)
 */
#define DTLS_TRANSMISSION_INTERVAL          (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define DTLS_SESSION_TIMER_START_DELAY      (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define SECONDS_IN_A_DAY                    (24 * 60 * 60LL)

#define LOG_OPENSSL_ERROR(s)                    while ((sslErr = ERR_get_error()) != 0) { \
                                                    if (sslErr != SSL_ERROR_WANT_WRITE && sslErr != SSL_ERROR_WANT_READ) { \
                                                        DLOGW("%s failed with %s", (s), ERR_error_string(sslErr, NULL)); \
                                                    } \
                                                }

typedef enum {
   SRTP_PROFILE_AES128_CM_HMAC_SHA1_80 = SRTP_AES128_CM_SHA1_80,
   SRTP_PROFILE_AES128_CM_HMAC_SHA1_32 = SRTP_AES128_CM_SHA1_32,
} SRTP_PROFILE;

typedef enum {
    NEW,
    CONNECTING, /* DTLS is in the process of negotiating a secure connection and verifying the remote fingerprint. */
    CONNECTED,  /* DTLS has completed negotiation of a secure connection and verified the remote fingerprint. */
    CLOSED,     /* The transport has been closed intentionally as the result of receipt of a close_notify alert */
    FAILED,     /* The transport has failed as the result of an error */
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

  SRTP_PROFILE srtpProfile;
} DtlsKeyingMaterial, *PDtlsKeyingMaterial;

typedef struct {
    BOOL created;
    X509 *pCert;
    EVP_PKEY *pKey;
} DtlsSessionCertificateInfo, *PDtlsSessionCertificateInfo;

typedef struct {
    volatile ATOMIC_BOOL isStarted;
    volatile ATOMIC_BOOL sslInitFinished;
    volatile ATOMIC_BOOL shutdown;
    SSL_CTX *pSslCtx;
    CHAR certFingerprints[MAX_RTCCONFIGURATION_CERTIFICATES][CERTIFICATE_FINGERPRINT_LENGTH + 1];
    UINT32 certificateCount;
    DtlsSessionCallbacks dtlsSessionCallbacks;
    TIMER_QUEUE_HANDLE timerQueueHandle;
    UINT32 timerId;
    // dtls message must fit into a UDP packet
    BYTE outgoingDataBuffer[MAX_UDP_PACKET_SIZE];
    UINT32 outgoingDataLen;
    UINT64 dtlsSessionStartTime;
    RTC_DTLS_TRANSPORT_STATE state;

    SSL *pSsl;
    MUTEX sslLock;
} DtlsSession, *PDtlsSession;

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
STATUS dtlsCheckOutgoingDataBuffer(PDtlsSession);
STATUS dtlsCertificateFingerprint(X509*, PCHAR);
STATUS dtlsGenerateCertificateFingerprints(PDtlsSession, PDtlsSessionCertificateInfo);
STATUS createCertificateAndKey(INT32, BOOL, X509 **ppCert, EVP_PKEY **ppPkey);
STATUS freeCertificateAndKey(X509 **ppCert, EVP_PKEY **ppPkey);
STATUS dtlsValidateRtcCertificates(PRtcCertificate, PUINT32);
STATUS createSslCtx(PDtlsSessionCertificateInfo, UINT32, SSL_CTX**);
STATUS dtlsSessionChangeState(PDtlsSession, RTC_DTLS_TRANSPORT_STATE);

#ifdef  __cplusplus
}
#endif
#endif  //__KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__
