
This is the first release of Amazon Kinesis Video WebRTC C SDK

Platforms tested on:
- Linux
- MacOS
- x64
- ARMv5

Web Browsers tested on:
- Chrome
- Firefox

Release tagged at:
- 909277d164532f09543d7bf39aedd97d1601b585

Whats’s new:
- Starter samples of master, viewer and GStreamer master
- Cross compilation support
- Signaling Client
- Callbacks provided for Receiver end bandwidth estimation support for Congestion Control, FIR and PLI
- Network interface filtering callback provision to reduce time taken to gather TURN/ICE candidates
- STUN/TURN support
- IPv4 support
- IoT certificate authentication integration with signaling client
- Pre-generated certs support while creating DTLS session to help smaller embedded devices

Known Issues:
- Incorrect transceiver direction breaks SDP exchange in Firefox 
- No IPv6 support

Documentation:
Information about the WebRTC public APIs can be found here:
https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/
