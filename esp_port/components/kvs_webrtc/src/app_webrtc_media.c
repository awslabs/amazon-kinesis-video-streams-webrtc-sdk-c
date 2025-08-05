/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_webrtc_media.h"
#include "WebRtcLogging.h"
#include "webrtc_mem_utils.h"
#include "flash_wrapper.h"
#include "fileio.h"
#include "signaling_serializer.h"
#include "media_stream.h"
#include "sdkconfig.h"

static const char *TAG = "app_webrtc_media";

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;
    CHK_ERR(pSize != NULL, STATUS_NULL_ARG, "[KVS Master] Invalid file size");
    size = *pSize;
    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));
CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

/* Media transmission functions */

/**
 * @brief Send a video frame to all connected peers
 *
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @param is_key_frame TRUE if this is a key frame
 * @return STATUS code of the execution
 */
STATUS webrtcAppSendVideoFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp, BOOL is_key_frame)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    Frame frame = {0};
    UINT32 i;

    CHK(frame_data != NULL, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    pSampleConfiguration = gSampleConfiguration;

    frame.version = FRAME_CURRENT_VERSION;
    frame.frameData = frame_data;
    frame.size = frame_size;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = timestamp;
    frame.decodingTs = timestamp;

    // Set key frame flag if needed
    if (is_key_frame) {
        frame.flags = FRAME_FLAG_KEY_FRAME;
    }

    MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        // Check if the peer connection is connected by checking if it's not NULL
        if (pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection != NULL) {
            retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for video failed with 0x%08" PRIx32 , retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                vTaskDelay(pdMS_TO_TICKS(100));
                retStatus = STATUS_SUCCESS;
            }
        }
    }
    MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

CleanUp:
    return retStatus;
}

/**
 * @brief Send an audio frame to all connected peers
 *
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @return STATUS code of the execution
 */
STATUS webrtcAppSendAudioFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    Frame frame = {0};
    UINT32 i;

    CHK(frame_data != NULL, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    pSampleConfiguration = gSampleConfiguration;

    frame.version = FRAME_CURRENT_VERSION;
    frame.frameData = frame_data;
    frame.size = frame_size;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = timestamp;
    frame.decodingTs = timestamp;

    MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        // Check if the peer connection is connected by checking if it's not NULL
        if (pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection != NULL) {
            retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for audio failed with 0x%08" PRIx32 , retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                vTaskDelay(pdMS_TO_TICKS(100));
                retStatus = STATUS_SUCCESS;
            }
        }
    }
    MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

CleanUp:
    return retStatus;
}

/* Media sender thread functions */

/**
 * @brief Thread function to send video frames from camera
 */
