# App WebRTC Component

This is the main application layer component that provides the unified WebRTC interface for ESP32 applications.

## ⚠️ **Current Status: Transitional Architecture**

**Important Note**: This component currently includes KVS SDK sources temporarily to maintain compilation. This is **NOT** the final clean architecture - it's a pragmatic intermediate step.

### **Current Temporary Setup**:
- ✅ **Pluggable Interface**: Applications properly use `kvs_signaling_client_if_get()` and `kvs_peer_connection_if_get()`
- ⚠️ **KVS Sources Included**: Temporarily includes KVS SDK sources to compile
- 🎯 **Goal**: Remove KVS sources and make this a pure application layer

### **Next Steps** (Future):
1. Abstract all KVS-specific code behind interfaces
2. Move KVS sources back to respective components
3. Make app_webrtc depend only on interfaces, not implementations

## Overview

The `app_webrtc` component serves as the application layer that abstracts WebRTC functionality and provides a pluggable architecture for different signaling and peer connection implementations.

## Architecture Role

In the 3-component architecture:
- **app_webrtc** ← YOU ARE HERE (Application layer and unified interface)
- **kvs_webrtc** (KVS peer connection implementation)
- **kvs_signaling** (KVS signaling implementation)

## Key Features

- **Unified Interface**: `app_webrtc_if.h` - Common interface for all signaling and peer connection implementations
- **Application API**: `app_webrtc.h` - High-level API for applications
- **Pluggable Architecture**: Support for different signaling and peer connection backends
- **State Management**: Centralized WebRTC state machine and session management
- **Media Abstraction**: Integration with media streams

## Files

### Public Headers
- `app_webrtc.h` - Main application API
- `app_webrtc_if.h` - Unified interface definitions for signaling and peer connections
- `null_peer_connection.h` - Null peer connection implementation (signaling-only mode)
- `bridge_peer_connection.h` - Bridge peer connection interface

### Sources
- `app_webrtc.c` - Main application logic and state machine
- `app_webrtc_media.c` - Media stream integration
- `null_peer_connection.c` - Null peer connection implementation

### Internal
- `app_webrtc_internal.h` - Internal definitions

## Dependencies

- **esp_common** - ESP-IDF common functionality
- **esp_timer** - Timer functionality
- **esp_http_client** - HTTP client
- **json** - JSON parsing
- **mbedtls** - TLS/crypto functionality
- **media_stream** - Media stream handling
- **signaling_serializer** - Message serialization

## Usage Examples

### All Examples Use This Component
All examples depend on `app_webrtc` as it provides the fundamental WebRTC application interface:

```cmake
REQUIRES "app_webrtc"
```

### Example Configurations

**webrtc_classic**:
```cmake
REQUIRES "app_webrtc" "kvs_webrtc" "kvs_signaling"  # Full KVS functionality
```

**esp_camera**:
```cmake
REQUIRES "app_webrtc" "kvs_webrtc"  # KVS peer connections + AppRTC signaling
```

**streaming_only**:
```cmake
REQUIRES "app_webrtc" "kvs_webrtc"  # KVS peer connections + Bridge signaling
```

**signaling_only**:
```cmake
REQUIRES "app_webrtc" "kvs_signaling"  # KVS signaling + Null peer connections
```

## API Usage

```c
#include "app_webrtc.h"

// Configure with pluggable interfaces
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = getKvsSignalingClientInterface();    // or getBridgeSignalingClientInterface()
config.peer_connection_if = getKvsPeerConnectionInterface();       // or getNullPeerConnectionInterface()

// Initialize and run
app_webrtc_init(&config);
app_webrtc_start();
```

## Pluggable Architecture

The component supports pluggable:
- **Signaling implementations**: KVS, AppRTC, Bridge, Custom
- **Peer connection implementations**: KVS, Bridge, Null (signaling-only), Custom

This enables flexible deployment scenarios while maintaining a consistent application interface.
