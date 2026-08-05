# NACK / RTX Retransmission (RFC 4588)

## What this covers

WebRTC media rides on UDP (secured with DTLS/SRTP), which has no delivery guarantee - packets can be dropped, reordered, or duplicated, so some loss is expected on any real network. When a receiver loses a video packet, it can ask the sender to resend it. This document explains how the KVS WebRTC C SDK implements that loss-recovery path - negotiation in SDP, parsing the retransmission request (NACK), and sending the retransmission as an RFC 4588 RTX packet - and the log lines to look for when troubleshooting.

In the typical KVS topology, the **master (camera / C SDK) is the RTP sender** and the **web/mobile viewer is the receiver**. The viewer detects packet loss and sends NACKs; the master buffers recently sent packets and retransmits them.

Retransmission in this SDK is **video-only** (H.264, H.265, VP8). Browsers don't negotiate RTX for audio either. Audio (Opus/G.711) relies on FEC and packet-loss concealment instead - see [the audio note](#what-about-audio).

## Background: NACK vs RTX

- **NACK** (Negative ACKnowledgement, RFC 4585) is the RTCP feedback message the receiver sends to report lost sequence numbers and request retransmission.
- **RTX** (RFC 4588) is the *retransmission payload format*. Instead of re-sending the original packet verbatim, the sender wraps it in a dedicated retransmission stream - a **separate SSRC**, a **separate (RTX) payload type**, and prepends a **2-byte OSN** (Original Sequence Number) to the payload. This lets the receiver cleanly tell "new media" from "a repair of old media" and recover the original sequence number.

RFC 4588 *does* allow for a sender to **decline to retransmit** ("the sender MAY selectively retransmit only the packets that it deems important and ignore NACK messages for other packets"). So the two conformant options are **send an RTX packet** or **don't retransmit**. Once RTX has been negotiated in SDP, the peers are expected to use it.

## The three compliance surfaces

RFC 4588 support in the SDK touches three places. All three must agree for retransmission to be compliant.

### 1. SDP negotiation

RTX is negotiated per video codec. The offer and answer play different roles:

**The offer (from the viewer) must contain**, for the codec it wants RTX on:

```
a=rtcp-fb:<pt> nack                 <- viewer will send NACKs for this codec (RFC 4585)
a=rtpmap:<rtxPt> rtx/90000          <- offers a dedicated RTX payload type
a=fmtp:<rtxPt> apt=<pt>             <- binds that RTX PT to the media codec's PT (RFC 4588)
```

The `90000` in `rtx/90000` is the **clock rate**. RFC 4588 requires the RTX clock rate to match the associated media codec's. All video codecs (H.264, H.265, VP8) use 90000 Hz, so video RTX is always `rtx/90000`; audio RTX (not used) would match the audio clock, e.g. `rtx/48000` for Opus.