PVOID sendVideoFramesFromCamera(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    media_stream_video_capture_t *video_capture = NULL;
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;
    UINT32 fps = 30;
    // Use precise frame duration to avoid timing drift
    const UINT64 frame_duration_100ns = HUNDREDS_OF_NANOS_IN_A_SECOND / fps;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    video_capture = (media_stream_video_capture_t*)pSampleConfiguration->video_capture;
    CHK(video_capture != NULL, STATUS_INTERNAL_ERROR);

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

    ESP_LOGI(TAG, "Initializing video capture");
    CHK(video_capture->init(&video_config, &video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(video_capture->start(video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    nextFrameTime = GETTIME();
    UINT64 currentTime = GETTIME();
    // Send frames until terminated
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }
        // Get frame from capture interface
        if (video_capture->get_frame(video_handle, &video_frame, 0) == ESP_OK && video_frame != NULL) {
            retStatus = webrtcAppSendVideoFrame(
                video_frame->buffer,
                video_frame->len,
                currentTime - refTime,
                video_frame->type == VIDEO_FRAME_TYPE_I
            );

            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "Failed to send video frame, status: 0x%08" PRIx32, retStatus);
            }

            // Release the frame
            video_capture->release_frame(video_handle, video_frame);
        }

        // Calculate next frame time (maintain FPS)
        nextFrameTime += frame_duration_100ns;
        currentTime = GETTIME();
        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= currentTime) {
            // ESP_LOGW(TAG, "Video frame pacing falling behind, resetting to current time");
            nextFrameTime = currentTime + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (video_handle != NULL && video_capture != NULL) {
        video_capture->stop(video_handle);
        video_capture->deinit(video_handle);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Thread function to send audio frames from microphone
 */
PVOID sendAudioFramesFromMic(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    UINT32 frameDurationMs = 20;
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    audio_capture = (media_stream_audio_capture_t*)pSampleConfiguration->audio_capture;
    CHK(audio_capture != NULL, STATUS_INTERNAL_ERROR);

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

    ESP_LOGI(TAG, "Initializing audio capture");
    CHK(audio_capture->init(&audio_config, &audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(audio_capture->start(audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    nextFrameTime = GETTIME();
    UINT64 currentTime = GETTIME();
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Get frame from capture interface
        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }
        if (audio_capture->get_frame(audio_handle, &audio_frame, 0) == ESP_OK && audio_frame != NULL) {
            retStatus = webrtcAppSendAudioFrame(
                audio_frame->buffer,
                audio_frame->len,
                currentTime - refTime
            );

            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "Failed to send audio frame, status: 0x%08" PRIx32, retStatus);
            }

            // Release the frame
            audio_capture->release_frame(audio_handle, audio_frame);
        }

        // Schedule next frame with precise timing
        nextFrameTime += frameDurationMs * 1000;
        // Handle case where we've fallen behind schedule
        currentTime = GETTIME();
        if (nextFrameTime <= currentTime) {
            // ESP_LOGW(TAG, "Frame pacing falling behind, resetting to current time");
            nextFrameTime = currentTime + frameDurationMs * 1000 / 2;
        }
    }

CleanUp:
    if (audio_handle != NULL && audio_capture != NULL) {
        audio_capture->stop(audio_handle);
        audio_capture->deinit(audio_handle);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Thread function to send video frames from sample files
 */
PVOID sendVideoFramesFromSamples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT64 startTime, lastFrameTime, elapsed;
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    frame.presentationTs = 0;

    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Read H.264 frame
        SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%04d.h264", pSampleConfiguration->frameIndex);

        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            CHK(NULL != (pSampleConfiguration->pVideoFrameBuffer =
                (PBYTE) REALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);
            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;
        CHK_STATUS(readFrameFromDisk(pSampleConfiguration->pVideoFrameBuffer, &frameSize, filePath));

        frame.index = (UINT32) ATOMIC_INCREMENT(&pSampleConfiguration->frameIndex);

        // Key frame every 45 frames
        if (frame.index % 45 == 0) {
            frame.flags = FRAME_FLAG_KEY_FRAME;
        } else {
            frame.flags = FRAME_FLAG_NONE;
        }

        CHK_STATUS(webrtcAppSendVideoFrame(frame.frameData, frame.size, frame.presentationTs,
                                          (frame.flags & FRAME_FLAG_KEY_FRAME) != 0));

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        // Simulate video frame rate
        elapsed = GETTIME() - lastFrameTime;
        if (elapsed < SAMPLE_VIDEO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed);
        }
        lastFrameTime = GETTIME();
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Thread function to send audio frames from sample files
 */
PVOID sendAudioFramesFromSamples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT64 startTime, lastFrameTime, elapsed;
    UINT32 audioIndex = 0;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    frame.presentationTs = 0;

    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Read OPUS frame
        SNPRINTF(filePath, MAX_PATH_LEN, "./opusSampleFrames/sample-%03d.opus", audioIndex);

        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->audioBufferSize) {
            CHK(NULL != (pSampleConfiguration->pAudioFrameBuffer =
                (PBYTE) REALLOC(pSampleConfiguration->pAudioFrameBuffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);
            pSampleConfiguration->audioBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pAudioFrameBuffer;
        frame.size = frameSize;
        CHK_STATUS(readFrameFromDisk(pSampleConfiguration->pAudioFrameBuffer, &frameSize, filePath));

        CHK_STATUS(webrtcAppSendAudioFrame(frame.frameData, frame.size, frame.presentationTs));

        frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;
        audioIndex = (audioIndex + 1) % NUMBER_OF_OPUS_FRAME_FILES;

        // Simulate audio frame rate
        elapsed = GETTIME() - lastFrameTime;
        if (elapsed < SAMPLE_AUDIO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION - elapsed);
        }
        lastFrameTime = GETTIME();
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID mediaSenderRoutine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    TID videoSenderTid = INVALID_TID_VALUE, audioSenderTid = INVALID_TID_VALUE;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;

    ESP_LOGI(TAG, "🚀 Media sender routine started, waiting for connection...");

    // Wait for the WebRTC connection to be established before starting media transmission
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->connected) && !ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    CHK(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag), retStatus);

    ESP_LOGI(TAG, "✅ WebRTC connection established, starting media transmission...");

    // Auto-detect source type based on available interfaces
    // Start video transmission
    if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_VIDEO ||
        pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {

        if (pSampleConfiguration->videoSource != NULL) {
            // Use the configured video source callback
            ESP_LOGI(TAG, "📹 Starting video using custom videoSource callback");
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->videoSenderTid, "videoSender", 8 * 1024, TRUE,
                          pSampleConfiguration->videoSource, (PVOID) pSampleConfiguration);
        } else if (pSampleConfiguration->video_capture != NULL) {
            // Use our built-in video capture from device
            ESP_LOGI(TAG, "📹 Starting video using device video_capture interface");
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->videoSenderTid, "videoSender", 8 * 1024, TRUE,
                          sendVideoFramesFromCamera, (PVOID) pSampleConfiguration);
        } else {
            // Fall back to sample files only if no other source is available
            ESP_LOGI(TAG, "📹 Starting video using sample files (fallback)");
            CHK_STATUS(THREAD_CREATE(&videoSenderTid, sendVideoFramesFromSamples, (PVOID) pSampleConfiguration));
        }
    }

    // Start audio transmission
    if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {
        if (pSampleConfiguration->audioSource != NULL) {
            // Use the configured audio source callback
            ESP_LOGI(TAG, "🎵 Starting audio using custom audioSource callback");
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->audioSenderTid, "audioSender", 8 * 1024, TRUE,
                          pSampleConfiguration->audioSource, (PVOID) pSampleConfiguration);
        } else if (pSampleConfiguration->audio_capture != NULL) {
            // Use our built-in audio capture from device
            ESP_LOGI(TAG, "🎵 Starting audio using device audio_capture interface");
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->audioSenderTid, "audioSender", 8 * 1024, TRUE,
                          sendAudioFramesFromMic, (PVOID) pSampleConfiguration);
        } else {
            // Fall back to sample files only if no other source is available
            ESP_LOGI(TAG, "🎵 Starting audio using sample files (fallback)");
            CHK_STATUS(THREAD_CREATE(&audioSenderTid, sendAudioFramesFromSamples, (PVOID) pSampleConfiguration));
        }
    }

    // Wait for threads to finish
    if (pSampleConfiguration->videoSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
    }
    if (pSampleConfiguration->audioSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
    }

    // Also wait for local fallback threads if they were created
    if (IS_VALID_TID_VALUE(videoSenderTid)) {
        THREAD_JOIN(videoSenderTid, NULL);
    }
    if (IS_VALID_TID_VALUE(audioSenderTid)) {
        THREAD_JOIN(audioSenderTid, NULL);
    }

