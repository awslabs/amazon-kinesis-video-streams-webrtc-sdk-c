# Amazon Kinesis Video Streams WebRTC SDK for ESP - API Usage Guide

This document provides guidance on using the Amazon Kinesis Video Streams WebRTC SDK APIs for ESP platforms.

## API Overview

The ESP port of the KVS WebRTC SDK provides a high-level application API through `app_webrtc.h` that simplifies WebRTC integration with ESP devices. The SDK uses a modular signaling architecture that supports multiple signaling protocols through the `webrtc_signaling_client_if_t`.

## Deployment Modes

The SDK supports multiple deployment modes based on different signaling approaches:

### 1. Classic Mode (Single Device)

Both signaling and media streaming are handled by a single ESP device using KVS signaling. This is the simplest mode and is implemented in the `webrtc_classic` example.

### 2. Split Mode (Two Devices)

Signaling and media streaming are split between two ESP devices:
- **Streaming Device** (ESP32-P4/S3): Handles media streaming using bridge signaling (`streaming_only` example)
- **Signaling Device** (ESP32-C6): Handles KVS signaling with bridge adapter (`signaling_only` example)

### 3. AppRTC Mode (Custom Signaling Server)

The ESP device connects to AppRTC-compatible signaling servers instead of AWS KVS. This is implemented in the `esp_camera` example.

### 4. Custom Signaling Mode

You can implement your own signaling protocol by creating a custom `webrtc_signaling_client_if_t`. See [CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md) for detailed implementation guidance.

## Key API Functions

### Core Application Functions

```c
// Initialize the WebRTC application
WEBRTC_STATUS app_webrtc_init(PWebRtcAppConfig pConfig);

// Run the WebRTC application (blocking)
WEBRTC_STATUS app_webrtc_run(void);

// Terminate the WebRTC application
WEBRTC_STATUS app_webrtc_terminate(void);

// Register a callback for WebRTC events
int32_t app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx);
```

### Split Mode Functions

```c
// Register a callback for sending signaling messages to bridge (used in split mode)
int app_webrtc_register_msg_callback(app_webrtc_send_msg_cb_t callback);

// Send a message from bridge to signaling server (used in split mode)
int app_webrtc_send_msg_to_signaling(signaling_msg_t *signalingMessage);

// Create and send an offer as the initiator
int app_webrtc_trigger_offer(char *pPeerId);
```

### ICE Server Management Functions

```c
// Get ICE servers configuration from the WebRTC application
WEBRTC_STATUS app_webrtc_get_ice_servers(uint32_t *pIceServerCount, void *pIceConfiguration);

// Query ICE server by index through signaling abstraction
WEBRTC_STATUS app_webrtc_get_server_by_idx(int index, bool useTurn, uint8_t **data, int *len, bool *have_more);

// Check if ICE configuration refresh is needed through signaling abstraction
WEBRTC_STATUS app_webrtc_is_ice_refresh_needed(bool *refreshNeeded);

// Trigger ICE configuration refresh through signaling abstraction
WEBRTC_STATUS app_webrtc_refresh_ice_configuration(void);
```

## Media Handling

The SDK uses media capture and player interfaces to handle audio and video. Instead of directly sending frames via API calls, you provide interfaces that the SDK uses to capture and play media.

### Media Interfaces

```c
// Get the default video capture interface
media_stream_video_capture_t* media_stream_get_video_capture_if(void);

// Get the default audio capture interface
media_stream_audio_capture_t* media_stream_get_audio_capture_if(void);

// Get the default video player interface
media_stream_video_player_t* media_stream_get_video_player_if(void);

// Get the default audio player interface
media_stream_audio_player_t* media_stream_get_audio_player_if(void);
```

These interfaces are passed to the WebRTC application configuration:

```c
app_webrtc_config_t config = WEBRTC_APP_CONFIG_DEFAULT();
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();
config.video_player = media_stream_get_video_player_if();
config.audio_player = media_stream_get_audio_player_if();
```

## Configuration

### WebRTC Application Configuration

The `app_webrtc_config_t` structure allows you to configure various aspects of the WebRTC application:

