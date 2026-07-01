# SDP Media Count Limit (`MAX_SDP_SESSION_MEDIA_COUNT`)

## Configuration

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

---

## What is it?

In WebRTC, every audio stream, video stream, and data channel gets its own "media section" (an `m=` line) in the SDP. The KVS WebRTC-C SDK stores these in a fixed-size array inside the `SessionDescription` struct, which is stack-allocated. `MAX_SDP_SESSION_MEDIA_COUNT` controls how many of these media sections the array can hold.

### Why there's a limit

Each media slot adds approximately **140 KB** to the `SessionDescription` struct. At the default of 5 slots, the struct is already ~825 KB - close to the 1 MB stack limit on Windows. Making this limit dynamic or overly large would cause stack overflows on constrained platforms.

### Tracks and transceivers

A **track** represents a single media source - one camera feed, one microphone, etc. A **transceiver** is the bidirectional pipeline associated with that media type. A single transceiver can send, receive, or both (controlled by its direction: `sendrecv`, `sendonly`, `recvonly`, or `inactive`). In the KVS WebRTC-C SDK, you create a transceiver by calling `addTransceiver` with a track attached. Each transceiver produces exactly one `m=` line in the SDP.

Think of it like a walkie-talkie channel: each channel can transmit, listen, or both - but you need a separate channel for each independent media stream (one for video, one for audio, etc.).

---

## What Consumes a Slot

Every `m=` line in the local SDP offer or answer counts toward this limit:

| Source | When it's created | Example m= line |
|--------|-------------------|-----------------|
| User-added audio transceiver | You call `addTransceiver` with an audio track | `m=audio 9 UDP/TLS/RTP/SAVPF 111` |
| User-added video transceiver | You call `addTransceiver` with a video track | `m=video 9 UDP/TLS/RTP/SAVPF 96` |
| Fake transceiver | Answering a remote m-line with no matching local transceiver | `m=video 9 UDP/TLS/RTP/SAVPF 96` (direction=inactive) |
| Data channel | `ENABLE_DATA_CHANNEL` is ON (the default) | `m=application 9 UDP/DTLS/SCTP webrtc-datachannel` |

### Fake transceivers

When the KVS WebRTC-C SDK generates an SDP answer, it must include one m-line for each m-line in the remote offer (per RFC 3264). If you haven't created a transceiver matching a remote m-line, the KVS WebRTC-C SDK creates a "fake transceiver" with `direction=inactive` to fill that slot.

This means if a remote peer offers 3 video streams but you only added 1 video transceiver, the KVS WebRTC-C SDK still uses 3 video slots (1 real + 2 fake).

### Data channel

When `ENABLE_DATA_CHANNEL` is `ON`, the data channel m-line is always appended last, after all transceivers. With the default limit of 5, this leaves 4 slots for video and audio transceivers.

---

## Stack Size Implications

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

**Why not heap-allocate `SessionDescription`?** 

The struct is part of the public API - SDK consumers stack-allocate it directly (e.g., `SessionDescription sd; deserializeSessionDescription(&sd, ...)`) and existing applications depend on this pattern. Changing it to a heap-allocated opaque type would be a breaking API change.

---

## Troubleshooting

### `STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT` error (0x5500000A)

The KVS WebRTC-C SDK returns this error when the number of m-lines would exceed `MAX_SDP_SESSION_MEDIA_COUNT`. This can happen during `createOffer`, `createAnswer`, or `setRemoteDescription` (when parsing the remote SDP).

Check the ERROR log message to see which operation hit the limit (see [Useful log messages](#useful-log-messages) below).

**Resolution:** Increase `MAX_SDP_SESSION_MEDIA_COUNT` via CMake and ensure your stack is large enough (see sizing table above).

### SEH exception `0xc0000005` when creating offer/answer

On Windows, a stack overflow caused by a `SessionDescription` that is too large for the thread's stack manifests as **SEH exception `0xc0000005`** (`STATUS_ACCESS_VIOLATION`) - this is a [Windows NTSTATUS code](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55), not a KVS SDK error code.

**Resolution (pick one):**

- Reduce `MAX_SDP_SESSION_MEDIA_COUNT` if you don't need 6 slots.
- Increase the thread stack size

### Remote peer's extra m-lines rejected

If a remote offer has more m-lines than `MAX_SDP_SESSION_MEDIA_COUNT`, the KVS WebRTC-C SDK will reject the SDP and return `STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT` during `setRemoteDescription` along with logging an ERROR log.

**Resolution:** Increase `MAX_SDP_SESSION_MEDIA_COUNT` to accommodate the remote peer's offer size.

### Useful log messages

The KVS WebRTC-C SDK logs ERROR-level messages when the media count limit is hit. Look for these in your logs:

| Log message | What it means |
|-------------|---------------|
| `Exceeded max media count. Max: %u, current: %u` | A remote SDP being parsed has more m-lines than `MAX_SDP_SESSION_MEDIA_COUNT`. The SDP will be rejected. |
| `Exceeded max media count while creating offer. Max: %u, current: %u` | You added more transceivers than the limit allows. The `createOffer` call will fail. |
| `Exceeded max media count while creating answer. Max: %u, current: %u` | The answer being generated (real + fake transceivers) exceeds the limit. |
| `Exceeded max media count while adding data channel. Max: %u, current: %u` | All slots are consumed by transceivers, leaving no room for the data channel m-line. Reduce your transceiver count by 1 or increase the limit. |
