/*******************************************
H265 RTP Payloader include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTPH265PAYLOADER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTPH265PAYLOADER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define FU_HEADER_SIZE   3
#define NALU_HEADER_SIZE   2
#define STAP_A_HEADER_SIZE   1
#define STAP_B_HEADER_SIZE   3
#define SINGLE_U_HEADER_SIZE 1
#define FU_A_INDICATOR       28
#define FU_B_INDICATOR       29
#define STAP_A_INDICATOR     24
#define STAP_B_INDICATOR     25

#define AP_TYPE_ID  48
#define FU_TYPE_ID  49

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    PayloadHdr (Type=49)       |   FU header   | DONL (cond)   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
 *  | DONL (cond)   |                                               |
 *  |-+-+-+-+-+-+-+-+                                               |
 *  |                         FU payload                            |
 *  |                                                               |
 *  |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                               :...OPTIONAL RTP padding        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

STATUS createPayloadForH265(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);
STATUS getNextNaluLengthH265(PBYTE, UINT32, PUINT32, PUINT32);
STATUS createPayloadFromNaluH265(UINT32, PBYTE , UINT32, PPayloadArray, PUINT32, PUINT32);
STATUS depayH265FromRtpPayload(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

#ifdef __cplusplus
}
#endif
#endif