```c
// Default configuration macro
app_webrtc_config_t config = WEBRTC_APP_CONFIG_DEFAULT();

// Configure signaling interface and config
config.signaling_client_if = kvs_signaling_client_if_get();  // or your custom interface
config.signaling_cfg = &signalingConfig;  // Opaque pointer to signaling-specific config

// Configure WebRTC settings
config.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
config.logLevel = 3;
config.mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;

// Configure media interfaces
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();
config.video_player = media_stream_get_video_player_if();
config.audio_player = media_stream_get_audio_player_if();
config.receive_media = true;
```

### Core Configuration Fields

| Field | Type | Description |
|-------|------|-------------|
| `signaling_client_if` | `webrtc_signaling_client_if_t*` | Signaling client interface (KVS, Bridge, AppRTC, Custom) |
| `signaling_cfg` | `void*` | Opaque pointer to signaling-specific configuration |
| `role_type` | `webrtc_signaling_channel_role_type_t` | Role type (master or viewer) |
| `trickleIce` | `bool` | Whether to use trickle ICE |
| `useTurn` | `bool` | Whether to use TURN servers |
| `logLevel` | `uint32_t` | Log level (0-5) |
| `signaling_only` | `bool` | If true, disable media streaming components |
| `audioCodec` | `app_webrtc_rtc_codec_t` | Audio codec to use |
| `videoCodec` | `app_webrtc_rtc_codec_t` | Video codec to use |
| `mediaType` | `app_webrtc_streaming_media_t` | Media type (video-only or audio+video) |
| `video_capture` | `void*` | Video capture interface |
| `audio_capture` | `void*` | Audio capture interface |
| `video_player` | `void*` | Video player interface |
| `audio_player` | `void*` | Audio player interface |
| `receive_media` | `bool` | Whether to receive media |

### Signaling Role Types

```c
typedef enum {
    WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER = 0,  // Initiates connections
    WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,      // Receives connections
} webrtc_signaling_channel_role_type_t;
```

### Media Types

```c
typedef enum {
    APP_WEBRTC_MEDIA_VIDEO,        // Video only
    APP_WEBRTC_MEDIA_AUDIO_VIDEO,  // Both audio and video
} app_webrtc_streaming_media_t;
```

### Codec Types

```c
typedef enum {
    APP_WEBRTC_RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE = 1,
    APP_WEBRTC_RTC_CODEC_OPUS = 2,
    APP_WEBRTC_RTC_CODEC_VP8 = 3,
    APP_WEBRTC_RTC_CODEC_MULAW = 4,
    APP_WEBRTC_RTC_CODEC_ALAW = 5,
    APP_WEBRTC_RTC_CODEC_H265 = 7,
} app_webrtc_rtc_codec_t;
```

## Event Handling

The SDK provides an event-based system for handling WebRTC state changes:

```c
// Event callback type
typedef void (*app_webrtc_event_callback_t) (app_webrtc_event_data_t *event_data, void *user_ctx);

// Event data structure
typedef struct {
    app_webrtc_event_t event_id;
    UINT32 status_code;
    CHAR peer_id[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    CHAR message[256];
} app_webrtc_event_data_t;

// Example event handler
static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            // WebRTC stack initialized
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            // Signaling connection established
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            // Peer connection established
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            // Media streaming started
            break;
        // Handle other events...
    }
}
```

### Key Events

| Event | Description |
|-------|-------------|
| `APP_WEBRTC_EVENT_INITIALIZED` | WebRTC stack has been initialized |
| `APP_WEBRTC_EVENT_DEINITIALIZING` | WebRTC stack is being deinitialized |
| `APP_WEBRTC_EVENT_SIGNALING_CONNECTING` | Attempting to connect to signaling server |
| `APP_WEBRTC_EVENT_SIGNALING_CONNECTED` | Connected to signaling server |
| `APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED` | Disconnected from signaling server |
| `APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED` | Peer connection request received |
| `APP_WEBRTC_EVENT_PEER_CONNECTED` | Peer connection established |
| `APP_WEBRTC_EVENT_PEER_DISCONNECTED` | Peer has disconnected |
| `APP_WEBRTC_EVENT_STREAMING_STARTED` | Media streaming has started |
| `APP_WEBRTC_EVENT_STREAMING_STOPPED` | Media streaming has stopped |
| `APP_WEBRTC_EVENT_RECEIVED_OFFER` | Received SDP offer from peer |
| `APP_WEBRTC_EVENT_SENT_ANSWER` | Sent SDP answer to peer |
| `APP_WEBRTC_EVENT_ERROR` | General error occurred |
| `APP_WEBRTC_EVENT_SIGNALING_ERROR` | Error in signaling |
| `APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED` | Peer connection failed |

