# Media Stream Component

This component provides unified media interfaces for the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF. It abstracts hardware-specific implementations and provides a consistent API for video capture, audio capture, video playback, and audio playback.

## Overview

The Media Stream component serves as the hardware abstraction layer between the WebRTC SDK and ESP32 media devices. It handles:

- **Video Capture**: Camera frame capture and H.264 encoding
- **Audio Capture**: Microphone audio capture and Opus encoding
- **Video Playback**: H.264 frame decoding and display
- **Audio Playback**: Opus frame decoding and audio output

## Core Components

### Video Components
- **`H264FrameGrabber`**: Captures camera frames, encodes using H.264 encoder, and queues encoded frames
- **`H264FramePlayer`**: Decodes H.264 frames and displays on LCD/output device

### Audio Components
- **`OpusFrameGrabber`**: Records I2S audio data, encodes using Opus encoder, and queues encoded frames
- **`OpusAudioPlayer`**: Decodes Opus frames and plays through I2S audio output using ring buffer

## API Interfaces

The component provides four main interface types:

### Video Capture Interface (`media_stream_video_capture_t`)
```c
media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
// Provides: init, start, stop, get_frame, release_frame, deinit
```

### Audio Capture Interface (`media_stream_audio_capture_t`)
```c
media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
// Provides: init, start, stop, get_frame, release_frame, deinit
```

### Video Player Interface (`media_stream_video_player_t`)
```c
media_stream_video_player_t *video_player = media_stream_get_video_player_if();
// Provides: init, start, stop, play_frame, deinit
```

### Audio Player Interface (`media_stream_audio_player_t`)
```c
media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();
// Provides: init, start, stop, play_frame, deinit
```

## Quick Usage

### Basic Integration with WebRTC

```c
#include "media_stream.h"
#include "app_webrtc.h"

// Get media interfaces
media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
media_stream_video_player_t *video_player = media_stream_get_video_player_if();
media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

// Configure WebRTC with media interfaces
app_webrtc_config_t webrtc_config = WEBRTC_APP_CONFIG_DEFAULT();
webrtc_config.video_capture = video_capture;
webrtc_config.audio_capture = audio_capture;
webrtc_config.video_player = video_player;
webrtc_config.audio_player = audio_player;

// Initialize and run WebRTC
app_webrtc_init(&webrtc_config);
app_webrtc_run();
```

### Direct Interface Usage

```c
// Initialize video capture
video_capture_config_t config = {
    .resolution = VIDEO_RESOLUTION_VGA,
    .fps = 30,
    .codec = VIDEO_CODEC_H264
};

video_capture_handle_t handle;
video_capture->init(&config, &handle);
video_capture->start(handle);

// Get H.264 encoded frames
video_frame_t *frame;
esp_err_t ret = video_capture->get_frame(handle, &frame, 1000);
if (ret == ESP_OK) {
    // Process H.264 frame data (frame->buffer, frame->len)
    video_capture->release_frame(handle, frame);
}
```

## Custom Implementation

You can implement custom media interfaces for specialized hardware:

```c
// Implement interface functions
static esp_err_t my_camera_init(video_capture_config_t *config, video_capture_handle_t *handle) {
    // Initialize your custom camera hardware
    return ESP_OK;
}

// Create interface instance
media_stream_video_capture_t* my_custom_camera_if_get(void) {
    static media_stream_video_capture_t interface = {
        .init = my_camera_init,
        .start = my_camera_start,
        .get_frame = my_camera_get_frame,  // Must return H.264 encoded frames
        .release_frame = my_camera_release_frame,
        .stop = my_camera_stop,
        .deinit = my_camera_deinit
    };
    return &interface;
}

// Use custom interface
webrtc_config.video_capture = my_custom_camera_if_get();
```

## Frame Requirements

- **Video frames**: Must be H.264 encoded with proper SPS/PPS headers
- **Audio frames**: Must be Opus encoded at 48kHz sample rate
- **Timestamps**: Should be monotonic and represent presentation time
- **Memory management**: Proper allocation/deallocation via release_frame()

## Dependencies

- ESP-IDF components: `driver`, `esp_codec`, `esp_cam`
- External codecs: H.264 encoder/decoder, Opus encoder/decoder
- Hardware: Camera module, I2S audio interface

## Documentation

- **API Reference**: See header files in `include/` directory
- **Developer Guide**: `../../docs/en/api-reference/media_stream.rst`
- **Examples**: `../../examples/` - Working WebRTC applications
- **Configuration**: `../../docs/en/api-reference/webrtc_config.rst`

## Supported Hardware

- **ESP32-S3** with camera modules (OV2640, OV3660, etc.)
- **ESP32-P4** with advanced camera and audio capabilities
- **ESP32** with external camera/audio via I2S/SPI
- **Custom hardware** via interface implementation
