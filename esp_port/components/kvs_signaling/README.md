# KVS Signaling Component

This component provides AWS Kinesis Video Streams (KVS) signaling functionality for ESP32 platforms.

## Overview

The `kvs_signaling` component handles:
- KVS signaling client implementation
- WebSocket-based signaling communication
- HTTP API calls for KVS services
- Clock skew correction and retry logic
- Integration with AWS IoT credentials

## Architecture

This component was separated from the main `kvs_webrtc` component to provide better separation of concerns:
- **kvs_signaling**: Handles signaling protocol and communication
- **kvs_webrtc**: Focuses on WebRTC peer connections and media

## Files

### Headers
- `kvs_signaling.h` - Main signaling client interface
- `SignalingESP.h` - ESP-specific signaling implementation

### Sources
- `kvs_signaling.c` - KVS signaling client wrapper implementation
- `SignalingESP.c` - ESP WebSocket signaling implementation
- `LwsApiCallsESP.c` - HTTP API calls with ESP adaptations

## Dependencies

- **esp_common** - ESP-IDF common functionality
- **esp_timer** - Timer functionality
- **esp_http_client** - HTTP client for API calls
- **json** - JSON parsing
- **libwebsockets** - WebSocket implementation
- **mbedtls** - TLS/crypto functionality
- **kvs_utils** - KVS utility functions
- **signaling_serializer** - Message serialization
- **credential** - AWS credential management

## Usage

Include the header in your application:
```c
#include "kvs_signaling.h"
```

Add the component to your CMakeLists.txt:
```cmake
REQUIRES "kvs_signaling"
```

## Configuration

The component supports ESP-IDF configuration options:
- `CONFIG_USE_ESP_WEBSOCKET_CLIENT` - Use ESP WebSocket client
- `CONFIG_USE_ESP_TLS_FOR_KVS` - Use ESP-TLS for TLS connections

## Examples

See the following examples for usage:
- `webrtc_classic` - Full KVS signaling with WebRTC
- `signaling_only` - Signaling without peer connections
