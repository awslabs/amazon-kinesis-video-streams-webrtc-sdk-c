# Frame Size Limits

## What this covers

The KVS WebRTC C SDK limits the size of a single media frame. This document lists the limits, what happens when a frame exceeds one, and what to do instead.

## Summary

| Direction | Define | Value | What happens to a larger frame | Error |
|-----------|--------|-------|--------------------------------|-------|
| Outbound | `MAX_OUTBOUND_FRAME_SIZE_BYTES` | 5 MiB (5,242,880 bytes) | `writeFrame()` rejects it. Nothing is sent. | `STATUS_RTP_FRAME_TOO_LARGE` (`0x5c000005`) |

The limit is defined in [Include.h](../src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h).

---

## Outbound (`MAX_OUTBOUND_FRAME_SIZE_BYTES`)

### Behavior

`writeFrame()` checks the frame size before it does anything else. A frame larger than `MAX_OUTBOUND_FRAME_SIZE_BYTES`:

- returns `STATUS_RTP_FRAME_TOO_LARGE`,
- is not packetized or sent,
- is not counted in the outbound RTP stats,
- logs one error: `Frame size <size> exceeds the maximum outbound frame size 5242880`.

A frame of exactly `MAX_OUTBOUND_FRAME_SIZE_BYTES` is accepted.

### Why there is a limit

RTP has no frame size limit of its own, but every receiver buffers a bounded number of packets per frame. A frame that is larger than the receiver can reassemble is never delivered, and the receiver may stop delivering video for the rest of the session.

Browsers built on libwebrtc (Chrome, Edge, Safari) buffer at most 2048 packets per frame, about 2.4 MB at the default MTU. If browsers are viewers, keep frames well below that.

Real video frames, including 4K keyframes, are far below 5 MiB.

### If a frame is rejected

- **Video:** if `writeFrame()` returns `STATUS_RTP_FRAME_TOO_LARGE` for real video, check the encoder settings (bitrate, keyframe size).
- **Files and other large payloads:** do not send them as video frames. Send them over a data channel (`dataChannelSend()`) instead, split into smaller messages.
- **Use cases that need larger frames:** change `MAX_OUTBOUND_FRAME_SIZE_BYTES` in [Include.h](../src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h) and rebuild the SDK. Make sure your receivers can reassemble frames of that size.

## Troubleshooting

### Video stops at the viewer

A frame that is too large for the viewer is one cause. Check the sender first, then the viewer.

**Sender (SDK):** look for this error in the logs, or a `STATUS_RTP_FRAME_TOO_LARGE` return from `writeFrame()`:

```
Frame size <size> exceeds the maximum outbound frame size 5242880
```

If it appears, see [If a frame is rejected](#if-a-frame-is-rejected).

**Browser viewer:** open `chrome://webrtc-internals` (`edge://webrtc-internals` in Edge, `about:webrtc` in Firefox) in a new tab **before** you start the session, then join. Under the inbound video track (`inbound-rtp` … `kind=video`):

- **`packetsReceived` / `bytesReceived` keep climbing but `framesReceived` stops:** packets arrive but the browser can't reassemble them into frames. Compare with the size of the frames you send. libwebrtc can't reassemble a frame larger than about 2.4 MB at the default MTU.
- **`freezeCount` / `totalFreezesDuration`:** confirms and quantifies the freeze.

**C viewer:** call `rtcPeerConnectionGetMetrics()` with `RTC_STATS_TYPE_INBOUND_RTP` for the video transceiver and read `inboundRtpStreamStats`. The same pattern applies: `received.packetsReceived` keeps climbing but `framesReceived` stops. A climbing `received.packetsDiscarded` means the jitter buffer is discarding the packets.

For other causes of a freeze, see [Debugging from the receiver](TROUBLESHOOTING_H265_FRAMES.md#debugging-from-the-receiver-chrome).

## References

- [RFC 3550](https://datatracker.ietf.org/doc/html/rfc3550) - RTP
- Code: `Rtp.c` (`writeFrame()`), `Include.h` (`MAX_OUTBOUND_FRAME_SIZE_BYTES`, `STATUS_RTP_FRAME_TOO_LARGE`, `dataChannelSend()`)
