<h1 align="center">
  Amazon Kinesis Video Streams C WebRTC SDK
  <br>
</h1>

<h4 align="center">Pure C WebRTC Client for Amazon Kinesis Video Streams </h4>

<p align="center">
  <a href="https://travis-ci.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c"> <img src="https://travis-ci.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.svg?branch=master" alt="Build Status"> </a>
  <a href="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c"> <img src="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/branch/master/graph/badge.svg" alt="Coverage Status"> </a>
</p>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build">Build</a> •
  <a href="#run">Run</a> •
  <a href="#documentation">Documentation</a> •
  <a href="#setup-iot">Setup IoT</a> •
  <a href="#use-pre-generated-certificates">Use Pre-generated Certificates</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>

## Key Features
* Audio/Video Support
  - VP8
  - H264
  - Opus
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
* Developer Controlled Media Pipeline
  - Raw Media for Input/Output
  - Callbacks for [Congestion Control](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/pull/201), FIR and PLI (set on RtcRtpTransceiver)
* DataChannels
* NACKs
* STUN/TURN Support
* IPv4/IPv6
* Signaling Client Included
  - KVS Provides STUN/TURN and Signaling Backend
  - Connect with [Android](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-android)/[iOS](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-ios)/[Web](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js) using pre-made samples
* Portable
  - Tested on Linux/MacOS
  - Tested on x64, ARMv5
  - Build system designed for pleasant cross-compilation
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

On Ubuntu and Raspberry Pi OS you can get the libraries by running
```
$ sudo apt-get install libssl-dev libcurl4-openssl-dev liblog4cplus-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools
```

By default we download all the libraries from GitHub and build them locally, so should require nothing to be installed ahead of time.
If you do wish to link to existing libraries you can use the following flags to customize your build.


#### Cross-Compilation

If you wish to cross-compile `CC` and `CXX` are respected when building the library and all its dependencies. You will also need to set `BUILD_OPENSSL_PLATFORM`, `BUILD_LIBSRTP_HOST_PLATFORM` and `BUILD_LIBSRTP_DESTINATION_PLATFORM`. See our [.travis.yml](.travis.yml) for an example of this. Every commit is cross compiled to ensure that it continues to work.

#### Static Builds

If `-DBUILD_STATIC_LIBS=TRUE` then all dependencies and KVS WebRTC libraries will be built as static libraries.

#### CMake Arguments
You can pass the following options to `cmake ..`.

