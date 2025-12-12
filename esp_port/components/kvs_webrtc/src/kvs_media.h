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
 * @brief Reference counting structure for shared media capture interfaces
 */
typedef struct {
    void* video_capture;          // Video capture interface pointer
    void* audio_capture;          // Audio capture interface pointer
    int video_ref_count;          // Number of sessions using video capture
    int audio_ref_count;          // Number of sessions using audio capture
    bool video_initialized;       // Whether video capture is initialized
    bool audio_initialized;       // Whether audio capture is initialized
} kvs_media_shared_state_t;

/* Adaptive bitrate control feature toggle */
#ifndef KVS_MEDIA_ENABLE_ADAPTIVE_BITRATE
#define KVS_MEDIA_ENABLE_ADAPTIVE_BITRATE   0       /* Enable adaptive bitrate control (1=enabled, 0=disabled) */
#endif

/* Adaptive bitrate control parameters */
#define KVS_MEDIA_BITRATE_STEP_KBPS         50      /* Bitrate adjustment step: 50 kbps */
#define KVS_MEDIA_MIN_BITRATE_KBPS          500     /* Minimum bitrate: 500 kbps */
#define KVS_MEDIA_MAX_BITRATE_KBPS          2000    /* Maximum bitrate: 2 Mbps */
#define KVS_MEDIA_SMOOTH_FRAMES_THRESHOLD   100     /* Increase bitrate after 100 smooth frames */

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

    // Video resolution settings (0 = use default: 1280x720@30fps)
    // These are applied at initialization time. Runtime changes are possible
    // but have limitations - see video_capture_set_resolution() documentation.
    // If requested resolution is not supported, camera will use closest available.
    uint16_t video_width;                 // Desired video width
    uint16_t video_height;                // Desired video height
    uint8_t video_fps;                    // Desired FPS
} kvs_media_config_t;

/**
 * @brief Start global media transmission threads (call once at client level)
 *
 * This starts global video and audio transmission threads that serve
 * all sessions. Follows the official KVS pattern from kvsWebRTCClientMaster.c
 *
 * @param client_data Client to start global media for
 * @param config Media configuration
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_start_global_transmission(void* client_data, kvs_media_config_t* config);

/**
 * @brief Stop global media transmission threads
 *
 * @param client_data Client to stop global media for
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_stop_global_transmission(void* client_data);

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
 * @brief Stop media reception for a session (global transmission handled separately)
 *
 * This stops session-specific reception and cleans up media resources.
 *
 * @param session Session to stop media for
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_stop_session(kvs_pc_session_t* session);

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

/**
 * @brief Initialize global media state (call once at startup)
 */
STATUS kvs_media_init_shared_state(void);

/**
 * @brief Cleanup global media state (call once at shutdown)
 */
void kvs_media_cleanup_shared_state(void);

/**
 * @brief Get the global video capture handle
 *
 * Returns the global video capture handle that is shared across all sessions.
 * This is needed for bitrate control and other video capture operations.
 *
 * @return void* Global video capture handle, or NULL if not initialized
 */
void* kvs_media_get_global_video_handle(void);

/**
 * @brief Set up media players for a session
 *
 * Initializes video and audio players for receiving media.
 *
 * @param session Session to set up players for
 * @param config Media configuration
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_setup_players(kvs_pc_session_t* session, kvs_media_config_t* config);

/**
 * @brief Set up frame reception callbacks for a session
 *
 * Registers callbacks with transceivers to receive video/audio frames.
 * Should be called after connection is established (CONNECTED state).
 *
 * @param session Session to set up callbacks for
 * @return STATUS_SUCCESS on success, error code on failure
 */
STATUS kvs_media_setup_frame_callbacks(kvs_pc_session_t* session);

/**
 * @brief Adjust video bitrate dynamically based on network conditions
 *
 * This is a thread-safe helper that adjusts the global video capture bitrate.
 * It applies bounds and logs the adjustment reason.
 *
 * @param video_capture Video capture interface
 * @param handle Video capture handle
 * @param adjustment_kbps Bitrate adjustment in kbps (positive to increase, negative to decrease)
 * @param min_bitrate_kbps Minimum allowed bitrate
 * @param max_bitrate_kbps Maximum allowed bitrate
 * @param reason Reason for adjustment (for logging)
 * @param current_bitrate Pointer to current bitrate variable (will be updated)
 * @return true if bitrate was adjusted, false otherwise
 */
bool kvs_media_adjust_video_bitrate(
    media_stream_video_capture_t *video_capture,
    video_capture_handle_t handle,
    int32_t adjustment_kbps,
    uint32_t min_bitrate_kbps,
    uint32_t max_bitrate_kbps,
    const char *reason,
    uint32_t *current_bitrate);

#ifdef __cplusplus
}
#endif

#endif /* __KVS_MEDIA_H__ */
