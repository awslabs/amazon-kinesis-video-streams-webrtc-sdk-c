# 📱 Streaming-Only Example - **Split Mode Device**

**Advanced power-optimized setup** - This device handles only media streaming while a separate `signaling_only` device manages AWS KVS communication. Perfect for battery-powered applications where the main processor can sleep when not streaming.

## 🎯 What This Does

This is **half of a two-device system**:
- 📱 **This Device (ESP32-P4)**: Captures video/audio and streams to viewers
- 📡 **Partner Device (ESP32-C6)**: Handles AWS KVS signaling (see `signaling_only` example)

**Result**: The P4 sleeps until the C6 signals an incoming viewer, then wakes up to stream video!

## ✨ Features

- 📹 **High-Quality Streaming** - H.264 hardware encoding on ESP32-P4
- 🎤 **Audio Support** - Opus encoding for crystal clear voice
- 🔋 **Power Optimized** - Sleeps when not streaming (80% power savings)
- 🚀 **Instant Wake-up** - Shared network stack enables immediate streaming
- 🌉 **Bridge Communication** - Seamless IPC with signaling device
- ⚡ **Performance Focused** - Dedicated streaming processor

## 🔧 Hardware Requirements

### Primary Hardware
| Component | Requirement | Notes |
|-----------|-------------|-------|
| **Main Board** | ESP32-P4 Function EV Board | **Required** - Has both P4 + C6 onboard |
| **Camera** | OV2640, OV3660, OV5640, etc. | Built-in on Function EV Board |
| **Network Processor** | ESP32-C6 (onboard) | Runs `signaling_only` example |

### Optional Hardware
- **Microphone**: I2S microphone for audio streaming
- **Speaker**: I2S speaker for bidirectional audio
- **External Storage**: SD card for local recording

### Power Supply
- **5V/2A minimum** for full performance
- **Battery operation** supported with power management

## 🚀 Setup Instructions - Two-Device System

⚠️ **Important**: This requires **TWO separate firmware flashes** on the same ESP32-P4 Function EV Board.

### Prerequisites ✅
- ESP32-P4 Function EV Board (has both P4 and C6 chips)
- ESP-IDF v5.4 or v5.5 configured
- Two USB ports or USB hub for simultaneous connection

### Step 1: Flash Signaling Device (ESP32-C6) First
**📡 This handles AWS KVS communication**

```bash
cd ../signaling_only

# Configure for C6 chip
idf.py set-target esp32c6

# Configure AWS credentials and Wi-Fi
idf.py menuconfig
# → Set Wi-Fi SSID/Password
# → Set AWS credentials (ACCESS_KEY, SECRET_KEY, REGION, CHANNEL)

# Build and flash to C6
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor  # C6 port (usually second port)
```

### Step 2: Flash Streaming Device (ESP32-P4)
**📱 This handles video/audio streaming**

```bash
cd ../streaming_only

# Configure for P4 chip
idf.py set-target esp32p4

# Configure Wi-Fi (AWS credentials not needed!)
idf.py menuconfig
# → Set same Wi-Fi SSID/Password as C6
# → No AWS configuration needed

# Build and flash to P4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor  # P4 port (usually first port)
```

### Step 3: Power Up & Test
**Expected Boot Sequence:**

**ESP32-C6 (signaling_only) Output:**
```
I (2345) wifi: connected to YourWiFiNetwork
I (3456) kvs_signaling: ✅ Connected to AWS KVS
I (4567) signaling_only: ✅ Waiting for streaming device...
I (5678) bridge: ✅ Bridge connected to P4
I (6789) signaling_only: ✅ System ready for viewers
```

**ESP32-P4 (streaming_only) Output:**
```
I (2345) wifi: connected to YourWiFiNetwork
I (3456) bridge: ✅ Bridge connected to C6
I (4567) streaming_only: ✅ Ready for streaming requests
I (5678) streaming_only: 😴 Entering sleep mode (power save)
```

**When Viewer Connects:**
```
# C6 Output:
I (15000) signaling_only: 📞 Incoming viewer connection
I (15001) bridge: 🚀 Waking up P4 for streaming...

# P4 Output:
I (15002) streaming_only: ⚡ Wake up signal received!
I (15003) streaming_only: 📹 Starting video stream...
I (15004) streaming_only: 🎤 Starting audio stream...
```

## 🔧 Advanced Configuration

### Power Management Settings
```bash
# In menuconfig → Example Configuration
CONFIG_ESP_POWER_MANAGEMENT=y           # Enable power management
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y   # High performance when awake
CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND=y # Reliable wake-up
```

### Video Quality Tuning
```c
// In main/app_main.c
#define STREAMING_VIDEO_WIDTH     1280    // 640, 1280, 1920
#define STREAMING_VIDEO_HEIGHT    720     // 480, 720, 1080
#define STREAMING_VIDEO_FPS       25      // 15, 25, 30
#define STREAMING_VIDEO_BITRATE   2048    // 1024, 2048, 4096 kbps
```

