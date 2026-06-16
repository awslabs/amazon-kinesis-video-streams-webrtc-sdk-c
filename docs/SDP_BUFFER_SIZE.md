# SDP Buffer Size Configuration

## What is this?

When two WebRTC peers connect through the KVS Signaling service, they exchange Session Description Protocol (SDP) messages that describe their media capabilities (codecs, tracks, ICE candidates, etc.). These messages travel through the signaling channel as base64-encoded payloads inside a JSON envelope.

The SDK uses fixed-size buffers to hold these messages at each stage of the pipeline. By default, these are sized for typical use cases. If your application produces larger SDPs (e.g. many media tracks, or many candidates), you may need to increase the buffer. If you're on a memory-constrained embedded device with predictable SDP shapes, you may want to reduce it.

This document explains how the buffers relate, how to configure them, and how to diagnose size-related failures.

---

## Buffer Pipeline

When the SDK receives a signaling message (e.g., an SDP offer from a remote peer), data flows through three buffers:

```
┌───────────────────────────────────────────────────────────────────────────┐
│                        KVS Signaling Service                              │
│                                                                           │
│  SendSdpOffer / SendSdpAnswer / SendIceCandidate                          │
│                                                                           │
└────────────────────────────────┬──────────────────────────────────────────┘
                                 │ WebSocket frame
                                 ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  Step 1. LWS_MESSAGE_BUFFER_SIZE (WebSocket receive buffer)                    │
│                                                                           │
│  Holds (accumulates) the raw WebSocket frames as they arrive.             │
│  Emits a callback when the entire message arrives.                        │
│                                                                           │
│  Example of a JSON message from the signaling service:                    │
│                                                                           │
│  {"messageType":"SDP_OFFER",                                              │
│   "senderClientId":"viewer-abc",                                          │
│   "messagePayload":"eyJ0eXBlIjoib2ZmZXIiLCJzZHAiOiJ2PTBcclxuby0uLi4="}    │
│                                                                           │
│  Size: derived from MAX_SIGNALING_MESSAGE_LEN + alignment bytes           │
└────────────────────────────────┬──────────────────────────────────────────┘
                                 │ JSON parsed
                                 ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  Step 2. MAX_SIGNALING_MESSAGE_LEN (SignalingMessage.payload[])           │
│                                                                           │
│  Holds the parsed "messagePayload" field - still base64-encoded.          │
│  This is the content of the "messagePayload" JSON field:                  │
│                                                                           │
│  eyJ0eXBlIjoib2ZmZXIiLCJzZHAiOiJ2PTBcclxuby0uLi4=                         │
│                                                                           │
│  Size: (MAX_SESSION_DESCRIPTION_INIT_SDP_LEN * 4/3) + 1024                │
│  The 4/3 accounts for base64 expansion; 1024 for the inner JSON keys.     │
└────────────────────────────────┬──────────────────────────────────────────┘
                                 │ base64 decoded + JSON parsed
                                 ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  Step 3. MAX_SESSION_DESCRIPTION_INIT_SDP_LEN                             │
│                               (RtcSessionDescriptionInit.sdp[])           │
│                                                                           │
│  Holds the decoded SDP text, the final plain-text session description:    │
│                                                                           │
│  v=0                                                                      │
│  o=- 5242433420278410664 2 IN IP4 127.0.0.1                               │
│  s=-                                                                      │
│  t=0 0                                                                    │
│  a=group:BUNDLE 0 1                                                       │
│  m=audio 9 UDP/TLS/RTP/SAVPF 111 ...                                      │
│  ...                                                                      │
│                                                                           │
│  Size: KVS_SDP_BUFFER_SIZE (default 25000 bytes)                          │
│  This is the ONE value you configure. Everything else derives from it.    │
└───────────────────────────────────────────────────────────────────────────┘
```

### Step 1: WebSocket receive buffer (`LWS_MESSAGE_BUFFER_SIZE`)

The first buffer that incoming data hits. It accumulates WebSocket frame fragments from the signaling service into a single contiguous message. The content at this stage is the **full JSON envelope** from the service — including `messageType`, `senderClientId`, and the base64-encoded `messagePayload` field.

If the incoming frame exceeds this buffer, the SDK rejects it with `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` and logs the size mismatch.

### Step 2: Signaling message buffer (`MAX_SIGNALING_MESSAGE_LEN`)

After the JSON envelope is parsed, the `messagePayload` field is extracted into `SignalingMessage.payload[]`. At this stage the content is **still base64-encoded** — it's the raw `messagePayload` string from the signaling service JSON.

The size is derived as `(MAX_SESSION_DESCRIPTION_INIT_SDP_LEN * 4/3) + 1024`. The `4/3` factor accounts for base64 encoding expansion (3 raw bytes become 4 base64 characters). The `1024` accounts for the inner JSON keys (`{"type":"offer","sdp":"..."}`) that wrap the SDP inside the payload.

### Step 3: Decoded SDP buffer (`MAX_SESSION_DESCRIPTION_INIT_SDP_LEN`)

The final destination. The base64 payload is decoded, revealing a JSON object with `type` and `sdp` fields. The `sdp` value (plain-text SDP) is copied into `RtcSessionDescriptionInit.sdp[]`.

If the decoded SDP exceeds this size, the SDK rejects it with `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED`.

### Size relationship

All three buffers are derived from a single knob (`KVS_SDP_BUFFER_SIZE`), which represents the **on-wire signaling message size** (Step 2):

