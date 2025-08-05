# ICE Server Bridge Architecture

## Overview

This document explains the **ICE Server Bridge** implementation that enables proper TURN credential sharing between `signaling_only` and `streaming_only` devices in split-mode WebRTC configurations.

## Problem Statement

In split-mode WebRTC setups:
- **signaling_only device**: Connects to AWS KVS signaling and retrieves ICE servers (including TURN credentials)
- **streaming_only device**: Handles media streaming but has no direct access to signaling

**Without ICE server bridging**, the streaming device only has access to hardcoded STUN servers, causing connection failures when NAT traversal requires TURN servers.

## Solution Architecture

### ICE Server Bridge Flow

```
┌─────────────┐    ┌──────────────┐    ┌─────────┐    ┌──────────────┐    ┌──────────────┐
│ Main Task   │    │Signaling_Only│    │ AWS KVS │    │WebRTC_Bridge │    │Streaming_Only│
└─────┬───────┘    └──────┬───────┘    └────┬────┘    └──────┬───────┘    └──────┬───────┘
      │                   │                 │                │                   │
      │ 1. Initialization Phase             │                │                   │
      ├─app_webrtc_init()─►                 │                │                   │
      ├─app_webrtc_run()──►                 │                │                   │
      │    [non-blocking] │                 │                │                   │
      │                   │                 │                │                   │
      │ 2. Signaling Connection             │                │                   │
      │                   ├─Connect────────►│                │                   │
      │                   │◄─ICE servers────┤                │                   │
      │                   │ (STUN/TURN with │                │                   │
      │                   │   credentials)  │                │                   │
      │                   │◄─SIGNALING──────┤                │                   │
      │                   │   CONNECTED     │                │                   │
      │                   │   event         │                │                   │
      │                   │                 │                │                   │
      │ 3. ICE Server Transfer (Async)      │                │                   │
      │                   │                 │                │                   │
      │                   ├─schedule_ice────┤                │                   │
      │                   │  servers_       │                │                   │
      │                   │  transfer()     │                │                   │
      │                   │                 │                │                   │
      │                   │ [Work Queue]    │                │                   │
      │                   ├─webrtcAppGet────┤                │                   │
      │                   │  IceServers()   │                │                   │
      │                   ├─create_ice──────┤                │                   │
      │                   │  servers_msg()  │                │                   │
      │                   ├─ICE_SERVERS────►├─Forward ICE───►│                   │
      │                   │   message       │  configuration │                   │
      │                   │                 │                ├─Store ICE servers─┤
      │                   │                 │                │in bridge_signaling│
      │                   │                 │                │                   │
      │ 4. WebRTC Negotiation               │                │                   │
      │                   │◄─Offer received─┤                │                   │
      │                   │                 │                │                   │
      │                   ├─Forward offer──►├─Offer─────────►│                   │
      │                   │                 │                │                   │
      │                   │                 │                ├─bridgeGetIce ─────┤
      │                   │                 │                │  Servers()        │
      │                   │                 │                │  returns stored   │
      │                   │                 │                │  ICE servers      │
      │                   │                 │                │                   │
      │                   │                 │                ├─ Create peer──────┤
      │                   │                 │                │  connection with  │
      │                   │                 │                │  TURN credentials │
      │                   │                 │                │                   │
      │                   │                 │                │ ✅ TURN candidates│
      │                   │                 │                │   gathered        │
      │                   │                 │                │   successfully!   │
```

## Key Components

### 1. Signaling Serializer Extension
**Location**: `esp_port/components/signaling_serializer/`

**Added Message Type**:
```c
typedef enum {
    SIGNALING_MSG_TYPE_OFFER,
    SIGNALING_MSG_TYPE_ANSWER,
    SIGNALING_MSG_TYPE_ICE_CANDIDATE,
    SIGNALING_MSG_TYPE_ICE_SERVERS,  // ← NEW: Transfer ICE server configuration
} signaling_msg_type;
```

**ICE Server Data Structures**:
```c
typedef struct {
    char urls[127 + 1];                    // STUN/TURN URL
    char username[256 + 1];               // TURN username
    char credential[256 + 1];             // TURN password
} ss_ice_server_t;

typedef struct {
    uint32_t ice_server_count;
    ss_ice_server_t ice_servers[16];      // Array of ICE servers
} ss_ice_servers_payload_t;
```

### 2. Signaling_Only Device (Sender)
**Location**: `esp_port/examples/signaling_only/`

**Key Functions**:
- `send_ice_servers_to_streaming_device()`: Retrieves and forwards ICE servers
- `app_webrtc_get_ice_servers()`: Gets ICE config from signaling client
- Event-driven timing using `APP_WEBRTC_EVENT_SIGNALING_CONNECTED`

### 3. Streaming_Only Device (Receiver)
**Location**: `esp_port/examples/streaming_only/components/bridge_signaling/`

**Key Changes**:
- `BridgeSignalingClientData`: Stores received ICE servers
- `bridgeGetIceServers()`: Returns stored ICE servers instead of empty list
- ICE server message handling in bridge message processor

## Critical Timing Considerations

### ⏰ TURN Credential Expiration
TURN credentials from AWS typically expire in **~5 seconds**. The implementation ensures:

1. **Fresh Credentials**: ICE servers are retrieved only after signaling connects
2. **Event-Driven**: Triggered by `SIGNALING_CONNECTED` event, executed via work queue
3. **Non-Blocking**: Event callbacks remain lightweight, heavy work delegated to worker thread
4. **Just-In-Time**: Minimal delay between retrieval and WebRTC usage
5. **Robust**: Work queue provides proper error handling and task management

