# Amazon Kinesis Video Streams WebRTC SDK for ESP - API Usage Guide

This document provides guidance on using the Amazon Kinesis Video Streams WebRTC SDK APIs for ESP platforms.

## API Overview

The ESP port of the KVS WebRTC SDK provides a **simplified high-level API** through `app_webrtc.h` that makes WebRTC integration with ESP devices straightforward and intuitive. The new API features:

### 🚀 **New Simplified API Design**
- **Minimal Configuration**: Only 4 essential fields needed for most use cases
- **Smart Defaults**: Reasonable defaults for role, codecs, ICE settings, and logging
- **Auto-Detection**: Automatically detects signaling-only vs streaming mode
- **Advanced APIs**: Override any default with dedicated configuration functions
- **Pluggable Architecture**: Modular signaling and peer connection interfaces

### 🏗️ **Pluggable Architecture**
The SDK uses a modular architecture with interchangeable components:
- **Signaling Interfaces**: `webrtc_signaling_client_if_t` (KVS, AppRTC, Bridge, Custom)
- **Peer Connection Interfaces**: `webrtc_peer_connection_if_t` (KVS, Bridge, Custom)
- **Media Interfaces**: Standardized capture/player interfaces for audio/video

## Deployment Modes

The SDK supports multiple deployment modes using the new pluggable architecture:

### 1. Classic Mode (Single Device)
**Architecture**: `kvs_signaling + kvs_peer_connection`

Both signaling and media streaming handled by one ESP device using AWS KVS.

```c
// Set up KVS signaling configuration
static kvs_signaling_config_t kvs_signaling_cfg = {
    .pChannelName = "ScaryTestChannel",
    .useIotCredentials = true,  // or false for direct AWS credentials
    .awsRegion = "us-east-1",
    .caCertPath = "/spiffs/certs/cacert.pem",
    // IoT credentials or direct AWS credentials...
};

// Configure WebRTC with simplified API
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &kvs_signaling_cfg;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();

app_webrtc_init(&config);
app_webrtc_run();
```

**Example**: `webrtc_classic`

### 2. AppRTC Mode (Browser Compatible)
**Architecture**: `apprtc_signaling + kvs_peer_connection`

Uses AppRTC-compatible signaling for direct browser integration.

```c
// Configure AppRTC signaling (browser-compatible)
apprtc_signaling_config_t apprtc_config = {
    .serverUrl = NULL,      // Use default AppRTC server
    .roomId = NULL,         // Will be set based on role type
    .autoConnect = false,
    .connectionTimeout = 30000,
    .logLevel = 3
};

// Configure WebRTC app with simplified API
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = apprtc_signaling_client_if_get();
config.signaling_cfg = &apprtc_config;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();

// Advanced configuration: Set role and enable bidirectional media
app_webrtc_init(&config);
app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_MASTER);
app_webrtc_enable_media_reception(true);
app_webrtc_run();
```

**Example**: `esp_camera`

### 3. Split Mode - Streaming Device
**Architecture**: `bridge_signaling + kvs_peer_connection`

Handles media streaming while receiving signaling from partner device.

```c
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = getBridgeSignalingClientInterface();
config.signaling_cfg = &bridge_config;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = media_stream_get_video_capture_if();
```

**Example**: `streaming_only`

### 4. Split Mode - Signaling Device
**Architecture**: `kvs_signaling + bridge_peer_connection`

Handles AWS KVS signaling and forwards to streaming device via bridge.

```c
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &kvs_signaling_cfg;
config.peer_connection_if = bridge_peer_connection_if_get();
// No media interfaces = auto-detected signaling-only mode
```

**Example**: `signaling_only`

### 5. Custom Credentials Mode (ESP RainMaker Integration)

Use a **credential callback function** for dynamic credential provisioning. This is the **recommended approach** for ESP RainMaker integration and other systems that provide AWS credentials at runtime.