## Example Usage

### Classic Mode Example

```c
#include "app_webrtc.h"
#include "media_stream.h"

static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            ESP_LOGI(TAG, "WebRTC Initialized");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "Signaling Connected");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            ESP_LOGI(TAG, "Peer Connected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "Streaming Started");
            break;
        // Handle other events...
    }
}

void app_main(void)
{
    // Initialize NVS, WiFi, time sync, etc. (standard ESP-IDF setup)
    // ...

    // Initialize work queue (required for ICE operations)
    if (esp_work_queue_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }
    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    // Register the event callback *before* init to catch all events
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback");
    }

    // Get the media capture interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Set up KVS signaling configuration
    static kvs_signaling_config_t kvs_signaling_cfg = {0};
    kvs_signaling_cfg.pChannelName = "ScaryTestChannel";
    kvs_signaling_cfg.awsRegion = "us-east-1";
    kvs_signaling_cfg.caCertPath = "/spiffs/certs/cacert.pem";

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
    // Configure IoT Core credentials
    kvs_signaling_cfg.useIotCredentials = true;
    kvs_signaling_cfg.iotCoreCredentialEndpoint = CONFIG_AWS_IOT_CORE_CREDENTIAL_ENDPOINT;
    kvs_signaling_cfg.iotCoreCert = CONFIG_AWS_IOT_CORE_CERT;
    kvs_signaling_cfg.iotCorePrivateKey = CONFIG_AWS_IOT_CORE_PRIVATE_KEY;
    kvs_signaling_cfg.iotCoreRoleAlias = CONFIG_AWS_IOT_CORE_ROLE_ALIAS;
    kvs_signaling_cfg.iotCoreThingName = CONFIG_AWS_IOT_CORE_THING_NAME;
#else
    // Configure direct AWS credentials
    kvs_signaling_cfg.useIotCredentials = false;
    kvs_signaling_cfg.awsAccessKey = CONFIG_AWS_ACCESS_KEY_ID;
    kvs_signaling_cfg.awsSecretKey = CONFIG_AWS_SECRET_ACCESS_KEY;
    kvs_signaling_cfg.awsSessionToken = CONFIG_AWS_SESSION_TOKEN;
#endif

    // Configure WebRTC app with KVS signaling
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
    webrtcConfig.logLevel = 2;
    webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;

    // Set signaling interface and config (as opaque pointers)
    webrtcConfig.signaling_client_if = kvs_signaling_client_if_get();
    webrtcConfig.signaling_cfg = &kvs_signaling_cfg;

    // Set media interfaces
    webrtcConfig.video_capture = video_capture;
    webrtcConfig.audio_capture = audio_capture;
    webrtcConfig.video_player = NULL;
    webrtcConfig.audio_player = NULL;
    webrtcConfig.receive_media = false;  // Enable media reception

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application (blocking)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        app_webrtc_terminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }
}
```

### Split Mode - Streaming Only Example

