/*******************************************
VP8 RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define VP8_PAYLOAD_DESCRIPTOR_SIZE                     1
#define VP8_PAYLOAD_DESCRIPTOR_START_OF_PARTITION_VALUE 0X10

// A key frame starts with a 3 byte frame tag whose lowest bit is 0, the start code 0x9d 0x01 0x2a and 4 bytes of
// dimensions https://tools.ietf.org/html/rfc6386#section-9.1
#define VP8_KEY_FRAME_HEADER_SIZE 10

STATUS createPayloadForVP8(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS depayVP8FromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);
// Whether a VP8 frame is a key frame
BOOL isVP8KeyFrame(PBYTE, UINT32);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H
