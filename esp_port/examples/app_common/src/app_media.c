/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "app_media.h"
#include "app_webrtc.h"

static const char *TAG = "app_media";

#define NUMBER_OF_H264_FRAME_FILES               60 //1500
#define NUMBER_OF_H265_FRAME_FILES               1500
#define NUMBER_OF_OPUS_FRAME_FILES               618

#define DEFAULT_FPS_VALUE                        25
#define DEFAULT_VIDEO_HEIGHT_PIXELS              720
#define DEFAULT_VIDEO_WIDTH_PIXELS               1280
#define DEFAULT_AUDIO_OPUS_CHANNELS              2
#define DEFAULT_AUDIO_OPUS_SAMPLE_RATE_HZ        48000
#define DEFAULT_AUDIO_OPUS_BITS_PER_SAMPLE       16

#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)

// Static handles for media components
static video_capture_handle_t s_video_handle = NULL;
static audio_capture_handle_t s_audio_handle = NULL;
static video_player_handle_t s_video_player = NULL;
static audio_player_handle_t s_audio_player = NULL;

esp_err_t app_media_init(video_capture_handle_t *video_handle, audio_capture_handle_t *audio_handle)
{
    esp_err_t ret = ESP_OK;

    // Initialize video capture with H264 configuration
    video_capture_config_t video_config = {
        .codec = VIDEO_CODEC_H264,
        .resolution = {
            .width = 640,
            .height = 480,
            .fps = 30
        },
        .quality = 80,
        .bitrate = 500, // 500 kbps
        .codec_specific = NULL
    };

    // Initialize audio capture with Opus configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = 48000,
            .channels = 1,
            .bits_per_sample = 16
        },
        .bitrate = 64,  // 64 kbps
        .frame_duration_ms = 20,
        .codec_specific = NULL
    };

    // Initialize video capture
    ESP_LOGI(TAG, "Initializing video capture");
    ret = video_capture_init(&video_config, &s_video_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize video capture");
        return ret;
    }

    // Initialize audio capture
    ESP_LOGI(TAG, "Initializing audio capture");
    ret = audio_capture_init(&audio_config, &s_audio_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio capture");
        video_capture_deinit(s_video_handle);
        s_video_handle = NULL;
        return ret;
    }

    // Set the output parameters
    if (video_handle) {
        *video_handle = s_video_handle;
    }

    if (audio_handle) {
        *audio_handle = s_audio_handle;
    }

    return ESP_OK;
}