```c
// Credential callback implementation (e.g., for ESP RainMaker)
int rmaker_fetch_aws_credentials(uint64_t user_data,
                                const char **pAK, uint32_t *pAKLen,
                                const char **pSK, uint32_t *pSKLen,
                                const char **pTok, uint32_t *pTokLen,
                                uint64_t *pExp)
{
    // Use ESP RainMaker's streamlined credential API
    esp_rmaker_aws_credentials_t *credentials = esp_rmaker_get_aws_security_token("esp-videostream-v1-NodeRole");
    if (!credentials) {
        return -1;
    }

    // Set output pointers to credential data
    *pAK = credentials->access_key;
    *pAKLen = credentials->access_key_len;
    *pSK = credentials->secret_key;
    *pSKLen = credentials->secret_key_len;
    *pTok = credentials->session_token;
    *pTokLen = credentials->session_token_len;
    *pExp = credentials->expiration * HUNDREDS_OF_NANOS_IN_A_SECOND;  // Convert to 100ns units

    return 0;  // Success
}

// Configure KVS signaling with credential callback
static kvs_signaling_config_t kvs_signaling_cfg = {
    .pChannelName = "esp-v1-<node-id>",
    .awsRegion = "us-east-1",  // Or use esp_rmaker_get_aws_region()
    .caCertPath = "/spiffs/certs/cacert.pem",

    // Credential callback has highest precedence
    .fetch_credentials_cb = rmaker_fetch_aws_credentials,
    .fetch_credentials_user_data = 0,  // Optional user data
};

// Standard WebRTC configuration
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &kvs_signaling_cfg;
config.peer_connection_if = kvs_peer_connection_if_get();
// ... add media interfaces ...

app_webrtc_init(&config);
app_webrtc_run();
```

**Benefits of Credential Callbacks:**
- ✅ **Dynamic credentials**: Fresh tokens fetched on-demand
- ✅ **Automatic renewal**: Handles expiration transparently
- ✅ **Memory optimization**: External RAM allocation with proper cleanup
- ✅ **Integration ready**: Perfect for ESP RainMaker, custom auth systems

### 6. Custom Signaling Mode

Implement your own signaling by creating custom `webrtc_signaling_client_if_t` and/or `webrtc_peer_connection_if_t`. See [CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md) for implementation guidance.

## Key API Functions

### 🏗️ Core Application API (New Simplified API)

```c
// Essential configuration (minimal setup)
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();     // Required: signaling interface
config.signaling_cfg = &kvs_signaling_cfg;                     // Required: signaling config
config.peer_connection_if = kvs_peer_connection_if_get();       // Required: peer connection interface

// Optional: Media interfaces (set to NULL for signaling-only applications)
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();
config.video_player = media_stream_get_video_player_if();       // For receiving video
config.audio_player = media_stream_get_audio_player_if();       // For receiving audio

// Initialize with smart defaults
WEBRTC_STATUS app_webrtc_init(app_webrtc_config_t *config);

// Run the WebRTC application (blocking)
WEBRTC_STATUS app_webrtc_run(void);

// Clean termination
WEBRTC_STATUS app_webrtc_terminate(void);

// Event notifications
int32_t app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx);
```

### 🔧 Advanced Configuration APIs (Override Defaults)

```c
// Role configuration (default: MASTER)
WEBRTC_STATUS app_webrtc_set_role(webrtc_channel_role_type_t role);

// ICE configuration (default: trickle ICE + TURN enabled)
WEBRTC_STATUS app_webrtc_set_ice_config(bool trickle_ice, bool use_turn);

// Logging (default: INFO level)
WEBRTC_STATUS app_webrtc_set_log_level(uint32_t level);

// Codec selection (default: OPUS + H.264)
WEBRTC_STATUS app_webrtc_set_codecs(app_webrtc_rtc_codec_t audio_codec, app_webrtc_rtc_codec_t video_codec);

// Media type (default: auto-detected)
WEBRTC_STATUS app_webrtc_set_media_type(app_webrtc_streaming_media_t media_type);

// Media reception (default: disabled for IoT devices)
WEBRTC_STATUS app_webrtc_enable_media_reception(bool enable);

// Force signaling-only mode (default: auto-detected)
WEBRTC_STATUS app_webrtc_set_signaling_only_mode(bool enable);
```

