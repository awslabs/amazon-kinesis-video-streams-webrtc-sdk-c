/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WEBRTC_MEDIA_H__
#define __APP_WEBRTC_MEDIA_H__

#include "app_webrtc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Media transmission functions */
STATUS webrtcAppSendVideoFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp, BOOL is_key_frame);
STATUS webrtcAppSendAudioFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp);

/* Media sender thread functions */
PVOID sendVideoFramesFromCamera(PVOID args);
PVOID sendAudioFramesFromMic(PVOID args);
PVOID sendVideoFramesFromSamples(PVOID args);
PVOID sendAudioFramesFromSamples(PVOID args);
PVOID mediaSenderRoutine(PVOID customData);

/* Media reception functions */
PVOID sampleReceiveAudioVideoFrame(PVOID args);
VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame);
VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame);

/* Data channel functions */
#ifdef ENABLE_DATA_CHANNEL
VOID onDataChannelMessage(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen);
VOID onDataChannel(UINT64 customData, PRtcDataChannel pRtcDataChannel);
#endif

/* Bandwidth estimation handlers */
VOID sampleBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate);
VOID sampleSenderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt, UINT64 duration);

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEBRTC_MEDIA_H__ */