esp_err_t app_media_start(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle)
{
    esp_err_t ret = ESP_OK;

    // Use passed handles or defaults if NULL
    video_capture_handle_t vid_handle = video_handle ? video_handle : s_video_handle;
    audio_capture_handle_t aud_handle = audio_handle ? audio_handle : s_audio_handle;

    if (vid_handle) {
        ret = video_capture_start(vid_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start video capture");
            return ret;
        }
    }

    if (aud_handle) {
        ret = audio_capture_start(aud_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start audio capture");
            if (vid_handle) {
                video_capture_stop(vid_handle);
            }
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t app_media_stop(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle)
{
    // Use passed handles or defaults if NULL
    video_capture_handle_t vid_handle = video_handle ? video_handle : s_video_handle;
    audio_capture_handle_t aud_handle = audio_handle ? audio_handle : s_audio_handle;

    if (vid_handle) {
        video_capture_stop(vid_handle);
    }

    if (aud_handle) {
        audio_capture_stop(aud_handle);
    }

    return ESP_OK;
}

PVOID sendMediaStreamVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame = {0};
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;
    UINT64 refTime = GETTIME();
    UINT64 lastFrameTime = refTime;

    // Initialize video capture with H264 configuration
    video_capture_config_t video_config = {
        .codec = VIDEO_CODEC_H264,
        .resolution = {
            .width = 640,
            .height = 480,
            .fps = 30
        },
        .quality = 80,
        .bitrate = 500, // 500 kbps
        .codec_specific = NULL
    };

    // Initialize video capture
    ESP_LOGI(TAG, "Initializing video capture");
    if (video_capture_init(&video_config, &video_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize video capture");
        return (PVOID) (ULONG_PTR) STATUS_INTERNAL_ERROR;
    }

    // Start video capture
    if (video_capture_start(video_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start video capture");
        video_capture_deinit(video_handle);
        return (PVOID) (ULONG_PTR) STATUS_INTERNAL_ERROR;
    }

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Get H264 frame from camera
        UINT64 startTime = GETTIME();
        if (video_capture_get_frame(video_handle, &video_frame, 0) == ESP_OK) {
            if (video_frame != NULL && video_frame->len > 0) {
                frame.frameData = video_frame->buffer;
                frame.size = video_frame->len;
                frame.presentationTs = startTime - refTime;
                frame.decodingTs = frame.presentationTs;
                frame.trackId = DEFAULT_VIDEO_TRACK_ID;

                // Set frame flags based on frame type
                if (video_frame->type == VIDEO_FRAME_TYPE_I) {
                    frame.flags = FRAME_FLAG_KEY_FRAME;
                } else {
                    frame.flags = FRAME_FLAG_NONE;
                }

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                for (UINT32 i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                    retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
                    if (retStatus != STATUS_SUCCESS) {
                        ESP_LOGW(TAG, "writeFrame failed with 0x%08" PRIx32, retStatus);
                    }
                }
                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

                // Release the frame when done
                video_capture_release_frame(video_handle, video_frame);
                video_frame = NULL;
            }
        }

        // Adjust sleep for proper timing
        UINT64 elapsed = startTime - lastFrameTime;
        if (elapsed < SAMPLE_VIDEO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // still some small delay
        }
        lastFrameTime = startTime;
    }

    // Clean up
    video_capture_stop(video_handle);
    video_capture_deinit(video_handle);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendMediaStreamAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame = {0};
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;
    UINT64 refTime = GETTIME();
    UINT64 lastFrameTime = refTime;

    // Initialize audio capture with Opus configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = 48000,
            .channels = 1,
            .bits_per_sample = 16
        },
        .bitrate = 64,  // 64 kbps
        .frame_duration_ms = 20,
        .codec_specific = NULL
    };

    // Initialize audio capture
    ESP_LOGI(TAG, "Initializing audio capture");
    if (audio_capture_init(&audio_config, &audio_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio capture");
        return (PVOID) (ULONG_PTR) STATUS_INTERNAL_ERROR;
    }

    // Start audio capture
    if (audio_capture_start(audio_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio capture");
        audio_capture_deinit(audio_handle);
        return (PVOID) (ULONG_PTR) STATUS_INTERNAL_ERROR;
    }

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 startTime = GETTIME();

        // Get Opus frame from microphone
        if (audio_capture_get_frame(audio_handle, &audio_frame, 0) == ESP_OK) {
            if (audio_frame != NULL && audio_frame->len > 0) {
                frame.frameData = audio_frame->buffer;
                frame.size = audio_frame->len;
                // Add the reference time to the presentation timestamp
                frame.presentationTs = startTime - refTime;
                frame.decodingTs = frame.presentationTs;
                frame.trackId = DEFAULT_AUDIO_TRACK_ID;

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                for (UINT32 i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                    retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
                    if (retStatus != STATUS_SUCCESS) {
                        ESP_LOGW(TAG, "writeFrame failed with 0x%08" PRIx32, retStatus);
                    } else if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && retStatus == STATUS_SUCCESS) {
                        PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                        pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                    }
                }
                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

                // Release the frame when done
                audio_capture_release_frame(audio_handle, audio_frame);
                audio_frame = NULL;
            }
        }

        // Adjust sleep for proper timing
        UINT64 elapsed = startTime - lastFrameTime;
        if (elapsed < SAMPLE_AUDIO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION - elapsed % SAMPLE_AUDIO_FRAME_DURATION);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // still some small delay
        }
        lastFrameTime = startTime;
    }

    // Clean up
    audio_capture_stop(audio_handle);
    audio_capture_deinit(audio_handle);

    return (PVOID) (ULONG_PTR) retStatus;
}

VOID appMediaVideoFrameHandler(UINT64 customData, PFrame pFrame)
{
    if (s_video_player == NULL) {
        video_player_config_t config = {
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

        if (video_player_init(&config, &s_video_player) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video player");
            return;
        }

        if (video_player_start(s_video_player) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start video player");
            video_player_deinit(s_video_player);
            s_video_player = NULL;
            return;
        }
    }

    // Check if this is a keyframe
    bool is_keyframe = (pFrame->flags & FRAME_FLAG_KEY_FRAME) != 0;

    // Play the received video frame
    esp_err_t ret = video_player_play_frame(s_video_player, pFrame->frameData, pFrame->size, is_keyframe);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to play video frame: %d", ret);
    }
}

VOID appMediaAudioFrameHandler(UINT64 customData, PFrame pFrame)
{
    if (s_audio_player == NULL) {
        audio_player_config_t config = {
            .codec = AUDIO_PLAYER_CODEC_OPUS,
            .format = {
                .sample_rate = 48000,
                .channels = 1,
                .bits_per_sample = 16
            },
            .buffer_ms = 500,  // 500ms buffer
            .codec_specific = NULL
        };

        if (audio_player_init(&config, &s_audio_player) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize audio player");
            return;
        }

        if (audio_player_start(s_audio_player) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start audio player");
            audio_player_deinit(s_audio_player);
            s_audio_player = NULL;
            return;
        }
    }

    // Play the received opus frame
    esp_err_t ret = audio_player_play_frame(s_audio_player, pFrame->frameData, pFrame->size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to play audio frame: %d", ret);
    }
}

