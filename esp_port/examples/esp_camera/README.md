# ESP Camera WebRTC Example

This example demonstrates WebRTC video streaming from an ESP32 camera to a web browser using the Espressif AppRTC-based signaling server (webrtc.espressif.com). It uses a standalone configuration where the ESP32 handles both media streaming and signaling directly with the Espressif signaling server.

## Features

- Video streaming using H.264 encoder
- Audio streaming using Opus encoder
- WebRTC media transmission
- Direct connection to webrtc.espressif.com for AppRTC-based signaling
- Standalone operation (no separate signaling component needed)
    - Signaling is handled as the part of this application, while the KVS WebRTC is initialized in streaming_only mode.

## Hardware Required

- ESP32-P4 with camera module (OV2640, OV3660, OV5640, etc.)
- Microphone connected to I2S input (optional for audio)
- Speaker or headphone output (optional for audio playback)

## How to Use

### Build and Flash

Build the project and flash it to the ESP32-P4 board:

```
idf.py build
idf.py -p PORT flash
```

### Configuration

Configure Wi-Fi credentials via menuconfig:

```
idf.py menuconfig
```

Navigate to "Example Configuration" and set:
- Wi-Fi SSID
- Wi-Fi Password

### Connection

1. Power on the ESP32-P4 device.
2. The device will connect to your Wi-Fi network.
3. It will establish a connection to webrtc.espressif.com for signaling.
4. When a WebRTC peer connection is established, the device will start streaming media.
5. Access the web interface at webrtc.espressif.com to view the stream from your browser.

## Architecture

This example implements:
- Media capture through camera and microphone interfaces
- H.264 encoding for video
- Opus encoding for audio
- WebRTC media stack for streaming
- Direct signaling with the Espressif AppRTC-based server

The STREAMING_ONLY configuration is used to disable the default KVS signaling functionality, allowing the device to work directly with the Espressif signaling server instead.

## Troubleshooting

- If video streaming doesn't work, check camera connections
- If audio doesn't work, verify microphone and speaker connections
- Check that the device can connect to webrtc.espressif.com
- Make sure your browser supports WebRTC (Chrome, Firefox, or Edge recommended)
- Verify Wi-Fi connectivity and network settings

## License

Apache License 2.0