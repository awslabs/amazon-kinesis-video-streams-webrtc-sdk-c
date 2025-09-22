/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file kvs_media.c
 * @brief KVS Media handling implementation
 *
 * This file implements all media-related functionality for KVS WebRTC,
 * separated from the peer connection logic in kvs_webrtc.c for better
 * code organization and maintainability.
 *
 * Features implemented:
 * - Media capture and transmission (camera/microphone)
 * - Media reception and playback (video/audio players)
 * - Sample file fallback for development/testing
 * - Frame processing and handling
 */

#include "kvs_media.h"
#include "kvs_webrtc_internal.h"  // For session structure access
#include "esp_log.h"
#include "media_stream.h"
#include "fileio.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

static const char *TAG = "kvs_media";

/* Global media state following KVS official pattern */
static struct {
    void* client_data;                    // KVS client for session iteration
    kvs_media_config_t config;           // Global media configuration
    TID video_sender_tid;                // Global video thread
    TID audio_sender_tid;                // Global audio thread
    BOOL global_media_started;           // Global media threads running
    BOOL terminated;                     // Global termination flag
    MUTEX global_media_mutex;            // Protects global state

    // Media capture handles (shared)
    void* video_handle;
    void* audio_handle;

    // Frame buffers (shared across all sessions)
    PBYTE video_frame_buffer;
    PBYTE audio_frame_buffer;
    UINT32 video_buffer_size;
    UINT32 audio_buffer_size;
    UINT64 frame_index;
} g_global_media = {0};

// Sample file fallback settings
#define KVS_SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / 30)  // 30 FPS
#define KVS_SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)  // 20ms audio frames
#define KVS_NUMBER_OF_OPUS_FRAME_FILES 20  // Number of OPUS sample files to cycle through
#define KVS_MAX_PATH_LEN 256

/* Forward declarations for internal functions */
static STATUS kvs_media_setup_players(kvs_pc_session_t* session, kvs_media_config_t* config);
static STATUS kvs_media_setup_frame_callbacks(kvs_pc_session_t* session);

/* Global media thread functions - following KVS official pattern */
static PVOID kvs_global_video_sender_thread(PVOID args);
static PVOID kvs_global_audio_sender_thread(PVOID args);
static STATUS kvs_iterate_sessions_send_frame(Frame* frame, BOOL is_video);

/* Sample file fallback functions (used by global threads) */
static STATUS kvs_media_read_frame_from_disk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath);

/* Sample file fallback functionality */

/**
 * @brief Read frame data from disk file - equivalent to readFrameFromDisk in app_webrtc_media.c
 */
static STATUS kvs_media_read_frame_from_disk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;
    CHK_ERR(pSize != NULL, STATUS_NULL_ARG, "[KVS Media] Invalid file size");
    size = *pSize;
    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));
CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

// Note: Old per-session functions removed - replaced by global media threads

// Note: Old per-session media functions removed - functionality moved to global threads

/* Global media implementation following KVS official pattern */

/**
 * @brief Initialize global media state (call once at startup)
 */
STATUS kvs_media_init_shared_state(void)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(!IS_VALID_MUTEX_VALUE(g_global_media.global_media_mutex), STATUS_INVALID_OPERATION);

    // Clear the state first
    MEMSET(&g_global_media, 0x00, SIZEOF(g_global_media));

    // Then initialize properly
    g_global_media.global_media_mutex = MUTEX_CREATE(FALSE);
    CHK_ERR(IS_VALID_MUTEX_VALUE(g_global_media.global_media_mutex), STATUS_NOT_ENOUGH_MEMORY, "Failed to create global media mutex");

    g_global_media.video_sender_tid = INVALID_TID_VALUE;
    g_global_media.audio_sender_tid = INVALID_TID_VALUE;
    g_global_media.global_media_started = FALSE;
    g_global_media.terminated = FALSE;

CleanUp:
    return retStatus;
}

/**
 * @brief Cleanup global media state (call once at shutdown)
 */
void kvs_media_cleanup_shared_state(void)
{
    if (IS_VALID_MUTEX_VALUE(g_global_media.global_media_mutex)) {
        MUTEX_FREE(g_global_media.global_media_mutex);
        g_global_media.global_media_mutex = INVALID_MUTEX_VALUE;
    }

    // Free global frame buffers
    SAFE_MEMFREE(g_global_media.video_frame_buffer);
    SAFE_MEMFREE(g_global_media.audio_frame_buffer);

    MEMSET(&g_global_media, 0x00, SIZEOF(g_global_media));
}

