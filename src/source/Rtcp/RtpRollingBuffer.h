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
STATUS addRtpPacket(PRollingBuffer, PRtpPacket);
STATUS getValidSeqIndexList(PRollingBuffer, PUINT16, PUINT32, PUINT64, PUINT32);

#ifdef  __cplusplus

}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTP_ROLLING_BUFFER_H