CleanUp:
    // clean the flag of the media thread.
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);

    ESP_LOGI(TAG, "🛑 Media sender routine finished");
    CHK_LOG_ERR(retStatus);
    return NULL;
}

/* Media reception functions */

/**
 * @brief Receiver for audio/video frames
 */
PVOID sampleReceiveAudioVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Setting up media reception callbacks");

    // Get the sample configuration
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    CHK(pSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    // Lock for player initialization
    MUTEX_LOCK(pSampleConfiguration->playerLock);

    // Initialize video player if available and not already initialized
    if (pSampleConfiguration->video_player != NULL && pSampleConfiguration->video_player_handle == NULL) {
        media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->video_player;

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

        ESP_LOGI(TAG, "Initializing video player");
        if (video_player->init(&video_config, &pSampleConfiguration->video_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video player");
        } else if (video_player->start(pSampleConfiguration->video_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start video player");
        } else {
            ESP_LOGI(TAG, "Video player initialized successfully");
        }
    }

    // Initialize audio player if available and not already initialized
    if (pSampleConfiguration->audio_player != NULL && pSampleConfiguration->audio_player_handle == NULL) {
        media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audio_player;

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

        ESP_LOGI(TAG, "Initializing audio player");
        if (audio_player->init(&audio_config, &pSampleConfiguration->audio_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize audio player");
        } else if (audio_player->start(pSampleConfiguration->audio_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start audio player");
        } else {
            ESP_LOGI(TAG, "Audio player initialized successfully");
        }
    }

    // Increment the active session count
    pSampleConfiguration->activePlayerSessionCount++;

    MUTEX_UNLOCK(pSampleConfiguration->playerLock);

    // Set up callback for video frames
    if (pSampleConfiguration->video_player != NULL && pSampleConfiguration->video_player_handle != NULL) {
        CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver,
                                      (UINT64)(uintptr_t) pSampleStreamingSession,
                                      sampleVideoFrameHandler));
    }

    // Set up callback for audio frames
    if (pSampleConfiguration->audio_player != NULL && pSampleConfiguration->audio_player_handle != NULL) {
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                  (UINT64)(uintptr_t) pSampleStreamingSession,
                                  sampleAudioFrameHandler));
    }

CleanUp:
    return (PVOID) (uintptr_t) retStatus;
}

