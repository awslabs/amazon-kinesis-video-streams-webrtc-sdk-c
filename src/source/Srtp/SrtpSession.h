//
// Secure RTP
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_SRTP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_SRTP__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __SrtpSession SrtpSession;
struct __SrtpSession {
    // holds the srtp context for transmit  operations
    srtp_t srtp_transmit_session;
    // holds the srtp context for receive  operations
    srtp_t srtp_receive_session;
};
typedef SrtpSession* PSrtpSession;

STATUS initSrtpSession(PBYTE receiveKey, PBYTE transmitKey, KVS_SRTP_PROFILE profile, PSrtpSession* ppSrtpSession);

STATUS decryptSrtpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len);
STATUS decryptSrtcpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len);

STATUS encryptRtpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len);
STATUS encryptRtcpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len);

STATUS freeSrtpSession(PSrtpSession* ppSrtpSession);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_SRTP__
