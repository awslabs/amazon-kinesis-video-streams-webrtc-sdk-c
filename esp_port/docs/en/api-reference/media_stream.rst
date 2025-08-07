Media Stream API
================

This guide explains how to work with media streaming in the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF.

Overview
--------

The Media Stream API provides a unified interface for capturing, encoding, and playing audio and video streams. It abstracts the underlying hardware and provides a consistent API across different ESP32 variants.

The media stream component consists of four key interfaces:

* **Video Capture**: Captures and encodes video frames from camera devices (H.264)
* **Audio Capture**: Captures and encodes audio samples from microphone devices (Opus)
* **Video Player**: Plays received video frames on display devices
* **Audio Player**: Plays received audio samples through speaker devices

Getting Media Interfaces
-------------------------

The media stream component provides getter functions to obtain interface pointers:

.. code-block:: c

   #include "media_stream.h"

   // Get the default media interfaces
   media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
   media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
   media_stream_video_player_t *video_player = media_stream_get_video_player_if();
   media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

   // Check interfaces are available
   if (video_capture == NULL || audio_capture == NULL ||
       video_player == NULL || audio_player == NULL) {
       ESP_LOGE(TAG, "Failed to get media interfaces");
       return;
   }

Video Capture Interface
-----------------------

The video capture interface captures frames from camera and encodes them as H.264:

.. code-block:: c

   // Configure video capture
   video_capture_config_t config = {
       .resolution = VIDEO_RESOLUTION_VGA,   // 640x480
       .fps = 30,
       .codec = VIDEO_CODEC_H264,
   };

   // Initialize video capture
   video_capture_handle_t handle;
   esp_err_t ret = video_capture->init(&config, &handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to initialize video capture");
       return;
   }

   // Start capturing
   ret = video_capture->start(handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to start video capture");
       return;
   }

   // Get encoded H.264 frames
   video_frame_t *frame;
   ret = video_capture->get_frame(handle, &frame, 1000); // 1 second timeout
   if (ret == ESP_OK) {
       // Frame contains H.264 encoded data
       ESP_LOGI(TAG, "Got H.264 frame: %d bytes, type: %s",
                frame->len, frame->type == VIDEO_FRAME_TYPE_IDR ? "IDR" : "P");

       // Use the frame data (frame->buffer, frame->len)
       // ...

       // Release the frame when done
       video_capture->release_frame(handle, frame);
   }

   // Stop and cleanup
   video_capture->stop(handle);
   video_capture->deinit(handle);

Audio Capture Interface
-----------------------

The audio capture interface captures audio and encodes it as Opus:

.. code-block:: c

   // Configure audio capture
   audio_capture_config_t config = {
       .sample_rate = 48000,
       .channels = 1,              // Mono
       .codec = AUDIO_CODEC_OPUS,
   };

   // Initialize audio capture
   audio_capture_handle_t handle;
   esp_err_t ret = audio_capture->init(&config, &handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to initialize audio capture");
       return;
   }

   // Start capturing
   ret = audio_capture->start(handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to start audio capture");
       return;
   }

   // Get encoded Opus frames
   audio_frame_t *frame;
   ret = audio_capture->get_frame(handle, &frame, 1000); // 1 second timeout
   if (ret == ESP_OK) {
       // Frame contains Opus encoded data
       ESP_LOGI(TAG, "Got Opus frame: %d bytes", frame->len);

       // Use the frame data (frame->buffer, frame->len)
       // ...

       // Release the frame when done
       audio_capture->release_frame(handle, frame);
   }

   // Stop and cleanup
   audio_capture->stop(handle);
   audio_capture->deinit(handle);

Video Player Interface
----------------------

The video player interface plays received H.264 video frames:

.. code-block:: c

   // Configure video player
   video_player_config_t config = {
       .output_type = VIDEO_OUTPUT_LCD,    // or VIDEO_OUTPUT_NONE for headless
   };

   // Initialize video player
   video_player_handle_t handle;
   esp_err_t ret = video_player->init(&config, &handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to initialize video player");
       return;
   }

   // Start player
   ret = video_player->start(handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to start video player");
       return;
   }

   // Play received H.264 frame
   ret = video_player->play_frame(handle, h264_data, h264_len, is_keyframe);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to play frame");
   }

   // Stop and cleanup
   video_player->stop(handle);
   video_player->deinit(handle);

Audio Player Interface
----------------------

The audio player interface plays received Opus audio frames:

.. code-block:: c

   // Configure audio player
   audio_player_config_t config = {
       .sample_rate = 48000,
       .channels = 1,              // Mono
       .output_type = AUDIO_OUTPUT_I2S,
   };

   // Initialize audio player
   audio_player_handle_t handle;
   esp_err_t ret = audio_player->init(&config, &handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to initialize audio player");
       return;
   }

   // Start player
   ret = audio_player->start(handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to start audio player");
       return;
   }

   // Play received Opus frame
   ret = audio_player->play_frame(handle, opus_data, opus_len);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to play audio frame");
   }

   // Stop and cleanup
   audio_player->stop(handle);
   audio_player->deinit(handle);

Frame Callbacks
---------------

The media stream component also supports callback-based frame handling:

.. code-block:: c

   // Callback for frames ready to be sent (from capture)
   void frame_ready_callback(media_stream_type_t media_type,
                            uint8_t *frame_data, size_t frame_size,
                            uint64_t timestamp, bool is_key_frame,
                            void *user_data)
   {
       if (media_type == MEDIA_STREAM_VIDEO) {
           ESP_LOGI(TAG, "Video frame ready: %d bytes, keyframe: %s",
                    frame_size, is_key_frame ? "yes" : "no");
       } else {
           ESP_LOGI(TAG, "Audio frame ready: %d bytes", frame_size);
       }

       // Send frame via WebRTC
       // ...
   }

   // Callback for received frames (from remote peer)
   void frame_received_callback(uint32_t stream_id, media_stream_type_t media_type,
                               uint8_t *frame_data, size_t frame_size,
                               uint64_t timestamp, bool is_key_frame,
                               void *user_data)
   {
       if (media_type == MEDIA_STREAM_VIDEO) {
           ESP_LOGI(TAG, "Video frame received: %d bytes", frame_size);
           // Play video frame
       } else {
           ESP_LOGI(TAG, "Audio frame received: %d bytes", frame_size);
           // Play audio frame
       }
   }

   // Register callbacks
   media_stream_register_frame_ready_cb(frame_ready_callback, NULL);
   media_stream_register_frame_received_cb(frame_received_callback, NULL);

High-Level Media Functions
--------------------------

For convenience, the media stream component provides high-level functions:

.. code-block:: c

   // Initialize all media components
   video_capture_handle_t video_handle;
   audio_capture_handle_t audio_handle;

   esp_err_t ret = media_stream_init(&video_handle, &audio_handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to initialize media stream");
       return;
   }

   // Start media capture
   ret = media_stream_start(video_handle, audio_handle);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to start media stream");
       return;
   }

   // ... streaming happens via callbacks ...

   // Stop media capture
   media_stream_stop(video_handle, audio_handle);

   // Cleanup
   media_stream_deinit(video_handle, audio_handle);

Usage in WebRTC Applications
-----------------------------

The media interfaces are typically passed to the WebRTC application configuration:

.. code-block:: c

   #include "app_webrtc.h"
   #include "media_stream.h"

   // Get media interfaces
   media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
   media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
   media_stream_video_player_t *video_player = media_stream_get_video_player_if();
   media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

   // Configure WebRTC app
   app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

   // Pass media interfaces to WebRTC
   webrtcConfig.video_capture = video_capture;
   webrtcConfig.audio_capture = audio_capture;
   webrtcConfig.video_player = video_player;
   webrtcConfig.audio_player = audio_player;
   webrtcConfig.receive_media = true;  // Enable media reception

   // Initialize WebRTC application
   WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
   if (status == WEBRTC_STATUS_SUCCESS) {
       app_webrtc_run();
   }