/**
 * @brief Handler for received video frames
 */
VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame)
{
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession)(uintptr_t) customData;

    if (pFrame == NULL || pFrame->frameData == NULL || pSampleStreamingSession == NULL) {
        ESP_LOGW(TAG, "Invalid video frame or session data");
        return;
    }

    // Get the sample configuration and video player interface
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL || pSampleConfiguration->video_player == NULL) {
        ESP_LOGW(TAG, "Video player not available");
        return;
    }

    media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->video_player;
    if (pSampleConfiguration->video_player_handle == NULL) {
        ESP_LOGW(TAG, "Video player not initialized");
        return;
    }

    // Create video frame structure
    video_frame_t video_frame = {
        .buffer = pFrame->frameData,
        .len = pFrame->size,
        .timestamp = pFrame->presentationTs,
        .type = (pFrame->flags & FRAME_FLAG_KEY_FRAME) ? VIDEO_FRAME_TYPE_I : VIDEO_FRAME_TYPE_P
    };

    // Send frame to player
    esp_err_t err = video_player->play_frame(pSampleConfiguration->video_player_handle,
                                           video_frame.buffer, video_frame.len,
                                           video_frame.type == VIDEO_FRAME_TYPE_I);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write video frame to player: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Handler for received audio frames
 */
VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame)
{
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession)(uintptr_t) customData;

    if (pFrame == NULL || pFrame->frameData == NULL || pSampleStreamingSession == NULL) {
        ESP_LOGW(TAG, "Invalid audio frame or session data");
        return;
    }

    // Get the sample configuration and audio player interface
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL || pSampleConfiguration->audio_player == NULL) {
        ESP_LOGW(TAG, "Audio player not available");
        return;
    }

    media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audio_player;
    if (pSampleConfiguration->audio_player_handle == NULL) {
        ESP_LOGW(TAG, "Audio player not initialized");
        return;
    }

    // Create audio frame structure
    audio_frame_t audio_frame = {
        .buffer = pFrame->frameData,
        .len = pFrame->size,
        .timestamp = pFrame->presentationTs
    };

    // Send frame to player
    esp_err_t err = audio_player->play_frame(pSampleConfiguration->audio_player_handle,
                                           audio_frame.buffer, audio_frame.len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write audio frame to player: %s", esp_err_to_name(err));
    }
}

/* Data channel functions */