* `-DBUILD_STATIC_LIBS` -- Build all KVS WebRTC and third-party libraries as static libraries.
* `-DADD_MUCLIBC`  -- Add -muclibc c flag
* `-DBUILD_DEPENDENCIES` -- Whether or not to build depending libraries from source
* `-DBUILD_OPENSSL_PLATFORM` -- If buildng OpenSSL what is the target platform
* `-DBUILD_LIBSRTP_HOST_PLATFORM` -- If buildng LibSRTP what is the current platform
* `-DBUILD_LIBSRTP_DESTINATION_PLATFORM` -- If buildng LibSRTP what is the destination platform
* `-DBUILD_TEST=TRUE` -- Build unit/integration tests, may be useful for confirm support for your device. `./tst/webrtc_client_test`
* `-DCODE_COVERAGE` --  Enable coverage reporting
* `-DCOMPILER_WARNINGS` -- Enable all compiler warnings
* `-DADDRESS_SANITIZER` -- Build with AddressSanitizer
* `-DMEMORY_SANITIZER` --  Build with MemorySanitizer
* `-DTHREAD_SANITIZER` -- Build with ThreadSanitizer
* `-DUNDEFINED_BEHAVIOR_SANITIZER` -- Build with UndefinedBehaviorSanitizer
* `-DLINK_PROFILER` -- Link with gperftools (available profiler options are listed [here](https://github.com/gperftools/gperftools))

### Build
To build the library and the provided samples run make in the build directory you executed CMake.

`make`

## Run
### Setup your environment with your AWS account credentials and AWS region:
* First set the appropriate environment variables so you can connect to KVS. If you want to use IoT certificate instead, check <a href="#setup-iot">Setup IoT</a>.

```
export AWS_ACCESS_KEY_ID= <AWS account access key>
export AWS_SECRET_ACCESS_KEY= <AWS account secret key>
```

* Optionally, set AWS_SESSION_TOKEN if integrating with temporary token

```
export AWS_SESSION_TOKEN=<session token>
```

* Region is optional, if not being set, then us-west-2 will be used as default region.

```
export AWS_DEFAULT_REGION= <AWS region>
```

### Setup desired log level:
Set up the desired log level. The log levels and corresponding values currently available are:
1. `LOG_LEVEL_VERBOSE` ---- 1
2. `LOG_LEVEL_DEBUG`   ---- 2
3. `LOG_LEVEL_INFO`    ---- 3
4. `LOG_LEVEL_WARN`    ---- 4
5. `LOG_LEVEL_ERROR`   ---- 5
6. `LOG_LEVEL_FATAL`   ---- 6
7. `LOG_LEVEL_SILENT`  ---- 7

To set a log level, run the following command:
```
export AWS_KVS_LOG_LEVEL = <LOG_LEVEL>
```

For example:
```
export AWS_KVS_LOG_LEVEL = 2 switches on DEBUG level logs while runnning the samples
```

Note: The default log level is `LOG_LEVEL_WARN`.

* Optionally, set path to SSL CA certificate with variable (`../certs/cert.pem` is default one and points to file in this repository):

```
export AWS_KVS_CACERT_PATH=../certs/cert.pem
```

### Running the Samples
After executing `make` you will have the following sample applications in your `build/samples` directory:

* `kvsWebrtcClientMaster` - This application sends sample H264/Opus frames (path: `/samples/h264SampleFrames` and `/samples/opusSampleFrames`) via WebRTC. It also accepts incoming audio, if enabled in the browser. When checked in the browser, it prints the metadata of the received audio packets in your terminal.
* `kvsWebrtcClientViewer` - This application accepts sample H264/Opus frames and prints them out.
* `kvsWebrtcClientMasterGstSample` - This application sends sample H264/Opus frames from a GStreamer pipeline. It also will playback incoming audio via an `autoaudiosink`.

Run any of the sample applications by passing to it the name that you want to give to your signaling channel. The application creates the signaling channel using the name you provide. For example, to create a signaling channel called myChannel and to start sending sample H264/Opus frames via this channel, run the following command from `build/` directory:

```
./samples/kvsWebrtcClientMaster myChannel
```

When the command line application prints "Signaling client connection to socket established", you can proceed to the next step.

Now that your signaling channel is created and the connected master is streaming media to it, you can view this stream. To do so, open the [WebRTC SDK Test Page](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html) using the steps in Using the Kinesis Video Streams with WebRTC Test Page and set the following values using the same AWS credentials and the same signaling channel that you specified for the master above:
* Access key ID
* Secret access key
* Signaling channel name
* Client ID (optional)

Choose Start viewer to start live video streaming of the sample H264/Opus frames.

## Setup IoT
* To use IoT certificate to authenticate with KVS signaling, please refer to [Controlling Access to Kinesis Video Streams Resources Using AWS IoT](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html) for provisioning details.
* A sample IAM policy for the IoT role looks like below, policy can be modified based on your permission requirement.

```
{
   "Version":"2012-10-17",
   "Statement":[
      {
          "Effect":"Allow",
          "Action":[
            "kinesisvideo:DescribeSignalingChannel",
            "kinesisvideo:CreateSignalingChannel",
            "kinesisvideo:GetSignalingChannelEndpoint",
            "kinesisvideo:GetIceServerConfig",
            "kinesisvideo:ConnectAsMaster",
          ],
          "Resource":"arn:aws:kinesisvideo:*:*:channel/${credentials-iot:ThingName}/*"
      }
   ]
}
```

Note: "kinesisvideo:CreateSignalingChannel" can be removed if you are running with existing KVS signaling channels. Viewer sample requires "kinesisvideo:ConnectAsViewer" permission. Integration test requires both "kinesisvideo:ConnectAsViewer" and "kinesisvideo:DeleteSignalingChannel" permission.

* With the IoT certificate, IoT credentials provider endpoint (Note: it is not the endpoint on IoT AWS Console!), public key and private key ready, you can replace the static credentials provider createStaticCredentialProvider() and freeStaticCredentialProvider() with IoT credentials provider like below, the credentials provider for [samples](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/master/samples/Common.c) is in createSampleConfiguration():

```
createLwsIotCredentialProvider(
            "coxxxxxxxx168.credentials.iot.us-west-2.amazonaws.com",  // IoT credentials endpoint
            "/Users/username/Downloads/iot-signaling/certificate.pem",  // path to iot certificate
            "/Users/username/Downloads/iot-signaling/private.pem.key", // path to iot private key
            "/Users/username/Downloads/iot-signaling/cacert.pem", // path to CA cert
            "KinesisVideoSignalingCameraIoTRoleAlias", // IoT role alias
            channelName, // iot thing name, recommended to be same as your channel name
            &pSampleConfiguration->pCredentialProvider));

freeIotCredentialProvider(&pSampleConfiguration->pCredentialProvider);
```

## Use Pre-generated Certificates
The certificate generating function (createCertificateAndKey) in createDtlsSession() can take between 5 - 15 seconds in low performance embedded devices, it is called for every peer connection creation when KVS WebRTC receives an offer. To avoid this extra start-up latency, certificate can be pre-generated and passed in when offer comes.

**Important Note: It is recommended to rotate the certificates often - preferably for every peer connection to avoid a compromised client weakening the security of the new connections.**

Take kvsWebRTCClientMaster as sample, add RtcCertificate certificates[CERT_COUNT]; to **SampleConfiguration** in Samples.h.
Then pass in the pre-generated certificate in initializePeerConnection() in Common.c.

```
configuration.certificates[0].pCertificate = pSampleConfiguration->certificates[0].pCertificate;
configuration.certificates[0].pPrivateKey = pSampleConfiguration->certificates[0].pPrivateKey;

where, `configuration` is of type `RtcConfiguration` in the function that calls `initializePeerConnection()`.

Doing this will make sure that `createCertificateAndKey() would not execute since a certificate is already available.`
```

## Provide Hardware Entropy Source

In the mbedTLS version, the SDK uses /dev/urandom on Unix and CryptGenRandom API on Windows to get a strong entropy source. On some systems, these APIs might not be available. So, it's **strongly suggested** that you bring your own hardware entropy source. To do this, you need to follow these steps:

1. Uncomment `MBEDTLS_ENTROPY_HARDWARE_ALT` in configs/config_mbedtls.h
2. Write your own entropy source implementation by following this function signature: https://github.com/ARMmbed/mbedtls/blob/v2.25.0/include/mbedtls/entropy_poll.h#L81-L92
3. Include your implementation source code in the linking process

## DEBUG
### Getting the SDPs
If you would like to print out the SDPs, run this command:
`export DEBUG_LOG_SDP=TRUE`

### File logging
If you would like to enable file logging, run this command:
`export AWS_ENABLE_FILE_LOGGING=TRUE`
You can also change settings such as buffer size, number of log files for rotation and log file path in the samples

### Adjust MTU
If ICE connection can be established successfully but media can not be transferred, make sure the actual MTU is higher than the MTU setting here: https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/master/src/source/PeerConnection/Rtp.h#L12.

You can also change settings such as buffer size, number of log files for rotation and log file path in the samples

## Clang Checks
This SDK has clang format checks enforced in builds. In order to avoid re-iterating and make sure your code
complies, use the `scripts/check-clang.sh` to check for compliance and `scripts/clang-format.sh` to ensure compliance.

## Tracing high memory and/or cpu usage
If you would like to specifically find the code path that causes high memory and/or cpu usage, you need to recompile the SDK with this command:
`cmake .. -DLINK_PROFILER=ON`

The flag will link the SDK with [gperftools](https://github.com/gperftools/gperftools) profiler.

### Heap Profile

You can run your program as you normally would. You only need to specify the following environment variable to get the heap profile:

`HEAPPROFILE=/tmp/heap.prof /path/to/your/binary`

More information about what environment variables you can configure can be found [here](https://gperftools.github.io/gperftools/heapprofile.html)

### CPU Profile

Similar to the heap profile, you only need to specify the following environment variable to get the CPU profile:

`CPUPROFILE=/tmp/cpu.prof /path/to/your/binary`

More information about what environment variables you can configure can be found [here](https://gperftools.github.io/gperftools/cpuprofile.html)

## Documentation
All Public APIs are documented in our [Include.h](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/master/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h), we also generate a [Doxygen](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/) each commit for easier navigation.

Refer to [related](#related) for more about WebRTC and KVS.

## Outbound hostname and port requirements
* KVS endpoint : TCP 443 (ex: kinesisvideo.us-west-2.amazonaws.com)
* HTTPS channel endpoint : TCP 443 (ex: r-2c136a55.kinesisvideo.us-west-2.amazonaws.com)
* WSS channel endpoint : TCP 443 (ex: m-26d02974.kinesisvideo.us-west-2.amazonaws.com)
* STUN endpoint : UDP 443 (ex: stun.kinesisvideo.us-west-2.amazonaws.com)
* TURN endpoint : UDP/TCP 443 (ex: 34-219-91-62.t-1cd92f6b.kinesisvideo.us-west-2.amazonaws.com:443)

The least common denominator for hostname is `*.kinesisvideo.<region>.amazonaws.com` and port is 443.

## Related
* [What Is Amazon Kinesis Video Streams with WebRTC](https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/what-is-kvswebrtc.html)
* [JS SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js/)
* [iOS SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-ios/)
* [Android SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-android/)

## License

This library is licensed under the Apache 2.0 License.
