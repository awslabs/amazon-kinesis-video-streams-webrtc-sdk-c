# ESP-IDF Port of Amazon Kinesis Video Streams WebRTC SDK

This directory contains the ESP-IDF port of the Amazon Kinesis Video Streams WebRTC SDK. The port is organized as a collection of ESP-IDF components that can be used in ESP-IDF projects.

## Directory Structure

```
esp_port/
├── components/             # ESP-IDF components
│   ├── api_call/           # API call handling component
│   ├── credential/         # Credential management
│   ├── esp_hosted/         # ESP hosted functionality
│   ├── esp_usrsctp/        # SCTP protocol implementation for ESP
│   ├── esp_webrtc_utils/   # WebRTC utilities for ESP
│   ├── esp_wifi_remote/    # Remote WiFi functionality
│   ├── kvs_utils/          # KVS utility functions
│   ├── kvs_webrtc/         # Main KVS WebRTC component
│   ├── libllhttp/          # HTTP parsing library
│   ├── libsrtp2/           # SRTP (Secure Real-time Transport Protocol) library
│   ├── libwebsockets/      # WebSocket library
│   ├── network_coprocessor/ # Network coprocessor support
│   `── state_machine/      # State machine implementation
└── examples/               # Example applications
```

## Components

### libsrtp2
The `libsrtp2` component is a port of the Cisco libSRTP library, which provides implementations of the Secure Real-time Transport Protocol (SRTP) and the Secure Real-time Transport Control Protocol (SRTCP). This component is essential for secure media transport in WebRTC applications.

### Other Components
- `kvs_webrtc`: Main component implementing the KVS WebRTC functionality
- `esp_webrtc_utils`: Helper utilities for WebRTC implementation
- `esp_usrsctp`: User-land SCTP implementation for data channel support, specifically modified for Espressif
- `libwebsockets`: WebSocket implementation for signaling
- Additional components provide supporting functionality for the WebRTC stack

## Configuration

Each component may have its own configuration options that can be set through `menuconfig`. Refer to the individual component documentation for specific configuration options.

## Notes

- The port uses mbedTLS for cryptographic operations
- Some components have specific ESP-IDF version requirements
- Check the examples directory for implementation references