/**
 * @brief Global video sender thread - equivalent to sendVideoPackets in kvsWebRTCClientMaster.c
 * Captures video frames and sends them to ALL active sessions
 */
static PVOID kvs_global_video_sender_thread(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[KVS_MAX_PATH_LEN + 1];
    UINT64 startTime, lastFrameTime, elapsed;
    media_stream_video_capture_t *video_capture = NULL;
    video_frame_t *video_frame = NULL;
    UINT64 refTime = GETTIME();
    const UINT64 frame_duration_100ns = KVS_SAMPLE_VIDEO_FRAME_DURATION;

    ESP_LOGI(TAG, "Global video sender thread started");

    // Initialize capture interface if available
    if (g_global_media.config.video_capture != NULL) {
        video_capture = (media_stream_video_capture_t*)g_global_media.config.video_capture;

        video_capture_config_t video_config = {
            .codec = VIDEO_CODEC_H264,
            .resolution = {
                .width = 640,
                .height = 480,
                .fps = 30
            },
            .quality = 80,
            .bitrate = 500,
            .codec_specific = NULL
        };

        ESP_LOGI(TAG, "Initializing global video capture");
        if (video_capture->init(&video_config, &g_global_media.video_handle) != ESP_OK ||
            video_capture->start(g_global_media.video_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video capture - falling back to samples");
            video_capture = NULL;
        }
    }

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = 0;

    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!g_global_media.terminated) {
        BOOL frame_available = FALSE;

        if (video_capture != NULL) {
            // Get frame from camera
            if (video_capture->get_frame(g_global_media.video_handle, &video_frame, 0) == ESP_OK && video_frame != NULL) {
                frame.frameData = video_frame->buffer;
                frame.size = video_frame->len;
                frame.flags = (video_frame->type == VIDEO_FRAME_TYPE_I) ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
                frame_available = TRUE;
            }
    } else {
            // Fall back to sample files
            SNPRINTF(filePath, KVS_MAX_PATH_LEN, "./h264SampleFrames/frame-%04" PRIu64 ".h264",
                     (UINT64)(g_global_media.frame_index % 300 + 1));

            if (kvs_media_read_frame_from_disk(NULL, &frameSize, filePath) == STATUS_SUCCESS) {
                // Re-alloc if needed
                if (frameSize > g_global_media.video_buffer_size) {
                    g_global_media.video_frame_buffer = (PBYTE) REALLOC(g_global_media.video_frame_buffer, frameSize);
                    if (g_global_media.video_frame_buffer != NULL) {
                        g_global_media.video_buffer_size = frameSize;
                    }
                }

                if (g_global_media.video_frame_buffer != NULL &&
                    kvs_media_read_frame_from_disk(g_global_media.video_frame_buffer, &frameSize, filePath) == STATUS_SUCCESS) {
                    frame.frameData = g_global_media.video_frame_buffer;
                    frame.size = frameSize;
                    frame.flags = (g_global_media.frame_index % 45 == 0) ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
                    frame_available = TRUE;
                }
            }
        }

        if (frame_available) {
            frame.index = (UINT32) ATOMIC_INCREMENT(&g_global_media.frame_index);
            frame.presentationTs += frame_duration_100ns;

            // Send frame to ALL sessions (KVS official pattern)
            STATUS send_status = kvs_iterate_sessions_send_frame(&frame, TRUE);
            if (STATUS_FAILED(send_status)) {
                ESP_LOGW(TAG, "Failed to send frame to sessions: 0x%08x", send_status);
            }

            // Release camera frame if used
            if (video_capture != NULL && video_frame != NULL) {
                video_capture->release_frame(g_global_media.video_handle, video_frame);
                video_frame = NULL;
            }
        }

        // Frame rate control
        elapsed = GETTIME() - lastFrameTime;
        if (elapsed < frame_duration_100ns) {
            THREAD_SLEEP(frame_duration_100ns - elapsed);
        }
        lastFrameTime = GETTIME();
    }

    // Cleanup
    if (video_capture != NULL && g_global_media.video_handle != NULL) {
        video_capture->stop(g_global_media.video_handle);
        video_capture->deinit(g_global_media.video_handle);
        g_global_media.video_handle = NULL;
    }

    ESP_LOGI(TAG, "Global video sender thread finished");
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Global audio sender thread - equivalent to sendAudioPackets in kvsWebRTCClientMaster.c
 * Captures audio frames and sends them to ALL active sessions
 */
static PVOID kvs_global_audio_sender_thread(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[KVS_MAX_PATH_LEN + 1];
    UINT64 lastFrameTime;
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_frame_t *audio_frame = NULL;
    UINT32 audioIndex = 0;
    const UINT64 frame_duration_100ns = KVS_SAMPLE_AUDIO_FRAME_DURATION;

    ESP_LOGI(TAG, "Global audio sender thread started");

    // Initialize capture interface if available
    if (g_global_media.config.audio_capture != NULL) {
        audio_capture = (media_stream_audio_capture_t*)g_global_media.config.audio_capture;

        audio_capture_config_t audio_config = {
            .codec = AUDIO_CODEC_OPUS,
            .format = {
                .sample_rate = 16000,
                .channels = 1,
                .bits_per_sample = 16
            },
            .bitrate = 16000,
            .frame_duration_ms = 20,
            .codec_specific = NULL
        };

        ESP_LOGI(TAG, "Initializing global audio capture");
        if (audio_capture->init(&audio_config, &g_global_media.audio_handle) != ESP_OK ||
            audio_capture->start(g_global_media.audio_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize audio capture - falling back to samples");
            audio_capture = NULL;
        }
    }

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.flags = FRAME_FLAG_NONE;
    frame.presentationTs = 0;

    lastFrameTime = GETTIME();

    while (!g_global_media.terminated) {
        BOOL frame_available = FALSE;

        if (audio_capture != NULL) {
            // Get frame from microphone
            if (audio_capture->get_frame(g_global_media.audio_handle, &audio_frame, 0) == ESP_OK && audio_frame != NULL) {
                frame.frameData = audio_frame->buffer;
                frame.size = audio_frame->len;
                frame_available = TRUE;
            }
    } else {
            // Fall back to sample files
            SNPRINTF(filePath, KVS_MAX_PATH_LEN, "./opusSampleFrames/sample-%03u.opus", audioIndex + 1);

            if (kvs_media_read_frame_from_disk(NULL, &frameSize, filePath) == STATUS_SUCCESS) {
                // Re-alloc if needed
                if (frameSize > g_global_media.audio_buffer_size) {
                    g_global_media.audio_frame_buffer = (PBYTE) REALLOC(g_global_media.audio_frame_buffer, frameSize);
                    if (g_global_media.audio_frame_buffer != NULL) {
                        g_global_media.audio_buffer_size = frameSize;
                    }
                }

                if (g_global_media.audio_frame_buffer != NULL &&
                    kvs_media_read_frame_from_disk(g_global_media.audio_frame_buffer, &frameSize, filePath) == STATUS_SUCCESS) {
                    frame.frameData = g_global_media.audio_frame_buffer;
                    frame.size = frameSize;
                    frame_available = TRUE;
                    audioIndex = (audioIndex + 1) % KVS_NUMBER_OF_OPUS_FRAME_FILES;
                }
            }
        }

        if (frame_available) {
            frame.presentationTs += frame_duration_100ns;

            // Send frame to ALL sessions (KVS official pattern)
            kvs_iterate_sessions_send_frame(&frame, FALSE);

            // Release microphone frame if used
            if (audio_capture != NULL && audio_frame != NULL) {
                audio_capture->release_frame(g_global_media.audio_handle, audio_frame);
                audio_frame = NULL;
            }
        }

        // Frame rate control
        THREAD_SLEEP(frame_duration_100ns);
        lastFrameTime = GETTIME();
    }

    // Cleanup
    if (audio_capture != NULL && g_global_media.audio_handle != NULL) {
        audio_capture->stop(g_global_media.audio_handle);
        audio_capture->deinit(g_global_media.audio_handle);
        g_global_media.audio_handle = NULL;
    }

    ESP_LOGI(TAG, "Global audio sender thread finished");
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Callback function for session iteration - called for each active session
 * This implements the writeFrame call from the official KVS pattern
 */
static STATUS kvs_session_frame_callback(UINT64 callerData, PHashEntry pHashEntry)
{
    STATUS retStatus = STATUS_SUCCESS;
    Frame* frame = (Frame*)callerData;
    kvs_pc_session_t* session = NULL;
    STATUS writeStatus;

    CHK(frame != NULL && pHashEntry != NULL, STATUS_NULL_ARG);

    session = (kvs_pc_session_t*)pHashEntry->value;
    CHK(session != NULL && !session->terminated, STATUS_NULL_ARG);

    // Determine which transceiver to use based on frame track ID
    PRtcRtpTransceiver transceiver = NULL;
    if (frame->trackId == DEFAULT_VIDEO_TRACK_ID) {
        transceiver = session->video_transceiver;
    } else if (frame->trackId == DEFAULT_AUDIO_TRACK_ID) {
        transceiver = session->audio_transceiver;
    }

    if (transceiver != NULL) {
        writeStatus = writeFrame(transceiver, frame);
        if (writeStatus != STATUS_SUCCESS && writeStatus != STATUS_SRTP_NOT_READY_YET) {
            ESP_LOGW(TAG, "writeFrame failed for session %s: 0x%08x", session->peer_id, writeStatus);
        }
        // Don't propagate SRTP_NOT_READY as error - it's expected during connection setup
        retStatus = STATUS_SUCCESS;
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Iterate through all active sessions and send frame - equivalent to kvsWebRTCClientMaster.c loop
 * This is the KEY function that implements the official KVS pattern
 */
static STATUS kvs_iterate_sessions_send_frame(Frame* frame, BOOL is_video)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(frame != NULL, STATUS_NULL_ARG);
    CHK(g_global_media.client_data != NULL, STATUS_INVALID_OPERATION);

    // Get client data to access active sessions
    kvs_pc_client_t* client = (kvs_pc_client_t*)g_global_media.client_data;
    CHK(client != NULL && client->activeSessions != NULL, STATUS_NULL_ARG);

    // Protect against race condition with session destruction by using the session count mutex
    if (!IS_VALID_MUTEX_VALUE(client->session_count_mutex)) {
        retStatus = STATUS_INVALID_OPERATION;
        goto CleanUp;
    }

    MUTEX_LOCK(client->session_count_mutex);

    // Check if we still have active sessions after acquiring the lock
    if (client->session_count == 0) {
        MUTEX_UNLOCK(client->session_count_mutex);
        retStatus = STATUS_SUCCESS; // Not an error, just no sessions to send to
        goto CleanUp;
    }

    // Use the official KVS pattern: iterate through sessions and call writeFrame for each
    // This is equivalent to the loop in kvsWebRTCClientMaster.c
    retStatus = hashTableIterateEntries(client->activeSessions, (UINT64)frame, kvs_session_frame_callback);

    MUTEX_UNLOCK(client->session_count_mutex);

CleanUp:
    return retStatus;
}

/* Public API implementations following KVS official pattern */

/**
 * @brief Start global media transmission threads (called once at client level)
 */
STATUS kvs_media_start_global_transmission(void* client_data, kvs_media_config_t* config)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(client_data != NULL && config != NULL, STATUS_NULL_ARG);
    CHK(IS_VALID_MUTEX_VALUE(g_global_media.global_media_mutex), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(g_global_media.global_media_mutex);

    if (g_global_media.global_media_started) {
        ESP_LOGI(TAG, "Global media threads already started");
        goto CleanUp;
    }

    // Store client data and configuration
    g_global_media.client_data = client_data;
    MEMCPY(&g_global_media.config, config, SIZEOF(kvs_media_config_t));
    g_global_media.terminated = FALSE;

    ESP_LOGI(TAG, "Starting global media threads (KVS official pattern)");

    // Start global video thread
    if (config->video_capture != NULL || config->enable_sample_fallback) {
        CHK_STATUS(THREAD_CREATE_EX_EXT(&g_global_media.video_sender_tid, "kvsGlobalVideo", 8 * 1024, TRUE,
                                       kvs_global_video_sender_thread, NULL));
        ESP_LOGI(TAG, "Global video sender thread started");
    }

    // Start global audio thread
    if (config->audio_capture != NULL || config->enable_sample_fallback) {
        CHK_STATUS(THREAD_CREATE_EX_EXT(&g_global_media.audio_sender_tid, "kvsGlobalAudio", 8 * 1024, TRUE,
                                       kvs_global_audio_sender_thread, NULL));
        ESP_LOGI(TAG, "Global audio sender thread started");
    }

    g_global_media.global_media_started = TRUE;
    ESP_LOGI(TAG, "Global media transmission started successfully");

CleanUp:
    MUTEX_UNLOCK(g_global_media.global_media_mutex);
    return retStatus;
}

/**
 * @brief Stop global media transmission threads
 */
STATUS kvs_media_stop_global_transmission(void* client_data)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(client_data != NULL, STATUS_NULL_ARG);
    CHK(IS_VALID_MUTEX_VALUE(g_global_media.global_media_mutex), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(g_global_media.global_media_mutex);

    if (!g_global_media.global_media_started) {
        ESP_LOGI(TAG, "Global media threads not running");
        goto CleanUp;
    }

    ESP_LOGI(TAG, "Stopping global media threads");

    // Signal termination
    g_global_media.terminated = TRUE;

    // Wait for threads to complete
    if (IS_VALID_TID_VALUE(g_global_media.video_sender_tid)) {
        THREAD_JOIN(g_global_media.video_sender_tid, NULL);
        g_global_media.video_sender_tid = INVALID_TID_VALUE;
        ESP_LOGI(TAG, "Global video thread stopped");
    }

    if (IS_VALID_TID_VALUE(g_global_media.audio_sender_tid)) {
        THREAD_JOIN(g_global_media.audio_sender_tid, NULL);
        g_global_media.audio_sender_tid = INVALID_TID_VALUE;
        ESP_LOGI(TAG, "Global audio thread stopped");
    }

    g_global_media.global_media_started = FALSE;
    ESP_LOGI(TAG, "Global media transmission stopped successfully");

CleanUp:
    MUTEX_UNLOCK(g_global_media.global_media_mutex);
    return retStatus;
}

/**
 * @brief Media reception routine - equivalent to receiveAudioVideoSource in app_webrtc.c
 */
static PVOID kvs_media_reception_routine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    CHK(session != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "🎧 Media reception routine started for peer: %s", session->peer_id);

    // Wait for connection to be established
    while (!session->media_started && !session->terminated) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms
    }

    if (session->terminated) {
        ESP_LOGI(TAG, "Media reception terminated before connection established");
        goto CleanUp;
    }

    ESP_LOGI(TAG, "Setting up media reception for peer: %s", session->peer_id);

    // Setup media players and frame callbacks happens in kvs_media_start_reception
    // This thread just stays alive while reception is active
    while (!session->terminated && session->media_started) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms
    }

CleanUp:
    ESP_LOGI(TAG, "Media reception routine finished for peer: %s", session->peer_id);
    CHK_LOG_ERR(retStatus);
    return NULL;
}

// Note: kvs_media_start_transmission() removed - replaced with global transmission

STATUS kvs_media_start_reception(kvs_pc_session_t* session, kvs_media_config_t* config)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL && config != NULL, STATUS_NULL_ARG);

    // Copy media config to session for access by media threads
    session->client->config.video_player = config->video_player;
    session->client->config.audio_player = config->audio_player;
    session->client->config.receive_media = config->receive_media;

    // Setup media players
    CHK_STATUS(kvs_media_setup_players(session, config));

    // Setup frame reception callbacks
    CHK_STATUS(kvs_media_setup_frame_callbacks(session));

    // Start receive thread if needed
    if (config->receive_media && !session->receive_thread_started) {
        CHK_STATUS(THREAD_CREATE_EX_EXT(&session->receive_audio_video_tid, "kvs_receiveAV", 8 * 1024, TRUE,
                                       kvs_media_reception_routine, (PVOID)session));
        session->receive_thread_started = TRUE;
    }

