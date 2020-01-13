<h1 align="center">
  Amazon Kinesis Video Streams C WebRTC SDK
  <br>
</h1>

<h4 align="center">Pure C WebRTC Client for Amazon Kinesis Video Streams </h4>

<p align="center">
  <a href="https://travis-ci.org/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c"> <img src="https://travis-ci.org/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.svg?branch=master" alt="Build Status"> </a>
  <a href="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c"> <img src="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/branch/master/graph/badge.svg" alt="Coverage Status"> </a>
</p>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build">Build</a> •
  <a href="#run">Run</a> •
  <a href="#documentation">documentation</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>

## Key Features
* Audio/Video Support
  - VP8
  - H264
  - Opus
* Developer Controlled Media Pipeline
  - Raw Media for Input/Output
  - [API emits feedback for QoS (bitrate suggestions)](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/issues/114)
* DataChannels
* NACKs
* STUN/TURN Support
* IPv4/[IPv6 TODO](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/issues/115)
* Signaling Client Included
  - KVS Provides STUN/TURN and Signaling Backend
  - Connect with [Android](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-android)/[iOS](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-ios)/[Web](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js) using pre-made samples
* Portable
  - Tested on Linux/MacOS
  - Tested on x64, ARMv5
* Small Install Size
  - Sub 200k library size
  - OpenSSL, libsrtp, libjsmn, libusrsctp and libwebsockets dependencies.

## Build
### Download
To download run the following command:

`git clone --recursive https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git`

You will also need to install `pkg-config` and `CMake` and a build enviroment

### Configure
Create a build directory in the newly checked out repository, and execute CMake from it.

`mkdir -p amazon-kinesis-video-streams-webrtc-sdk-c/build; cd amazon-kinesis-video-streams-webrtc-sdk-c/build; cmake .. `

We have provided an example of using GStreamer to capture/encode video, and then send via this library. This is only build if `pkg-config` finds
GStreamer is installed on your system.

By default we download all the libraries from GitHub and build them locally, so should require nothing to be installed ahead of time.
If you do wish to link to existing libraries you can use the following flags to customize your build.

You can pass the following options to `cmake ..`.

* `-DADD_MUCLIBC` -- Add -muclibc c flag
* `-DBUILD_DEPENDENCIES` -- Whether or not to build depending libraries from source
* `-DBUILD_OPENSSL` -- If building dependencies, whether or not building openssl from source
* `-DBUILD_TEST=TRUE` -- Build unit/integration tests, may be useful for confirm support for your device. `./tst/webrtc_client_test`
* `-DCODE_COVERAGE` --  Enable coverage reporting
* `-DCOMPILER_WARNINGS` -- Enable all compiler warnings
* `-DADDRESS_SANITIZER` -- Build with AddressSanitizer
* `-DMEMORY_SANITIZER` --  Build with MemorySanitizer
* `-DTHREAD_SANITIZER` -- Build with ThreadSanitizer
* `-DUNDEFINED_BEHAVIOR_SANITIZER` Build with UndefinedBehaviorSanitizer`

### Build
To build the library and the provided samples run make in the build directory you executed CMake.

`make`

## Run
### Setup your environment with your AWS account credentials:
First set the appropriate environment variables so you can connect to KVS

```
export AWS_ACCESS_KEY_ID= <AWS account access key>
export AWS_SECRET_ACCESS_KEY= <AWS account secret key>
```

### Running the Samples
After executing `make` you will have the following sample applications in your `build` directory:

* `kvsWebrtcClientMaster` - This application sends sample H264/Opus frames (path: `/samples/h264SampleFrames` and `/samples/opusSampleFrames`) via WebRTC. It also accepts incoming audio, if enabled in the browser. When checked in the browser, it prints the metadata of the received audio packets in your terminal.
* `kvsWebrtcClientViewer` - This application accepts sample H264/Opus frames and prints them out.
* `kvsWebrtcClientMasterGstSample` - This application sends sample H264/Opus frames from a GStreamer pipeline. It also will playback incoming audio via an `autoaudiosink`.

Run any of the sample applications by passing to it the name that you want to give to your signaling channel. The application creates the signaling channel using the name you provide. For example, to create a signaling channel called myChannel and to start sending sample H264/Opus frames via this channel, run the following command:

```
./kvsWebrtcClientMaster myChannel
```

When the command line application prints "Signaling client connection to socket established", you can proceed to the next step.

Now that your signaling channel is created and the connected master is streaming media to it, you can view this stream. To do so, open the [WebRTC SDK Test Page](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html) using the steps in Using the Kinesis Video Streams with WebRTC Test Page and set the following values using the same AWS credentials and the same signaling channel that you specified for the master above:
* Access key ID
* Secret access key
* Signaling channel name
* Client ID (optional)

Choose Start viewer to start live video streaming of the sample H264/Opus frames.

## Documentation
All Public APIs are documented in our [Include.h](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/master/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h) refer to [related](#related) for more about WebRTC and KVS.

## Related
* [What Is Amazon Kinesis Video Streams with WebRTC](https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/what-is-kvswebrtc.html)
* [JS SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js/)
* [iOS SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-ios/)
* [Android SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-android/)

## License

This library is licensed under the Apache 2.0 License.
