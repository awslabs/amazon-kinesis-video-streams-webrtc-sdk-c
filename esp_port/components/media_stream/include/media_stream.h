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
#include <stddef.h>
#include "esp_err.h"
#include "audio_capture.h"
#include "video_capture.h"
#include "audio_player.h"
#include "video_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Media stream types
 */
typedef enum {
    MEDIA_STREAM_VIDEO,
    MEDIA_STREAM_AUDIO
} media_stream_type_t;

/**
 * @brief Frame received callback function
 *
 * @param stream_id Stream ID of the received frame
 * @param media_type Type of media (audio or video)
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @param is_key_frame Whether this is a key frame (only relevant for video)
 * @param user_data User data passed to the callback
 */
typedef void (*media_stream_frame_received_cb_t)(uint32_t stream_id,
                                                 media_stream_type_t media_type,
                                                 uint8_t *frame_data,
                                                 size_t frame_size,
                                                 uint64_t timestamp,
                                                 bool is_key_frame,
                                                 void *user_data);

/**
 * @brief Frame ready callback function
 *
 * @param media_type Type of media (audio or video)
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @param is_key_frame Whether this is a key frame (only relevant for video)
 * @param user_data User data passed to the callback
 */
typedef void (*media_stream_frame_ready_cb_t)(media_stream_type_t media_type,
                                              uint8_t *frame_data,
                                              size_t frame_size,
                                              uint64_t timestamp,
                                              bool is_key_frame,
                                              void *user_data);

/**
 * @brief Audio capture interface
 */