```c
#include "app_webrtc.h"
#include "media_stream.h"
#include "webrtc_bridge.h"

void app_main(void)
{
    WEBRTC_STATUS status;

    // Initialize NVS, WiFi, etc. (standard ESP-IDF setup)
    // ...

    // Initialize signaling serializer
    signaling_serializer_init();

    // Initialize work queue with larger stack for streaming operations
    esp_work_queue_config_t work_queue_config = ESP_WORK_QUEUE_CONFIG_DEFAULT();
    work_queue_config.stack_size = 24 * 1024;

    if (esp_work_queue_init_with_config(&work_queue_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }
    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    // Register the event callback *before* init to catch all events
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback");
    }

    // Get the media capture interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Set up bridge signaling interface and config
    bridge_signaling_config_t bridge_config = {
        .client_id = "streaming_client",
        .log_level = 2
    };

    // Configure WebRTC with bridge signaling
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
    webrtcConfig.signaling_client_if = getBridgeSignalingClientInterface();
    webrtcConfig.signaling_cfg = &bridge_config;

    // Pass the media capture interfaces directly
    webrtcConfig.video_capture = video_capture;
    webrtcConfig.audio_capture = audio_capture;
    webrtcConfig.video_player = video_player;
    webrtcConfig.audio_player = audio_player;
    webrtcConfig.receive_media = true; // Enable media reception

    ESP_LOGI(TAG, "Initializing WebRTC application with bridge signaling");

    // Initialize WebRTC application
    status = app_webrtc_init(&webrtcConfig);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        goto CleanUp;
    }

    // Start webrtc bridge
    webrtc_bridge_start();

    ESP_LOGI(TAG, "Streaming example initialized, waiting for signaling messages");

    // Run WebRTC application
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        goto CleanUp;
    }

CleanUp:
    // Do not terminate the WebRTC application in streaming-only mode
    // Only streaming sessions are created and destroyed internally
    // app_webrtc_terminate();
}
```

### Split Mode - Signaling Only Example

```c
#include "app_webrtc.h"
#include "signaling_bridge_adapter.h"

void app_main(void)
{
    WEBRTC_STATUS status;

    // Initialize NVS, WiFi, etc. (standard ESP-IDF setup)
    // ...

    // Initialize work queue (required for ICE operations)
    if (esp_work_queue_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }
    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    ESP_LOGI(TAG, "Setting up WebRTC application with KVS signaling and app_webrtc state machine");

    // Set up KVS signaling configuration
    static kvs_signaling_config_t g_kvsSignalingConfig = {0};
    g_kvsSignalingConfig.pChannelName = "ScaryTestChannel";
    g_kvsSignalingConfig.awsRegion = CONFIG_AWS_DEFAULT_REGION;
    g_kvsSignalingConfig.caCertPath = "/spiffs/certs/cacert.pem";

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
    ESP_LOGI(TAG, "Using IoT Core credentials");
    g_kvsSignalingConfig.useIotCredentials = true;
    g_kvsSignalingConfig.iotCoreCredentialEndpoint = CONFIG_AWS_IOT_CORE_CREDENTIAL_ENDPOINT;
    g_kvsSignalingConfig.iotCoreCert = CONFIG_AWS_IOT_CORE_CERT;
    g_kvsSignalingConfig.iotCorePrivateKey = CONFIG_AWS_IOT_CORE_PRIVATE_KEY;
    g_kvsSignalingConfig.iotCoreRoleAlias = CONFIG_AWS_IOT_CORE_ROLE_ALIAS;
    g_kvsSignalingConfig.iotCoreThingName = CONFIG_AWS_IOT_CORE_THING_NAME;
#else
    ESP_LOGI(TAG, "Using static AWS credentials");
    g_kvsSignalingConfig.awsAccessKey = CONFIG_AWS_ACCESS_KEY_ID;
    g_kvsSignalingConfig.awsSecretKey = CONFIG_AWS_SECRET_ACCESS_KEY;
    g_kvsSignalingConfig.awsSessionToken = CONFIG_AWS_SESSION_TOKEN;
#endif

    // Register WebRTC event callback
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

    // Configure and initialize signaling bridge adapter (handles all bridge communication)
    signaling_bridge_adapter_config_t adapter_config = {
        .user_ctx = NULL
    };

    WEBRTC_STATUS adapter_status = signaling_bridge_adapter_init(&adapter_config);
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize signaling bridge adapter: 0x%08x", adapter_status);
        return;
    }

    // Start the signaling bridge adapter (starts bridge and handles all communication)
    adapter_status = signaling_bridge_adapter_start();
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start signaling bridge adapter: 0x%08x", adapter_status);
        return;
    }

    // Configure WebRTC application for signaling-only mode with split mode support
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

    // Configure signaling with KVS - pass the SAME config that was working
    webrtcConfig.signaling_client_if = kvs_signaling_client_if_get();
    webrtcConfig.signaling_cfg = &g_kvsSignalingConfig;

    // WebRTC configuration
    webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    webrtcConfig.trickleIce = true;
    webrtcConfig.useTurn = true;
    webrtcConfig.logLevel = 2;
    webrtcConfig.signalingOnly = true; // Disable media components

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    status = app_webrtc_init(&webrtcConfig);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    ESP_LOGI(TAG, "Starting WebRTC application");

    // Run WebRTC application (blocking)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        app_webrtc_terminate();
    }
}
```

