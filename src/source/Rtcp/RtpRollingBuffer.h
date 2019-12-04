/*******************************************
RTCP Rolling Buffer include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

STATUS createRtpRollingBuffer(UINT32, PRollingBuffer*);
STATUS freeRtpRollingBuffer(PRollingBuffer*);
STATUS freeRtpRollingBufferData(PUINT64);
STATUS rtpRollingBufferAddRtpPacket(PRollingBuffer pRollingBuffer, PRtpPacket pRtpPacket);
STATUS rtpRollingBufferGetValidSeqIndexList(PRollingBuffer pRollingBuffer, PUINT16 pSequenceNumberList, PUINT32 pSequenceNumberListLen, PUINT64 pValidSeqIndexList, PUINT32 pValidIndexListLen);

#ifdef  __cplusplus

}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H
