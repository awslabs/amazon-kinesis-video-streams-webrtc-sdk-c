# ESP-IDF WebSocket Signaling Implementation

This directory contains an ESP-IDF native implementation of the WebRTC signaling client for the Amazon Kinesis Video Streams WebRTC SDK. It replaces the libwebsockets-based implementation with ESP-IDF's native WebSocket and HTTP client libraries.

## Benefits

1. **Reduced Memory Footprint**: Uses ESP-IDF's native WebSocket client instead of the more resource-intensive libwebsockets library.

2. **Native ESP-IDF Integration**: Integrates directly with ESP-IDF's event loop system for improved compatibility and performance.

3. **Lower Resource Usage**: Optimized for resource-constrained ESP32 devices.

## Implementation Details

The implementation consists of two main files:

- **SignalingESP.c/h**: Main signaling client implementation replacing the original Signaling.c
- **LwsApiCallsESP.c**: Implementation of WebSocket and HTTP API calls using ESP-IDF libraries

## Features

- Full compatibility with the original KVS WebRTC SDK API
- Maintains the same function signatures for seamless integration
- Handles WebSocket connection management, reconnection, and message passing
- Currently uses stub implementations for HTTP API calls that need to be completed

## Usage

To use this implementation:

1. Set the `USE_ESP_WEBSOCKET` flag to `ON` in `esp_port/components/kvs_webrtc/CMakeLists.txt`
2. Ensure the ESP-IDF websocket client component is available in your project

```cmake
option(USE_ESP_WEBSOCKET "Use ESP-IDF WebSocket client instead of libwebsockets" ON)
```

## Current Limitations

1. HTTP API calls (for operations like describeChannel, createChannel, etc.) are currently implemented as stubs and need to be completed using ESP-IDF's HTTP client.
2. File caching functionality is not yet implemented for ESP.
3. Some diagnostics metrics might need adjustment to fully match the original implementation.

## Planned Improvements

1. Complete HTTP API call implementations for all signaling operations
2. Add file caching support
3. Add comprehensive logging and error handling
4. Improve connection reliability and reconnection logic

## Contributing

When modifying this implementation, please ensure:

1. All public-facing functions maintain the same signatures as the original SDK
2. Error handling follows the same patterns as the original code
3. Memory usage is kept to a minimum
4. Changes do not break compatibility with the rest of the SDK

## License

This implementation follows the same license as the Amazon Kinesis Video Streams WebRTC SDK.