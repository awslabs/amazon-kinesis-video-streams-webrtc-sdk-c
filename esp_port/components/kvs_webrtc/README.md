# ESP-IDF Native Implementations for KVS WebRTC SDK

This directory contains ESP-IDF native implementations that optimize the Amazon Kinesis Video Streams WebRTC SDK for ESP platforms. It includes two main ESP-specific optimizations:

1. **ESP WebSocket Signaling**: Replaces libwebsockets with ESP-IDF's native WebSocket client
2. **ESP-TLS Implementation**: Replaces direct mbedTLS with ESP-TLS and certificate bundle support

## Benefits

### ESP WebSocket Signaling
1. **Reduced Memory Footprint**: Uses ESP-IDF's native WebSocket client instead of the more resource-intensive libwebsockets library.
2. **Native ESP-IDF Integration**: Integrates directly with ESP-IDF's event loop system for improved compatibility and performance.
3. **Lower Resource Usage**: Optimized for resource-constrained ESP32 devices.

### ESP-TLS Implementation
4. **No Certificate File Dependencies**: Uses ESP certificate bundle, eliminating need for `/spiffs/certs/cacert.pem` files.
5. **Platform Optimized TLS**: Leverages ESP-IDF's optimized TLS implementation with better memory management.
6. **Simplified Configuration**: Works out-of-the-box without certificate setup or file system requirements.

## Implementation Details

This component provides two ESP-specific implementations:

### ESP WebSocket Signaling
- **SignalingESP.c/h**: Main signaling client implementation replacing the original Signaling.c
- **LwsApiCallsESP.c**: Implementation of WebSocket and HTTP API calls using ESP-IDF libraries

### ESP-TLS Implementation
- **Tls_esp.c**: ESP-TLS based TLS session management replacing direct mbedTLS implementation
- **Configuration**: Controlled via `CONFIG_USE_ESP_TLS_FOR_KVS` Kconfig option

📖 **For detailed ESP-TLS configuration and usage, see [ESP_TLS_README.md](ESP_TLS_README.md)**

## Features

- Full compatibility with the original KVS WebRTC SDK API
- Maintains the same function signatures for seamless integration
- Handles WebSocket connection management, reconnection, and message passing
- Currently uses stub implementations for HTTP API calls that need to be completed

## Usage

### ESP WebSocket Signaling
To use the ESP WebSocket implementation:

1. Set the `USE_ESP_WEBSOCKET_CLIENT` option to `y` in menuconfig:
   ```
   Component config → KVS WebRTC Configuration → Use esp_websocket_client instead of libwebscoekts
   ```

### ESP-TLS Implementation
To use the ESP-TLS implementation:

1. Set the `USE_ESP_TLS_FOR_KVS` option to `y` in menuconfig:
   ```
   Component config → KVS WebRTC Configuration → Use ESP-TLS for KVS TLS connections
   ```

2. Or set in `sdkconfig`:
   ```
   CONFIG_USE_ESP_TLS_FOR_KVS=y
   ```

**Both implementations can be used independently or together for maximum optimization.**

## When ESP-TLS is Used

The ESP-TLS implementation automatically handles TLS connections for:

1. **TURN over TLS**: When TURN servers use `turns:` protocol with TCP transport
   - Example: `turns:stun.kinesisvideo.us-east-1.amazonaws.com:443?transport=tcp`
2. **Secure TURN connections**: When `iceServer.isSecure = TRUE` and protocol is TCP

### Benefits vs File-Based Certificates
- ✅ **No certificate files needed** - uses ESP certificate bundle
- ✅ **Automatic certificate updates** with ESP-IDF releases
- ✅ **Lower memory usage** - no file I/O overhead
- ✅ **Simplified deployment** - works out-of-the-box

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