If the offer has no `rtx/90000` + `apt=<pt>` for the codec in use, RTX is not on the table and the sender (master) uses the plain-resend fallback. Some viewers strip RTX from the offer when they filter codecs (see [Troubleshooting](#rtx-not-used-plain-resend-instead)).

**The answer (from the master / C SDK) should contain**, mirroring the offer and adding the sender's own RTX stream:

```
a=rtcp-fb:<pt> nack                 <- master accepts receiving NACKs
a=rtpmap:<rtxPt> rtx/90000          <- accepts the RTX payload type
a=fmtp:<rtxPt> apt=<pt>             <- same RTX-PT -> media-PT binding
a=ssrc-group:FID <ssrc> <rtxSsrc>   <- declares the master's own RTX SSRC (RFC 5576 FID group)
```

Example H.265 answer (payload type 51, RTX payload type 52):

```
a=rtpmap:51 H265/90000
a=rtcp-fb:51 nack
a=rtpmap:52 rtx/90000
a=fmtp:52 apt=51
a=ssrc-group:FID 1896949801 493082045
```

RTX is **per-transceiver**. In a multi-track session, each video track gets its own media SSRC *and* its own RTX SSRC.

Code: `setPayloadTypesFromOffer()` parses the offer's `apt=` mapping and `populateSingleMediaSection()` emits the answer (both in `SessionDescription.c`). If the offer has no `apt=` mapping for the codec in use, RTX is not negotiated and the sender uses the plain-resend fallback.

### 2. Inbound: parsing the retransmit request (the NACK)

The request the sender receives is an RTCP **Generic NACK** (RFC 4585 §6.2.1): PT = 205, FMT = 1, followed by the packet-sender SSRC, the media-source SSRC, and one or more FCI entries. Each FCI entry is a 16-bit **PID** (first lost sequence number) plus a 16-bit **BLP** (bitmask; bit *i* set means `PID + i + 1` was also lost) - up to 17 sequence numbers per entry.

Code: `onRtcpPacket()` dispatches it (`Rtcp.c`); `rtcpNackListGet()` expands PID+BLP into a flat list of missing sequence numbers (`RtcpPacket.c`).

### 3. Outbound: sending the retransmission

The sender can only retransmit a packet that is **still in its rolling buffer**. The buffer holds a bounded window of recently sent packets, sized from the per-transceiver configuration (`configureTransceiverRollingBuffer()` - duration × expected bitrate); older packets are evicted. If a NACK arrives for a packet that has already aged out (e.g. high path RTT, buffer sized too small for the bitrate), it cannot be retransmitted - the request is skipped and logged as `STATUS_ROLLING_BUFFER_NOT_IN_RANGE`.

For each requested sequence number found in the sender's rolling buffer, the SDK will construct the new packet, and send it.

Code: `constructRetransmitRtpPacketFromBytes()` (`RtpPacket.c`) builds the RTX packet: a new RTP header (RTX SSRC, RTX payload type, a fresh sequence number in the RTX stream's own space), a **2-byte big-endian OSN** = the original sequence number, then the original payload unchanged. The OSN is the first two bytes of the **payload**, not an RTP header extension.

## What about audio?

Audio codecs (Opus, MULAW, ALAW) advertise **no** `nack` and **no** RTX (same as browsers). Audio is latency-sensitive. A retransmission usually arrives too late to be useful, so audio recovers via **FEC** (Opus in-band FEC / RED, RFC 2198) and packet-loss concealment instead.

**This is by design, but not something RFC 4588 forbids.** RFC 4588 is codec-agnostic - it *could* be applied to audio - but in practice the WebRTC ecosystem doesn't. Browsers behave the same way:

- **Chrome / Firefox / Safari** do not put `a=rtpmap:<pt> rtx/90000` + `apt=` on the audio m-line in their offers/answers; audio NACK/RTX is effectively not used. Opus FEC (`useinbandfec=1`) and RED are the audio loss-recovery path instead.
- Because the browser (viewer) never offers audio RTX, there is nothing for this SDK to negotiate for audio even if it wanted to - the offer/answer simply won't contain it.

Video-only RTX here reflects the same choice every major WebRTC implementation makes.

## Important log lines to look for

Quick reference - what to grep for and what it tells you:

| Log line (substring) | Source | Level | What it means                                                                                                       |
|----------------------|--------|-------|---------------------------------------------------------------------------------------------------------------------|
| `RTX negotiated for codec` | `setTransceiverPayloadTypes()` | INFO | RTX resolved for this codec - retransmits will be RFC 4588 RTX packets. Good.                                       |
| `RTX not resolved for codec` | `setTransceiverPayloadTypes()` | **WARN** | RTX did **not** resolve - NACKs will be answered with plain resends. First line to investigate if you expected RTX. |
| `Resent packet ssrc ... seq ... succeeded` | `resendPacketOnNack()` | VERBOSE | A retransmission was sent.                                                                                          |
| `Retransmit fallback (no RTX)` | `resendPacketOnNack()` | VERBOSE | This retransmit took the plain-resend branch (RTX not negotiated for the codec).                                    |
| `Retransmit STATUS_ROLLING_BUFFER_NOT_IN_RANGE` | `resendPacketOnNack()` | VERBOSE | NACKed packet already aged out of the rolling buffer - buffer too small for the path RTT/loss.                      |
| `Sender re-transmitter is not created` | `resendPacketOnNack()` | ERROR | No retransmitter for the SSRC - retransmission not set up for that transceiver.                                     |

Details for each, grouped by when they fire:

### Negotiation time (once per transceiver)

Emitted by `setTransceiverPayloadTypes()` after the RTX lookup:

```
RTX negotiated for codec <c>: payloadType <pt>, rtxPayloadType <rtxPt>
```
RTX resolved - retransmissions for this codec will be RFC 4588 RTX packets. (`rtxPayloadType != payloadType`.)

```
RTX not resolved for codec <c> (no matching rtx/apt in remote SDP); NACKs will fall back to plain resend on payloadType <pt>
```
**Warning.** Either the remote didn't offer RTX for this codec, or (if it did) something upstream failed to resolve it. NACKs will be answered with plain resends. If you *expected* RTX here, this is the first line to investigate.

### Retransmit time (verbose)

From `resendPacketOnNack()`:

```
Resent packet ssrc <ssrc> seq <seq> succeeded          # a retransmission went out
Retransmit fallback (no RTX) for ssrc <ssrc> seq <seq> pt <pt>   # took the plain-resend branch
Retransmit STATUS_ROLLING_BUFFER_NOT_IN_RANGE ...      # packet aged out of the buffer before the NACK arrived
```

## Verifying end-to-end

1. **webrtc-internals (Chrome viewer):** on the inbound-rtp video stream, look for `rtxSsrc` and the `retransmittedPacketsReceived` / `retransmittedBytesReceived` counters. These are created only when Chrome actually receives RTX packets - if RTX is working and loss occurred, they appear and increment. Their **absence** during loss means no RTX stream arrived (see Troubleshooting).
2. **Wireshark (decrypted or TURN-relayed):** confirm retransmits arrive on the **RTX SSRC** with the **RTX payload type**, and that the first two payload bytes are the OSN. If instead you see duplicate sequence numbers on the *original* SSRC/PT, that's the plain-resend fallback.
3. **Device logs:** the negotiation-time line above tells you, before any loss, whether RTX resolved for each codec.

## Troubleshooting

### RTX not used (plain resend instead)

Symptoms: `retransmittedPacketsReceived` stat doesn't appear in the viewer's webrtc-internals for a codec that *is* seeing loss; Wireshark shows duplicate-sequence packets on the original payload type instead of RTX-PT packets on a separate SSRC; the device logs `RTX not resolved for codec ...`.

This is decided **per codec, not per track count** - if it reproduces, it reproduces in both single-track and multi-track for the affected codec. It is not a multi-track issue.

Checklist:
- Inspect the SDP: confirm the offer/answer actually negotiated RTX for the codec in use (`a=fmtp:<rtxPt> apt=<pt>` present). Some viewers strip RTX from the offer if they filter codecs (e.g. `setCodecPreferences` built from a filtered `getCapabilities().codecs` that drops the `video/rtx` entry).
- Confirm `rtxPayloadType != payloadType` for the sender (negotiation-time log). If they're equal despite RTX being in the SDP, the codec→RTX-table lookup isn't resolving - verify the `RTC_CODEC_*` → `RTC_RTX_CODEC_*` mapping in `setTransceiverPayloadTypes()`.

### NACKs arrive but nothing is retransmitted

- `Retransmit STATUS_ROLLING_BUFFER_NOT_IN_RANGE`: the NACKed packet was already evicted from the rolling buffer before the NACK arrived. **The master can only buffer a bounded window of recently sent packets**, sized by `configureTransceiverRollingBuffer()` (duration × expected bitrate ÷ MTU). A packet older than that window is gone and cannot be retransmitted. This shows up when:
  - the path RTT is high, so NACKs arrive late relative to the window;
  - the actual bitrate exceeds the configured expected bitrate, so the window holds fewer real seconds than intended;
  - a burst/"NACK storm" requests packets spanning more than the window.

  Mitigate by increasing the rolling-buffer duration (and/or the expected bitrate it is sized against) via `configureTransceiverRollingBuffer()` - at the cost of more sender memory. There is a practical ceiling; the buffer is not meant to hold the entire session.
- `Sender re-transmitter is not created ...`: no retransmitter exists for the SSRC - retransmission wasn't set up for that transceiver.

### Error log messages

| Log message | Level | What it means | What to do |
|-------------|-------|---------------|------------|
| `RTX not resolved for codec <c> (no matching rtx/apt in remote SDP); NACKs will fall back to plain resend on payloadType <pt>` | WARN | RTX did not resolve for this video codec; retransmits will be plain resends. | Confirm the offer carries `rtx/90000` + `apt=<pt>` for the codec; confirm the `RTC_CODEC_* -> RTC_RTX_CODEC_*` mapping resolves (`rtxPayloadType != payloadType`). |
| `No RTX codec mapping for codec <c>` | ERROR | `getRtxCodecKeyForCodec()` was called for a codec with no RTX mapping. Expected never to fire for the supported video codecs; audio is guarded out before the call. | Indicates a new/unsupported video codec reached the RTX path - add its `RTC_CODEC_* -> RTC_RTX_CODEC_*` mapping. |
| `Sender re-transmitter is not created successfully for an existing ssrcs ...` | ERROR | A NACK arrived for a known SSRC but no retransmitter was allocated for that transceiver. | Ensure the transceiver was set up as a sender with a rolling buffer / retransmitter. |
| `Receiving NACK for non existing ssrcs: senderSsrc ... receiverSsrc ...` | ERROR | A NACK referenced SSRCs that don't match any transceiver. | Usually a stale/duplicate NACK or SSRC mismatch; check the negotiated SSRCs. |
| `Retransmit STATUS_ROLLING_BUFFER_NOT_IN_RANGE ...` | VERBOSE | NACKed packet already evicted from the rolling buffer. | Increase rolling-buffer duration/bitrate via `configureTransceiverRollingBuffer()` (see above). |

### Relevant status codes

| Status code | Name | Meaning |
|-------------|------|---------|
| `0x5500000B` | `STATUS_SESSION_DESCRIPTION_CODEC_NOT_MAPPED_TO_RTX` | No `RTC_CODEC_* -> RTC_RTX_CODEC_*` mapping for the codec (returned by `getRtxCodecKeyForCodec()`). |
| `0x61000001` | `STATUS_ROLLING_BUFFER_NOT_IN_RANGE` | NACKed sequence number is outside the rolling buffer window (already evicted). |
| `0x60000005` | `STATUS_RTCP_INPUT_SSRC_INVALID` | NACK references SSRCs not matching any transceiver. |
| `0x0000000D` | `STATUS_INVALID_OPERATION` | NACK for an SSRC with no allocated retransmitter. |

## Glossary

| Term | Stands for | Meaning in this document                                                                                                                                        |
|------|-----------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **NACK** | Negative ACKnowledgement | RTCP feedback message the receiver sends to report lost packet(s) and request retransmission (RFC 4585).                                                        |
| **RTX** | Retransmission (payload format) | RFC 4588 scheme for sending a retransmitted packet on a separate stream - own SSRC, own payload type, with an OSN header - rather than re-sending the original. |
| **OSN** | Original Sequence Number | The 2-byte field at the front of an RTX packet's *payload* carrying the sequence number of the original packet being repaired. Not an RTP header extension.     |
| **plain resend** | - | Re-sending the original packet as-is (same SSRC, payload type, and sequence number). The fallback used when RTX is not negotiated.                              |
| **RTP** | Real-time Transport Protocol | Carries the media packets (RFC 3550).                                                                                                                           |
| **RTCP** | RTP Control Protocol | Control/feedback channel alongside RTP (carries NACK, PLI, sender/receiver reports, etc.).                                                                      |
| **SSRC** | Synchronization Source | 32-bit ID of an RTP stream. The media stream and its RTX stream use *different* SSRCs.                                                                          |
| **PT** | Payload Type | 7-bit RTP field identifying the codec/format. The RTX stream uses its own dynamic PT, mapped to the media PT via `apt=`.                                        |
| **PID** | Packet ID | In a NACK, the first lost sequence number in an FCI entry (RFC 4585 §6.2.1).                                                                                    |
| **BLP** | Bitmask of Lost Packets | In a NACK, the 16-bit mask following the PID; bit *i* set means `PID + i + 1` was also lost.                                                                    |
| **FCI** | Feedback Control Information | Payload portion of an RTCP feedback packet; for a NACK, each FCI entry is one PID+BLP pair.                                                                     |
| **FMT** | Feedback Message Type | Sub-type in the RTCP feedback header. A Generic NACK is FMT = 1 with PT = 205.                                                                                  |
| **apt** | associated payload type | SDP fmtp attribute (`a=fmtp:<rtxPt> apt=<pt>`) binding an RTX payload type to its media codec's payload type (RFC 4588).                                        |
| **FID** | Flow Identification | SDP `a=ssrc-group:FID <media> <rtx>` grouping that binds a media SSRC to its RTX SSRC (RFC 5576).                                                               |
| **PLI** | Picture Loss Indication | RTCP feedback asking the sender for a fresh keyframe; the fallback when retransmission can't recover the stream.                                                |
| **FEC** | Forward Error Correction | Proactive redundancy that repairs loss without a round-trip; used for audio instead of NACK/RTX.                                                                |
| **rolling buffer** | - | Per-sender ring of recently sent packets, keyed by sequence number, that retransmissions are pulled from.                                                       |

## References

- [RFC 4588](https://datatracker.ietf.org/doc/html/rfc4588) - RTP Retransmission Payload Format
- [RFC 4585](https://datatracker.ietf.org/doc/html/rfc4585) - RTP/AVPF (NACK feedback)
- [RFC 5576](https://datatracker.ietf.org/doc/html/rfc5576) - Source-Specific Media Attributes (`ssrc-group:FID`)
- Code: `SessionDescription.c` (negotiation), `Rtcp.c` / `RtcpPacket.c` (NACK parsing), `Retransmitter.c` / `RtpPacket.c` (retransmit + RTX construction)
