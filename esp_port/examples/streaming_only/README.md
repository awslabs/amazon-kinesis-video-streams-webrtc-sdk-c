# Streaming-Only Example

This example demonstrates the streaming-only part of the WebRTC implementation for ESP32. It separates the WebRTC functionality into two parts - this component handles the media streaming aspects (video/audio capture and transmission) while relying on the separate `signaling_only` component for AWS KVS signaling.

## Features

- Video streaming using H.264 encoder
- Audio streaming using Opus encoder
- WebRTC media transmission
- Bridge-based communication with `signaling_only` component

## Hardware Required

- ESP32-P4 with camera module (OV2640, OV3660, OV5640, etc.)
- Microphone connected to I2S input (optional for audio)
- Speaker or headphone output (optional for audio playback)

## How to Use

### Build and Flash

1. Build the project and flash it to the ESP32-P4 board:

```
idf.py build
idf.py -p PORT flash
```

2. You'll also need to build and flash the `signaling_only` example to an ESP32-C6 board.

### Configuration

Configure Wi-Fi credentials via menuconfig:

```
idf.py menuconfig
```

Navigate to "Example Configuration" and set:
- Wi-Fi SSID
- Wi-Fi Password

### Connection

1. Power on both the streaming_only (ESP32-P4) and signaling_only (ESP32-C6) devices.
2. They will automatically connect via their bridge interface.
3. The signaling_only device handles the AWS KVS connectivity and signaling.
4. When a WebRTC peer connection is established, this component will start streaming media.

## Architecture

This example implements:
- Media capture through camera and microphone interfaces
- H.264 encoding for video
- Opus encoding for audio
- WebRTC media stack for streaming
- Bridge-based IPC with the signaling component

The streaming_only component doesn't need AWS credentials as all signaling is handled by the signaling_only component.

## Troubleshooting

- If video streaming doesn't work, check camera connections
- If audio doesn't work, verify microphone and speaker connections
- Ensure both streaming_only and signaling_only examples are running
- Check that signaling_only has valid AWS credentials and correct channel configuration

## License

Apache License 2.0