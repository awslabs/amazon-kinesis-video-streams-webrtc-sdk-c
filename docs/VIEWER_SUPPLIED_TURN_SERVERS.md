# Viewer-Supplied TURN Servers

## Overview

This is an application-side optimization for the camera (master). It is not part of the library. The library does not read TURN servers from the SDP offer. Only an application that adds this logic uses them. The viewer sends the camera a set of TURN servers in its SDP offer. The camera application passes these TURN servers to the peer connection instead of calling GetIceServerConfig. If the offer has no TURN servers, the camera uses the original lazy caching logic.

The master samples in this repository support this. The sample viewer of the [KVS WebRTC JavaScript SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js) sends the TURN servers when **Send TURN servers to the master in the SDP offer** is selected.

## Problem

The camera gets its TURN servers from GetIceServerConfig through the signaling client. The signaling client caches the result lazily. It calls GetIceServerConfig again only when the cached credentials expire.

TURN credentials are short-lived. If they have expired when an offer arrives, the camera calls GetIceServerConfig before it creates the peer connection. The connection still works, but this call adds latency on the hot path.

In a typical use case, viewers check a camera a few times a day. The cached credentials have then expired at most checks, so the time to first frame is higher.

The camera cannot refresh the credentials just in time. It does not know when a viewer will connect.

## Solution

This approach has the viewer application send the TURN server details with the SDP offer. The viewer already calls GetIceServerConfig for its own peer connection. The camera then does not need to call it.

We do not recommend that the camera continuously keep a warm set of TURN servers. Each refresh uses API quota, even when no viewer connects.

## Viewer

GetIceServerConfig returns more than one set of TURN servers. The viewer sends the camera a set that it does not use. It adds the set, unchanged from the [GetIceServerConfig](https://docs.aws.amazon.com/kinesisvideostreams/latest/APIReference/API_signaling_GetIceServerConfig.html) response, to the offer's message payload as an `IceServerList` property:

```json
{
  "type": "offer",
  "sdp": "<SDP>",
  "IceServerList": [
    {
      "Password": "<password>",
      "Ttl": <ttl>,
      "Uris": ["turn:...?transport=udp", "turns:...?transport=udp", "turns:...?transport=tcp"],
      "Username": "<username>"
    }
  ]
}
```

The set keeps all of its URIs, even if the camera uses only some of them. The camera decides which URIs to use, based on its use case and requirements.

## Camera

The master camera samples do this in `samples/common/Common.c`. To turn it off, set `useViewerTurnServers` to `FALSE` in the sample configuration and rebuild the samples. This setting belongs to the samples, not the library. `getTurnServersFromOffer()` reads the first set of `IceServerList`. `initializePeerConnection()` sets these TURN servers in the `RtcConfiguration` that it passes to `createPeerConnection()`.

The camera uses the signaling client's cached TURN servers when:

- the offer has no `IceServerList`,
- `IceServerList` cannot be read, or
- the credentials expire within `SAMPLE_VIEWER_ICE_SERVER_EXPIRY_GRACE_PERIOD`.

Viewers that do not send `IceServerList` work as before.

## Considerations

- Make sure that the maximum signaling message length (`KVS_SIGNALING_MESSAGE_LEN`) is set appropriately.
- Make sure that the TURN credentials the viewer sends have enough time left for the camera to connect to the TURN server.
- Use one set of TURN servers on the viewer and send the camera another set. If the viewer uses every set, it generates more ICE candidates, which increases the time to first frame.
- The camera uses the TURN servers that the viewer sends. This assumes that both peers authenticate to KVS Signaling and trust each other, as described in [Supported architecture](../README.md#supported-architecture).

## Troubleshooting

These log lines come from the samples, except the GetIceServerConfig line, which comes from the library's signaling client. Each log line appears at the log level in the table and at more detailed levels. The camera's default log level is WARN. See [Setup Logging](../README.md#setup-logging). The JS test page sets its own log level.

| Source | Log level | Log line | Meaning |
|---|---|---|---|
| JS test page viewer | INFO | `[VIEWER] Sending <count> set(s) of TURN servers to the master with the offer` | The viewer sent TURN servers with the offer. |
| Master camera sample | INFO | `Received offer from peer <client id>` | The camera received the viewer's offer. |
| Master camera sample | INFO | `Using the <count> TURN server URI(s) the viewer supplied in the offer, skipping GetIceServerConfig` | The camera uses the viewer's TURN servers. |
| Library signaling client | INFO | `Retrieving ICE config through getIceServerConfig call again` | The camera called GetIceServerConfig itself. |
| Master camera sample | WARN | `The viewer-supplied TURN servers are incomplete: ...` | The set has no URIs, username or password. The camera uses its own TURN servers. |
| Master camera sample | WARN | `The viewer-supplied TURN credentials expire at ..., within the grace period of this master's clock (...)` | The credentials expire too soon. The camera uses its own TURN servers. |
| Master camera sample | WARN | `The offer's IceServerList is not a list of ICE server objects` | `IceServerList` has the wrong format. The camera uses its own TURN servers. |
