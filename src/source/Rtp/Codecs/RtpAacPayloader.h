/*******************************************
AAC RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPAACPAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPAACPAYLOADER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// https://www.rfc-editor.org/rfc/rfc6416

STATUS createPayloadForAac(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS depayAacFromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTPAACPAYLOADER_H
