/*******************************************
VP8 RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

#define VP8_PAYLOAD_DESCRIPTOR_SIZE 1
#define VP8_PAYLOAD_DESCRIPTOR_START_OF_PARTITION_VALUE 0X10

STATUS createPayloadForVP8(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS depayVP8FromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

#pragma pack(pop, include_i)

#ifdef  __cplusplus

}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTPVP8PAYLOADER_H
