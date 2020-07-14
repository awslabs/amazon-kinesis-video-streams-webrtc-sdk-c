/*******************************************
H264 RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPH264PAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPH264PAYLOADER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define FU_A_HEADER_SIZE     2
#define FU_B_HEADER_SIZE     4
#define STAP_A_HEADER_SIZE   1
#define STAP_B_HEADER_SIZE   3
#define SINGLE_U_HEADER_SIZE 1
#define FU_A_INDICATOR       28
#define FU_B_INDICATOR       29
#define STAP_A_INDICATOR     24
#define STAP_B_INDICATOR     25
#define NAL_TYPE_MASK        31

/*
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | FU indicator  |   FU header   |                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 *  |                                                               |
 *  |                         FU payload                            |
 *  |                                                               |
 *  |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                               :...OPTIONAL RTP padding        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

STATUS createPayloadForH264(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS getNextNaluLength(PBYTE, UINT32, PUINT32, PUINT32);
STATUS createPayloadFromNalu(UINT32, PBYTE, UINT32, PPayloadArray, PUINT32, PUINT32);
STATUS depayH264FromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTPH264PAYLOADER_H