## 📋 Configuration Structures

### KVS Signaling Configuration
```c
typedef struct {
    // Channel configuration
    char *pChannelName;                      // Required: KVS channel name

    // AWS credentials (choose one approach)
    bool useIotCredentials;                  // true=IoT Core, false=direct AWS

    // IoT Core credentials (when useIotCredentials=true)
    char *iotCoreCredentialEndpoint;         // IoT credential endpoint URL
    char *iotCoreCert;                       // Path to device certificate
    char *iotCorePrivateKey;                 // Path to private key
    char *iotCoreRoleAlias;                  // Role alias for credential exchange
    char *iotCoreThingName;                  // IoT thing name

    // Direct AWS credentials (when useIotCredentials=false)
    char *awsAccessKey;                      // AWS access key ID
    char *awsSecretKey;                      // AWS secret access key
    char *awsSessionToken;                   // Session token (optional)

    // Credential callback (highest precedence - ESP RainMaker integration)
    kvs_fetch_credentials_cb_t fetch_credentials_cb;     // Callback function
    uint64_t fetch_credentials_user_data;               // User data for callback

    // Common AWS options
    char *awsRegion;                         // AWS region (e.g., "us-east-1")
    char *caCertPath;                        // CA certificate bundle path
} kvs_signaling_config_t;
```

### AppRTC Signaling Configuration
```c
typedef struct {
    char *serverUrl;                         // AppRTC server URL (NULL=default)
    char *roomId;                           // Room ID (NULL=auto-generated)
    bool autoConnect;                       // Auto-connect on init
    uint32_t connectionTimeout;            // Connection timeout (ms)
    uint32_t logLevel;                      // Logging level
} apprtc_signaling_config_t;
```

### Credential Callback Signature
```c
// Credential provider callback for dynamic credential fetching
typedef int (*kvs_fetch_credentials_cb_t)(
    uint64_t customData,                     // User data from fetch_credentials_user_data
    const char **pAccessKey,                // Output: AWS access key
    uint32_t *pAccessKeyLen,                // Output: Access key length
    const char **pSecretKey,                // Output: AWS secret key
    uint32_t *pSecretKeyLen,                // Output: Secret key length
    const char **pSessionToken,             // Output: Session token
    uint32_t *pSessionTokenLen,             // Output: Session token length
    uint64_t *pExpiration                   // Output: Expiration (100ns units)
);
```

### 🌉 Split Mode & Bridge Functions

```c
// Register callback for bridge message forwarding
int app_webrtc_register_msg_callback(app_webrtc_send_msg_cb_t callback);

// Send message from bridge to signaling server
int app_webrtc_send_msg_to_signaling(webrtc_message_t *message);

// Trigger offer creation (for initiator role)
int app_webrtc_trigger_offer(char *pPeerId);
```

### 🧊 ICE Server Management

```c
// Get ICE servers from signaling interface
WEBRTC_STATUS app_webrtc_get_ice_servers(uint32_t *pIceServerCount, void *pIceConfiguration);

// Query specific ICE server (for bridge/RPC patterns)
WEBRTC_STATUS app_webrtc_get_server_by_idx(int index, bool useTurn, uint8_t **data, int *len, bool *have_more);

// Check if ICE refresh needed
WEBRTC_STATUS app_webrtc_is_ice_refresh_needed(bool *refreshNeeded);

// Trigger background ICE refresh
WEBRTC_STATUS app_webrtc_refresh_ice_configuration(void);

// Update ICE servers during runtime
WEBRTC_STATUS app_webrtc_update_ice_servers(void);
```