CleanUp:
    return retStatus;
}

STATUS kvs_media_stop_session(kvs_pc_session_t* session)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL, STATUS_NULL_ARG);

    // Mark session as terminated (global media threads handle transmission termination separately)
    session->terminated = TRUE;

    // Wait for session-specific receive thread to complete
    if (session->receive_thread_started && IS_VALID_TID_VALUE(session->receive_audio_video_tid)) {
        THREAD_JOIN(session->receive_audio_video_tid, NULL);
        session->receive_thread_started = FALSE;
    }

    // Cleanup media players (session-specific)
    if (session->client->config.video_player != NULL && session->video_player_handle != NULL) {
        media_stream_video_player_t *video_player = (media_stream_video_player_t*)session->client->config.video_player;
        video_player->stop(session->video_player_handle);
        video_player->deinit(session->video_player_handle);
        session->video_player_handle = NULL;
    }

    if (session->client->config.audio_player != NULL && session->audio_player_handle != NULL) {
        media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)session->client->config.audio_player;
        audio_player->stop(session->audio_player_handle);
        audio_player->deinit(session->audio_player_handle);
        session->audio_player_handle = NULL;
    }

    // Session-specific frame buffers are now handled by global threads
    // No per-session cleanup needed

CleanUp:
    return retStatus;
}

