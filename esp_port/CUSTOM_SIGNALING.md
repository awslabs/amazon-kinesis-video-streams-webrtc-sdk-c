# Custom Signaling Integration Guide

This guide explains how to integrate custom signaling solutions with the ESP-IDF WebRTC SDK. The SDK provides a **pluggable architecture** with flexible signaling and peer connection abstraction layers that allow you to implement various signaling backends and peer connection strategies while maintaining a consistent, simplified API.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Signaling Interface Definition](#signaling-interface-definition)
- [Implementation Steps](#implementation-steps)
- [Configuration and Initialization](#configuration-and-initialization)
- [Existing Examples](#existing-examples)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)

## Architecture Overview

The WebRTC SDK uses a **pluggable architecture** that separates signaling, peer connections, and media handling into interchangeable components:

```
┌─────────────────────────────────────────────────────────────┐
│                Application Layer (Your Code)                │
├─────────────────────────────────────────────────────────────┤
│            app_webrtc.h (Simplified Public API)             │
│     • Smart defaults  • Auto-detection  • Advanced APIs     │
├─────────────────────────────────────────────────────────────┤
│ Signaling Interface │ Peer Connection If │  Media Interface │
│ webrtc_signaling_   │ webrtc_peer_       │ media_stream_    │
│client_if_t          │connection_if_t     │*_capture/player_t│
├─────────────────────────────────────────────────────────────┤
│ KVS │AppRTC│Bridge  │ KVS │Bridge│Custom │ Default│Custom   │
│Signaling │ Signaling │ WebRTC│Only │ Impl  │ Media │ Media  │
└─────────────────────────────────────────────────────────────┘
```

### 🚀 **New Simplified Integration**

**Before (Complex)**:
- 12+ configuration fields to set manually
- Hard-coded signaling backends
- Monolithic peer connection handling

**After (Simple)**:
- **4 essential fields** with smart defaults
- **Pluggable signaling** interfaces (KVS, AppRTC, Bridge, Custom)
- **Pluggable peer connections** (KVS WebRTC, Bridge-only, Custom)
- **Auto-detection** of signaling-only vs. streaming modes
- **Advanced APIs** to override any default

## Signaling Interface Definition

The core signaling interface is defined in `app_webrtc_if.h` as `webrtc_signaling_client_if_t`:

```c
typedef struct {
    // Initialize the signaling client with configuration
    WEBRTC_STATUS (*init)(void *signaling_cfg, void **ppSignalingClient);

    // Connect to signaling service
    WEBRTC_STATUS (*connect)(void *pSignalingClient);

    // Disconnect from signaling service
    WEBRTC_STATUS (*disconnect)(void *pSignalingClient);

    // Send a signaling message
    WEBRTC_STATUS (*send_message)(void *pSignalingClient, webrtc_message_t *pMessage);

    // Free resources
    WEBRTC_STATUS (*free)(void *pSignalingClient);

    // Set callbacks for signaling events
    WEBRTC_STATUS (*set_callbacks)(void *pSignalingClient,
                                 uint64_t customData,
                                 WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*),
                                 WEBRTC_STATUS (*on_state_changed)(uint64_t, webrtc_state_t),
                                 WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t));

    // Set the role type for the signaling client
    WEBRTC_STATUS (*set_role_type)(void *pSignalingClient, webrtc_signaling_channel_role_type_t role_type);

    // Get ICE server configuration (expects iceServers array, not full RtcConfiguration)
    WEBRTC_STATUS (*get_ice_servers)(void *pSignalingClient, uint32_t *pIceConfigCount, void *pIceServersArray);

    // Query ICE server by index (for bridge/RPC pattern)
    WEBRTC_STATUS (*get_ice_server_by_idx)(void *pSignalingClient, int index, bool useTurn, uint8_t **data, int *len, bool *have_more);

    // Check if ICE configuration refresh is needed (immediate, non-blocking check)
    WEBRTC_STATUS (*is_ice_refresh_needed)(void *pSignalingClient, bool *refreshNeeded);

    // Trigger ICE configuration refresh (background operation)
    WEBRTC_STATUS (*refresh_ice_configuration)(void *pSignalingClient);
} webrtc_signaling_client_if_t;
```

## Implementation Steps

### 🎯 **Quick Integration (Recommended)**

Use the simplified API with existing peer connection interfaces:

```c
// Configure with your custom signaling + existing peer connection
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = my_custom_signaling_if_get();
config.signaling_cfg = &my_signaling_config;
config.peer_connection_if = kvs_peer_connection_if_get();  // Use KVS peer connections
config.video_capture = media_stream_get_video_capture_if();

app_webrtc_init(&config);
app_webrtc_run();
```

### 🔧 **Advanced Integration (Custom Peer Connections)**

Implement both custom signaling AND custom peer connection interfaces for full control.

### Step 1: Create Your Signaling Implementation

Create a new component or source file with your signaling implementation:

```c
// my_custom_signaling.c

#include "app_webrtc.h"
#include "app_webrtc_if.h"

// Your custom signaling configuration structure
typedef struct {
    char *server_url;
    char *room_id;
    char *client_id;
    uint32_t connection_timeout;
    uint32_t log_level;
} my_custom_signaling_config_t;

// Your custom signaling client data
typedef struct {
    char *server_url;
    char *room_id;
    void *websocket_handle;
    webrtc_signaling_channel_role_type_t role;

    // Callbacks
    uint64_t customData;
    WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*);
    WEBRTC_STATUS (*on_state_changed)(uint64_t, webrtc_state_t);
    WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t);

    // Add your specific fields here
} my_custom_signaling_data_t;

static WEBRTC_STATUS my_custom_signaling_init(void *signaling_cfg, void **ppSignalingClient)
{
    my_custom_signaling_config_t *pConfig = (my_custom_signaling_config_t *)signaling_cfg;
    my_custom_signaling_data_t *pData = malloc(sizeof(my_custom_signaling_data_t));

    if (pData == NULL) {
        return WEBRTC_STATUS_NOT_ENOUGH_MEMORY;
    }

    // Initialize your signaling client
    pData->server_url = strdup(pConfig->server_url);
    pData->room_id = strdup(pConfig->room_id);
    pData->websocket_handle = NULL;
    pData->role = WEBRTC_CHANNEL_ROLE_TYPE_MASTER;

    // Initialize callbacks to NULL
    pData->customData = 0;
    pData->on_msg_received = NULL;
    pData->on_state_changed = NULL;
    pData->on_error = NULL;

    *ppSignalingClient = pData;
    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS my_custom_signaling_connect(void *pSignalingClient)
{
    my_custom_signaling_data_t *pData = (my_custom_signaling_data_t *)pSignalingClient;

    // Implement your connection logic
    // Connect to your signaling server
    // Set up message handlers

    // Notify state change
    if (pData->on_state_changed) {
        pData->on_state_changed(pData->customData, WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED);
    }

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS my_custom_signaling_send_message(void *pSignalingClient, webrtc_message_t *pMessage)
{
    my_custom_signaling_data_t *pData = (my_custom_signaling_data_t *)pSignalingClient;

    // Convert message to your protocol format and send
    // For example, JSON format over WebSocket

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS my_custom_signaling_set_callbacks(void *pSignalingClient,
                                                       uint64_t customData,
                                                       WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*),
                                                       WEBRTC_STATUS (*on_state_changed)(uint64_t, webrtc_state_t),
                                                       WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    my_custom_signaling_data_t *pData = (my_custom_signaling_data_t *)pSignalingClient;

    pData->customData = customData;
    pData->on_msg_received = on_msg_received;
    pData->on_state_changed = on_state_changed;
    pData->on_error = on_error;

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS my_custom_signaling_set_role_type(void *pSignalingClient, webrtc_signaling_channel_role_type_t role_type)
{
    my_custom_signaling_data_t *pData = (my_custom_signaling_data_t *)pSignalingClient;
    pData->role = role_type;
    return WEBRTC_STATUS_SUCCESS;
}

// Implement other interface functions (disconnect, free, get_ice_servers, etc.)...

webrtc_signaling_client_if_t* my_custom_signaling_get_if(void)
{
    static webrtc_signaling_client_if_t customInterface = {
        .init = my_custom_signaling_init,
        .connect = my_custom_signaling_connect,
        .disconnect = my_custom_signaling_disconnect,
        .send_message = my_custom_signaling_send_message,
        .free = my_custom_signaling_free,
        .set_callbacks = my_custom_signaling_set_callbacks,
        .set_role_type = my_custom_signaling_set_role_type,
        .get_ice_servers = my_custom_signaling_get_ice_servers,
        .get_ice_server_by_idx = my_custom_signaling_get_ice_server_by_idx,
        .is_ice_refresh_needed = my_custom_signaling_is_ice_refresh_needed,
        .refresh_ice_configuration = my_custom_signaling_refresh_ice_config
    };

    return &customInterface;
}
```

### Step 2: Handle Incoming Messages

Implement message reception and callback invocation:

```c
void myCustomMessageHandler(const char *message)
{
    my_custom_signaling_data_t *pData = getCurrentSignalingData();

    // Parse your protocol message
    webrtc_message_t webrtcMessage = {0};
    parseCustomMessage(message, &webrtcMessage);

    // Invoke the callback
    if (pData->on_msg_received) {
        pData->on_msg_received(pData->customData, &webrtcMessage);
    }
}
```

## Configuration and Initialization

### Step 3: Configure with Simplified API

```c
#include "app_webrtc.h"
#include "my_custom_signaling.h"
#include "kvs_peer_connection.h"  // or your custom peer connection

void app_main(void)
{
    // Initialize NVS, WiFi, time sync, etc. (standard ESP-IDF setup)
    // ... standard initialization code ...

    // Register event callback (optional)
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

    // Get media interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    // Configure your signaling
    my_custom_signaling_config_t customConfig = {
        .server_url = "wss://my-signaling-server.com",
        .room_id = "my-room",
        .client_id = "esp32-client",
        .connection_timeout = 30000,
        .log_level = 3
    };

    // Configure WebRTC with simplified API
    app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration - what you MUST provide
    config.signaling_client_if = my_custom_signaling_if_get();
    config.signaling_cfg = &customConfig;
    config.peer_connection_if = kvs_peer_connection_if_get();  // Use existing KVS peer connections

    // Media interfaces for streaming
    config.video_capture = video_capture;
    config.audio_capture = audio_capture;
    config.video_player = video_player;
    config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC with custom signaling + simplified API");
    ESP_LOGI(TAG, "Smart defaults: MASTER role, H.264+OPUS, trickle ICE");

    // Initialize WebRTC application with smart defaults
    WEBRTC_STATUS status = app_webrtc_init(&config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC app: 0x%08x", status);
        return;
    }

    // Advanced configuration: Override defaults if needed
    app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);  // Change role
    app_webrtc_enable_media_reception(true);               // Enable media reception

    // Start the WebRTC application
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08x", status);
        app_webrtc_terminate();
    }
}
```

## Existing Examples

### 1. webrtc_classic: KVS Signaling + KVS Peer Connection

**Location**: `examples/webrtc_classic/main/webrtc_main.c`
**Architecture**: `kvs_signaling + kvs_peer_connection`

Uses AWS KVS for both signaling and peer connections (full functionality):

```c
// Configure with simplified API
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &kvs_signaling_cfg;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = media_stream_get_video_capture_if();

// Smart defaults: MASTER role, H.264+OPUS, trickle ICE, TURN
app_webrtc_init(&config);
app_webrtc_enable_media_reception(true);  // Override default
app_webrtc_run();
```

**Key Features**:
- Full AWS KVS integration
- Simplified 4-field configuration
- Smart defaults with advanced override APIs
- Automatic ICE server management

### 2. streaming_only: Bridge Signaling + KVS Peer Connection

**Location**: `examples/streaming_only/main/streaming_only_main.c`
**Architecture**: `bridge_signaling + kvs_peer_connection`

Receives signaling from partner device, handles full WebRTC peer connections:

```c
// Configure with simplified API
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = getBridgeSignalingClientInterface(); // signaling relay via webrtc_bridge
config.signaling_cfg = &bridge_config;
config.peer_connection_if = kvs_peer_connection_if_get();  // Full WebRTC
config.video_capture = video_capture;
config.audio_capture = audio_capture;
config.video_player = video_player;
config.audio_player = audio_player;

app_webrtc_init(&config);
app_webrtc_enable_media_reception(true);  // Bi-directional streaming
webrtc_bridge_start();  // Start bridge communication
app_webrtc_run();
```

**Key Features**:
- Power-efficient split mode architecture
- Receives signaling via bridge from partner device
- Full WebRTC peer connections for media streaming
- Bi-directional media support

### 3. signaling_only: KVS Signaling + Bridge Peer Connection

**Location**: `examples/signaling_only/main/signaling_only_main.c`
**Architecture**: `kvs_signaling + bridge_peer_connection`

Handles AWS KVS signaling, forwards messages via bridge (no WebRTC peer connections):

```c
// Configure with simplified API - signaling-only mode
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &kvs_signaling_cfg;
config.peer_connection_if = bridge_peer_connection_if_get();  // Fake peer doing signaling relay via webrtc_bridge

// Automatically optimizes for memory and power efficiency
app_webrtc_init(&config);
app_webrtc_run();
```

**Key Features**:
- AWS KVS signaling with bridge forwarding
- No WebRTC SDK initialization (memory efficient)
- signaling-only (i.e, bridged_peer) mode (As peer_connection_if intentionally sets create_session NULL)
- Always-on connectivity with partner device

### 4. esp_camera: AppRTC Signaling + KVS Peer Connection

**Location**: `examples/esp_camera/main/esp_webrtc_camera_main.c`
**Architecture**: `apprtc_signaling + kvs_peer_connection`

Browser-compatible AppRTC signaling with full WebRTC peer connections:

```c
// Configure with simplified API
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = apprtc_signaling_client_if_get();
config.signaling_cfg = &apprtc_config;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = video_capture;
config.audio_capture = audio_capture;
config.video_player = video_player;
config.audio_player = audio_player;

app_webrtc_init(&config);
// Advanced APIs: Set role after initialization
app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_MASTER);
app_webrtc_enable_media_reception(true);
app_webrtc_run();
```

**Key Features**:
- AppRTC protocol (browser-compatible)
- Room-based sessions with CLI control
- No AWS account needed
- Advanced configuration APIs for role management

## Message Flow Examples

### Basic Signaling Flow

```
Application          Signaling Interface         Your Implementation
     │                        │                          │
     ├─ app_webrtc_run() ──────→│                          │
     │                        ├─ connect() ─────────────→│
     │                        │                          ├─ Connect to server
     │                        │                          │
     │                        │←─ messageReceived() ─────┤
     │←─ handleOffer() ───────┤                          │
     │                        │                          │
     ├─ sendAnswer() ─────────→│                          │
     │                        ├─ send_message() ─────────→│
     │                        │                          ├─ Send via transport
```

### ICE Server Management

```
     │                        │                          │
     ├─ needsIceRefresh() ────→│                          │
     │                        ├─ is_ice_refresh_needed() ──→│
     │                        │                          ├─ Check expiration
     │                        │←─ true ──────────────────┤
     │                        │                          │
     ├─ refreshIce() ─────────→│                          │
     │                        ├─ refresh_ice_configuration()→│
     │                        │                          ├─ Fetch new servers
     │                        │←─ success ───────────────┤
```

## Best Practices

### 1. Thread Safety

Ensure your signaling implementation is thread-safe:

```c
typedef struct {
    SemaphoreHandle_t mutex;
    // Your data...
} MySignalingData;

static WEBRTC_STATUS my_custom_signaling_send_message(void *pSignalingClient, webrtc_message_t *pMessage)
{
    MySignalingData *pData = (MySignalingData *)pSignalingClient;

    xSemaphoreTake(pData->mutex, portMAX_DELAY);
    // Your send logic
    xSemaphoreGive(pData->mutex);

    return WEBRTC_STATUS_SUCCESS;
}
```

### 2. Memory Management

Always clean up resources properly:

```c
static WEBRTC_STATUS myCustomFree(void *pSignalingClient)
{
    MySignalingData *pData = (MySignalingData *)pSignalingClient;

    if (pData) {
        if (pData->server_url) {
            free(pData->server_url);
        }
        if (pData->mutex) {
            vSemaphoreDelete(pData->mutex);
        }
        free(pData);
    }

    return WEBRTC_STATUS_SUCCESS;
}
```

### 3. Error Handling

Provide meaningful error information:

```c
static WEBRTC_STATUS my_custom_signaling_connect(void *pSignalingClient)
{
    MySignalingData *pData = (MySignalingData *)pSignalingClient;

    int result = connectToServer(pData->server_url);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to connect to signaling server: %d", result);
        return WEBRTC_STATUS_NETWORK_ERROR;
    }

    ESP_LOGI(TAG, "Successfully connected to signaling server");
    return WEBRTC_STATUS_SUCCESS;
}
```

### 4. Work Queue Integration

For ICE operations that might require network calls, ensure the work queue is available:

```c
static WEBRTC_STATUS myCustomRefreshIceConfiguration(void *pSignalingClient)
{
    // Offload heavy operations to work queue to prevent stack overflow
    esp_err_t result = esp_work_queue_add_task(iceRefreshTask, pSignalingClient);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Work queue unavailable, deferring ICE refresh");
        return WEBRTC_STATUS_RETRY_LATER;
    }

    return WEBRTC_STATUS_SUCCESS;
}
```

## Troubleshooting

### Common Issues

1. **Stack Overflow in ICE Operations**
   - Ensure work queue is initialized before WebRTC operations
   - Use `esp_work_queue_add_task()` for network-heavy operations
   - Monitor stack usage in critical paths

2. **Message Parsing Errors**
   - Validate message format before processing
   - Handle malformed messages gracefully
   - Log detailed error information

3. **Connection Failures**
   - Check network connectivity
   - Verify server URLs and certificates
   - Implement connection retry logic

4. **Memory Leaks**
   - Always pair malloc/free calls
   - Clean up in error paths
   - Use valgrind or heap tracing for debugging

### Debug Logging

Enable detailed logging to troubleshoot issues:

```c
#define TAG "my_custom_signaling"

// Enable verbose logging
esp_log_level_set(TAG, ESP_LOG_VERBOSE);

// Log message flows
ESP_LOGV(TAG, "Sending message: type=%d, payload_len=%d",
         message->messageType, message->payloadLen);
```

### Testing Your Implementation

1. **Unit Tests**: Test individual functions
2. **Integration Tests**: Test with WebRTC stack
3. **Interoperability Tests**: Test with other WebRTC clients
4. **Stress Tests**: Test under load and error conditions

## Advanced Topics

### Custom Message Types

Extend the message types for your specific needs:

```c
typedef enum {
    ESP_WEBRTC_MESSAGE_OFFER = 1,
    ESP_WEBRTC_MESSAGE_ANSWER = 2,
    ESP_WEBRTC_MESSAGE_ICE_CANDIDATE = 3,
    // Add your custom types
    MY_CUSTOM_MESSAGE_TYPE = 100
} esp_webrtc_message_type_t;
```

### Protocol Extensions

Add custom protocol features while maintaining compatibility:

```c
typedef struct {
    webrtc_message_t base;
    // Your extensions
    char *custom_field;
    uint32_t custom_data;
} my_extended_message_t;
```

---

## Additional Resources

- **[API_USAGE.md](API_USAGE.md)**: Complete API documentation and usage examples for all deployment modes
- **Example Implementations**: Refer to the existing signaling implementations in the `components/` directory
- **WebRTC Samples**: Check the `examples/` directory for complete working applications