typedef struct {
    /**
     * @brief Initialize audio capture
     *
     * @param config Audio capture configuration
     * @param handle Pointer to store the audio capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*init)(audio_capture_config_t *config, audio_capture_handle_t *handle);

    /**
     * @brief Start audio capture
     *
     * @param handle Audio capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*start)(audio_capture_handle_t handle);

    /**
     * @brief Stop audio capture
     *
     * @param handle Audio capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*stop)(audio_capture_handle_t handle);

    /**
     * @brief Get audio frame
     *
     * @param handle Audio capture handle
     * @param frame Pointer to store the audio frame
     * @param timeout_ms Timeout in milliseconds
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*get_frame)(audio_capture_handle_t handle, audio_frame_t **frame, uint32_t timeout_ms);

    /**
     * @brief Release audio frame
     *
     * @param handle Audio capture handle
     * @param frame Audio frame to release
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*release_frame)(audio_capture_handle_t handle, audio_frame_t *frame);

    /**
     * @brief Deinitialize audio capture
     *
     * @param handle Audio capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*deinit)(audio_capture_handle_t handle);
} media_stream_audio_capture_t;

/**
 * @brief Video capture interface
 */
typedef struct {
    /**
     * @brief Initialize video capture
     *
     * @param config Video capture configuration
     * @param handle Pointer to store the video capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*init)(video_capture_config_t *config, video_capture_handle_t *handle);

    /**
     * @brief Start video capture
     *
     * @param handle Video capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*start)(video_capture_handle_t handle);

    /**
     * @brief Stop video capture
     *
     * @param handle Video capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*stop)(video_capture_handle_t handle);

    /**
     * @brief Get video frame
     *
     * @param handle Video capture handle
     * @param frame Pointer to store the video frame
     * @param timeout_ms Timeout in milliseconds
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*get_frame)(video_capture_handle_t handle, video_frame_t **frame, uint32_t timeout_ms);

    /**
     * @brief Release video frame
     *
     * @param handle Video capture handle
     * @param frame Video frame to release
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*release_frame)(video_capture_handle_t handle, video_frame_t *frame);

    /**
     * @brief Deinitialize video capture
     *
     * @param handle Video capture handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*deinit)(video_capture_handle_t handle);

    /**
     * @brief Set video bitrate dynamically
     *
     * @param handle Video capture handle
     * @param bitrate_kbps New bitrate in kbps
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*set_bitrate)(video_capture_handle_t handle, uint32_t bitrate_kbps);

    /**
     * @brief Get current video bitrate
     *
     * @param handle Video capture handle
     * @param bitrate_kbps Pointer to store current bitrate in kbps
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*get_bitrate)(video_capture_handle_t handle, uint32_t *bitrate_kbps);
} media_stream_video_capture_t;

/**
 * @brief Audio player interface
 */
typedef struct {
    /**
     * @brief Initialize audio player
     *
     * @param config Audio player configuration
     * @param handle Pointer to store the audio player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*init)(audio_player_config_t *config, audio_player_handle_t *handle);

    /**
     * @brief Start audio playback
     *
     * @param handle Audio player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*start)(audio_player_handle_t handle);

    /**
     * @brief Stop audio playback
     *
     * @param handle Audio player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*stop)(audio_player_handle_t handle);

    /**
     * @brief Play audio frame
     *
     * @param handle Audio player handle
     * @param data Pointer to encoded audio data
     * @param len Length of encoded audio data in bytes
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*play_frame)(audio_player_handle_t handle, const uint8_t *data, uint32_t len);

    /**
     * @brief Deinitialize audio player
     *
     * @param handle Audio player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*deinit)(audio_player_handle_t handle);
} media_stream_audio_player_t;

/**
 * @brief Video player interface
 */
typedef struct {
    /**
     * @brief Initialize video player
     *
     * @param config Video player configuration
     * @param handle Pointer to store the video player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*init)(video_player_config_t *config, video_player_handle_t *handle);

    /**
     * @brief Start video playback
     *
     * @param handle Video player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*start)(video_player_handle_t handle);

    /**
     * @brief Stop video playback
     *
     * @param handle Video player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*stop)(video_player_handle_t handle);

    /**
     * @brief Play video frame
     *
     * @param handle Video player handle
     * @param data Pointer to encoded video data
     * @param len Length of encoded video data in bytes
     * @param is_keyframe Whether this frame is a keyframe
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*play_frame)(video_player_handle_t handle, const uint8_t *data,
                            uint32_t len, bool is_keyframe);

    /**
     * @brief Deinitialize video player
     *
     * @param handle Video player handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t (*deinit)(video_player_handle_t handle);
} media_stream_video_player_t;

/**
 * @brief Get the default audio capture interface
 *
 * @return media_stream_audio_capture_t* Pointer to the audio capture interface
 */
media_stream_audio_capture_t* media_stream_get_audio_capture_if(void);

/**
 * @brief Get the default video capture interface
 *
 * @return media_stream_video_capture_t* Pointer to the video capture interface
 */
media_stream_video_capture_t* media_stream_get_video_capture_if(void);

/**
 * @brief Get file-based video capture interface
 *
 * This interface reads video frames from files on disk (e.g., /spiffs/samples/frame-XXXX.h264)
 * Useful for testing or when camera hardware is not available.
 *
 * @return media_stream_video_capture_t* Pointer to the file-based video capture interface
 */
media_stream_video_capture_t* media_stream_get_file_video_capture_if(void);

/**
 * @brief Get file-based audio capture interface
 *
 * This interface reads audio frames from embedded files or disk.
 * Useful for testing or when microphone hardware is not available.
 *
 * @return media_stream_audio_capture_t* Pointer to the file-based audio capture interface
 */
media_stream_audio_capture_t* media_stream_get_file_audio_capture_if(void);

/**
 * @brief Register a callback for received frames
 *
 * @param callback Function to call when a frame is received
 * @param user_data User data to pass to the callback
 */
void media_stream_register_frame_received_cb(media_stream_frame_received_cb_t callback, void *user_data);

/**
 * @brief Register a callback for frames ready to be sent
 *
 * @param callback Function to call when a frame is ready to be sent
 * @param user_data User data to pass to the callback
 */
void media_stream_register_frame_ready_cb(media_stream_frame_ready_cb_t callback, void *user_data);

/**
 * @brief Initialize media components
 *
 * @param video_handle Optional pointer to store video capture handle
 * @param audio_handle Optional pointer to store audio capture handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_init(video_capture_handle_t *video_handle, audio_capture_handle_t *audio_handle);

/**
 * @brief Start media capture
 *
 * @param video_handle Video capture handle or NULL to use default
 * @param audio_handle Audio capture handle or NULL to use default
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_start(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Stop media capture
 *
 * @param video_handle Video capture handle or NULL to use default
 * @param audio_handle Audio capture handle or NULL to use default
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_stop(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Deinitialize media components
 *
 * @param video_handle Video capture handle or NULL to use default
 * @param audio_handle Audio capture handle or NULL to use default
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_deinit(video_capture_handle_t video_handle, audio_capture_handle_t audio_handle);

/**
 * @brief Handle incoming video frames
 *
 * @param stream_id Stream ID of the received frame
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @param is_key_frame Whether this is a key frame
 */
void media_stream_handle_video_frame(uint32_t stream_id, uint8_t *frame_data, size_t frame_size,
                                  uint64_t timestamp, bool is_key_frame);

/**
 * @brief Handle incoming audio frames
 *
 * @param stream_id Stream ID of the received frame
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 */
void media_stream_handle_audio_frame(uint32_t stream_id, uint8_t *frame_data,
                                     size_t frame_size, uint64_t timestamp);

/**
 * @brief Read a media frame from disk
 *
 * @param frame_data Buffer to store the frame data (NULL to get size)
 * @param size Pointer to size of buffer (in) and actual size read (out)
 * @param frame_path Path to the frame file
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_read_frame_from_disk(uint8_t *frame_data, uint32_t *size, const char *frame_path);

/**
 * @brief Get a sample video frame from file
 *
 * @param frame_data Buffer to store the frame data
 * @param max_size Maximum size of the buffer
 * @param actual_size Actual size of the frame data
 * @param timestamp Timestamp of the frame
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_get_sample_video_frame(uint8_t *frame_data, size_t max_size,
                                              size_t *actual_size, uint64_t *timestamp);

/**
 * @brief Get a sample audio frame from file
 *
 * @param frame_data Buffer to store the frame data
 * @param max_size Maximum size of the buffer
 * @param actual_size Actual size of the frame data
 * @param timestamp Timestamp of the frame
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_get_sample_audio_frame(uint8_t *frame_data, size_t max_size,
                                              size_t *actual_size, uint64_t *timestamp);

/**
 * @brief Configuration structure for file-based sample frame paths
 */
typedef struct {
    const char *mount_point;          /**< Mount point (e.g., "/spiffs") */
    const char *video_file_prefix;    /**< Video file prefix (e.g., "samples/frame") */
    const char *audio_file_prefix;    /**< Audio file prefix (e.g., "samples/sample") */
    const char *video_file_extension; /**< Video file extension (e.g., ".h264") */
    const char *audio_file_extension; /**< Audio file extension (e.g., ".opus") */
} media_stream_file_config_t;

/**
 * @brief Configure file paths for sample frame reading
 *
 * This function allows configuring the mount point and file naming patterns
 * for file-based streaming. If not called, defaults are used:
 * - Mount point: "/spiffs"
 * - Video prefix: "samples/frame", extension: ".h264"
 * - Audio prefix: "samples/sample", extension: ".opus"
 *
 * @param config Configuration structure with file paths (NULL fields use defaults)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t media_stream_configure_file_paths(const media_stream_file_config_t *config);

/**
 * @brief Get the default audio player interface
 *
 * @return media_stream_audio_player_t* Pointer to the audio player interface
 */
media_stream_audio_player_t* media_stream_get_audio_player_if(void);

/**
 * @brief Get the default video player interface
 *
 * @return media_stream_video_player_t* Pointer to the video player interface
 */
media_stream_video_player_t* media_stream_get_video_player_if(void);

#ifdef __cplusplus
}
#endif
