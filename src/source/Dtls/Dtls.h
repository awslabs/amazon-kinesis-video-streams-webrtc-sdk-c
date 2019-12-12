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
#define GENERATED_CERTIFICATE_DAYS 36500
#define GENERATED_CERTIFICATE_NAME (PUINT8) "KVS-WebRTC-Client"
#define KEYING_EXTRACTOR_LABEL "EXTRACTOR-dtls_srtp"

/*
 * DTLS transmission interval timer (in 100ns)
 */
#define DTLS_TRANSMISSION_INTERVAL (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define LOG_OPENSSL_ERROR(s)                    while ((sslErr = ERR_get_error()) != 0) { \
                                                    if (sslErr != SSL_ERROR_WANT_WRITE && sslErr != SSL_ERROR_WANT_READ) { \
                                                        DLOGW("%s failed with %s", (s), ERR_error_string(sslErr, NULL)); \
                                                    } \
                                                }

typedef enum {
   SRTP_PROFILE_AES128_CM_HMAC_SHA1_80 = SRTP_AES128_CM_SHA1_80,
   SRTP_PROFILE_AES128_CM_HMAC_SHA1_32 = SRTP_AES128_CM_SHA1_32,
} SRTP_PROFILE;

// Callback that is fired when Dtls Server wishes to send packet
typedef VOID (*DtlsSessionOutboundPacketFunc)(UINT64, PBYTE, UINT32);

typedef struct {
    UINT64 customData;
    DtlsSessionOutboundPacketFunc outboundPacketFn;
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
    SSL_CTX *pSslCtx;
    X509 *pCert;
    EVP_PKEY *pKey;
    DtlsSessionCallbacks dtlsSessionCallbacks;
    TIMER_QUEUE_HANDLE timerQueueHandle;
    UINT32 timerId;
    volatile BOOL isStarted;
    // dtls message must fit into a UDP packet
    BYTE outgoingDataBuffer[MAX_UDP_PACKET_SIZE];
    UINT32 outgoingDataLen;

    SSL *pSsl;
    MUTEX sslLock;
} DtlsSession, *PDtlsSession;

STATUS createCertificateAndKey(X509 **ppCert, EVP_PKEY **ppPkey);
STATUS freeCertificateAndKey(X509 **ppCert, EVP_PKEY **ppPkey);

STATUS createDtlsSession(PDtlsSessionCallbacks, TIMER_QUEUE_HANDLE, PDtlsSession*);
STATUS freeDtlsSession(PDtlsSession*);

STATUS dtlsSessionStart(PDtlsSession, BOOL);
STATUS dtlsSessionProcessPacket(PDtlsSession, PBYTE, PINT32);
STATUS dtlsSessionIsInitFinished(PDtlsSession, PBOOL);
STATUS dtlsSessionPopulateKeyingMaterial(PDtlsSession, PDtlsKeyingMaterial);
STATUS dtlsSessionGenerateLocalCertificateFingerprint(PDtlsSession, PCHAR, UINT32);
STATUS dtlsSessionVerifyRemoteCertificateFingerprint(PDtlsSession, PCHAR);
STATUS dtlsSessionPutApplicationData(PDtlsSession, PBYTE, INT32);
STATUS dtlsCheckOutgoingDataBuffer(PDtlsSession);

#ifdef  __cplusplus
}
#endif
#endif  //__KINESIS_VIDEO_WEBRTC_CLIENT_DTLS_DTLS__