#ifdef ENABLE_DATA_CHANNEL
VOID onDataChannelMessage(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, strLen, tokenCount;
    CHAR pMessageSend[MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE], errorMessage[200];
    PCHAR json;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    PSampleConfiguration pSampleConfiguration;
    DataChannelMessage dataChannelMessage;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];

    CHK(pMessage != NULL && pDataChannel != NULL, STATUS_NULL_ARG);

    if (pSampleStreamingSession == NULL) {
        STRCPY(errorMessage, "Could not generate stats since the streaming session is NULL");
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        DLOGE("%s", errorMessage);
        goto CleanUp;
    }

    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL) {
        STRCPY(errorMessage, "Could not generate stats since the sample configuration is NULL");
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        DLOGE("%s", errorMessage);
        goto CleanUp;
    }

    if (pSampleConfiguration->enableSendingMetricsToViewerViaDc) {
        jsmn_init(&parser);
        json = (PCHAR) pMessage;
        tokenCount = jsmn_parse(&parser, json, STRLEN(json), tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));

        MEMSET(dataChannelMessage.content, '\0', SIZEOF(dataChannelMessage.content));
        MEMSET(dataChannelMessage.firstMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.firstMessageFromViewerTs));
        MEMSET(dataChannelMessage.firstMessageFromMasterTs, '\0', SIZEOF(dataChannelMessage.firstMessageFromMasterTs));
        MEMSET(dataChannelMessage.secondMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.secondMessageFromViewerTs));
        MEMSET(dataChannelMessage.secondMessageFromMasterTs, '\0', SIZEOF(dataChannelMessage.secondMessageFromMasterTs));
        MEMSET(dataChannelMessage.lastMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.lastMessageFromViewerTs));

        if (tokenCount > 1) {
            if (tokens[0].type != JSMN_OBJECT) {
                STRCPY(errorMessage, "Invalid JSON received, please send a valid json as the SDK is operating in datachannel-benchmarking mode");
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
                DLOGE("%s", errorMessage);
                retStatus = STATUS_INVALID_API_CALL_RETURN_JSON;
                goto CleanUp;
            }
            DLOGI("DataChannel json message: %.*s\n", pMessageLen, pMessage);

            for (i = 1; i < tokenCount; i++) {
                if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "content")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.content, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "firstMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    // parse and retain this message from the viewer to send it back again
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.firstMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "firstMessageFromMasterTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.firstMessageFromMasterTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    } else {
                        // if this timestamp was not assigned during the previous message session, add it now
                        SNPRINTF(dataChannelMessage.firstMessageFromMasterTs, 20, "%llu", GETTIME() / 10000);
                        break;
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "secondMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    // parse and retain this message from the viewer to send it back again
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.secondMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "secondMessageFromMasterTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.secondMessageFromMasterTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    } else {
                        // if this timestamp was not assigned during the previous message session, add it now
                        SNPRINTF(dataChannelMessage.secondMessageFromMasterTs, 20, "%llu", GETTIME() / 10000);
                        break;
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "lastMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.lastMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                }
            }

            if (STRLEN(dataChannelMessage.lastMessageFromViewerTs) == 0) {
                // continue sending the data_channel_metrics_message with new timestamps until we receive the lastMessageFromViewerTs from the viewer
                SNPRINTF(pMessageSend, MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE, DATA_CHANNEL_MESSAGE_TEMPLATE, MASTER_DATA_CHANNEL_MESSAGE,
                         dataChannelMessage.firstMessageFromViewerTs, dataChannelMessage.firstMessageFromMasterTs,
                         dataChannelMessage.secondMessageFromViewerTs, dataChannelMessage.secondMessageFromMasterTs,
                         dataChannelMessage.lastMessageFromViewerTs);
                DLOGI("Master's response: %s", pMessageSend);

                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pMessageSend, STRLEN(pMessageSend));
            } else {
                // now that we've received the last message, send across the signaling, peerConnection, ice metrics
                SNPRINTF(pSampleStreamingSession->pSignalingClientMetricsMessage, MAX_SIGNALING_CLIENT_METRICS_MESSAGE_SIZE,
                         SIGNALING_CLIENT_METRICS_JSON_TEMPLATE, pSampleConfiguration->signalingClientMetrics.signalingStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.offerReceivedTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.answerTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.describeChannelStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.describeChannelEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getSignalingChannelEndpointStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getSignalingChannelEndpointEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getIceServerConfigStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getIceServerConfigEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getTokenStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getTokenEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.createChannelStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.createChannelEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.connectStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.connectEndTime);
                DLOGI("Sending signaling metrics to the viewer: %s", pSampleStreamingSession->pSignalingClientMetricsMessage);

                CHK_STATUS(peerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->peerConnectionMetrics));
                SNPRINTF(pSampleStreamingSession->pPeerConnectionMetricsMessage, MAX_PEER_CONNECTION_METRICS_MESSAGE_SIZE,
                         PEER_CONNECTION_METRICS_JSON_TEMPLATE,
                         pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionStartTime,
                         pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionConnectedTime);
                DLOGI("Sending peer-connection metrics to the viewer: %s", pSampleStreamingSession->pPeerConnectionMetricsMessage);

                CHK_STATUS(iceAgentGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->iceMetrics));
                SNPRINTF(pSampleStreamingSession->pIceAgentMetricsMessage, MAX_ICE_AGENT_METRICS_MESSAGE_SIZE, ICE_AGENT_METRICS_JSON_TEMPLATE,
                         pSampleStreamingSession->iceMetrics.kvsIceAgentStats.candidateGatheringStartTime,
                         pSampleStreamingSession->iceMetrics.kvsIceAgentStats.candidateGatheringEndTime);
                DLOGI("Sending ice-agent metrics to the viewer: %s", pSampleStreamingSession->pIceAgentMetricsMessage);

                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pSignalingClientMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pSignalingClientMetricsMessage));
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pPeerConnectionMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pPeerConnectionMetricsMessage));
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pIceAgentMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pIceAgentMetricsMessage));
            }
        } else {
            DLOGI("DataChannel string message: %.*s\n", pMessageLen, pMessage);
            STRCPY(errorMessage, "Send a json message for benchmarking as the C SDK is operating in benchmarking mode");
            retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        }
    } else {
        if (isBinary) {
            DLOGI("DataChannel Binary Message");
        } else {
            DLOGI("DataChannel String Message: %.*s\n", pMessageLen, pMessage);
        }
        // Send Echo message to the viewer
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pMessage, pMessageLen);
    }
    if (retStatus != STATUS_SUCCESS) {
        DLOGI("[KVS Master] dataChannelSend(): operation returned status code: 0x%08x \n", retStatus);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
}