### 📡 Data Channel Functions

```c
// Set data channel callbacks for peer
WEBRTC_STATUS app_webrtc_set_data_channel_callbacks(const char *peer_id,
                                                    app_webrtc_rtc_on_open_t onOpen,
                                                    app_webrtc_rtc_on_message_t onMessage,
                                                    uint64_t custom_data);

// Send data through data channel
WEBRTC_STATUS app_webrtc_send_data_channel_message(const char *peer_id,
                                                   void *pDataChannel,
                                                   bool isBinary,
                                                   const uint8_t *pMessage,
                                                   uint32_t messageLen);
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

### 🚀 Simplified WebRTC Configuration

The new simplified API focuses on essential configuration while providing intelligent defaults:

#### Essential Configuration (Required)
```c
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

// Essential: Signaling interface
config.signaling_client_if = kvs_signaling_client_if_get();    // Choose your signaling
config.signaling_cfg = &kvs_signaling_cfg;                   // Signaling-specific config

// Essential: Peer connection interface
config.peer_connection_if = kvs_peer_connection_if_get();     // Choose your peer connection
config.implementation_config = NULL;                         // Implementation-specific config (optional)

// Optional: Media interfaces (NULL = signaling-only mode)
config.video_capture = media_stream_get_video_capture_if();
config.audio_capture = media_stream_get_audio_capture_if();
config.video_player = media_stream_get_video_player_if();
config.audio_player = media_stream_get_audio_player_if();
```

#### Smart Defaults (No Configuration Needed)
The API automatically provides reasonable defaults:
- **Role**: `WEBRTC_CHANNEL_ROLE_TYPE_MASTER` (initiates connections)
- **ICE**: Trickle ICE enabled, TURN servers enabled
- **Codecs**: OPUS (audio), H.264 (video)
- **Logging**: INFO level
- **Media Reception**: Disabled (most IoT devices are senders)
- **Mode**: Auto-detected (from interfaces provided)

#### Advanced Configuration (Override Defaults)
```c
// After app_webrtc_init(), override any defaults:
app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);        // Change role
app_webrtc_set_ice_config(false, true);                     // Disable trickle ICE
app_webrtc_set_log_level(2);                                 // Enable DEBUG logging
app_webrtc_enable_media_reception(true);                    // Enable media reception
app_webrtc_set_codecs(APP_WEBRTC_CODEC_OPUS, APP_WEBRTC_CODEC_VP8);  // Use VP8
```

### Core Configuration Fields

| Field | Type | Description |
|-------|------|-------------|
| `signaling_client_if` | `webrtc_signaling_client_if_t*` | **Required**: Signaling interface (KVS, AppRTC, Bridge, Custom) |
| `signaling_cfg` | `void*` | **Required**: Opaque pointer to signaling-specific configuration |
| `peer_connection_if` | `webrtc_peer_connection_if_t*` | **Required**: Peer connection interface (KVS, Bridge, Custom) |
| `implementation_config` | `void*` | Optional: Implementation-specific configuration |
| `video_capture` | `void*` | Optional: Video capture interface (NULL = no video) |
| `audio_capture` | `void*` | Optional: Audio capture interface (NULL = no audio) |
| `video_player` | `void*` | Optional: Video player interface (NULL = no video reception) |
| `audio_player` | `void*` | Optional: Audio player interface (NULL = no audio reception) |

### Available Interfaces

#### Signaling Interfaces
```c
// AWS KVS signaling (full AWS integration)
webrtc_signaling_client_if_t* kvs_signaling_client_if_get(void);

// AppRTC signaling (browser-compatible)
webrtc_signaling_client_if_t* apprtc_signaling_client_if_get(void);

