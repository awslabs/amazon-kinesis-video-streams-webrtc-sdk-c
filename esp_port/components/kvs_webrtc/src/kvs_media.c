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

// Sample file fallback settings
#define KVS_SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / 30)  // 30 FPS
#define KVS_SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)  // 20ms audio frames
#define KVS_NUMBER_OF_OPUS_FRAME_FILES 20  // Number of OPUS sample files to cycle through
#define KVS_MAX_PATH_LEN 256

/* Forward declarations for internal functions */
static STATUS kvs_media_setup_players(kvs_pc_session_t* session, kvs_media_config_t* config);
static STATUS kvs_media_setup_frame_callbacks(kvs_pc_session_t* session);
static PVOID kvs_media_transmission_routine(PVOID customData);
static PVOID kvs_media_reception_routine(PVOID customData);

/* Sample file fallback functions */
static STATUS kvs_media_read_frame_from_disk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath);
static PVOID kvs_media_send_video_samples(PVOID args);
static PVOID kvs_media_send_audio_samples(PVOID args);
static PVOID kvs_media_send_video_from_camera(PVOID args);
static PVOID kvs_media_send_audio_from_mic(PVOID args);

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

/**
 * @brief Thread function to send video frames from sample files
 */
static PVOID kvs_media_send_video_samples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *) args;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[KVS_MAX_PATH_LEN + 1];
    UINT64 startTime, lastFrameTime, elapsed;
    CHK(session != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = 0;

    startTime = GETTIME();
    lastFrameTime = startTime;

    ESP_LOGI(TAG, "🎬 Sample video sender started for peer: %s", session->peer_id);

    while (!session->terminated) {
        // Read H.264 frame
        SNPRINTF(filePath, KVS_MAX_PATH_LEN, "./h264SampleFrames/frame-%04" PRIu64 ".h264", (UINT64)session->frame_index);

        CHK_STATUS(kvs_media_read_frame_from_disk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > session->video_buffer_size) {
            CHK(NULL != (session->video_frame_buffer =
                (PBYTE) REALLOC(session->video_frame_buffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);
            session->video_buffer_size = frameSize;
        }

        frame.frameData = session->video_frame_buffer;
        frame.size = frameSize;
        CHK_STATUS(kvs_media_read_frame_from_disk(session->video_frame_buffer, &frameSize, filePath));

        frame.index = (UINT32) ATOMIC_INCREMENT(&session->frame_index);

        // Key frame every 45 frames
        if (frame.index % 45 == 0) {
            frame.flags = FRAME_FLAG_KEY_FRAME;
        } else {
            frame.flags = FRAME_FLAG_NONE;
        }

        // Send frame via video transceiver
        if (session->video_transceiver != NULL) {
            retStatus = writeFrame(session->video_transceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for video failed with 0x%08" PRIx32, retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                retStatus = STATUS_SUCCESS;
            }
        }

        frame.presentationTs += KVS_SAMPLE_VIDEO_FRAME_DURATION;

        // Simulate video frame rate
        elapsed = GETTIME() - lastFrameTime;
        if (elapsed < KVS_SAMPLE_VIDEO_FRAME_DURATION) {
            THREAD_SLEEP(KVS_SAMPLE_VIDEO_FRAME_DURATION - elapsed);
        }
        lastFrameTime = GETTIME();
    }

CleanUp:
    ESP_LOGI(TAG, "🛑 Sample video sender finished for peer: %s", session->peer_id);
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Thread function to send audio frames from sample files
 */
static PVOID kvs_media_send_audio_samples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *) args;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[KVS_MAX_PATH_LEN + 1];
    UINT64 startTime, lastFrameTime, elapsed;
    UINT32 audioIndex = 0;

    CHK(session != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = 0;

    startTime = GETTIME();
    lastFrameTime = startTime;

    ESP_LOGI(TAG, "🎵 Sample audio sender started for peer: %s", session->peer_id);

    while (!session->terminated) {
        // Read OPUS frame
        SNPRINTF(filePath, KVS_MAX_PATH_LEN, "./opusSampleFrames/sample-%03" PRIu32 ".opus", audioIndex);

        CHK_STATUS(kvs_media_read_frame_from_disk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > session->audio_buffer_size) {
            CHK(NULL != (session->audio_frame_buffer =
                (PBYTE) REALLOC(session->audio_frame_buffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);
            session->audio_buffer_size = frameSize;
        }

        frame.frameData = session->audio_frame_buffer;
        frame.size = frameSize;
        CHK_STATUS(kvs_media_read_frame_from_disk(session->audio_frame_buffer, &frameSize, filePath));

        // Send frame via audio transceiver
        if (session->audio_transceiver != NULL) {
            retStatus = writeFrame(session->audio_transceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for audio failed with 0x%08" PRIx32, retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                retStatus = STATUS_SUCCESS;
            }
        }

        frame.presentationTs += KVS_SAMPLE_AUDIO_FRAME_DURATION;
        audioIndex = (audioIndex + 1) % KVS_NUMBER_OF_OPUS_FRAME_FILES;

        // Simulate audio frame rate
        elapsed = GETTIME() - lastFrameTime;
        if (elapsed < KVS_SAMPLE_AUDIO_FRAME_DURATION) {
            THREAD_SLEEP(KVS_SAMPLE_AUDIO_FRAME_DURATION - elapsed);
        }
        lastFrameTime = GETTIME();
    }

CleanUp:
    ESP_LOGI(TAG, "🛑 Sample audio sender finished for peer: %s", session->peer_id);
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

/* Media capture functions */

/**
 * @brief Thread function to send video frames from camera
 */
static PVOID kvs_media_send_video_from_camera(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *) args;
    media_stream_video_capture_t *video_capture = NULL;
    video_frame_t *video_frame = NULL;
    Frame frame;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    UINT32 fps = 30;
    const UINT64 frame_duration_100ns = HUNDREDS_OF_NANOS_IN_A_SECOND / fps;

    CHK(session != NULL, STATUS_NULL_ARG);
    CHK(session->client->config.video_capture != NULL, STATUS_INTERNAL_ERROR);

    video_capture = (media_stream_video_capture_t*)session->client->config.video_capture;

    // Initialize video capture
    video_capture_config_t video_config = {
        .codec = VIDEO_CODEC_H264,
        .resolution = {
            .width = 640,
            .height = 480,
            .fps = fps
        },
        .quality = 80,
        .bitrate = 500, // 500 kbps
        .codec_specific = NULL
    };

    ESP_LOGI(TAG, "📹 Initializing video capture for peer: %s", session->peer_id);
    CHK(video_capture->init(&video_config, &session->video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(video_capture->start(session->video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;

    nextFrameTime = GETTIME();
    UINT64 currentTime = GETTIME();

    ESP_LOGI(TAG, "📹 Camera video transmission started for peer: %s", session->peer_id);

    // Send frames until terminated
    while (!session->terminated) {
        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        // Get frame from capture interface
        if (video_capture->get_frame(session->video_handle, &video_frame, 0) == ESP_OK && video_frame != NULL) {
            frame.frameData = video_frame->buffer;
            frame.size = video_frame->len;
            frame.presentationTs = currentTime - refTime;
            frame.decodingTs = frame.presentationTs;
            frame.index++;

            // Set key frame flag if needed
            if (video_frame->type == VIDEO_FRAME_TYPE_I) {
                frame.flags = FRAME_FLAG_KEY_FRAME;
            } else {
                frame.flags = FRAME_FLAG_NONE;
            }

            // Send frame via video transceiver
            if (session->video_transceiver != NULL) {
                retStatus = writeFrame(session->video_transceiver, &frame);
                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGW(TAG, "writeFrame for video failed with 0x%08" PRIx32, retStatus);
                }
                if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                    retStatus = STATUS_SUCCESS;
                }
            }

            // Release the frame
            video_capture->release_frame(session->video_handle, video_frame);
        }

        // Calculate next frame time (maintain FPS)
        nextFrameTime += frame_duration_100ns;
        currentTime = GETTIME();
        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= currentTime) {
            nextFrameTime = currentTime + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (session->video_handle != NULL && video_capture != NULL) {
        video_capture->stop(session->video_handle);
        video_capture->deinit(session->video_handle);
        session->video_handle = NULL;
    }

    ESP_LOGI(TAG, "🛑 Camera video transmission finished for peer: %s", session->peer_id);
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Thread function to send audio frames from microphone
 */
static PVOID kvs_media_send_audio_from_mic(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *) args;
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_frame_t *audio_frame = NULL;
    Frame frame;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    UINT32 frameDurationMs = 20;

    CHK(session != NULL, STATUS_NULL_ARG);
    CHK(session->client->config.audio_capture != NULL, STATUS_INTERNAL_ERROR);

    audio_capture = (media_stream_audio_capture_t*)session->client->config.audio_capture;

    // Initialize audio capture
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = 16000,
            .channels = 1,
            .bits_per_sample = 16
        },
        .bitrate = 16000,
        .frame_duration_ms = frameDurationMs,
        .codec_specific = NULL
    };

    ESP_LOGI(TAG, "🎵 Initializing audio capture for peer: %s", session->peer_id);
    CHK(audio_capture->init(&audio_config, &session->audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(audio_capture->start(session->audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.flags = FRAME_FLAG_NONE;

    nextFrameTime = GETTIME();
    UINT64 currentTime = GETTIME();

    ESP_LOGI(TAG, "🎵 Microphone audio transmission started for peer: %s", session->peer_id);

    while (!session->terminated) {
        // Get frame from capture interface
        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        if (audio_capture->get_frame(session->audio_handle, &audio_frame, 0) == ESP_OK && audio_frame != NULL) {
            frame.frameData = audio_frame->buffer;
            frame.size = audio_frame->len;
            frame.presentationTs = currentTime - refTime;
            frame.decodingTs = frame.presentationTs;
            frame.index++;

            // Send frame via audio transceiver
            if (session->audio_transceiver != NULL) {
                retStatus = writeFrame(session->audio_transceiver, &frame);
                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGW(TAG, "writeFrame for audio failed with 0x%08" PRIx32, retStatus);
                }
                if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                    retStatus = STATUS_SUCCESS;
                }
            }

            // Release the frame
            audio_capture->release_frame(session->audio_handle, audio_frame);
        }

        // Schedule next frame with precise timing
        nextFrameTime += frameDurationMs * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // Handle case where we've fallen behind schedule
        currentTime = GETTIME();
        if (nextFrameTime <= currentTime) {
            nextFrameTime = currentTime + frameDurationMs * HUNDREDS_OF_NANOS_IN_A_MILLISECOND / 2;
        }
    }

CleanUp:
    if (session->audio_handle != NULL && audio_capture != NULL) {
        audio_capture->stop(session->audio_handle);
        audio_capture->deinit(session->audio_handle);
        session->audio_handle = NULL;
    }

    ESP_LOGI(TAG, "🛑 Microphone audio transmission finished for peer: %s", session->peer_id);
    return (PVOID) (ULONG_PTR) retStatus;
}

/* Public API implementations */

/**
 * @brief Main media transmission routine - equivalent to mediaSenderRoutine in app_webrtc_media.c
 */
static PVOID kvs_media_transmission_routine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;
    TID videoSenderTid = INVALID_TID_VALUE, audioSenderTid = INVALID_TID_VALUE;

    CHK(session != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "🚀 Media transmission routine started for peer: %s", session->peer_id);

    // Wait for connection to be established
    while (!session->media_started && !session->terminated) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms
    }

    if (session->terminated) {
        ESP_LOGI(TAG, "🛑 Media transmission terminated before connection established");
        goto CleanUp;
    }

    ESP_LOGI(TAG, "✅ Connection established, starting media transmission for peer: %s", session->peer_id);

    // Start video transmission if available
    if (session->client->config.video_capture != NULL) {
        ESP_LOGI(TAG, "📹 Starting video transmission from camera for peer: %s", session->peer_id);
        CHK_STATUS(THREAD_CREATE_EX_EXT(&videoSenderTid, "kvsVideoSender", 8 * 1024, TRUE,
                                       kvs_media_send_video_from_camera, (PVOID) session));
    } else {
        // Fall back to sample files if no capture interface is available
        ESP_LOGI(TAG, "📹 Starting video transmission from sample files for peer: %s (fallback)", session->peer_id);
        CHK_STATUS(THREAD_CREATE_EX_EXT(&videoSenderTid, "kvsVideoSampleSender", 8 * 1024, TRUE,
                                       kvs_media_send_video_samples, (PVOID) session));
    }

    // Start audio transmission if available
    if (session->client->config.audio_capture != NULL) {
        ESP_LOGI(TAG, "🎵 Starting audio transmission from microphone for peer: %s", session->peer_id);
        CHK_STATUS(THREAD_CREATE_EX_EXT(&audioSenderTid, "kvsAudioSender", 8 * 1024, TRUE,
                                       kvs_media_send_audio_from_mic, (PVOID) session));
    } else {
        // Fall back to sample files if no capture interface is available
        ESP_LOGI(TAG, "🎵 Starting audio transmission from sample files for peer: %s (fallback)", session->peer_id);
        CHK_STATUS(THREAD_CREATE_EX_EXT(&audioSenderTid, "kvsAudioSampleSender", 8 * 1024, TRUE,
                                       kvs_media_send_audio_samples, (PVOID) session));
    }

    // Wait for termination
    while (!session->terminated) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms
    }

    // Wait for media threads to complete
    if (IS_VALID_TID_VALUE(videoSenderTid)) {
        THREAD_JOIN(videoSenderTid, NULL);
    }
    if (IS_VALID_TID_VALUE(audioSenderTid)) {
        THREAD_JOIN(audioSenderTid, NULL);
    }

CleanUp:
    ESP_LOGI(TAG, "🛑 Media transmission routine finished for peer: %s", session->peer_id);
    CHK_LOG_ERR(retStatus);
    return NULL;
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
        ESP_LOGI(TAG, "🛑 Media reception terminated before connection established");
        goto CleanUp;
    }

    ESP_LOGI(TAG, "✅ Setting up media reception for peer: %s", session->peer_id);

    // Setup media players and frame callbacks happens in kvs_media_start_reception
    // This thread just stays alive while reception is active
    while (!session->terminated && session->media_started) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms
    }

CleanUp:
    ESP_LOGI(TAG, "🛑 Media reception routine finished for peer: %s", session->peer_id);
    CHK_LOG_ERR(retStatus);
    return NULL;
}

STATUS kvs_media_start_transmission(kvs_pc_session_t* session, kvs_media_config_t* config)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL && config != NULL, STATUS_NULL_ARG);

    // Copy media config to session for access by media threads
    session->client->config.video_capture = config->video_capture;
    session->client->config.audio_capture = config->audio_capture;

    // Start the main media transmission routine
    if (config->video_capture != NULL || config->audio_capture != NULL || config->enable_sample_fallback) {
        CHK_STATUS(THREAD_CREATE_EX_EXT(&session->media_sender_tid, "kvs_mediaSender", 6 * 1024, TRUE,
                                       kvs_media_transmission_routine, (PVOID)session));
        session->media_sender_thread_started = TRUE;
        session->media_threads_started = TRUE;
    }

CleanUp:
    return retStatus;
}

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

STATUS kvs_media_stop(kvs_pc_session_t* session)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(session != NULL, STATUS_NULL_ARG);

    // Mark session as terminated to stop all media threads
    session->terminated = TRUE;

    // Wait for media threads to complete
    if (session->media_sender_thread_started && IS_VALID_TID_VALUE(session->media_sender_tid)) {
        THREAD_JOIN(session->media_sender_tid, NULL);
        session->media_sender_thread_started = FALSE;
    }

    if (session->receive_thread_started && IS_VALID_TID_VALUE(session->receive_audio_video_tid)) {
        THREAD_JOIN(session->receive_audio_video_tid, NULL);
        session->receive_thread_started = FALSE;
    }

    // Cleanup media players
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

    // Free media buffers
    SAFE_MEMFREE(session->video_frame_buffer);
    SAFE_MEMFREE(session->audio_frame_buffer);
    session->video_buffer_size = 0;
    session->audio_buffer_size = 0;

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
