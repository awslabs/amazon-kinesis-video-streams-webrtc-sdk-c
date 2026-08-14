# SDP Limits and Buffer Sizing

The KVS WebRTC-C SDK uses fixed-size buffers and arrays to receive, hold, and parse SDP messages. These dimensions are made configurable at compile-time. This document consolidates them, so they are easy to find and compare. Use <kbd>Ctrl</kbd>+<kbd>F</kbd> for the CMake variable or error you are chasing.

## Summary

| Limit | CMake variable / define | Default | Range | What it bounds | Exceeded error |
|-------|-------------------------|---------|-------|----------------|----------------|
| Attribute count | `MAX_SDP_ATTRIBUTES_COUNT` | 256 | 32-2048 | `a=` attributes **per SDP section** (session-level and each media section) | `STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED` (`0x5600000F`) |
| Media count | `MAX_SDP_SESSION_MEDIA_COUNT` | 5 | 1-6 | `m=` lines (media sections) in an SDP | `STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT` (`0x5500000A`) |
| Buffer size: signaling message (on-wire, base64) | `KVS_SIGNALING_MESSAGE_LEN` → `MAX_SIGNALING_MESSAGE_LEN` | 18750 (legacy)* | 10000-40000 | Base64-encoded signaling message as received off the WebSocket | `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` (`0x5d000028`) |
| Buffer size: decoded SDP (un-base64'd) | derived → `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` | 25000 (legacy)* | `(KVS_SIGNALING_MESSAGE_LEN - 1024) * 3/4` | Plain-text SDP after base64 decode | `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED` (`0x55000006`) |

The two buffer-size rows are **not independent knobs**. Both are driven by the single `KVS_SIGNALING_MESSAGE_LEN` value: you set the on-wire signaling size, and the decoded SDP buffer is derived downward from it (accounting for base64 decoding). See [Buffer Size](#3-sdp-buffer-size-kvs_signaling_message_len).

\* If `KVS_SIGNALING_MESSAGE_LEN` is not set, the SDK uses legacy defaults (signaling buffer 18750, decoded SDP buffer 25000) that do not follow the derivation formula.

When diagnosing a rejection, match the *error code* to the right limit above.

Usage example:

```bash
cmake .. -DKVS_SIGNALING_MESSAGE_LEN=40000 -DMAX_SDP_SESSION_MEDIA_COUNT=6
cmake .. -DMAX_SDP_ATTRIBUTES_COUNT=320
```

---

## 1. SDP Attribute Count (`MAX_SDP_ATTRIBUTES_COUNT`)

### Configuration

`MAX_SDP_ATTRIBUTES_COUNT` controls how many `a=` attributes a single SDP section can hold. It applies both to session-level attributes and, separately, to each media (`m=`) section. The allowed range is **32 to 2048**. The floor of 32 is deliberately above the smallest real browser media section (Chrome 150 audio, 8 codecs, is 28 attributes) and comfortably above a minimal single-codec section (~12-20 attributes), so a lower value would reject ordinary offers and leave the SDK unable to parse a functional SDP.

**Default:** 256

**Override:** Pass `-DMAX_SDP_ATTRIBUTES_COUNT=N` to CMake:

```bash
cmake .. -DMAX_SDP_ATTRIBUTES_COUNT=320
```

### What is it?

Each `a=` line in an SDP section (`a=rtpmap`, `a=rtcp-fb`, `a=fmtp`, `a=extmap`, `a=ssrc`, …) is stored as one entry in a fixed-size `sdpAttributes[]` array. `MAX_SDP_ATTRIBUTES_COUNT` is that array's capacity.

### Why the count keeps growing

The number of attributes a peer advertises trends upward over time. Browser updates and new or revised RFCs continually add more for peers to negotiate: additional codecs and codec profiles, more RTP header extensions, and more feedback mechanisms. Much of this is emitted per payload type, so each new codec or feedback type multiplies the line count within a section rather than adding a single line.

Some examples of this growth:

- New per-payload-type RTCP feedback, such as `a=rtcp-fb:<pt> rrtr` and `a=rtcp-fb:<pt> ack ccfb` (RFC 8888 congestion control feedback), plus session-level lines like `a=rtcp-xr:rcvr-rtt=all`.
- Broader codec support: each additional codec or profile contributes its own `a=rtpmap` / `a=fmtp` / `a=rtcp-fb` lines, and usually a retransmission (`rtx`) payload type with its own lines.

A **recvonly** video transceiver is the worst case, because the browser offers *every codec it can decode* (a superset of the sendrecv set), so the payload-type list, and therefore the attribute count, is much larger.

### Memory implications

`MAX_SDP_ATTRIBUTES_COUNT` scales **both** the session-level attribute array **and** every media section's array inside `SessionDescription`:

```
Per-attribute slot = sizeof(SdpAttributes)
                   = attributeName[MAX_SDP_ATTRIBUTE_NAME_LENGTH + 1] (33)
                   + attributeValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH + 1] (513)
                   = 546 bytes

Attribute storage ~ MAX_SDP_ATTRIBUTES_COUNT x 546 x (1 session + MAX_SDP_SESSION_MEDIA_COUNT media sections)
```

At the defaults (256 attributes, 5 media sections + 1 session), this is roughly **819 KB**. Raising the cap adds about **3.2 KB per +1** (546 bytes across 6 arrays):

| `MAX_SDP_ATTRIBUTES_COUNT` | Per section | Total attribute storage (1 + 5) |
|----------------------------|-------------|----------------------------------|
| 256 (default) | ~136 KB | ~819 KB |
| 284 | ~151 KB | ~908 KB |
| 320 | ~170 KB | ~1.00 MB |
| 512 | ~273 KB | ~1.60 MB |

Unlike `MAX_SDP_SESSION_MEDIA_COUNT` (see below), the `SessionDescription` used to parse remote SDPs is **heap-allocated** in the PeerConnection path (`MEMCALLOC`), so raising this cap grows a heap allocation rather than the stack. (Unit tests that stack-allocate `SessionDescription` remain fine at these sizes.)

### Recommendation

Two levers address this, and a KVS deployment typically controls **both**: the mobile/browser viewer *and* the C SDK peer built into the application. Choose based on your application requirements:

- **Trim the viewer's offer (often the leaner fix).** The C SDK only *acts on* a subset of attributes: the ICE/DTLS parameters, `mid`, and the `rtpmap`/`fmtp`/`rtcp-fb` of the codecs it actually negotiates. Attributes for codecs it will never select, and the per-codec `rrtr`/`ack ccfb` feedback attached to them, are stored but never used. A large recvonly offer therefore fills the array mostly with entries the SDK ignores. Restricting the browser to the codecs you actually use keeps the section well under the cap and costs no extra memory. See the [viewer-side steps under Troubleshooting](#status_sdp_attribute_max_exceeded-error-0x5600000f).
- **Raise the cap.** Rebuild with `-DMAX_SDP_ATTRIBUTES_COUNT=N` (e.g. **320** for headroom). Appropriate when you must accept arbitrary/unmodified browser offers, or when you don't control the viewer, but note it enlarges *every* attribute array, including storage for attributes the SDK never reads.

Keep the default at 256 unless you have a specific reason to change it; it is sufficient for typical sendrecv negotiations.

### Troubleshooting

#### `STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED` error (`0x5600000F`)

`deserializeSessionDescription()` returns this when a section holds more `a=` attributes than `MAX_SDP_ATTRIBUTES_COUNT`.

**Resolution.** A deployment usually owns both peers, so either lever works. See [Recommendation](#recommendation) for how to choose. Often the leaner fix is to **shrink the browser's offer** rather than grow the C SDK's arrays, because the SDK ignores the extra codecs' attributes anyway.

- **Shrink the viewer's offer.** Reduce the codecs the browser offers so the media section stays under the cap. Use [`RTCRtpTransceiver.setCodecPreferences()`](https://developer.mozilla.org/en-US/docs/Web/API/RTCRtpTransceiver/setCodecPreferences) to restrict to just the codecs your application actually negotiates. Fewer payload types means proportionally fewer `a=rtpmap` / `a=rtcp-fb` / `a=fmtp` / `a=rrtr` lines:

  ```js
  const tx = pc.addTransceiver('video', { direction: 'recvonly' });
  const { codecs } = RTCRtpReceiver.getCapabilities('video');
  // Keep only H.264 (+ its retransmission), drop VP8/VP9/AV1/H.265/red/ulpfec/flexfec.
  tx.setCodecPreferences(codecs.filter(c => ['video/H264', 'video/rtx'].includes(c.mimeType)));
  ```

  `recvonly` transceivers are the worst case, because the browser offers *every* codec it can decode (see [Why the count keeps growing](#why-the-count-keeps-growing)); pruning them has the largest effect. The browser's per-payload-type RTCP feedback (`rrtr`, `ack ccfb`) is not directly controllable, so reducing the codec count, which multiplies that feedback, is the effective lever.
- **Raise the cap.** Rebuild with `-DMAX_SDP_ATTRIBUTES_COUNT=N`. Use this when you must accept arbitrary/unmodified browser offers or don't control the viewer; it enlarges every attribute array, including storage for attributes the SDK never reads.

> [!NOTE]
> Raising the byte buffers (`KVS_SIGNALING_MESSAGE_LEN`) does **not** help this case: the SDP can be well within the byte limit and still exceed the attribute count. This is a count limit, not a length limit.

---

## 2. SDP Media Count (`MAX_SDP_SESSION_MEDIA_COUNT`)

### Configuration

`MAX_SDP_SESSION_MEDIA_COUNT` controls how many `m=` lines (media sections) an SDP can hold. The allowed range is **1 to 6**.

**Default:** 5

**Override:** Pass `-DMAX_SDP_SESSION_MEDIA_COUNT=N` to CMake:

```bash
cmake .. -DMAX_SDP_SESSION_MEDIA_COUNT=6
```

- **5 (default):** Supports standard configuration of 1 video + 1 audio transceiver + data channel with extra slots to spare. Also supports 4 video + 1 audio + no data channel.
- **6 (maximum):** Supports 4 video + 1 audio + data channel.

**Note**: [KVS TURN relay has a hard bandwidth limit](https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-limits.html#limits-turn-service), which cannot sustain that many video streams at reasonable quality.

Ensure your thread stack sizes are sufficient when increasing the number of slots.

### What is it?

In WebRTC, every audio stream, video stream, and data channel gets its own "media section" (an `m=` line) in the SDP. The KVS WebRTC-C SDK stores these in a fixed-size array inside the `SessionDescription` struct, which is stack-allocated. `MAX_SDP_SESSION_MEDIA_COUNT` controls how many of these media sections the array can hold.

#### Why there's a limit

Each media slot adds approximately **140 KB** to the `SessionDescription` struct. At the default of 5 slots, the struct is already ~825 KB - close to the 1 MB stack limit on Windows. Making this limit dynamic or overly large would cause stack overflows on constrained platforms.

#### Tracks and transceivers

A **track** represents a single media source - one camera feed, one microphone, etc. A **transceiver** is the bidirectional pipeline associated with that media type. A single transceiver can send, receive, or both (controlled by its direction: `sendrecv`, `sendonly`, `recvonly`, or `inactive`). In the KVS WebRTC-C SDK, you create a transceiver by calling `addTransceiver` with a track attached. Each transceiver produces exactly one `m=` line in the SDP.

Think of it like a walkie-talkie channel: each channel can transmit, listen, or both - but you need a separate channel for each independent media stream (one for video, one for audio, etc.).

### What Consumes a Slot

Every `m=` line in the local SDP offer or answer counts toward this limit:

| Source | When it's created | Example m= line |
|--------|-------------------|-----------------|
| User-added audio transceiver | You call `addTransceiver` with an audio track | `m=audio 9 UDP/TLS/RTP/SAVPF 111` |
| User-added video transceiver | You call `addTransceiver` with a video track | `m=video 9 UDP/TLS/RTP/SAVPF 96` |
| Fake transceiver | Answering a remote m-line with no matching local transceiver | `m=video 9 UDP/TLS/RTP/SAVPF 96` (direction=inactive) |
| Data channel | `ENABLE_DATA_CHANNEL` is ON (the default) | `m=application 9 UDP/DTLS/SCTP webrtc-datachannel` |

#### Fake transceivers

When the KVS WebRTC-C SDK generates an SDP answer, it must include one m-line for each m-line in the remote offer (per RFC 3264). If you haven't created a transceiver matching a remote m-line, the KVS WebRTC-C SDK creates a "fake transceiver" with `direction=inactive` to fill that slot.

This means if a remote peer offers 3 video streams but you only added 1 video transceiver, the KVS WebRTC-C SDK still uses 3 video slots (1 real + 2 fake).

#### Data channel

When `ENABLE_DATA_CHANNEL` is `ON`, the data channel m-line is always appended last, after all transceivers. With the default limit of 5, this leaves 4 slots for video and audio transceivers.

### Stack Size Implications

The `SessionDescription` struct is stack-allocated in several SDK functions. Its size grows linearly with the media count:

```
Per-slot size = MAX_SDP_ATTRIBUTES_COUNT (256) x sizeof(SdpAttributes) (546 bytes)
             + other fields (~978 bytes)
             ~ 140,754 bytes per slot

Total struct ~ (N x 140 KB) + 137 KB fixed overhead
```

| `MAX_SDP_SESSION_MEDIA_COUNT` | Approximate struct size |
|-------------------------------|------------------------|
| 5 (default) | ~825 KB |
| 6 | ~962 KB |

> [!NOTE]
> The per-slot size above itself depends on `MAX_SDP_ATTRIBUTES_COUNT` (Section 1). Raising *both* knobs compounds: total attribute storage scales with `MAX_SDP_ATTRIBUTES_COUNT x (1 + MAX_SDP_SESSION_MEDIA_COUNT)`.

**Why not heap-allocate `SessionDescription`?**

The struct is part of the public API - SDK consumers stack-allocate it directly (e.g., `SessionDescription sd; deserializeSessionDescription(&sd, ...)`) and existing applications depend on this pattern. Changing it to a heap-allocated opaque type would be a breaking API change. (Internally, the PeerConnection remote-description path does heap-allocate it.)

### Troubleshooting

#### `STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT` error (`0x5500000A`)

The KVS WebRTC-C SDK returns this error when the number of m-lines would exceed `MAX_SDP_SESSION_MEDIA_COUNT`. This can happen during `createOffer`, `createAnswer`, or `setRemoteDescription` (when parsing the remote SDP).

**Resolution:** Increase `MAX_SDP_SESSION_MEDIA_COUNT` via CMake and ensure your stack is large enough (see sizing table above).

#### SEH exception `0xc0000005` when creating offer/answer

On Windows, a stack overflow caused by a `SessionDescription` that is too large for the thread's stack manifests as **SEH exception `0xc0000005`** (`STATUS_ACCESS_VIOLATION`) - this is a [Windows NTSTATUS code](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55), not a KVS SDK error code.

**Resolution (pick one):**

- Reduce `MAX_SDP_SESSION_MEDIA_COUNT` if you don't need 6 slots.
- Increase the thread stack size

#### Remote peer's extra m-lines rejected

If a remote offer has more m-lines than `MAX_SDP_SESSION_MEDIA_COUNT`, the KVS WebRTC-C SDK will reject the SDP and return `STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT` during `setRemoteDescription` along with logging an ERROR log.

**Resolution:** Increase `MAX_SDP_SESSION_MEDIA_COUNT` to accommodate the remote peer's offer size.

#### Useful log messages

The KVS WebRTC-C SDK logs ERROR-level messages when the media count limit is hit. Look for these in your logs:

| Log message | What it means |
|-------------|---------------|
| `Exceeded max media count. Max: %u, current: %u` | A remote SDP being parsed has more m-lines than `MAX_SDP_SESSION_MEDIA_COUNT`. The SDP will be rejected. |
| `Exceeded max media count while creating offer. Max: %u, current: %u` | You added more transceivers than the limit allows. The `createOffer` call will fail. |
| `Exceeded max media count while creating answer. Max: %u, current: %u` | The answer being generated (real + fake transceivers) exceeds the limit. |
| `Exceeded max media count while adding data channel. Max: %u, current: %u` | All slots are consumed by transceivers, leaving no room for the data channel m-line. Reduce your transceiver count by 1 or increase the limit. |

---

## 3. SDP Buffer Size (`KVS_SIGNALING_MESSAGE_LEN`)

### What is this?

When two WebRTC peers connect through the KVS Signaling service, they exchange Session Description Protocol (SDP) messages that describe their media capabilities (codecs, tracks, ICE candidates, etc.). These messages travel through the signaling channel as base64-encoded payloads inside a JSON envelope.

The same signaling message buffer is also used for ICE candidate messages, though these are typically much smaller and unlikely to exceed the default limits.

The SDK uses fixed-size buffers to hold these messages at each stage of the pipeline. By default, these are sized for typical use cases. If your application produces larger SDPs (e.g. many media tracks, or many candidates), you may need to increase the buffer. If you're on a memory-constrained embedded device with predictable SDP shapes, you may want to reduce it.

#### KVS Signaling API reference
- https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/async-message-reception-api.html
- https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/SendSdpOffer.html
- https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/SendSdpAnswer.html
- https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/SendIceCandidate.html

### Buffer Pipeline

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
│  Step 1. LWS_MESSAGE_BUFFER_SIZE (WebSocket receive buffer)               │
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
│  Size: KVS_SIGNALING_MESSAGE_LEN (default 25000 bytes)                    │
│  This is the ONE value you configure. Everything else derives from it.    │
└───────────────────────────────────────────────────────────────────────────┘
```

#### Step 1: WebSocket receive buffer (`LWS_MESSAGE_BUFFER_SIZE`)

The first buffer that incoming data hits. It accumulates WebSocket frame fragments from the signaling service into a single contiguous message. The content at this stage is the **full JSON envelope** from the service - including `messageType`, `senderClientId`, and the base64-encoded `messagePayload` field.

If the incoming frame exceeds this buffer, the SDK rejects it with `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` and logs the size mismatch.

#### Step 2: Signaling message buffer (`MAX_SIGNALING_MESSAGE_LEN`)

After the JSON envelope is parsed, the `messagePayload` field is extracted into `SignalingMessage.payload[]`. At this stage the content is **still base64-encoded** - it's the raw `messagePayload` string from the signaling service JSON.

The size is derived as `(MAX_SESSION_DESCRIPTION_INIT_SDP_LEN * 4/3) + 1024`. The `4/3` factor accounts for base64 encoding expansion (3 raw bytes become 4 base64 characters). The `1024` accounts for the inner JSON keys (`{"type":"offer","sdp":"..."}`) that wrap the SDP inside the payload.

#### Step 3: Decoded SDP buffer (`MAX_SESSION_DESCRIPTION_INIT_SDP_LEN`)

The final destination. The base64 payload is decoded, revealing a JSON object with `type` and `sdp` fields. The `sdp` value (plain-text SDP) is copied into `RtcSessionDescriptionInit.sdp[]`.

If the decoded SDP exceeds this size, the SDK rejects it with `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED`.

#### Size relationship

All three buffers are derived from a single knob (`KVS_SIGNALING_MESSAGE_LEN`), which represents the **on-wire signaling message size** (Step 2):

| Step   | Buffer                                 | Default   | With `KVS_SIGNALING_MESSAGE_LEN`           | What it holds                                |
|:------:|----------------------------------------|-----------|--------------------------------------------|----------------------------------------------|
|   1    | `LWS_MESSAGE_BUFFER_SIZE`              | ~18766    | `KVS_SIGNALING_MESSAGE_LEN + alignment`    | Full WebSocket frame (JSON envelope)         |
|   2    | `MAX_SIGNALING_MESSAGE_LEN`            | 18750     | `KVS_SIGNALING_MESSAGE_LEN`                | Base64-encoded payload + inner JSON overhead |
|   3    | `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` | 25000     | `(KVS_SIGNALING_MESSAGE_LEN - 1024) * 3/4` | Decoded SDP text                             |

You set `KVS_SIGNALING_MESSAGE_LEN` which directly controls the signaling message size (Step 2). Step 3 is derived downward (accounting for base64 decoding), and Step 1 is derived upward (adding alignment).

> [!NOTE]
> The legacy defaults (18750/25000) don't follow the formula. The signaling buffer is too small to deliver a full 25 KB SDP. The effective decoded SDP limit with defaults is ~13 KB. Setting `KVS_SIGNALING_MESSAGE_LEN` fixes this by properly coordinating both values.

### Configuration

The value you set is the **signaling message size**: the on-wire message including base64-encoded payload.
The SDK derives the decoded SDP buffer from it: `decoded_sdp_limit = (KVS_SIGNALING_MESSAGE_LEN - 1024) * 3/4`.

Also see: https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/SendSdpOffer.html

#### Build-time CMake variable

```bash
# Default
cmake ..

# Maximum
cmake .. -DKVS_SIGNALING_MESSAGE_LEN=40000
```

| Parameter  | Value                                        |
|------------|----------------------------------------------|
| Default    | Not set (legacy: signaling=18750, SDP=25000) |
| Minimum    | 10000 bytes                                  |
| Maximum    | 40000 bytes                                  |

#### When to increase

- You're seeing `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED` or `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` errors
- Your remote peers send SDPs with many media sections (e.g., multi-track configurations)
- You include ICE candidates in the SDP (non-trickle ICE) which inflates SDP size

#### When to decrease

> [!CAUTION]
> **Advanced option.** Do not reduce the buffer size unless you have measured and confirmed the maximum signaling message size your viewers will produce. Browser updates may add new codecs or extensions that increase SDP size without warning. If you reduce the buffer below what a viewer sends, the offer will get rejected.

- Running on memory-constrained embedded hardware (<1 GB RAM)
- You have a fixed, predictable SDP shape (e.g., single-camera robot that always negotiates 1 video + 1 audio track)
- You want to reduce stack usage - `RtcSessionDescriptionInit` is stack-allocated at multiple call sites

#### Stack impact

`RtcSessionDescriptionInit` contains a `CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1]` field and is stack-allocated.
Increasing `KVS_SIGNALING_MESSAGE_LEN` increases this derived value and thus stack pressure at each call site.
`SignalingMessage` also grows (its `payload[]` is sized to `KVS_SIGNALING_MESSAGE_LEN + 1`). If you increase significantly, verify your thread stack size can accommodate it:

```bash
# Check system default stack size
ulimit -s    # typically 8192 KB on Linux, 8 MB on macOS
```

The SDK's `KVS_STACK_SIZE` CMake variable can be used to set thread stack size if needed.

### Troubleshooting

#### Error log messages

Enable ERROR level logs to see rejection messages.

| Log message | What it means | What to do                                                                              |
|-------------|---------------|-----------------------------------------------------------------------------------------|
| `Received SDP size (X bytes) exceeds configured MAX_SESSION_DESCRIPTION_INIT_SDP_LEN (Y bytes). Increase KVS_SIGNALING_MESSAGE_LEN in CMake to accommodate larger SDPs.` | A remote peer sent an SDP that doesn't fit in the decoded buffer. | Rebuild with `-DKVS_SIGNALING_MESSAGE_LEN=<value larger than X>`                              |
| `Signaling message size (X bytes so far) exceeds receive buffer (Y bytes). Increase KVS_SIGNALING_MESSAGE_LEN in CMake to accommodate larger messages.` | The raw WebSocket frame from the signaling service doesn't fit in the LWS receive buffer. | Same fix: increase `KVS_SIGNALING_MESSAGE_LEN`. The LWS buffer derives from it automatically. |

#### Relevant status codes

| Status code | Name | Meaning |
|-------------|------|---------|
| `0x55000006` | `STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED` | Decoded SDP exceeds `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` |
| `0x5d000028` | `STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN` | Raw signaling message exceeds WebSocket receive buffer |
| `0x5d000025` | `STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING` | Outbound message (after base64 encoding) exceeds send buffer |

### Reference

#### Related defines

| Define                                 | Location        | Description                                                       |
|----------------------------------------|-----------------|-------------------------------------------------------------------|
| `KVS_SIGNALING_MESSAGE_LEN`            | CMake variable  | The single configuration point (signaling message size)           |
| `LWS_MESSAGE_BUFFER_SIZE`              | `LwsApiCalls.h` | WebSocket receive buffer (derived from signaling len)             |
| `MAX_SIGNALING_MESSAGE_LEN`            | `Include.h`     | Signaling message buffer (= `KVS_SIGNALING_MESSAGE_LEN` or 18750) |
| `MAX_SESSION_DESCRIPTION_INIT_SDP_LEN` | `Include.h`     | Decoded SDP buffer (derived from signaling len, or 25000)         |

#### Struct fields sized by these defines

```c
typedef struct {
    SDP_TYPE type;
    CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1];  // ← derived from KVS_SIGNALING_MESSAGE_LEN
} RtcSessionDescriptionInit;

typedef struct {
    // ...
    CHAR payload[MAX_SIGNALING_MESSAGE_LEN + 1];  // ← set by KVS_SIGNALING_MESSAGE_LEN
} SignalingMessage;
```
