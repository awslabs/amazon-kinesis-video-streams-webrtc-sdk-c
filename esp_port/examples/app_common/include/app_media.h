/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Common media streaming functions for WebRTC examples
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "audio_capture.h"
#include "video_capture.h"
#include "audio_player.h"
#include "video_player.h"
#include "com/amazonaws/kinesis/video/webrtcclient/Include.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize video and audio handles with default configurations
 *
 * @param video_handle Pointer to store the video handle
 * @param audio_handle Pointer to store the audio handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_media_init(video_capture_handle_t *video_handle, audio_capture_handle_t *audio_handle);

/**
 * @brief Start media capture (video and audio)
 *
 * @param video_handle Video capture handle
 * @param audio_handle Audio capture handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_media_start(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Stop media capture (video and audio)
 *
 * @param video_handle Video capture handle
 * @param audio_handle Audio capture handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_media_stop(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Send video packets thread function for WebRTC
 *
 * @param args PSampleConfiguration pointer
 * @return PVOID Status code
 */
PVOID sendMediaStreamVideoPackets(PVOID args);

/**
 * @brief Send audio packets thread function for WebRTC
 *
 * @param args PSampleConfiguration pointer
 * @return PVOID Status code
 */
PVOID sendMediaStreamAudioPackets(PVOID args);

/**
 * @brief Handler for received video frames
 *
 * @param customData User-defined data
 * @param pFrame Received video frame
 */
VOID appMediaVideoFrameHandler(UINT64 customData, PFrame pFrame);

/**
 * @brief Handler for received audio frames
 *
 * @param customData User-defined data
 * @param pFrame Received audio frame
 */
VOID appMediaAudioFrameHandler(UINT64 customData, PFrame pFrame);

/**
 * @brief Clean up media resources
 *
 * @param video_handle Video capture handle
 * @param audio_handle Audio capture handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_media_deinit(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Read a media frame from disk
 *
 * @param pFrame Buffer to store the frame data
 * @param pSize Pointer to size of buffer (in) and actual size read (out)
 * @param frameFilePath Path to the frame file
 * @return STATUS STATUS_SUCCESS on success, otherwise an error code
 */
STATUS app_media_read_frame_from_disk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath);

/**
 * @brief Send video packets from sample files
 *
 * @param args PSampleConfiguration pointer
 * @return PVOID Status code
 */
PVOID sendFileVideoPackets(PVOID args);

/**
 * @brief Send audio packets from sample files
 *
 * @param args PSampleConfiguration pointer
 * @return PVOID Status code
 */
PVOID sendFileAudioPackets(PVOID args);

#ifdef __cplusplus
}
#endif