| Step   | Buffer                                 | Default   | With `KVS_SDP_BUFFER_SIZE`           | What it holds                                |
|:------:|----------------------------------------|-----------|--------------------------------------|----------------------------------------------|
|   1    | `LWS_MESSAGE_BUFFER_SIZE`              | ~18766    | `KVS_SDP_BUFFER_SIZE + alignment`    | Full WebSocket frame (JSON envelope)         |
|   2    | `MAX_SIGNALING_MESSAGE_LEN`            | 18750     | `KVS_SDP_BUFFER_SIZE`                | Base64-encoded payload + inner JSON overhead |
|   3    | `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` | 25000     | `(KVS_SDP_BUFFER_SIZE - 1024) * 3/4` | Decoded SDP text                             |

You set `KVS_SDP_BUFFER_SIZE` which directly controls the signaling message size (Step 2). Step 3 is derived downward (accounting for base64 decoding), and Step 1 is derived upward (adding alignment).

> [!NOTE]
> The legacy defaults (18750/25000) don't follow the formula. The signaling buffer is too small to deliver a full 25 KB SDP. The effective decoded SDP limit with defaults is ~13 KB. Setting `KVS_SDP_BUFFER_SIZE` fixes this by properly coordinating both values.

---

## Configuration

The value you set is the **signaling message size**: the on-wire message including base64-encoded payload.
The SDK derives the decoded SDP buffer from it: `decoded_sdp_limit = (KVS_SDP_BUFFER_SIZE - 1024) * 3/4`.

Also see: https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/SendSdpOffer.html

### Build-time CMake variable

```bash
# Default
cmake ..

# Maximum
cmake .. -DKVS_SDP_BUFFER_SIZE=40000
```

| Parameter  | Value                                        |
|------------|----------------------------------------------|
| Default    | Not set (legacy: signaling=18750, SDP=25000) |
| Minimum    | 10000 bytes                                  |
| Maximum    | 40000 bytes                                  |

### When to increase

- You're seeing `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED` or `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` errors
- Your remote peers send SDPs with many media sections (e.g., multi-track configurations)
- You include ICE candidates in the SDP (non-trickle ICE) which inflates SDP size

### When to decrease

> [!CAUTION]
> **Advanced option.** Do not reduce the buffer size unless you have measured and confirmed the maximum signaling message size your viewers will produce. Browser updates may add new codecs or extensions that increase SDP size without warning. If you reduce the buffer below what a viewer sends, the offer will get rejected.

- Running on memory-constrained embedded hardware (<1 GB RAM)
- You have a fixed, predictable SDP shape (e.g., single-camera robot that always negotiates 1 video + 1 audio track)
- You want to reduce stack usage - `RtcSessionDescriptionInit` is stack-allocated at multiple call sites

### Stack impact

`RtcSessionDescriptionInit` contains a `CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1]` field and is stack-allocated.
Increasing `KVS_SDP_BUFFER_SIZE` increases this derived value and thus stack pressure at each call site.
`SignalingMessage` also grows (its `payload[]` is sized to `KVS_SDP_BUFFER_SIZE + 1`). If you increase significantly, verify your thread stack size can accommodate it:

```bash
# Check system default stack size
ulimit -s    # typically 8192 KB on Linux, 8 MB on macOS
```

The SDK's `KVS_STACK_SIZE` CMake variable can be used to set thread stack size if needed.

---

## Troubleshooting

### Error log messages

Enable ERROR level logs to see rejection messages.

| Log message | What it means | What to do                                                                              |
|-------------|---------------|-----------------------------------------------------------------------------------------|
| `Received SDP size (X bytes) exceeds configured MAX_SESSION_DESCRIPTION_INIT_SDP_LEN (Y bytes). Increase KVS_SDP_BUFFER_SIZE in CMake to accommodate larger SDPs.` | A remote peer sent an SDP that doesn't fit in the decoded buffer. | Rebuild with `-DKVS_SDP_BUFFER_SIZE=<value larger than X>`                              |
| `Signaling message size (X bytes so far) exceeds receive buffer (Y bytes). Increase KVS_SDP_BUFFER_SIZE in CMake to accommodate larger messages.` | The raw WebSocket frame from the signaling service doesn't fit in the LWS receive buffer. | Same fix: increase `KVS_SDP_BUFFER_SIZE`. The LWS buffer derives from it automatically. |

### Relevant status codes

| Status code | Name | Meaning |
|-------------|------|---------|
| `0x5200000d` | `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED` | Decoded SDP exceeds `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` |
| `0x5e000015` | `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` | Raw signaling message exceeds WebSocket receive buffer |
| `0x52000008` | `STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING` | Outbound message (after base64 encoding) exceeds send buffer |

## Reference

### Related defines

| Define                                 | Location        | Description                                                 |
|----------------------------------------|-----------------|-------------------------------------------------------------|
| `KVS_SDP_BUFFER_SIZE`                  | CMake variable  | The single configuration point (signaling message size)     |
| `LWS_MESSAGE_BUFFER_SIZE`              | `LwsApiCalls.h` | WebSocket receive buffer (derived from signaling len)       |
| `MAX_SIGNALING_MESSAGE_LEN`            | `Include.h`     | Signaling message buffer (= `KVS_SDP_BUFFER_SIZE` or 18750) |
| `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` | `Include.h`     | Decoded SDP buffer (derived from signaling len, or 25000)   |

### Struct fields sized by these defines

```c
typedef struct {
    SDP_TYPE type;
    CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1];  // ← derived from KVS_SDP_BUFFER_SIZE
} RtcSessionDescriptionInit;

typedef struct {
    // ...
    CHAR payload[MAX_SIGNALING_MESSAGE_LEN + 1];  // ← set by KVS_SDP_BUFFER_SIZE
} SignalingMessage;
```
