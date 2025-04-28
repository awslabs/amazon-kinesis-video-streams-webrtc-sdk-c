# WebRTC Classic Example

This example demonstrates the classic mode of WebRTC operation for ESP32 devices, where signaling and streaming are performed on the same chip.

## Features

- Complete WebRTC implementation on a single ESP32 device
- Video streaming from camera
- Audio streaming from microphone
- AWS KVS integration with both direct credentials and IoT Core credentials
- Configurable media settings (resolution, bitrate, etc.)
- Automatic reconnection on network issues

## Hardware Required

- One of the following ESP32 development boards:
  - ESP32-WROVER-KIT with camera module
  - ESP32-S3-EYE
  - ESP32-P4-Function-EV-Board (with ESP32-C6 as network coprocessor)
- Microphone connected to I2S input (optional for audio)
- Speaker or headphone output (optional for audio playback)

## How to Use

### Build and Flash

1. Configure the project:

```bash
# For ESP32
idf.py set-target esp32

# For ESP32-S3
idf.py set-target esp32s3

# For ESP32-P4
idf.py set-target esp32p4
```

2. Configure Wi-Fi and AWS credentials:

```bash
idf.py menuconfig
```

Navigate to "Example Configuration" and set:
- ESP_WIFI_SSID: Your Wi-Fi network name
- ESP_WIFI_PASSWORD: Your Wi-Fi password
- AWS_ACCESS_KEY_ID: Your AWS access key
- AWS_SECRET_ACCESS_KEY: Your AWS secret key
- AWS_DEFAULT_REGION: Your AWS region (e.g., us-west-2)
- AWS_KVS_CHANNEL: Your Kinesis Video Stream channel name

Alternatively, you can use IoT Core credentials by enabling `CONFIG_IOT_CORE_ENABLE_CREDENTIALS` and placing the required certificates in the `app_common/spiffs_image/certs/` directory.

3. Build and flash the project:

```bash
idf.py build
idf.py -p [PORT] flash monitor
```

### Using with Network Coprocessor (ESP32-P4 + ESP32-C6)

When using the ESP32-P4-Function-EV-Board, you need to:

1. Flash the `network_adapter` example to the ESP32-C6 coprocessor:
   ```bash
   cd ../network_adapter
   idf.py set-target esp32c6
   idf.py build
   idf.py -p [C6_PORT] flash
   ```

2. Then flash this `webrtc_classic` example to the ESP32-P4:
   ```bash
   cd ../webrtc_classic
   idf.py set-target esp32p4
   idf.py build
   idf.py -p [P4_PORT] flash
   ```

### Viewing the Stream

To view the WebRTC stream:

1. Open the [KVS WebRTC Test Page](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html)
2. Enter your AWS credentials
3. Enter the same channel name configured in your ESP32 application
4. Click "Start Viewer" to view the stream

## Configuration Options

### AWS Settings

You can configure AWS-related settings:

- KVS channel name
- AWS region
- Log level
- Credential type (direct or IoT Core)

## Troubleshooting

- If the device fails to connect to Wi-Fi, check your Wi-Fi credentials
- If the device connects to Wi-Fi but fails to connect to AWS, verify your AWS credentials and region
- If no video is displayed, check that your camera is properly connected and supported
- If you experience poor video quality, try reducing the resolution or bitrate

## License

Apache License 2.0
