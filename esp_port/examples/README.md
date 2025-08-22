# 📚 ESP WebRTC Examples

This directory contains example applications that demonstrate the **new simplified WebRTC API** for ESP32 devices. All examples now use a consistent, easy-to-use interface with reasonable defaults and advanced configuration options.

## 🎯 Choose Your Example

| Example | Use Case | Hardware | AWS Required | Best For |
|---------|----------|----------|--------------|----------|
| **[webrtc_classic](webrtc_classic/README.md)** | 🏆 **Start here!** Single-device streaming | ESP32/S3/P4 + camera | ✅ Yes (KVS) | Learning WebRTC, simple setups |
| **[esp_camera](esp_camera/README.md)** | Browser-compatible streaming | ESP32-CAM modules | ❌ No (AppRTC) | Testing, no AWS account |
| **[streaming_only](streaming_only/README.md)** | Power-optimized media device | ESP32-P4 (split mode) | ❌ No (bridge) | High performance, power saving |
| **[signaling_only](signaling_only/README.md)** | Always-on signaling device | ESP32-C6 (split mode) | ✅ Yes (KVS) | Power optimization, IoT |

## 🚀 Simplified API Overview

All examples now use the new unified API that provides:

🔧 **Credentials Management Simplified**: ESP RainMaker integration now uses centralized `esp_rmaker_get_aws_security_token()` for streamlined AWS credential handling with automatic memory management.

### Minimal Configuration
```c
// Only 4 essential settings needed
app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
config.signaling_client_if = kvs_signaling_client_if_get();
config.signaling_cfg = &signaling_config;
config.peer_connection_if = kvs_peer_connection_if_get();
config.video_capture = media_stream_get_video_capture_if();

app_webrtc_init(&config);
app_webrtc_run();
```

### Smart Defaults
- **Role**: MASTER (initiates connections)
- **Codecs**: H.264 (video) + OPUS (audio)
- **ICE**: Trickle ICE + TURN servers enabled
- **Mode**: Depending upon the pluggable interfaces chosen (signaling_client_if and peer_connection_if), the example behaves accordingly

### Advanced Configuration APIs
```c
// Override defaults after initialization
app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);
app_webrtc_enable_media_reception(true);
app_webrtc_set_ice_config(false, true); // Disable trickle ICE
app_webrtc_set_log_level(2); // Enable debug logging
```

## 🏗️ Architecture Patterns

### Pattern 1: Classic Mode (Single Device)
```
┌─────────────────────────────────┐
│         ESP32 Device            │
│  ┌─────────────┐ ┌─────────────┐│
│  │  Signaling  │ │   Media     ││
│  │  (AWS KVS)  │ │  Streaming  ││
│  └─────────────┘ └─────────────┘│
└─────────────────────────────────┘
```
**Examples**: webrtc_classic, esp_camera

### Pattern 2: Split Mode (Two Devices)
```
┌─────────────────┐    ┌─────────────────┐
│    ESP32-C6     │    │    ESP32-P4     │
│ ┌─────────────┐ │◄──►│ ┌─────────────┐ │
│ │  Signaling  │ │IPC │ │   Media     │ │
│ │  (AWS KVS)  │ │    │ │  Streaming  │ │
│ └─────────────┘ │    │ └─────────────┘ │
└─────────────────┘    └─────────────────┘
```
**Examples**: signaling_only + streaming_only

## 🔧 Building Examples

### Quick Start
```bash
cd examples/webrtc_classic     # Choose your example
idf.py set-target esp32s3      # Set your target
idf.py menuconfig             # Configure WiFi & credentials
idf.py build flash monitor    # Build, flash, and monitor
```

### Prerequisites
- **ESP-IDF v5.4** or v5.5
- **Applied patches** (see main README)
- **WiFi network** with internet access
- **AWS account** (for KVS examples) or use esp_camera for testing

### Common Configuration
All examples require WiFi configuration in menuconfig:
```
Example Configuration Options:
  ESP_WIFI_SSID = "YourWiFiNetwork"
  ESP_WIFI_PASSWORD = "YourPassword"
```

## 📖 Documentation Structure

Each example includes:
- **README.md**: Complete setup guide with hardware requirements
- **Troubleshooting**: Common issues and solutions
- **Architecture**: Technical details and code examples
- **Advanced Configuration**: Performance tuning and customization

For detailed instructions, refer to the README file within each example directory.
