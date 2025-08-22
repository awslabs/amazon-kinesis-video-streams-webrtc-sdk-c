# KVS WebRTC Component

This component provides AWS Kinesis Video Streams (KVS) WebRTC peer connection functionality for ESP32 platforms.

## Overview

The `kvs_webrtc` component contains the KVS SDK WebRTC peer connection implementation, including ICE handling, DTLS, SRTP, and media processing.

## Architecture Role

In the 3-component architecture:
- **app_webrtc** (Application layer and unified interface)
- **kvs_webrtc** ← YOU ARE HERE (KVS peer connection implementation)
- **kvs_signaling** (KVS signaling implementation)

## Key Features

- **KVS SDK Integration**: Full AWS Kinesis Video Streams WebRTC SDK
- **Peer Connection Management**: ICE, DTLS, SRTP handling
- **Media Processing**: Audio/video encoding, RTP packetization
- **STUN/TURN Support**: NAT traversal capabilities
- **Data Channels**: Bidirectional data communication
- **Codec Support**: H.264, H.265, VP8, Opus, G.711

## Files

### Headers
- `kvs_peer_connection.h` - KVS peer connection interface implementation
- `DataBuffer.h` - Data buffer utilities
- `WebRtcLogging.h` - Logging functionality

### Sources
- `kvs_peer_connection.c` - KVS peer connection implementation
- `DataBuffer.c` - Data buffer implementation
- `WebRtcLogging.c` - Logging implementation
- `Tls_esp.c` - ESP-specific TLS implementation

### KVS SDK Sources (Included)
- All KVS WebRTC SDK source files for peer connections
- Crypto, ICE, PeerConnection, RTC, RTP, SRTP, SCTP, SDP, STUN modules

## Dependencies

- **mbedtls** - TLS and cryptography
- **lwip** - TCP/IP stack
- **esp_timer** - Timer functionality
- **esp_system** - ESP-IDF system functions
- **esp_wifi** - WiFi functionality

## Usage

This component is used by applications that need KVS WebRTC peer connections:

### Examples Using This Component

**webrtc_classic**: Full KVS functionality
```cmake
REQUIRES "app_webrtc" "kvs_webrtc" "kvs_signaling"
```

**esp_camera**: KVS peer connections with AppRTC signaling
```cmake
REQUIRES "app_webrtc" "kvs_webrtc"
```

**streaming_only**: KVS peer connections with Bridge signaling
```cmake
REQUIRES "app_webrtc" "kvs_webrtc"
```

### Not Used By

**signaling_only**: Uses null peer connections instead
```cmake
REQUIRES "app_webrtc" "kvs_signaling"  # No kvs_webrtc
```

## Configuration

The component supports various ESP-IDF configuration options:
- `CONFIG_USE_ESP_TLS_FOR_KVS` - Use ESP-TLS instead of direct mbedTLS
- `CONFIG_ENABLE_DATA_CHANNEL` - Enable data channel support
- `CONFIG_PREFER_DYNAMIC_ALLOCS` - Use dynamic memory allocation

## KVS SDK Integration

This component wraps the AWS KVS WebRTC SDK to provide:
- Peer connection lifecycle management
- Media stream handling
- Network transport (UDP/TCP)
- Security (DTLS/SRTP)
- NAT traversal (STUN/TURN)

## API

The component implements the `webrtc_peer_connection_if_t` interface defined in `app_webrtc_if.h`:

```c
webrtc_peer_connection_if_t* kvs_peer_connection_if_get(void);
```

This allows it to be used as a pluggable peer connection backend in the unified app_webrtc architecture.