### 🔄 Event Trigger Points
```c
// Primary: Schedule ICE servers when signaling establishes fresh connection
case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
    schedule_ice_servers_transfer();  // Non-blocking work queue scheduling
    break;

// Secondary: Schedule ICE servers after AWS ICE fetch completes
case APP_WEBRTC_EVENT_SIGNALING_GET_ICE:
    schedule_ice_servers_transfer();  // Ensures ICE servers are available
    break;

// Backup: Schedule ICE servers before WebRTC negotiation
case APP_WEBRTC_EVENT_RECEIVED_OFFER:
    if (!ice_servers_sent) {
        schedule_ice_servers_transfer();  // Non-blocking work queue scheduling
    }
    break;

// Reset: Clear flag when connection drops (credentials invalid)
case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
    ice_servers_sent = false;
    break;
```

### 🧵 Work Queue Architecture
```c
// Lightweight event callback - just schedules work
static void schedule_ice_servers_transfer(void) {
    esp_work_queue_add_work(ice_servers_work_task, NULL);
}

// Heavy lifting done in dedicated worker thread
static void ice_servers_work_task(void* pvParameters) {
    // Retrieve ICE servers from signaling client
    // Create and serialize ICE servers message
    // Send via bridge to streaming device
}
```

## Usage

### Prerequisites
1. Both devices must be compiled with the bridge signaling support
2. Signaling serializer must be initialized: `signaling_serializer_init()`
3. Bridge communication must be established between devices

### Configuration
No additional configuration required - the bridge automatically detects and forwards ICE server messages.

### Debug Logging
Enable comprehensive logging to verify the flow:

**Signaling_Only Device**:
```
[signaling_only] Signaling connected successfully
[signaling_only] 🔄 Signaling connected - sending fresh ICE servers to streaming device
[signaling_only] 📤 Sending 2 ICE servers to streaming device
[signaling_only] ✅ Successfully sent ICE servers configuration to streaming device
```

**Streaming_Only Device**:
```
[bridge_signaling] 🔧 Received ICE servers configuration from signaling device
[bridge_signaling] ✅ Stored 2 ICE servers for WebRTC connection
[bridge_signaling] ICE Server 0: turns:54.202.170.151:443?transport=tcp (user: 1234567890:webrtc)
[bridge_signaling] ICE Server 1: stun:stun.kinesisvideo.us-east-1.amazonaws.com:443
[bridge_signaling] 🎯 Bridge signaling provided 2 ICE servers from signaling device
```

## Before vs After

### ❌ Before (Without ICE Bridge)
```
# Streaming device logs
iceAgentStartGathering(): [Srflx candidates setup time] Time taken: 0 ms
iceAgentStartGathering(): [Relay candidates setup time] Time taken: 0 ms
onConnectionStateChange(): New connection state 5  # FAILED
```

### ✅ After (With ICE Bridge)
```
# Streaming device logs
[bridge_signaling] 🎯 Bridge signaling provided 2 ICE servers from signaling device
iceAgentStartGathering(): [Srflx candidates setup time] Time taken: 150 ms
iceAgentStartGathering(): [Relay candidates setup time] Time taken: 320 ms
onConnectionStateChange(): New connection state 4  # CONNECTED
```

## Troubleshooting

### Issue: No ICE servers received
**Symptoms**: `❌ No ICE servers received from signaling device yet`

**Solutions**:
1. Verify signaling device connected successfully
2. Check bridge communication is working
3. Ensure `useTurn = TRUE` in signaling device config
4. Check work queue is properly initialized and running
5. Verify `esp_work_queue_add_work()` succeeds without errors

### Issue: TURN candidates not gathered
**Symptoms**: Only host candidates, no relay candidates

**Solutions**:
1. Verify ICE servers contain TURN URLs (starts with `turns:`)
2. Check TURN credentials are not expired
3. Ensure ESP-TLS is configured if using `turns:` over TCP

### Issue: Timing issues
**Symptoms**: Credentials expired before use

**Solutions**:
1. Verify ICE servers scheduled from event callback via work queue
2. Check `SIGNALING_CONNECTED` event is triggered properly
3. Ensure work queue is not overloaded or blocked
4. Reduce delay between ICE server retrieval and WebRTC connection

### Issue: Work queue scheduling failures
**Symptoms**: `❌ Failed to schedule ICE servers transfer`

**Solutions**:
1. Verify `esp_work_queue_init()` and `esp_work_queue_start()` called successfully
2. Check available heap memory for work queue operations
3. Ensure work queue task stack size is sufficient
4. Monitor work queue task execution logs

## Related Files

- `esp_port/components/signaling_serializer/` - Message serialization
- `esp_port/examples/signaling_only/main/signaling_only_main.c` - Sender logic
- `esp_port/examples/streaming_only/components/bridge_signaling/` - Receiver logic
- `esp_port/components/kvs_webrtc/src/app_webrtc.c` - ICE server retrieval
- `esp_port/components/kvs_webrtc/src/Tls_esp.c` - ESP-TLS for TURN over TCP

## Performance Impact

- **Memory**: ~10KB additional for ICE server storage per device
- **Network**: One additional message transfer (~1KB) per session
- **Latency**: ~10ms additional setup time for ICE server transfer
- **CPU**: Minimal - simple structure copying and serialization
- **Event Callbacks**: Remain lightweight due to work queue delegation
- **Concurrency**: Non-blocking operation allows parallel processing

### Work Queue Benefits
- **Responsiveness**: Event callbacks execute quickly (~1ms)
- **Reliability**: Dedicated worker thread handles network operations
- **Error Handling**: Work queue provides built-in retry and error recovery
- **Resource Management**: Automatic task scheduling and stack management

The performance impact is negligible compared to the significant reliability improvement for WebRTC connections requiring NAT traversal.