### AppRTC Mode Example

```c
#include "app_webrtc.h"
#include "media_stream.h"

void app_main(void)
{
    WEBRTC_STATUS status;

    // Initialize NVS, WiFi, time sync, etc. (standard ESP-IDF setup)
    // ...

    // Initialize signaling serializer for message format conversion
    signaling_serializer_init();

    // Initialize work queue (required for ICE operations)
    if (esp_work_queue_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }
    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    // Register the WebRTC event callback to receive events from the WebRTC SDK
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register WebRTC event callback");
    }

    // Get the media capture interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Configure AppRTC signaling
    apprtc_signaling_config_t apprtc_config = {
        .serverUrl = NULL,  // Use default AppRTC server
        .roomId = NULL,     // Will be set based on role type
        .autoConnect = false,
        .connectionTimeout = 30000,
        .logLevel = 3
    };

    // Configure WebRTC app
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

    // Set signaling interface to AppRTC
    webrtcConfig.signaling_client_if = apprtc_signaling_client_if_get();
    webrtcConfig.signaling_cfg = &apprtcConfig;

    // Set role type based on configuration
#if CONFIG_APPRTC_ROLE_TYPE == 0
    // This mode can be used when you want to connect to the existing room.
    // Will then receive the offer from the other peer and send the answer.
    webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    ESP_LOGI(TAG, "Configured as MASTER role");
#else
    // In this mode, the application will send the offer and wait for the answer.
    webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    ESP_LOGI(TAG, "Configured as VIEWER role");
#endif

    // Pass the media capture interfaces
    webrtcConfig.video_capture = video_capture;
    webrtcConfig.audio_capture = audio_capture;
    webrtcConfig.video_player = video_player;
    webrtcConfig.audio_player = audio_player;
    webrtcConfig.mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
    webrtcConfig.receive_media = true;  // Enable media reception

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    status = app_webrtc_init(&webrtcConfig);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    // Start the WebRTC application (this will handle signaling connection)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start WebRTC application: 0x%08x", status);
        app_webrtc_terminate();
        return;
    }
}
```

### Custom Signaling Mode Example

You can implement your own signaling protocol by creating a custom `webrtc_signaling_client_if_t`. This allows integration with any signaling server or protocol.

```c
#include "app_webrtc.h"
#include "my_custom_signaling.h"

void app_main(void)
{
    // ... standard initialization ...

    // Configure your custom signaling
    my_custom_signaling_config_t customConfig = {
        .server_url = "wss://my-signaling-server.com",
        .room_id = "my-room",
        .client_id = "esp32-client",
        .connection_timeout = 30000,
        .log_level = 3
    };

    // Configure WebRTC with custom signaling
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
    webrtcConfig.signaling_client_if = my_custom_signaling_if_get();
    webrtcConfig.signaling_cfg = &customConfig;

    // ... configure media interfaces ...

    // Initialize and run
    app_webrtc_init(&webrtcConfig);
    app_webrtc_run();
}
```

📖 **For detailed guidance on implementing custom signaling, see [CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md)**

### ESP RainMaker Integration

The SDK can be integrated with ESP RainMaker for device management and cloud connectivity. The `kvs_webrtc_camera` example in the ESP RainMaker repository demonstrates this integration.

#### Overview

- ESP RainMaker provides device management, connectivity, and a mobile app for controlling the camera
- The device uses RainMaker's secure authentication to obtain AWS credentials for KVS
- Users can view the camera stream directly in the ESP RainMaker mobile app

#### Key Components

1. **RainMaker Node Setup**: Create a RainMaker node with a camera device
2. **Secure Authentication**: Use RainMaker's certificate-based authentication for AWS IoT credentials
3. **Channel Naming**: Use the RainMaker node ID to create a unique KVS channel name
4. **Mobile App Integration**: Stream video directly to the ESP RainMaker mobile app