static STATUS kvs_media_setup_players(kvs_pc_session_t* session, kvs_media_config_t* config)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL && config != NULL, STATUS_NULL_ARG);

    // Initialize media players if not already done
    if (!session->media_players_initialized) {
        // Initialize video player if available
        if (config->video_player != NULL && session->video_player_handle == NULL) {
            media_stream_video_player_t *video_player = (media_stream_video_player_t*)config->video_player;

            // Initialize with default configuration
            video_player_config_t video_config = {
                .codec = VIDEO_PLAYER_CODEC_H264,
                .format = {
                    .width = 640,
                    .height = 480,
                    .framerate = 30
                },
                .buffer_frames = 5,
                .codec_specific = NULL,
                .display_handle = NULL
            };

            ESP_LOGI(TAG, "Initializing video player for peer: %s", session->peer_id);
            if (video_player->init(&video_config, &session->video_player_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize video player");
            } else if (video_player->start(session->video_player_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start video player");
                video_player->deinit(session->video_player_handle);
                session->video_player_handle = NULL;
            } else {
                ESP_LOGI(TAG, "Video player initialized successfully");
            }
        }

        // Initialize audio player if available
        if (config->audio_player != NULL && session->audio_player_handle == NULL) {
            media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)config->audio_player;

            // Initialize with default configuration
            audio_player_config_t audio_config = {
                .codec = AUDIO_PLAYER_CODEC_OPUS,
                .format = {
                    .sample_rate = 48000,
                    .channels = 1,
                    .bits_per_sample = 16
                },
                .buffer_ms = 500,
                .codec_specific = NULL
            };

            ESP_LOGI(TAG, "Initializing audio player for peer: %s", session->peer_id);
            if (audio_player->init(&audio_config, &session->audio_player_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize audio player");
            } else if (audio_player->start(session->audio_player_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start audio player");
                audio_player->deinit(session->audio_player_handle);
                session->audio_player_handle = NULL;
            } else {
                ESP_LOGI(TAG, "Audio player initialized successfully");
            }
        }

        session->media_players_initialized = true;
    }

CleanUp:
    return retStatus;
}

static STATUS kvs_media_setup_frame_callbacks(kvs_pc_session_t* session)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL, STATUS_NULL_ARG);

    // Set up frame reception callbacks
    if (session->client->config.video_player != NULL && session->video_player_handle != NULL) {
        ESP_LOGI(TAG, "Setting up video frame reception callback for peer: %s", session->peer_id);
        CHK_STATUS(transceiverOnFrame(session->video_transceiver,
                                      (UINT64)(uintptr_t) session,
                                      kvs_media_video_frame_handler));
    }

    if (session->client->config.audio_player != NULL && session->audio_player_handle != NULL) {
        ESP_LOGI(TAG, "Setting up audio frame reception callback for peer: %s", session->peer_id);
        CHK_STATUS(transceiverOnFrame(session->audio_transceiver,
                                      (UINT64)(uintptr_t) session,
                                      kvs_media_audio_frame_handler));
    }

CleanUp:
    return retStatus;
}