esp_err_t app_media_deinit(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle)
{
    // Use passed handles or defaults if NULL
    video_capture_handle_t vid_handle = video_handle ? video_handle : s_video_handle;
    audio_capture_handle_t aud_handle = audio_handle ? audio_handle : s_audio_handle;

    if (vid_handle) {
        video_capture_deinit(vid_handle);
        if (vid_handle == s_video_handle) {
            s_video_handle = NULL;
        }
    }

    if (aud_handle) {
        audio_capture_deinit(aud_handle);
        if (aud_handle == s_audio_handle) {
            s_audio_handle = NULL;
        }
    }

    // Cleanup players if initialized
    if (s_video_player) {
        video_player_stop(s_video_player);
        video_player_deinit(s_video_player);
        s_video_player = NULL;
    }

    if (s_audio_player) {
        audio_player_stop(s_audio_player);
        audio_player_deinit(s_audio_player);
        s_audio_player = NULL;
    }

    return ESP_OK;
}

// External declarations for embedded files
extern const uint8_t h264_frame_start[] asm("_binary_frame_001_h264_start");
extern const uint8_t h264_frame_end[] asm("_binary_frame_001_h264_end");
extern const uint8_t opus_sample_start[] asm("_binary_sample_001_opus_start");
extern const uint8_t opus_sample_end[] asm("_binary_sample_001_opus_end");

STATUS app_media_read_frame_from_disk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
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

PVOID sendFileVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    RtcEncoderStats encoderStats;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;
    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");

    frame.presentationTs = 0;
    UINT64 lastFrameTime;
    UINT64 refTime = GETTIME();
    lastFrameTime = refTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 startTime = GETTIME();
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        ESP_LOGD(TAG, "Sending H264 Sample: %" PRIu32, fileIndex);
        if (pSampleConfiguration->videoCodec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
            SNPRINTF(filePath, MAX_PATH_LEN, "/spiffs/samples/frame-%04" PRIu32 ".h264", fileIndex);
        }
        //  else if (pSampleConfiguration->videoCodec == RTC_CODEC_H265) {
        //     SNPRINTF(filePath, MAX_PATH_LEN, "./h265SampleFrames/frame-%04d.h265", fileIndex);
        // }

        CHK_STATUS(app_media_read_frame_from_disk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            CHK_ERR(pSampleConfiguration->pVideoFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, "[KVS Master] Failed to allocate video frame buffer");
            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;

        CHK_STATUS(app_media_read_frame_from_disk(frame.frameData, &frameSize, filePath));

        // based on bitrate of samples/h264SampleFrames/frame-*
        encoderStats.width = 640;
        encoderStats.height = 480;
        encoderStats.targetBitrate = 262000;
        frame.presentationTs = startTime - refTime;
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
            encoderStats.encodeTimeMsec = 4; // update encode time to an arbitrary number to demonstrate stats update
            updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &encoderStats);
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
                    ESP_LOGV(TAG, "writeFrame() failed with 0x%08" PRIx32, status);
                }
            } else {
                // Reset file index to ensure first frame sent upon SRTP ready is a key frame.
                fileIndex = 0;
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        // Adjust sleep in the case the sleep itself and writeFrame take longer than expected. Since sleep makes sure that the thread
        // will be paused at least until the given amount, we can assume that there's no too early frame scenario.
        // Also, it's very unlikely to have a delay greater than SAMPLE_VIDEO_FRAME_DURATION, so the logic assumes that this is always
        // true for simplicity.
        UINT64 elapsed = startTime - lastFrameTime;
        if (elapsed < SAMPLE_VIDEO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // still some small delay
        }
        lastFrameTime = startTime;
    }

CleanUp:
    ESP_LOGI(TAG, "Closing file video thread");
    CHK_LOG_ERR(retStatus);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendFileAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0;
    UINT32 i;
    STATUS status;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    frame.presentationTs = 0;
    UINT64 lastFrameTime;
    UINT64 refTime = GETTIME();
    lastFrameTime = refTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 startTime = GETTIME();
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;

        ESP_LOGD(TAG, "Sending Opus Sample: %" PRIu32, fileIndex);
        frame.frameData = opus_sample_start;
        frame.size = opus_sample_end - opus_sample_start;
        startTime = GETTIME();
        frame.presentationTs = startTime - refTime;

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
                    ESP_LOGV(TAG, "writeFrame() failed with 0x%08" PRIx32, status);
                } else if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                    PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                    pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                }
            } else {
                // Reset file index to stay in sync with video frames.
                fileIndex = 0;
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
        UINT64 elapsed = startTime - lastFrameTime;
        if (elapsed < SAMPLE_AUDIO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION - elapsed % SAMPLE_AUDIO_FRAME_DURATION);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // still some small delay
        }
        lastFrameTime = startTime;
    }

CleanUp:
    ESP_LOGI(TAG, "Closing file audio thread");
    return (PVOID) (ULONG_PTR) retStatus;
}