#### RainMaker Example

```c
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include "app_webrtc.h"
#include "media_stream.h"

// Initialize RainMaker with a camera device
static esp_err_t app_rainmaker_init(void)
{
    esp_rmaker_config_t config = {
        .enable_time_sync = false,
    };

    // Initialize RainMaker node
    esp_rmaker_node_t *node = esp_rmaker_node_init(&config, "ESP_WebRTC", "esp_webrtc");
    if (!node) {
        ESP_LOGE(TAG, "Failed to initialize RainMaker node");
        return ESP_FAIL;
    }

    // Create a camera device
    esp_rmaker_device_t *device = esp_rmaker_device_create("WebRTC_Device", "esp.device.camera", NULL);
    if (!device) {
        ESP_LOGE(TAG, "Failed to create device");
        return ESP_FAIL;
    }

    // Add the device to the node
    if (esp_rmaker_node_add_device(node, device) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to node");
        return ESP_FAIL;
    }

    // Create a channel parameter using the node ID
    const char *node_id = esp_rmaker_get_node_id();
    char channel_name[32] = {0};
    snprintf(channel_name, sizeof(channel_name), "esp-v1-%s", node_id);

    // Add the channel parameter to the device
    esp_rmaker_param_t *channel_param = esp_rmaker_param_create(
        "Channel", "esp.param.channel", esp_rmaker_str(channel_name), PROP_FLAG_READ);
    esp_rmaker_device_add_param(device, channel_param);

    // Start RainMaker
    if (esp_rmaker_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RainMaker");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// WebRTC initialization with RainMaker credentials
static void webrtc_main_start(void)
{
    // Get the node ID from RainMaker
    const char *node_id = esp_rmaker_get_node_id();
    static char channel_name[32] = {0};
    snprintf(channel_name, sizeof(channel_name), "esp-v1-%s", node_id);

    // Create paths to RainMaker certificates in NVS
    static char cert_path[80];
    static char key_path[80];
    snprintf(cert_path, sizeof(cert_path), "/nvs/%s/%s/%s",
             CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME,
             CONFIG_ESP_RMAKER_FACTORY_NAMESPACE,
             "client_cert");
    snprintf(key_path, sizeof(key_path), "/nvs/%s/%s/%s",
             CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME,
             CONFIG_ESP_RMAKER_FACTORY_NAMESPACE,
             "client_key");

    // Configure WebRTC with RainMaker credentials
    app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
    webrtcConfig.iotCoreCredentialEndpoint = "your-credential-endpoint.amazonaws.com"; // will be read from NVS
    webrtcConfig.awsRegion = "your-aws-region"; // Will be decoded from the token
    webrtcConfig.iotCoreCert = cert_path;
    webrtcConfig.iotCorePrivateKey = key_path;
    webrtcConfig.caCertPath = "/spiffs/certs/cacert.pem"; // Use cert bundle instead. Already using for http requests, use for websocket as well
    webrtcConfig.iotCoreRoleAlias = "esp-videostream-v1-NodeRole";
    webrtcConfig.iotCoreThingName = node_id;
    webrtcConfig.pChannelName = channel_name;

    // Get the media interfaces
    webrtcConfig.video_capture = media_stream_get_video_capture_if();
    webrtcConfig.audio_capture = media_stream_get_audio_capture_if();
    webrtcConfig.video_player = media_stream_get_video_player_if();
    webrtcConfig.audio_player = media_stream_get_audio_player_if();

    // Initialize and start WebRTC
    app_webrtc_init(&webrtcConfig);
    app_webrtc_run();
}

void app_main(void)
{
    // Initialize NVS, network, etc.

    // Initialize RainMaker
    app_rainmaker_init();

    // Start the network (provisioning or connection)
    app_network_start(POP_TYPE_RANDOM);

    // Synchronize time with SNTP
    esp_webrtc_time_sntp_time_sync_no_wait();

    // Start WebRTC in a separate task
    webrtc_main_start();
}
```

#### Required Configuration

To use RainMaker with KVS WebRTC, you need to:

