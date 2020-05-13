Release 1.2.0 of WebRTC C SDK

Release tagged at: <Commit:  <to-be-filled>

Whats’s new:
- Support on Windows
- Fixed broken data channel message receive
- Added capability to handle more transceivers 
- Allowing ICE username/password unto 256 bytes
- Added PRtcDataChannel to data channel callback arguments
- Added new data type to allow certain nullable types specified in RTC spec
- Provision to support trickle ICE based on Remote description
- File logging in samples

Improvements:
- Unit tested data channel send message after DTLS handshake
- Fix leak when ice restart fails

PIC Changes: (Commit:  <to-be-filled>)
- Build system enhancements
- File logger functionality moved to PIC from Producer C
- Fixed re-alloc bug

Producer C Changes: (Commit:  <to-be-filled>)
- Build system enhancement - navigation away from use of submodules
- Using only JSMN header
- Added file logging in samples
- New status codes for file credential provider

Known Issues:
* Incorrect transceiver direction breaks SDP exchange in Firefox and iOS
* Partial reliability does not work as expected. Need to add library level tests to validate
* Reduce memory allocations in sending audio/video packets

Documentation: Information about the WebRTC public APIs can be found here: https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/
