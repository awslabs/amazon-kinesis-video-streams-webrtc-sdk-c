# ESP-IDF Port of Amazon Kinesis Video Streams WebRTC SDK

This is a complete ESP-IDF port of the Amazon Kinesis Video Streams WebRTC SDK, enabling real-time audio/video streaming on ESP32 devices. The SDK supports multiple deployment modes and custom signaling protocols for maximum flexibility.

## 🚀 Quick Start

**Want to get streaming in 5 minutes?**

1. **Clone and setup**: `git clone --recursive <repo>` → Install ESP-IDF v5.4 → Apply patches
2. **Build example**: `cd examples/webrtc_classic` → Configure AWS credentials → `idf.py build flash monitor`
3. **Start streaming**: Open [WebRTC Test Page](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html) → Connect to your channel

📖 **Detailed setup instructions below** ⬇️

## 📋 Prerequisites

### Hardware Requirements
- **Single Device**: ESP32, ESP32-S3, ESP32-C6 with camera/microphone
- **Dual Device (Split Mode)**: ESP32-P4 Function EV Board (has both ESP32-P4 + ESP32-C6 onboard)
- **Camera**: Supported modules via esp_video OR esp32-camera
- **Network**: Wi-Fi connection with internet access

### Software Requirements
- **ESP-IDF**: v5.4 (release/v5.4 branch)
- **Development Host**: Linux, macOS, or Windows with ESP-IDF environment
- **AWS Account**: For KVS signaling (or use AppRTC for testing)

## 📱 Examples Overview

Choose the right example for your use case:

| Example | Description | Hardware | Use Case |
|---------|-------------|----------|----------|
| **[webrtc_classic](examples/webrtc_classic/)** | 🏆 **Start here!** Complete WebRTC on single device | ESP32/S3/C6 + camera | Simple streaming, prototyping, single device solutions |
| **[streaming_only](examples/streaming_only/)** | Media streaming device (split mode) | ESP32-P4 (main processor) | High-performance streaming, power optimization |
| **[signaling_only](examples/signaling_only/)** | Signaling device (split mode) | ESP32-C6 (network processor) | Power-efficient signaling, always-on connectivity |
| **[esp_camera](examples/esp_camera/)** | AppRTC compatible streaming | ESP32-CAM modules | Browser compatibility, no AWS account needed |

### What Should I Use?
- **👨‍💻 New to WebRTC?** → Start with `webrtc_classic`
- **🔋 Need power optimization?** → Use split mode (`streaming_only` + `signaling_only`)
- **🌐 Want browser compatibility?** → Try `esp_camera` with AppRTC
- **🛠️ Building custom signaling?** → See [Custom Signaling Guide](#custom-signaling-integration)

## Directory Structure

- `components/`: Contains Espressif-specific dependency code and components required for the port
- `examples/`: Contains example applications ready to be built using the ESP-IDF build system