Custom Media Interface Implementation
-------------------------------------

You can implement custom media interfaces for specialized hardware or requirements:

**Why Custom Interfaces?**

- **Custom camera sensors** (thermal, infrared, high-speed)
- **External audio devices** (USB microphones, digital audio interfaces)
- **Hardware acceleration** (custom encoding/decoding)
- **Industrial applications** (machine vision, audio processing)

**Implementation Steps**

**Step 1: Implement the Interface**

.. code-block:: c

   // Example: Custom video capture interface
   typedef struct {
       // Your device-specific context
       void *device_handle;
       bool is_initialized;
   } my_custom_camera_t;

   static esp_err_t my_camera_init(video_capture_config_t *config, video_capture_handle_t *handle)
   {
       my_custom_camera_t *ctx = malloc(sizeof(my_custom_camera_t));
       if (!ctx) return ESP_ERR_NO_MEM;

       // Initialize your custom camera hardware
       ctx->device_handle = my_camera_hardware_init(config);
       ctx->is_initialized = true;

       *handle = (video_capture_handle_t)ctx;
       return ESP_OK;
   }

   static esp_err_t my_camera_get_frame(video_capture_handle_t handle,
                                       video_frame_t **frame, uint32_t timeout_ms)
   {
       my_custom_camera_t *ctx = (my_custom_camera_t *)handle;

       // Capture raw frame from your hardware
       uint8_t *raw_data = my_camera_capture_raw();

       // Encode to H.264 (required for WebRTC)
       video_frame_t *encoded_frame = my_h264_encode(raw_data);

       *frame = encoded_frame;
       return ESP_OK;
   }

   // Implement all other interface functions...

   // Create interface instance
   media_stream_video_capture_t* getMyCustomCamera(void)
   {
       static media_stream_video_capture_t interface = {
           .init = my_camera_init,
           .start = my_camera_start,
           .stop = my_camera_stop,
           .get_frame = my_camera_get_frame,
           .release_frame = my_camera_release_frame,
           .deinit = my_camera_deinit
       };
       return &interface;
   }

**Step 2: Use Custom Interface**

.. code-block:: c

   // Configure WebRTC with custom interface
   app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

   // Use custom video capture
   webrtcConfig.video_capture = getMyCustomCamera();

   // Use default audio capture
   webrtcConfig.audio_capture = media_stream_get_audio_capture_if();

   // Initialize and run
   WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
   if (status == WEBRTC_STATUS_SUCCESS) {
       app_webrtc_run();
   }

**Implementation Requirements**

- **Video frames**: Must be H.264 encoded
- **Audio frames**: Must be Opus encoded
- **Frame timing**: Provide accurate timestamps
- **Memory management**: Implement proper frame allocation/release
- **Thread safety**: Handle concurrent access if needed
- **Error handling**: Return appropriate esp_err_t codes

**Frame Structure**

Video and audio frames must follow the expected structure:

.. code-block:: c

   typedef struct {
       uint8_t *buffer;        // Encoded frame data (H.264/Opus)
       size_t len;             // Frame size in bytes
       uint64_t timestamp;     // Presentation timestamp (microseconds)
       video_frame_type_t type; // For video: IDR, P, B frame type
   } video_frame_t;

   typedef struct {
       uint8_t *buffer;        // Encoded audio data (Opus)
       size_t len;             // Frame size in bytes
       uint64_t timestamp;     // Presentation timestamp (microseconds)
   } audio_frame_t;

See Also
--------

- :doc:`webrtc_config` - WebRTC application configuration
- :doc:`examples` - Complete working examples
- :doc:`../c-api-reference/media_stream` - Detailed API reference
- :doc:`../c-api-reference/video_capture` - Video capture API
- :doc:`../c-api-reference/audio_capture` - Audio capture API
- :doc:`../c-api-reference/video_player` - Video player API
- :doc:`../c-api-reference/audio_player` - Audio player API
