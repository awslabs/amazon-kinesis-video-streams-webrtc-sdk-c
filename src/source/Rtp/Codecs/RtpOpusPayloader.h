/*******************************************
Opus RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPOPUSPAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPOPUSPAYLOADER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STATUS createPayloadForOpus(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS depayOpusFromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTPOPUSPAYLOADER_H
