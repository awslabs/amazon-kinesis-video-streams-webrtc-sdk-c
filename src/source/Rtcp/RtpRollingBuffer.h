/*******************************************
RTCP Rolling Buffer include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// The rolling buffer index of an RTP packet is its extended sequence number: GET_UINT16_SEQ_NUM(index) is the
// packet's sequence number. The index of the last packet is the rolling buffer's headIndex - 1.
typedef struct {
    PRollingBuffer pRollingBuffer;
} RtpRollingBuffer, *PRtpRollingBuffer;

STATUS createRtpRollingBuffer(UINT32, PRtpRollingBuffer*);
STATUS freeRtpRollingBuffer(PRtpRollingBuffer*);
STATUS freeRtpRollingBufferData(PUINT64);
STATUS rtpRollingBufferAddRtpPacket(PRtpRollingBuffer, PRtpPacket);
STATUS rtpRollingBufferGetValidSeqIndexList(PRtpRollingBuffer, PUINT16, UINT32, PUINT64, PUINT32);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H