1. **Register certificates** with video streaming capability:
   ```bash
   python rainmaker_admin_cli.py certs devicecert register --inputfile <csvfile> --node_policies videostream
   ```

2. **Flash the factory partition** with the certificate:
   ```bash
   esptool.py -p <PORT> write_flash 0x3FA000 node-<number-NodeID>.bin
   ```

3. **Provision the device** using the ESP RainMaker mobile app

4. **Update partition table** to include the factory partition:
   ```csv
   fctry,    data, nvs,     0x3FA000,  0x6000
   ```

#### Benefits of RainMaker Integration

- **Simplified Authentication**: Uses RainMaker's existing secure authentication system
- **Mobile App Integration**: Built-in video streaming in the ESP RainMaker app
- **Device Management**: Remote monitoring, updates, and control
- **User Management**: Easy user association and access control
- **OTA Updates**: Simplified firmware updates through RainMaker

## Media Interface Implementation

The SDK provides default implementations for media capture and playback, but you can also implement your own interfaces if needed:

### Video Capture Interface

```c
typedef struct {
    esp_err_t (*init)(video_capture_config_t *config, video_capture_handle_t *handle);
    esp_err_t (*start)(video_capture_handle_t handle);
    esp_err_t (*stop)(video_capture_handle_t handle);
    esp_err_t (*get_frame)(video_capture_handle_t handle, video_frame_t **frame, uint32_t timeout_ms);
    esp_err_t (*release_frame)(video_capture_handle_t handle, video_frame_t *frame);
    esp_err_t (*deinit)(video_capture_handle_t handle);
} media_stream_video_capture_t;
```

### Audio Capture Interface

```c
typedef struct {
    esp_err_t (*init)(audio_capture_config_t *config, audio_capture_handle_t *handle);
    esp_err_t (*start)(audio_capture_handle_t handle);
    esp_err_t (*stop)(audio_capture_handle_t handle);
    esp_err_t (*get_frame)(audio_capture_handle_t handle, audio_frame_t **frame, uint32_t timeout_ms);
    esp_err_t (*release_frame)(audio_capture_handle_t handle, audio_frame_t *frame);
    esp_err_t (*deinit)(audio_capture_handle_t handle);
} media_stream_audio_capture_t;
```

### Video Player Interface

```c
typedef struct {
    esp_err_t (*init)(video_player_config_t *config, video_player_handle_t *handle);
    esp_err_t (*start)(video_player_handle_t handle);
    esp_err_t (*stop)(video_player_handle_t handle);
    esp_err_t (*play_frame)(video_player_handle_t handle, const uint8_t *data,
                            uint32_t len, bool is_keyframe);
    esp_err_t (*deinit)(video_player_handle_t handle);
} media_stream_video_player_t;
```

### Audio Player Interface

```c
typedef struct {
    esp_err_t (*init)(audio_player_config_t *config, audio_player_handle_t *handle);
    esp_err_t (*start)(audio_player_handle_t handle);
    esp_err_t (*stop)(audio_player_handle_t handle);
    esp_err_t (*play_frame)(audio_player_handle_t handle, const uint8_t *data, uint32_t len);
    esp_err_t (*deinit)(audio_player_handle_t handle);
} media_stream_audio_player_t;
```

## Troubleshooting

- **Connection Issues**: Check Wi-Fi connectivity and AWS credentials
- **Signaling Failures**: Verify AWS region and KVS channel configuration
- **Media Issues**: Check camera and microphone hardware connections
- **Performance Problems**: Consider reducing video resolution or frame rate
- **Stack Overflow**: Ensure work queue is initialized before WebRTC operations
- **ICE Server Issues**: Check TURN server configuration and network connectivity
- **Custom Signaling**: See [CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md) for troubleshooting custom implementations
- **Debugging**: Enable verbose logging by setting `logLevel` to a higher value

## Additional Resources

- **[CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md)**: Comprehensive guide for implementing custom signaling protocols
- **Example Applications**: See `esp_port/examples/` for complete working implementations
- **AWS KVS WebRTC Documentation**: Refer to AWS documentation for more details on the underlying SDK
- **Component Documentation**: Individual component READMEs in `esp_port/components/`
