# 🌐 ESP Camera WebRTC Example - **AppRTC Compatible**

**No AWS account needed!** This example demonstrates WebRTC video streaming using AppRTC-compatible signaling. Connect your ESP32 camera directly to web browsers through Espressif's AppRTC server at `webrtc.espressif.com`.

## 🎯 What You'll Build

A browser-compatible WebRTC camera that works directly with AppRTC protocol - perfect for development and testing without AWS setup.

**Expected Result**: Create or join rooms through CLI commands, then view your ESP32 camera feed directly in any web browser!

## ✨ Features

- 📹 **Live Video Streaming** - H.264 encoding with real-time transmission
- 🎤 **Audio Support** - Opus encoding for clear voice transmission
- 🌐 **Browser Compatible** - Works with any WebRTC-capable browser
- 🔧 **CLI Control** - Interactive commands for room management
- 🏠 **Room System** - Create new rooms or join existing ones
- 👥 **Role-Based** - MASTER (create rooms) or VIEWER (join rooms) modes
- ⚡ **No AWS Required** - Uses Espressif's free AppRTC server

## 🔧 Hardware Requirements

### Supported Development Boards
| Board | Video | Audio | Notes |
|-------|-------|-------|-------|
| **ESP32-S3-EYE** | ✅ Built-in camera | ✅ Built-in mic | **Recommended** - Ready to use |
| **ESP32-CAM** | ✅ OV2640 camera | ➕ Add I2S mic | Popular, affordable option |
| **ESP32-P4-Function-EV-Board** | ✅ High-quality camera | ✅ Built-in mic | **Premium** - Best performance |

### Additional Hardware (Optional)
- **Microphone**: I2S microphone for audio streaming
- **Speaker**: I2S speaker for audio playback

## 🚀 Quick Start - Browser Streaming

### Prerequisites ✅
- ESP-IDF v5.4 or v5.5 installed and configured
- ESP32 development board with camera (ESP32-S3-EYE recommended)
- Wi-Fi network with internet access
- **No AWS account needed!**

### Step 1: Configure Target Device
```bash
cd examples/esp_camera

# Choose your ESP32 variant
idf.py set-target esp32s3     # For ESP32-S3-EYE (recommended)
# idf.py set-target esp32     # For ESP32-CAM
# idf.py set-target esp32p4   # For ESP32-P4 Function EV Board
```

### Step 2: Configure Wi-Fi & AppRTC Settings
```bash
idf.py menuconfig
```

**Navigate to: Example Configuration Options**

#### Wi-Fi Settings (Required)
```
ESP_WIFI_SSID = "YourWiFiNetwork"
ESP_WIFI_PASSWORD = "YourPassword"
```

#### AppRTC Settings (Optional)
```
APPRTC_ROLE_TYPE = 0          # 0=MASTER (creates rooms), 1=VIEWER (joins rooms)
APPRTC_AUTO_CONNECT = y       # Auto-connect or manual CLI mode
APPRTC_USE_FIXED_ROOM = n     # Use fixed room ID or create new
```

### Step 3: Configure Console Output

Different Dev boards have different options for CONSOLE and LOGs. You may want to configure the console output as per your board:

```bash
idf.py menuconfig
# Go to Component config -> ESP System Settings -> Channel for console output
# (X) USB Serial/JTAG Controller # For ESP32-P4 Function_EV_Board V1.2 OR V1.5
# (X) Default: UART0 # For ESP32-P4 Function_EV_Board V1.4
```

**Note**: If the console selection is wrong, you will only see the initial bootloader logs. Please change the console as instructed above and reflash the app to see the complete logs.

### Step 4: Build & Flash
```bash
# Build the project
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Expected output:
# I (12345) esp_webrtc_camera: ESP32 WebRTC Camera Example
# I (12346) esp_webrtc_camera: got ip:192.168.1.100
# I (12347) esp_webrtc_camera: Connected to WiFi
# I (12348) esp_webrtc_camera: [WebRTC Event] WebRTC Initialized.
```

### Step 5: Control WebRTC Rooms via CLI 🎮

**Available CLI Commands:**
```bash
# Create a new room
join-room new

# Join an existing room
join-room <room_id>

# Check current room
get-room

# View connection status
status

# Check your role (MASTER/VIEWER)
get-role

# Disconnect from room
disconnect

# Retry connection
retry-room
```

### Step 6: View Your Stream! 📺

**Option A: Auto-Connect Mode (Default)**
1. **Device automatically creates/joins a room**
2. **Watch for room URL in serial monitor:**
   ```
   I (15000) esp_webrtc_camera: Room URL: https://webrtc.espressif.com/r/abc123
   ```