// Bridge signaling (for split mode streaming device)
webrtc_signaling_client_if_t* getBridgeSignalingClientInterface(void);
```

#### Peer Connection Interfaces
```c
// KVS peer connection (full WebRTC functionality)
webrtc_peer_connection_if_t* kvs_peer_connection_if_get(void);

// Bridge peer connection (signaling-only, no WebRTC SDK)
webrtc_peer_connection_if_t* bridge_peer_connection_if_get(void);
```

#### Media Interfaces
```c
// Get default media capture and player interfaces
media_stream_video_capture_t* media_stream_get_video_capture_if(void);
media_stream_audio_capture_t* media_stream_get_audio_capture_if(void);
media_stream_video_player_t* media_stream_get_video_player_if(void);
media_stream_audio_player_t* media_stream_get_audio_player_if(void);
```

### Signaling Role Types

```c
typedef enum {
    WEBRTC_CHANNEL_ROLE_TYPE_MASTER = 0,  // Initiates connections (default)
    WEBRTC_CHANNEL_ROLE_TYPE_VIEWER,      // Receives connections
} webrtc_channel_role_type_t;
```

### Media Types

```c
typedef enum {
    APP_WEBRTC_MEDIA_VIDEO,        // Video only
    APP_WEBRTC_MEDIA_AUDIO_VIDEO,  // Both audio and video (default when both interfaces provided)
} app_webrtc_streaming_media_t;
```

### Codec Types

```c
typedef enum {
    APP_WEBRTC_CODEC_H264 = 1,     // H.264 video codec (default)
    APP_WEBRTC_CODEC_OPUS = 2,     // OPUS audio codec (default)
    APP_WEBRTC_CODEC_VP8 = 3,      // VP8 video codec
    APP_WEBRTC_CODEC_MULAW = 4,    // MULAW audio codec
    APP_WEBRTC_CODEC_ALAW = 5,     // ALAW audio codec
    APP_WEBRTC_CODEC_H265 = 7,     // H.265 video codec
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

### Classic Mode Example (New Simplified API)

```c
#include "app_webrtc.h"
#include "kvs_signaling.h"
#include "kvs_peer_connection.h"
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

    // Register the event callback *before* init to catch all events
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

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

    // Configure WebRTC app with simplified API
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration - what you MUST provide
    config.signaling_client_if = kvs_signaling_client_if_get();
    config.signaling_cfg = &kvs_signaling_cfg;
    config.peer_connection_if = kvs_peer_connection_if_get();

    // Media interfaces for streaming
    config.video_capture = video_capture;
    config.audio_capture = audio_capture;
    config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC with simplified API");
    ESP_LOGI(TAG, "Smart defaults: MASTER role, H.264+OPUS codecs, trickle ICE");

    // Initialize WebRTC application with smart defaults
    WEBRTC_STATUS status = app_webrtc_init(&config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    // Advanced configuration: Override defaults if needed
    app_webrtc_enable_media_reception(true);  // Enable receiving media

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application (blocking)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        app_webrtc_terminate();
    }
}
```

### Split Mode - Streaming Only Example (New Simplified API)

```c
#include "app_webrtc.h"
#include "webrtc_bridge_signaling.h"
#include "kvs_peer_connection.h"
#include "media_stream.h"
#include "webrtc_bridge.h"

void app_main(void)
{
    // Initialize NVS, WiFi, etc. (standard ESP-IDF setup)
    // ...

    // Initialize signaling serializer
    signaling_serializer_init();

    // Register the event callback *before* init to catch all events
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

    // Get the media capture interfaces for bi-directional streaming
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    // Set up bridge signaling config
    bridge_signaling_config_t bridge_config = {
        .client_id = "streaming_client",
        .log_level = 2
    };

    // Configure WebRTC with simplified API - streaming mode
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration
    config.signaling_client_if = getBridgeSignalingClientInterface();
    config.signaling_cfg = &bridge_config;
    config.peer_connection_if = kvs_peer_connection_if_get();  // Full WebRTC

    // Media interfaces for bi-directional streaming
    config.video_capture = video_capture;
    config.audio_capture = audio_capture;
    config.video_player = video_player;
    config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC streaming device with simplified API");
    ESP_LOGI(TAG, "Mode: streaming-only (receives signaling via bridge)");

    // Initialize WebRTC application with auto-detected streaming mode
    WEBRTC_STATUS status = app_webrtc_init(&config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    // Enable media reception for bi-directional streaming
    app_webrtc_enable_media_reception(true);

    // Start webrtc bridge for communication with signaling device
    webrtc_bridge_start();

    ESP_LOGI(TAG, "Streaming device ready, waiting for signaling messages");

    // Run WebRTC application
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
    }
}
```

### Split Mode - Signaling Only Example (New Simplified API)

```c
#include "app_webrtc.h"
#include "kvs_signaling.h"
#include "bridge_peer_connection.h"

void app_main(void)
{
    // Initialize NVS, WiFi, etc. (standard ESP-IDF setup)
    // ...

    ESP_LOGI(TAG, "Setting up WebRTC signaling device with simplified API");

    // Register WebRTC event callback
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

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

    // Configure WebRTC with simplified API - signaling-only mode
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration
    config.signaling_client_if = kvs_signaling_client_if_get();
    config.signaling_cfg = &g_kvsSignalingConfig;
    config.peer_connection_if = bridge_peer_connection_if_get(); // Signaling-only

    // No media interfaces = auto-detected signaling-only mode
    // The API automatically optimizes for memory and power efficiency

    ESP_LOGI(TAG, "Initializing WebRTC with bridge peer connection interface");
    ESP_LOGI(TAG, "Mode: signaling-only (auto-detected, no media interfaces)");

    // Initialize WebRTC application
    WEBRTC_STATUS status = app_webrtc_init(&config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    ESP_LOGI(TAG, "Starting signaling-only WebRTC application");
    ESP_LOGI(TAG, "Ready to forward signaling messages to/from streaming device");

    // Run WebRTC application (blocking)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        app_webrtc_terminate();
    }
}
```

### AppRTC Mode Example (New Simplified API)

```c
#include "app_webrtc.h"
#include "apprtc_signaling.h"
#include "kvs_peer_connection.h"
#include "media_stream.h"

void app_main(void)
{
    WEBRTC_STATUS status;

    // Initialize NVS, WiFi, time sync, etc. (standard ESP-IDF setup)
    // ...

    // Initialize signaling serializer for message format conversion
    signaling_serializer_init();

    // Register the WebRTC event callback to receive events from the WebRTC SDK
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

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

    // Configure WebRTC app with simplified API
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration
    config.signaling_client_if = apprtc_signaling_client_if_get();
    config.signaling_cfg = &apprtc_config;
    config.peer_connection_if = kvs_peer_connection_if_get();

    // Media interfaces for bi-directional streaming
    config.video_capture = video_capture;
    config.audio_capture = audio_capture;
    config.video_player = video_player;
    config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC with AppRTC signaling + simplified API");
    ESP_LOGI(TAG, "Smart defaults: H.264+OPUS, trickle ICE, TURN enabled");

    // Initialize WebRTC application with smart defaults
    status = app_webrtc_init(&config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08x", status);
        return;
    }

    // Advanced configuration: Set role based on configuration
#if CONFIG_APPRTC_ROLE_TYPE == 0
    app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_MASTER);
    ESP_LOGI(TAG, "Configured as MASTER role using advanced API");
#else
    app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);
    ESP_LOGI(TAG, "Configured as VIEWER role using advanced API");
#endif

    // Enable media reception for bi-directional streaming
    app_webrtc_enable_media_reception(true);

    // Start the WebRTC application (this will handle signaling connection)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start WebRTC application: 0x%08x", status);
        app_webrtc_terminate();
        return;
    }

    ESP_LOGI(TAG, "WebRTC ready for CLI commands (join-room, status, etc.)");
}
```

### Custom Signaling Mode Example (New Simplified API)

You can implement your own signaling protocol using the pluggable architecture:

```c
#include "app_webrtc.h"
#include "my_custom_signaling.h"
#include "kvs_peer_connection.h"  // or your custom peer connection

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

    // Configure WebRTC with simplified API + custom signaling
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
    config.signaling_client_if = my_custom_signaling_if_get();
    config.signaling_cfg = &customConfig;
    config.peer_connection_if = kvs_peer_connection_if_get();  // Or custom peer connection
    config.video_capture = media_stream_get_video_capture_if();

    // Initialize with smart defaults
    app_webrtc_init(&config);
    app_webrtc_run();
}
```

📖 **For detailed guidance on implementing custom signaling, see [CUSTOM_SIGNALING.md](CUSTOM_SIGNALING.md)**

### ESP RainMaker Integration

The SDK can be integrated with ESP RainMaker for device management and cloud connectivity. The `kvs_webrtc_camera` example in the ESP RainMaker repository demonstrates this integration.

#### Overview

- ESP RainMaker provides device management, connectivity, and a mobile app for controlling the camera
- The device uses RainMaker's **streamlined AWS credentials API** to obtain temporary security tokens for KVS
- Users can view the camera stream directly in the ESP RainMaker mobile app
- **New**: Centralized credential management with simplified error handling and optimized memory usage

#### Key Components

1. **RainMaker Node Setup**: Create a RainMaker node with a camera device (automatically includes name and channel parameters)
2. **Secure Authentication**: Use RainMaker's **new `esp_rmaker_get_aws_security_token()`** function for simplified AWS credential retrieval
3. **Channel Naming**: Use the RainMaker node ID to create a unique KVS channel name via `esp_rmaker_get_aws_region()`
4. **Mobile App Integration**: Stream video directly to the ESP RainMaker mobile app with improved reliability
5. **Memory Optimization**: Credentials are allocated from external RAM and properly cleaned up to prevent memory issues

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

    // Create a camera device (automatically includes name and channel parameters)
    esp_rmaker_device_t *device = esp_rmaker_camera_device_create("WebRTC_Camera", NULL);
    if (!device) {
        ESP_LOGE(TAG, "Failed to create camera device");
        return ESP_FAIL;
    }

    // Add the device to the node
    if (esp_rmaker_node_add_device(node, device) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to node");
        return ESP_FAIL;
    }

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
    // Get AWS region using the new simplified API
    char *aws_region = esp_rmaker_get_aws_region();
    if (!aws_region) {
        ESP_LOGE(TAG, "Failed to get AWS region");
        return;
    }

    // Get the node ID from RainMaker for channel naming
    const char *node_id = esp_rmaker_get_node_id();
    static char channel_name[32] = {0};
    snprintf(channel_name, sizeof(channel_name), "esp-v1-%s", node_id);

    // Configure KVS signaling with simplified credential provider
    kvs_signaling_config_t kvs_config = {
        .awsRegion = aws_region,
        .pChannelName = channel_name,
        .useIotCredentials = true,
        .iotCoreRoleAlias = "esp-videostream-v1-NodeRole",
        .iotCoreThingName = node_id,
        // Credentials are now automatically fetched using esp_rmaker_get_aws_security_token()
    };

    // Configure WebRTC with new simplified API
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
    config.signaling_client_if = kvs_signaling_client_if_get();
    config.signaling_cfg = &kvs_config;
    config.peer_connection_if = kvs_peer_connection_if_get();

    // Get the media interfaces
    config.video_capture = media_stream_get_video_capture_if();
    config.audio_capture = media_stream_get_audio_capture_if();
    config.video_player = media_stream_get_video_player_if();
    config.audio_player = media_stream_get_audio_player_if();

    // Initialize and start WebRTC
    app_webrtc_init(&config);
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
