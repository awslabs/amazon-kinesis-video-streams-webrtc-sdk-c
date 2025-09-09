/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __KVS_MEDIA_H__
#define __KVS_MEDIA_H__

/**
 * @file kvs_media.h
 * @brief KVS Media handling implementation
 *
 * This file contains all media-related functionality for KVS WebRTC:
 * - Media capture and transmission
 * - Media reception and playback
 * - Sample file fallback for development
 * - Media frame handling and processing
 *
 * This is separated from kvs_webrtc.c to keep peer connection logic
 * focused and media logic organized.
 */

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "esp_log.h"
#include "media_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration - defined in kvs_webrtc.c
typedef struct kvs_pc_session_s kvs_pc_session_t;

/**
 * @brief Media transmission configuration
 */
typedef struct {
    // Media interfaces
    void* video_capture;                  // Video capture interface
    void* audio_capture;                  // Audio capture interface
    void* video_player;                   // Video player interface
    void* audio_player;                   // Audio player interface

    // Media settings
    bool receive_media;                   // Whether to receive media
    bool enable_sample_fallback;          // Use sample files when no capture available
} kvs_media_config_t;

/**
 * @brief Start media transmission for a session
 *
 * This starts both video and audio transmission threads based on the
 * available interfaces. Falls back to sample files if no capture
 * interfaces are available.
 *
 * @param session Session to start media for
 * @param config Media configuration
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_start_transmission(kvs_pc_session_t* session, kvs_media_config_t* config);

/**
 * @brief Start media reception for a session
 *
 * This sets up media players and frame reception callbacks for
 * receiving and playing media from the peer.
 *
 * @param session Session to start reception for
 * @param config Media configuration
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_start_reception(kvs_pc_session_t* session, kvs_media_config_t* config);

/**
 * @brief Stop media transmission and reception for a session
 *
 * This stops all media threads and cleans up media resources.
 *
 * @param session Session to stop media for
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_stop(kvs_pc_session_t* session);

/**
 * @brief Video frame handler for received frames
 *
 * This is called by the KVS SDK when video frames are received.
 * It forwards frames to the video player interface.
 *
 * @param customData Session data (kvs_pc_session_t*)
 * @param pFrame Received video frame
 */
VOID kvs_media_video_frame_handler(UINT64 customData, PFrame pFrame);

/**
 * @brief Audio frame handler for received frames
 *
 * This is called by the KVS SDK when audio frames are received.
 * It forwards frames to the audio player interface.
 *
 * @param customData Session data (kvs_pc_session_t*)
 * @param pFrame Received audio frame
 */
VOID kvs_media_audio_frame_handler(UINT64 customData, PFrame pFrame);

/**
 * @brief Print frame transmission statistics for debugging
 *
 * This prints detailed statistics about frame transmission including
 * success rates, FPS, and timing information for both video and audio.
 *
 * @param session Session to print stats for
 */
void kvs_media_print_stats(kvs_pc_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* __KVS_MEDIA_H__ */
