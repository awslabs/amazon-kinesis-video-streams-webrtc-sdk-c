Release 1.3.0 of WebRTC C SDK

Release tagged at: <Commit:  <to-be-filled>

Whats’s new:
- ICE server stats added (API: rtcPeerConnectionGetMetrics())
- RTP outbound and remote inbound stats added (API: rtcPeerConnectionGetMetrics())
- Signaling stats available (API: signalingClientGetMetrics())

Improvements:
- ICE candidate priority calculation bug fixed improving the candidate nomination process
- Improved SDP conformance with RFC to work with CRLF and LF
- Fixed interruption of sending process due to momentary network unreachable errors
- Fixed CPU spike when TURN candidates are not used by cleaning up the sockets they have opened

Producer C Changes: (Commit: e7d4868)
- Build system improvements
- README improvements on SDK versioning, development and release
- Added non-BSD strnstr implementation
- Fixed static libcurl build failure

Known Issues:
* Incorrect transceiver direction breaks SDP exchange in Firefox and iOS
* High memory allocations in sending audio/video packets
* Missing logging information about the selected candidates, e.g. IP address, server source, type of candidate chosen and protocol used
* Channel re-creation fails after deleting channel via console

Documentation: Information about the WebRTC public APIs can be found here: https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/