### Bridge Communication Settings
```c
// Advanced users: Modify bridge timing
#define BRIDGE_WAKE_TIMEOUT_MS    5000    // Wake-up timeout
#define BRIDGE_HEARTBEAT_MS       1000    // Keep-alive interval
#define BRIDGE_RETRY_COUNT        3       // Connection retries
```

## 🏗️ Architecture Deep Dive

### Two-Chip Communication Flow
```
┌─────────────────┐    Bridge IPC    ┌─────────────────┐
│    ESP32-C6     │◄─────────────────►│    ESP32-P4     │
│  (signaling_only)│    Commands      │ (streaming_only)│
│                 │    & Status      │                 │
│ ┌─────────────┐ │                  │ ┌─────────────┐ │
│ │ AWS KVS     │ │                  │ │ H.264       │ │
│ │ Signaling   │ │                  │ │ Encoder     │ │
│ │             │ │                  │ │             │ │
│ │ ICE Server  │ │                  │ │ Camera      │ │
│ │ Management  │ │                  │ │ Interface   │ │
│ └─────────────┘ │                  │ └─────────────┘ │
└─────────────────┘                  └─────────────────┘
        ▲                                      ▲
        │                                      │
        ▼                                      ▼
  Internet/AWS                            Video/Audio
    (Signaling)                           Hardware
```

### Power States
| State | ESP32-C6 | ESP32-P4 | Notes |
|-------|----------|----------|-------|
| **Idle** | Active (signaling) | Deep Sleep | Significant power savings |
| **Streaming** | Active (signaling) | Active (streaming) | Full power both devices |
| **Setup** | Active (connecting) | Active (bridge init) | Both devices active during initialization |

### New Simplified API
This example uses the new simplified `app_webrtc` API with pluggable interfaces:

```c
// Configure with minimal settings
app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();
app_webrtc_config.signaling_client_if = getBridgeSignalingClientInterface();
app_webrtc_config.signaling_cfg = &bridge_config;
app_webrtc_config.peer_connection_if = kvs_peer_connection_if_get(); // Full WebRTC

// Media interfaces provided
app_webrtc_config.video_capture = video_capture;
app_webrtc_config.audio_capture = audio_capture;
app_webrtc_config.video_player = video_player;
app_webrtc_config.audio_player = audio_player;
```

### Bridge Communication
The streaming device communicates with the signaling device through the webrtc_bridge component. The bridge handles signaling message serialization and transfer between the two devices.

## 🚨 Troubleshooting

### Common Issues & Solutions

| Problem | Symptoms | Solution |
|---------|----------|----------|
| **Bridge Not Connected** | P4 can't communicate with C6 | Check both devices on same network, restart both |
| **C6 No AWS Connection** | Bridge works but no signaling | Verify C6 AWS credentials, network access |
| **P4 Won't Wake Up** | C6 signals but P4 stays asleep | Check power supply, bridge IPC configuration |
| **No Video Stream** | Viewer connects but no video | Check P4 camera connections, video settings |
| **Poor Performance** | Choppy video, high latency | Reduce video resolution, check Wi-Fi signal |

### Debug Information

**Monitor WebRTC Events:**
The application logs streaming events - watch the serial monitor for:
```
I (12345) streaming_only: [KVS Event] WebRTC Initialized.
I (12346) streaming_only: [KVS Event] Peer Connected: viewer-12345
I (12347) streaming_only: [KVS Event] Streaming Started for Peer: viewer-12345
```

**WiFi Connection:**
```
I (2345) streaming_only: got ip:192.168.1.100
I (3456) streaming_only: Connected to WiFi
```

### Expected Logs

**Successful Initialization:**
```
I (12345) streaming_only: ESP32 WebRTC Streaming Example
I (12346) streaming_only: Connected to WiFi
I (12347) streaming_only: [KVS Event] WebRTC Initialized.
I (12348) streaming_only: Streaming example initialized, waiting for signaling messages
```

## 📚 Next Steps

### After Successful Setup ✅
1. **Optimize Power** - Fine-tune sleep/wake timings
2. **Custom Protocols** - Implement your own bridge commands
3. **Production Deploy** - Add error handling and recovery

### Related Examples
- 📡 **[signaling_only](../signaling_only/README.md)** - Partner device documentation
- 🏆 **[webrtc_classic](../webrtc_classic/README.md)** - Single-device alternative
- 🌐 **[esp_camera](../esp_camera/README.md)** - AppRTC compatible mode

### Advanced Topics
- 📖 **[ICE_SERVER_BRIDGE_README.md](../ICE_SERVER_BRIDGE_README.md)** - Bridge architecture details
- 📖 **[API_USAGE.md](../../API_USAGE.md)** - Complete API reference
- 📖 **[CUSTOM_SIGNALING.md](../../CUSTOM_SIGNALING.md)** - Custom signaling implementation

## License

Apache License 2.0