VOID onDataChannel(UINT64 customData, PRtcDataChannel pRtcDataChannel)
{
    DLOGI("New DataChannel has been opened %s \n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, customData, onDataChannelMessage);
}
#endif

/* Bandwidth estimation handlers */

VOID sampleBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate)
{
    UNUSED_PARAM(customData);
    DLOGV("received bitrate suggestion: %f", maximumBitrate);
#if CONFIG_IDF_TARGET_ESP32P4
    // FIXME: Do this via media_stream API
    extern esp_err_t esp_h264_hw_enc_set_bitrate(uint32_t bitrate);
    esp_h264_hw_enc_set_bitrate((uint32_t) maximumBitrate);
#endif
}

// Sample callback for TWCC. Average packet is calculated with exponential moving average (EMA). If average packet lost is <= 5%,
// the current bitrate is increased by 5%. If more than 5%, the current bitrate
// is reduced by percent lost. Bitrate update is allowed every second and is increased/decreased upto the limits
VOID sampleSenderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt,
                                            UINT64 duration)
{
    UNUSED_PARAM(duration);
    UINT64 videoBitrate, audioBitrate;
    UINT64 currentTimeMs, timeDiff;
    UINT32 lostPacketsCnt = txPacketsCnt - rxPacketsCnt;
    DOUBLE percentLost = (DOUBLE) ((txPacketsCnt > 0) ? (lostPacketsCnt * 100 / txPacketsCnt) : 0.0);
    SampleStreamingSession* pSampleStreamingSession = (SampleStreamingSession*) customData;

    if (pSampleStreamingSession == NULL) {
        DLOGW("Invalid streaming session (NULL object)");
        return;
    }

    // Calculate packet loss
    pSampleStreamingSession->twccMetadata.averagePacketLoss =
        EMA_ACCUMULATOR_GET_NEXT(pSampleStreamingSession->twccMetadata.averagePacketLoss, ((DOUBLE) percentLost));

    currentTimeMs = GETTIME();
    timeDiff = currentTimeMs - pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs;
    if (timeDiff < TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS) {
        // Too soon for another adjustment
        return;
    }

    MUTEX_LOCK(pSampleStreamingSession->twccMetadata.updateLock);
    videoBitrate = pSampleStreamingSession->twccMetadata.currentVideoBitrate;
    audioBitrate = pSampleStreamingSession->twccMetadata.currentAudioBitrate;

    if (pSampleStreamingSession->twccMetadata.averagePacketLoss <= 5) {
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        videoBitrate = (UINT64) MIN(videoBitrate * 1.05, MAX_VIDEO_BITRATE_KBPS);
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        audioBitrate = (UINT64) MIN(audioBitrate * 1.05, MAX_AUDIO_BITRATE_BPS);
    } else {
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        videoBitrate = (UINT64) MAX(videoBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_VIDEO_BITRATE_KBPS);
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        audioBitrate = (UINT64) MAX(audioBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_AUDIO_BITRATE_BPS);
    }

    // Update the session with the new bitrate and adjustment time
    pSampleStreamingSession->twccMetadata.newVideoBitrate = videoBitrate;
    pSampleStreamingSession->twccMetadata.newAudioBitrate = audioBitrate;
    MUTEX_UNLOCK(pSampleStreamingSession->twccMetadata.updateLock);

    pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs = currentTimeMs;

    DLOGI("Adjustment made: average packet loss = %.2f%%, timediff: %llu ms", pSampleStreamingSession->twccMetadata.averagePacketLoss, timeDiff);
    DLOGI("Suggested video bitrate %u kbps, suggested audio bitrate: %u bps, sent: %u bytes %u packets received: %u bytes %u packets in %lu msec",
          videoBitrate, audioBitrate, txBytes, txPacketsCnt, rxBytes, rxPacketsCnt, duration / 10000ULL);
}