3. **Open the URL in your browser** - you'll see live video!

**Option B: Manual CLI Mode**
1. **Create a new room:**
   ```
   join-room new
   ```
2. **Check room status:**
   ```
   status
   # Output: Room URL: https://webrtc.espressif.com/r/xyz789
   ```
3. **Open the room URL in your browser**

## 🔧 Advanced Configuration

### Role Types
- **MASTER Role**: Creates rooms and waits for viewers
- **VIEWER Role**: Can create rooms or join existing ones

### Manual Room Management
```bash
# Check current role
get-role

# Create new room as MASTER
join-room new

# Join specific room as VIEWER
join-room abc123

# Check connection status
status

# Retry if connection fails
retry-room
```

### Browser Testing
**Supported Browsers:** Chrome, Firefox, Edge, Safari
**Room URL Format:** `https://webrtc.espressif.com/r/<room_id>`

## 🚨 Troubleshooting

### Common Issues & Solutions

| Problem | Symptoms | Solution |
|---------|----------|----------|
| **Build Errors** | `STATUS.h not found` | Apply ESP-IDF patches (see main README) |
| **No Wi-Fi Connection** | `Failed to connect` | Check SSID/password in menuconfig |
| **AppRTC Connection Failed** | `Failed to connect to webrtc.espressif.com` | Check internet access, firewall settings |
| **No Video in Browser** | Browser shows "connecting" | Check camera connections, power supply |
| **Audio Issues** | No sound in browser | Verify microphone connections |

### Debug Information

**Monitor WebRTC Events:**
```
I (12345) esp_webrtc_camera: [WebRTC Event] WebRTC Initialized.
I (12346) esp_webrtc_camera: [WebRTC Event] Signaling Connected.
I (12347) esp_webrtc_camera: [WebRTC Event] Peer Connected: viewer-12345
I (12348) esp_webrtc_camera: [WebRTC Event] Streaming Started for Peer: viewer-12345
```

**CLI Command Output:**
```
join-room new
Creating a new room as MASTER...
Room join/create request queued successfully.
Check the logs for connection progress and WebRTC events.

status
WebRTC Status:
  Role: MASTER
  Signaling State: Connected
  Room ID: abc123def
  Room URL: https://webrtc.espressif.com/r/abc123def
```

### Expected Browser Behavior
1. **Open room URL** → Browser requests camera/microphone permissions
2. **Allow permissions** → You should see ESP32 camera feed immediately
3. **Audio/Video controls** → Use browser controls to mute/unmute

## 🏗️ Architecture Overview

This example demonstrates the simplified WebRTC API with AppRTC signaling:

```c
// Simplified configuration with AppRTC signaling
app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();
app_webrtc_config.signaling_client_if = apprtc_signaling_client_if_get();
app_webrtc_config.signaling_cfg = &apprtc_config;
app_webrtc_config.peer_connection_if = kvs_peer_connection_if_get();

// Media interfaces for bi-directional streaming
app_webrtc_config.video_capture = video_capture;
app_webrtc_config.audio_capture = audio_capture;
app_webrtc_config.video_player = video_player;
app_webrtc_config.audio_player = audio_player;

// Advanced API: Set role after initialization
app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_MASTER);
app_webrtc_enable_media_reception(true);
```

**Key Features:**
- **Simplified API**: Reasonable defaults with advanced configuration APIs
- **AppRTC Signaling**: Browser-compatible signaling via `webrtc.espressif.com`
- **CLI Control**: Interactive room management commands
- **Role-Based**: MASTER (create rooms) or VIEWER (join rooms) modes
- **No AWS Required**: Uses Espressif's free AppRTC server

## 📚 Next Steps

### After Success ✅
1. **Try Different Roles** - Test MASTER vs VIEWER modes
2. **Multi-Browser Testing** - Open same room in multiple browser tabs
3. **Custom Signaling** - See [CUSTOM_SIGNALING.md](../../CUSTOM_SIGNALING.md)

### Related Examples
- 🏆 **[webrtc_classic](../webrtc_classic/README.md)** - AWS KVS signaling version
- 📱 **[streaming_only](../streaming_only/README.md)** - Split mode streaming device
- 📡 **[signaling_only](../signaling_only/README.md)** - Split mode signaling device

### Documentation
- 📖 **[API_USAGE.md](../../API_USAGE.md)** - Complete API reference
- 📖 **[CUSTOM_SIGNALING.md](../../CUSTOM_SIGNALING.md)** - Custom signaling guide

## License

Apache License 2.0
