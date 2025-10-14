# Signaling-Only Example

This example demonstrates the signaling-only part of the WebRTC implementation for ESP32. It separates the WebRTC functionality into two parts - this component handles AWS KVS signaling while relying on the separate `streaming_only` component for media handling.

## Features

- AWS Kinesis Video Streams (KVS) WebRTC signaling
- SDP offer/answer exchange
- ICE candidate handling
- Bridge-based communication with `streaming_only` component

## Architecture

                    ┌─────────────────┐
                    │   AWS KVS       │
                    │   WebRTC        │
                    │   Signaling     │
                    └─────────────────┘
                            ↕
              ┌─────────────────────────────┐
              │     signaling_only          │
              │  ┌─────────────────────┐    │
              │  │ KVS Signaling IF    │    │
              │  │   (Clean Interface) │    │
              │  └─────────────────────┘    │
              └─────────────────────────────┘
                            ↕ webrtc_bridge
              ┌─────────────────────────────┐
              │     streaming_only          │
              │  ┌─────────────────────┐    │
              │  │    WebRTC App       │    │
              │  │       +             │    │
              │  │ Bridge Signaling IF │    │
              │  └─────────────────────┘    │
              └─────────────────────────────┘

## Hardware Required

- ESP32-C6 board
- Note: No camera or microphone is required for this component as it only handles signaling

## How to Use

### AWS Credentials Setup

1. Configure the AWS credentials:
   - If using IoT Core-based credentials, upload certificate files to the SPIFFS partition
   - If using direct credentials, configure them via environment variables

2. Setup the Kinesis Video Stream channel:
   - Create a Kinesis Video Stream channel in your AWS account
   - Configure the channel name in the code or via menuconfig

### Build and Flash

1. Build the project and flash it to the ESP32-C6 board:

```
idf.py build
idf.py -p PORT flash
```

2. You'll also need to build and flash the `streaming_only` example to an ESP32-P4 board.

### Configuration

Configure Wi-Fi credentials via menuconfig:

```
idf.py menuconfig
```

Navigate to "Example Configuration" and set:
- Wi-Fi SSID
- Wi-Fi Password
- AWS Region
- AWS KVS Channel Name

### Connection

1. Power on both the signaling_only (ESP32-C6) and streaming_only (ESP32-P4) devices.
2. They will automatically connect via their bridge interface.
3. This component will connect to AWS KVS and handle all signaling.
4. When a WebRTC peer connects to your KVS channel, this component will receive and forward signaling messages to the streaming_only component.

## Architecture

This example implements:
- AWS KVS WebRTC signaling client
- SDP offer/answer mechanism
- ICE candidate exchange
- Bridge-based IPC with the streaming component

The signaling_only component needs valid AWS credentials but doesn't require media hardware like cameras.

## Troubleshooting

- If connection to AWS fails, check your credentials and AWS region
- Verify Wi-Fi connectivity
- Ensure both signaling_only and streaming_only examples are running
- Check that the KVS channel name matches the one in your AWS console

## License

Apache License 2.0