VOID kvs_media_video_frame_handler(UINT64 customData, PFrame pFrame)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(uintptr_t)customData;

    if (pFrame == NULL || pFrame->frameData == NULL || session == NULL) {
        ESP_LOGW(TAG, "Invalid video frame or session data");
        return;
    }

    // Get the video player interface
    if (session->client->config.video_player == NULL || session->video_player_handle == NULL) {
        ESP_LOGW(TAG, "Video player not available for peer: %s", session->peer_id);
        return;
    }

    media_stream_video_player_t *video_player = (media_stream_video_player_t*)session->client->config.video_player;

    // Create video frame structure
    video_frame_t video_frame = {
        .buffer = pFrame->frameData,
        .len = pFrame->size,
        .timestamp = pFrame->presentationTs,
        .type = (pFrame->flags & FRAME_FLAG_KEY_FRAME) ? VIDEO_FRAME_TYPE_I : VIDEO_FRAME_TYPE_P
    };

    // Send frame to player
    esp_err_t err = video_player->play_frame(session->video_player_handle,
                                             video_frame.buffer, video_frame.len,
                                             video_frame.type == VIDEO_FRAME_TYPE_I);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write video frame to player: %s", esp_err_to_name(err));
    }
}

VOID kvs_media_audio_frame_handler(UINT64 customData, PFrame pFrame)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(uintptr_t)customData;

    if (pFrame == NULL || pFrame->frameData == NULL || session == NULL) {
        ESP_LOGW(TAG, "Invalid audio frame or session data");
        return;
    }

    // Get the audio player interface
    if (session->client->config.audio_player == NULL || session->audio_player_handle == NULL) {
        ESP_LOGW(TAG, "Audio player not available for peer: %s", session->peer_id);
        return;
    }

    media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)session->client->config.audio_player;

    // Create audio frame structure
    audio_frame_t audio_frame = {
        .buffer = pFrame->frameData,
        .len = pFrame->size,
        .timestamp = pFrame->presentationTs
    };

    // Send frame to player
    esp_err_t err = audio_player->play_frame(session->audio_player_handle,
                                             audio_frame.buffer, audio_frame.len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write audio frame to player: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Print frame transmission statistics for debugging
 */
void kvs_media_print_stats(kvs_pc_session_t* session)
{
    if (session == NULL) {
        return;
    }

    UINT64 currentTime = GETTIME();
    UINT64 sessionDuration = (currentTime - session->start_time) / HUNDREDS_OF_NANOS_IN_A_SECOND;

    if (sessionDuration == 0) sessionDuration = 1; // Avoid division by zero

    UINT64 totalVideoFrames = session->video_frames_sent + session->video_frames_dropped + session->video_frames_failed;
    UINT64 totalAudioFrames = session->audio_frames_sent + session->audio_frames_dropped + session->audio_frames_failed;

    ESP_LOGI(TAG, "Frame Statistics for peer: %s", session->peer_id);
    ESP_LOGI(TAG, "  Video: Sent=%llu, Dropped=%llu, Failed=%llu, Total=%llu",
             session->video_frames_sent, session->video_frames_dropped,
             session->video_frames_failed, totalVideoFrames);
    ESP_LOGI(TAG, "  Audio: Sent=%llu, Dropped=%llu, Failed=%llu, Total=%llu",
             session->audio_frames_sent, session->audio_frames_dropped,
             session->audio_frames_failed, totalAudioFrames);

    if (totalVideoFrames > 0) {
        DOUBLE videoSuccessRate = (DOUBLE)session->video_frames_sent / totalVideoFrames * 100.0;
        DOUBLE videoFPS = (DOUBLE)session->video_frames_sent / sessionDuration;
        ESP_LOGI(TAG, "  Video Success Rate: %.2f%%, FPS: %.2f", videoSuccessRate, videoFPS);
    }

    if (totalAudioFrames > 0) {
        DOUBLE audioSuccessRate = (DOUBLE)session->audio_frames_sent / totalAudioFrames * 100.0;
        DOUBLE audioFPS = (DOUBLE)session->audio_frames_sent / sessionDuration;
        ESP_LOGI(TAG, "  Audio Success Rate: %.2f%%, FPS: %.2f", audioSuccessRate, audioFPS);
    }

    if (session->first_video_frame_sent > 0) {
        UINT64 timeToFirstVideo = (session->first_video_frame_sent - session->start_time) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        ESP_LOGI(TAG, "  Time to first video frame: %llu ms", timeToFirstVideo);
    }

    if (session->first_audio_frame_sent > 0) {
        UINT64 timeToFirstAudio = (session->first_audio_frame_sent - session->start_time) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        ESP_LOGI(TAG, "  Time to first audio frame: %llu ms", timeToFirstAudio);
    }

    ESP_LOGI(TAG, "  Session Duration: %llu seconds", sessionDuration);
}
