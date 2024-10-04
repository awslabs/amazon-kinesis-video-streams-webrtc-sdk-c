<h1 align="center">
  Amazon Kinesis Video Streams C WebRTC SDK
  <br>
</h1>

<h4 align="center">Pure C WebRTC Client for Amazon Kinesis Video Streams </h4>

<p align="center">
  <a href="https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/actions/workflows/ci.yml"> <img src="https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/actions/workflows/ci.yml/badge.svg"> </a>
  <a href="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c"> <img src="https://codecov.io/gh/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/branch/main/graph/badge.svg" alt="Coverage Status"> </a>
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

>[!NOTE]
>We have switched from using the 'master' branch to the 'main' branch. Please update your references accordingly.
## New feature announcements
Please refer to the release notes in [Releases](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/releases) page

## Key Features
* Audio/Video Support
  - VP8
  - H264
  - Opus
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
* Developer Controlled Media Pipeline
  - Raw Media for Input/Output
  - Callbacks for [Congestion Control](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/pull/201), FIR and PLI (set on [RtcRtpTransceiver](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/structRtcInboundRtpStreamStats.html))
* DataChannels
* NACKs
* STUN/TURN Support
* IPv4/IPv6
* Signaling Client Included
  - KVS Provides STUN/TURN and Signaling Backend
  - Connect with [Android](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-android)/[iOS](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-ios)/[Web](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-js) using pre-made samples
* Storage for WebRTC [NEW]
  * Ingest media into a Kinesis Video Stream.
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

You will also need to install `pkg-config` and `CMake` and a build environment

### Configuring on Ubuntu / Unix

Create a build directory in the newly checked out repository, and execute CMake from it.

`mkdir -p amazon-kinesis-video-streams-webrtc-sdk-c/build; cd amazon-kinesis-video-streams-webrtc-sdk-c/build; cmake .. `

We have provided an example of using GStreamer to capture/encode video, and then send via this library. This is only build if `pkg-config` finds
GStreamer is installed on your system.

On Ubuntu and Raspberry Pi OS you can get the libraries by running
```shell
sudo apt-get install cmake m4 pkg-config libssl-dev libcurl4-openssl-dev liblog4cplus-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools 
```

By default we download all the libraries from GitHub and build them locally, so should require nothing to be installed ahead of time. If you do wish to link to existing libraries you can use the following flags to customize your build.

### Configuring on Windows

Install [MS Visual Studio Community / Enterprise](https://visualstudio.microsoft.com/vs/community/), [Strawberry perl](https://strawberryperl.com/), and [Chocolatey](https://chocolatey.org/install) if not installed already

Get the libraries by running the following in powershell
```shell
choco install gstreamer
choco install gstreamer-devel
curl.exe -o C:\tools\pthreads-w32-2-9-1-release.zip ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip
mkdir C:\tools\pthreads-w32-2-9-1-release\
Expand-Archive -Path C:\tools\pthreads-w32-2-9-1-release.zip -DestinationPath C:\tools\pthreads-w32-2-9-1-release
```

Modify the path to the downloaded and unzipped PThreads in cmake in `build_windows_openssl.bat` if needed / unzipped at a path other than the one mentioned above
```shell
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE -DEXT_PTHREAD_INCLUDE_DIR="C:/tools/pthreads-w32-2-9-1-release/Pre-built.2/include/" -DEXT_PTHREAD_LIBRARIES="C:/tools/pthreads-w32-2-9-1-release/Pre-built.2/lib/x64/libpthreadGC2.a" ..
```
Modify the path to MSVC as well in the `build_windows_openssl.bat` if needed / installed a different version / location

```shell
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
```

Allow long paths before we start the build
```shell
git config --system core.longpaths true
```

Note that if the paths are still too long (which can cause the build to fail unfortunately), we recommend renaming the folders to use shorter names and moving them to `C:/`

Build the SDK

```shell
.github\build_windows_openssl.bat
```

To run the sample application, make sure that you've exported the following paths and appended them to env:Path for powershell
```shell
$env:Path += ';C:\webrtc\open-source\bin;C:\tools\pthreads-w32-2-9-1-release\Pre-built.2\dll\x64;C:\webrtc\build'
```

### Dependency requirements

These would be applicable if the SDK is being linked with system dependencies instead of building from source by the SDK.
`libmbedtls`: `>= 2.25.0 & < 3.x.x`
`libopenssl`: `= 1.1.1x`
`libsrtp2` : `<= 2.5.0`
`libusrsctp` : `<= 0.9.5.0`
`libwebsockets` : `>= 4.2.0`

#### Cross-Compilation

If you wish to cross-compile `CC` and `CXX` are respected when building the library and all its dependencies. You will also need to set `BUILD_OPENSSL_PLATFORM`, `BUILD_LIBSRTP_HOST_PLATFORM` and `BUILD_LIBSRTP_DESTINATION_PLATFORM`. See our codecov.io for an example of this. Every commit is cross compiled to ensure that it continues to work.

#### Static Builds

If `-DBUILD_STATIC_LIBS=TRUE` then all dependencies and KVS WebRTC libraries will be built as static libraries.

#### CMake Arguments
You can pass the following options to `cmake ..`.

* `-DBUILD_SAMPLE` -- Build the sample executables. ON by default.
* `-DIOT_CORE_ENABLE_CREDENTIALS` -- Build the sample applications using IoT credentials. OFF by default.
* `-DBUILD_STATIC_LIBS` -- Build all KVS WebRTC and third-party libraries as static libraries. Default: OFF (shared build).
* `-DADD_MUCLIBC`  -- Add -muclibc c flag
* `-DBUILD_DEPENDENCIES` -- Whether or not to build depending libraries from source
* `-DBUILD_OPENSSL_PLATFORM` -- If building OpenSSL what is the target platform
* `-DBUILD_LIBSRTP_HOST_PLATFORM` -- If building LibSRTP what is the current platform
* `-DBUILD_LIBSRTP_DESTINATION_PLATFORM` -- If building LibSRTP what is the destination platform
* `-DBUILD_TEST=TRUE` -- Build unit/integration tests, may be useful for confirm support for your device. `./tst/webrtc_client_test`
* `-DCODE_COVERAGE` --  Enable coverage reporting
* `-DCOMPILER_WARNINGS` -- Enable all compiler warnings
* `-DADDRESS_SANITIZER` -- Build with AddressSanitizer
* `-DMEMORY_SANITIZER` --  Build with MemorySanitizer
* `-DTHREAD_SANITIZER` -- Build with ThreadSanitizer
* `-DUNDEFINED_BEHAVIOR_SANITIZER` -- Build with UndefinedBehaviorSanitizer
* `-DCMAKE_BUILD_TYPE` -- Build Release/Debug libraries. By default, the SDK generates Release build. The standard options are listed [here](https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#default-and-custom-configurations)
* `-DLINK_PROFILER` -- Link with gperftools (available profiler options are listed [here](https://github.com/gperftools/gperftools))
* `-DPKG_CONFIG_EXECUTABLE` -- Set pkg config path. This might be required to find gstreamer's pkg config specifically on Windows.
* `-DENABLE_KVS_THREADPOOL` -- Enable the KVS threadpool which is off by default.

To clean up the `open-source` and `build` folders from previous build, use `cmake --build . --target clean` from the `build` folder

For windows builds, you will have to include additional flags for libwebsockets CMake. Add the following flags to your cmake command, or edit the CMake file in ./CMake/Dependencies/libwebsockets-CMakeLists.txt with the following:

```shell
cmake .. -DLWS_HAVE_PTHREAD_H=1 -DLWS_EXT_PTHREAD_INCLUDE_DIR="C:\Program Files (x86)\pthreads\include" -DLWS_EXT_PTHREAD_LIBRARIES="C:\Program Files (x86)\pthreads\lib\x64\libpthreadGC2.a" -DLWS_WITH_MINIMAL_EXAMPLES=1
```

Be sure to edit the path to whatever pthread library you are using, and the proper path for your environment.

### Build
To build the library and the provided samples run make in the build directory you executed CMake.

`make`

### Building with dependencies off

In addition to the dependencies already installed, install the dependencies using the appropriate package manager. 

On Ubuntu:
`sudo apt-get install libsrtp2-dev libusrsctp-dev libwebsockets-dev`

On MacOS:
`brew install srtp libusrsctp libwebsockets `

To use OpenSSL:
```shell
cmake .. -DBUILD_DEPENDENCIES=OFF -DUSE_OPENSSL=ON
```

To use MBedTLS:
```shell
cmake .. -DBUILD_DEPENDENCIES=OFF -DUSE_OPENSSL=OFF -DUSE_MBEDTLS=ON
```

Note: Please follow the dependency requirements to confirm the version requirements are satisfied to use the SDK with system installed dependencies.
If the versions are not satisfied, this option would not work and enabling the SDK to build dependencies for you would be the best option to go ahead with.

## Run
### Setup your environment with your AWS account credentials and AWS region:
* First set the appropriate environment variables so you can connect to KVS. If you want to use IoT certificate instead, check <a href="#setup-iot">Setup IoT</a>.

```shell
export AWS_ACCESS_KEY_ID=<AWS account access key>
export AWS_SECRET_ACCESS_KEY=<AWS account secret key>
```

* Optionally, set AWS_SESSION_TOKEN if integrating with temporary token

```shell
export AWS_SESSION_TOKEN=<session token>
```

* Region is optional, if not being set, then us-west-2 will be used as default region.

```shell
export AWS_DEFAULT_REGION=<AWS region>
```

### Setup logging:
Set up the desired log level. The log levels and corresponding values currently available are:
1. `LOG_LEVEL_VERBOSE` ---- 1
2. `LOG_LEVEL_DEBUG`   ---- 2
3. `LOG_LEVEL_INFO`    ---- 3
4. `LOG_LEVEL_WARN`    ---- 4
5. `LOG_LEVEL_ERROR`   ---- 5
6. `LOG_LEVEL_FATAL`   ---- 6
7. `LOG_LEVEL_SILENT`  ---- 7
8. `LOG_LEVEL_PROFILE` ---- 8

To set a log level, run the following command:
```shell
export AWS_KVS_LOG_LEVEL=<LOG_LEVEL>
```

For example, the following command switches on `DEBUG` level logs while runnning the samples.
```shell
export AWS_KVS_LOG_LEVEL=2 
```

Note: The default log level is `LOG_LEVEL_WARN`.

Starting v1.8.0, by default, the SDK creates a log file that would have execution timing details of certain steps in connection establishment. It would be stored in the `build` directory as `kvsFileLogFilter.x`. In case you do not want to use defaults, you can modify certain parameters such as log file directory, log file size and file rotation index in the `createFileLoggerWithLevelFiltering` function in the samples.
In addition to these logs, if you would like to have other level logs in a file as well, run:

```shell
export AWS_ENABLE_FILE_LOGGING=TRUE
```

The SDK also tracks entry and exit of functions which increases the verbosity of the logs. This will be useful when you want to track the transitions within the codebase. To do so, you need to set log level to `LOG_LEVEL_VERBOSE` and add the following to the CMakeLists.txt file:
`add_definitions(-DLOG_STREAMING)`
Note: This log level is extremely VERBOSE and could flood the files if using file based logging strategy.

<details>
  <summary>Time-to-first-frame breakdown metrics</summary>
  
There is a flag in the sample application which (pSampleConfiguration->enableSendingMetricsToViewerViaDc) can be set to TRUE to send metrics from the master to the [JS viewer](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html). This helps get a detailed breakdown of time-to-first-frame and all the processes and API calls on master and the viewer both. This is intended to be used with the KVS WebRTC C SDK running as the master and the JS SDK as the viewer. The master sends peer, ice-agent, signaling and data-channel metrics to the viewer which are plotted ~ 20 seconds after the viewer is started. Since the timeline plot is intended to understand the time-to-first-frame, the sample web page needs to be refreshed and the master needs to be restarted if a new / updated plot is needed. While using the SDK in this mode, it is expected that all datachannel messages are JSON messages. This feature is only meant to be used for a single viewer at a time.
</details>

### Set path to SSL CA certificate (**Optional**)

If you have a custom CA certificate path to set, you can set it using:

```shell
export AWS_KVS_CACERT_PATH=../certs/cert.pem
```

Or, you can pass it in through the CMake flag:

```shell
cmake .. -DKVS_CA_CERT_PATH=/path/to/cert.pem
```

By default, the SSL CA certificate is set to [`../certs/cert.pem`](./certs/cert.pem) which points to the file in this repository.

### Running the Samples
After executing `make` you will have sample applications in your `build/samples` directory. From the `build/` directory, run any of the sample applications by passing to it the name of your signaling channel. If a signaling channel does not exist with the name you provide, the application creates one.

#### Sample: kvsWebrtcClientMaster
This application sends sample H264/Opus frames (path: `/samples/h264SampleFrames` and `/samples/opusSampleFrames`) via WebRTC. It also accepts incoming audio, if enabled in the browser. When checked in the browser, it prints the metadata of the received audio packets in your terminal. To run:
```shell
./samples/kvsWebrtcClientMaster <channelName> <storage-option> <audio-codec> <video-codec>
```

To use the **Storage for WebRTC** feature, run the same command as above but with an additional command line arg to enable the feature.  

```shell
./samples/kvsWebrtcClientMaster <channelName> 1 <audio-codec> <video-codec>
```

Allowed audio-codec: opus (default codec if nothing is specified)
Allowed video-codec: h264 (default codec if nothing is specified), h265

#### Sample: kvsWebrtcClientMasterGstSample
This application can send media from a GStreamer pipeline using test H264/Opus frames, device `autovideosrc` and `autoaudiosrc` input, or a received RTSP stream. It also will playback incoming audio via an `autoaudiosink`. To run:
```shell
./samples/kvsWebrtcClientMasterGstSample <channelName> <mediaType> <sourceType>
```
Pass the desired media and source type when running the sample. The mediaType can be `audio-video` or `video-only`. To use the **Storage For WebRTC** feature, use `audio-video-storage` as the mediaType. The source type can be `testsrc`, `devicesrc`, or `rtspsrc`. Specify the RTSP URI if using `rtspsrc`:

```shell
./samples/kvsWebrtcClientMasterGstSample <channelName> <mediaType> rtspsrc rtsp://<rtspUri>
```

Using the testsrc with audio and video-codec
```shell
./samples/kvsWebrtcClientMasterGstSample <channelName> <mediaType> <sourceType> <audio-codec> <video-codec>
```

Example:
```shell
./samples/kvsWebrtcClientMasterGstSample <channelName> audio-video testsrc opus h264
```

Allowed audio-codec: opus (default codec if nothing is specified)
Allowed video-codec: h264 (default codec if nothing is specified), h265

#### Sample: kvsWebrtcClientViewer
This application accepts sample H264/Opus frames by default. You can use other supported codecs by changing the value for `videoTrack.codec` and `audioTrack.codec` in _Common.c_. By default, this sample only logs the size of the audio and video buffer it receives. To write these frames to a file using GStreamer, use the _kvsWebrtcClientViewerGstSample_ instead.

To run:
```shell
./samples/kvsWebrtcClientViewer <channelName> <audio-codec> <video-codec>
```

Allowed audio-codec: opus (default codec if nothing is specified)
Allowed video-codec: h264 (default codec if nothing is specified), h265

#### Sample: kvsWebrtcClientViewerGstSample
This application is similar to the kvsWebrtcClientViewer. However, instead of just logging the media it receives, it generates a file using filesink. Make sure that your device has enough space to write the media to a file. You can also customize the receiving logic by modifying the functions in _GstAudioVideoReceiver.c_

To run:
```shell
./samples/kvsWebrtcClientViewerGstSample <channelName> <mediaType> <audio-codec> <video-codec>
```

Allowed audio-codec: opus (default codec if nothing is specified)
Allowed video-codec: h264 (default codec if nothing is specified), h265

##### Known issues:
Our GStreamer samples leverage [MatroskaMux](https://gstreamer.freedesktop.org/documentation/matroska/matroskamux.html?gi-language=c) to receive media from its peer and save it to a file. However, MatroskaMux is designed for scenarios where the media's format remains constant throughout streaming. When the media's format changes mid-streaming (referred to as "caps changes"), MatroskaMux encounters limitations, its behavior cannot be predicted and it may be unable to handle these changes, resulting in an error message like:

```shell
matroskamux matroska-mux.c:1134:gst_matroska_mux_video_pad_setcaps:<mux> error: Caps changes are not supported by Matroska
```
To address this issue, users need to adapt the pipeline to utilize components capable of managing dynamic changes in media formats. This might involve integrating different muxers or customizing the pipeline to handle caps changes effectively.

#### Sample: Generating sample frames

##### H264
```shell
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1500 ! timeoverlay ! videoconvert ! video/x-raw,format=I420,width=1280,height=720,framerate=25/1 ! queue ! x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! multifilesink location="frame-%04d.h264" index=1
```

##### H265
```shell
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1500 ! timeoverlay ! videoconvert ! video/x-raw,format=I420,width=1280,height=720,framerate=25/1 ! queue ! x265enc speed-preset=veryfast bitrate=512 tune=zerolatency ! video/x-h265,stream-format=byte-stream,alignment=au,profile=main ! multifilesink location="frame-%04d.h265" index=1
```

### Viewing Master Samples

After running one of the master samples, when the command line application prints "Signaling client connection to socket established", indicating that your signaling channel is created and the connected master is streaming media to it, you can view the stream. To do so, check the media playback viewer on the KVS Signaling Channels console or open the [WebRTC SDK Test Page](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html).

If using the WebRTC SDK Test Page, set the following values using the same AWS credentials and the same signaling channel that you specified for the master above:
* Access key ID
* Secret access key
* Signaling channel name
* Client ID (optional)
  
Then choose Start Viewer to start live video streaming of the sample H264/Opus frames.

## Setup IoT
* To use IoT certificate to authenticate with KVS signaling, please refer to [Controlling Access to Kinesis Video Streams Resources Using AWS IoT](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html) for provisioning details.
* A sample IAM policy for the IoT role looks like below, policy can be modified based on your permission requirement.

```json
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
            "kinesisvideo:ConnectAsMaster"
          ],
          "Resource":"arn:aws:kinesisvideo:*:*:channel/${credentials-iot:ThingName}/*"
      }
   ]
}
```

We recommend following [best practices](https://docs.aws.amazon.com/IAM/latest/UserGuide/best-practices.html) while setting up the IAM policy and not allow access to all channels in the account, but allow access to only the REQUIRED channel names if the use case demands it. KVS recommendation is to use iot thing name as channel name as per public docs.
https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html

Note: "kinesisvideo:CreateSignalingChannel" can be removed if you are running with existing KVS signaling channels. Viewer sample requires "kinesisvideo:ConnectAsViewer" permission. Integration test requires both "kinesisvideo:ConnectAsViewer" and "kinesisvideo:DeleteSignalingChannel" permission.

* With the IoT certificate, IoT credentials provider endpoint (Note: it is not the endpoint on IoT AWS Console!), public key and private key ready, you can replace the static credentials provider createStaticCredentialProvider() and freeStaticCredentialProvider() with IoT credentials provider like below, the credentials provider for [samples](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/samples/Common.c) is in createSampleConfiguration():

```c
createLwsIotCredentialProvider(
            "coxxxxxxxx168.credentials.iot.us-west-2.amazonaws.com",  // IoT credentials endpoint
            "/Users/username/Downloads/iot-signaling/certificate.pem",  // path to iot certificate
            "/Users/username/Downloads/iot-signaling/private.pem.key", // path to iot private key
            "/Users/username/Downloads/iot-signaling/cacert.pem", // path to CA cert
            "KinesisVideoSignalingCameraIoTRoleAlias", // IoT role alias
            "IoTThingName", // iot thing name, recommended to be same as your channel name
            &pSampleConfiguration->pCredentialProvider));

freeIotCredentialProvider(&pSampleConfiguration->pCredentialProvider);
```

### Running samples with IoT Core credentials

Build the samples using IoT Core credentials mode:

```shell
cmake .. -DIOT_CORE_ENABLE_CREDENTIALS=ON
make
```

Set the environment variables for IoT Core credentials:

```shell
export AWS_IOT_CORE_CREDENTIAL_ENDPOINT=xxxxx.credentials.iot.xxxxx.amazonaws.com
export AWS_IOT_CORE_PRIVATE_KEY=xxxxxxxx-private.pem.key 
export AWS_IOT_CORE_ROLE_ALIAS=xxxxxx
export AWS_IOT_CORE_THING_NAME=xxxxxx
export AWS_IOT_CORE_CERT=xxxxx-certificate.pem.crt
```

AWS access keys are ignored from environment variables if the sample was built in IoT Core credentials mode.

## TWCC support

Transport Wide Congestion Control (TWCC) is a mechanism in WebRTC designed to enhance the performance and reliability of real-time communication over the internet. TWCC addresses the challenges of network congestion by providing detailed feedback on the transport of packets across the network, enabling adaptive bitrate control and optimization of media streams in real-time. This feedback mechanism is crucial for maintaining high-quality audio and video communication, as it allows senders to adjust their transmission strategies based on comprehensive information about packet losses, delays, and jitter experienced across the entire transport path.

The importance of TWCC in WebRTC lies in its ability to ensure efficient use of available network bandwidth while minimizing the negative impacts of network congestion. By monitoring the delivery of packets across the network, TWCC helps identify bottlenecks and adjust the media transmission rates accordingly. This dynamic approach to congestion control is essential for preventing degradation in call quality, such as pixelation, stuttering, or drops in audio and video streams, especially in environments with fluctuating network conditions.

To learn more about TWCC, check [TWCC spec](https://datatracker.ietf.org/doc/html/draft-holmer-rmcat-transport-wide-cc-extensions-01)

### Enabling TWCC support

TWCC is enabled by default in the SDK samples (via `pSampleConfiguration->enableTwcc`) flag. In order to disable it, set this flag to `FALSE`.

```c
pSampleConfiguration->enableTwcc = FALSE;
```

If not using the samples directly, 2 things need to be done to set up Twcc:
1. Set the `disableSenderSideBandwidthEstimation` to `FALSE`:
```c
configuration.kvsRtcConfiguration.disableSenderSideBandwidthEstimation = FALSE;
```
2. Set the callback that will have the business logic to modify the bitrate based on packet loss information. The callback can be set using `peerConnectionOnSenderBandwidthEstimation()`:
```c
CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                                     sampleSenderBandwidthEstimationHandler));
```

## Use Pre-generated Certificates
The certificate generating function ([createCertificateAndKey](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/Dtls__openssl_8c.html#a451c48525b0c0a8919a880d6834c1f7f)) in createDtlsSession() can take between 5 - 15 seconds in low performance embedded devices, it is called for every peer connection creation when KVS WebRTC receives an offer. To avoid this extra start-up latency, certificate can be pre-generated and passed in when offer comes.

**Important Note: It is recommended to rotate the certificates often - preferably for every peer connection to avoid a compromised client weakening the security of the new connections.**

Take `kvsWebRTCClientMaster` as sample, add `RtcCertificate certificates[CERT_COUNT];` to **SampleConfiguration** in [Samples.h](./samples/Samples.h).
Then pass in the pre-generated certificate in initializePeerConnection() in [Common.c](./samples/Common.c).

```c
configuration.certificates[0].pCertificate = pSampleConfiguration->certificates[0].pCertificate;
configuration.certificates[0].pPrivateKey = pSampleConfiguration->certificates[0].pPrivateKey;
```

where, `configuration` is of type [`RtcConfiguration`](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/structRtcConfiguration.html) in the function that calls `initializePeerConnection()`.

Doing this will make sure that [`createCertificateAndKey()`](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/Dtls__openssl_8c.html#a451c48525b0c0a8919a880d6834c1f7f) would not execute since a certificate is already available.

## Provide Hardware Entropy Source

In the mbedTLS version, the SDK uses /dev/urandom on Unix and CryptGenRandom API on Windows to get a strong entropy source. On some systems, these APIs might not be available. So, it's **strongly suggested** that you bring your own hardware entropy source. To do this, you need to follow these steps:

1. Uncomment `MBEDTLS_ENTROPY_HARDWARE_ALT` in configs/config_mbedtls.h
2. Write your own entropy source implementation by following this function signature: https://github.com/ARMmbed/mbedtls/blob/v2.25.0/include/mbedtls/entropy_poll.h#L81-L92
3. Include your implementation source code in the linking process

## DEBUG
### Getting the SDPs
If you would like to print out the SDPs, run this command:
`export DEBUG_LOG_SDP=TRUE`

### Adjust MTU
If ICE connection can be established successfully but media can not be transferred, make sure the actual MTU is higher than the MTU setting here: https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/src/source/PeerConnection/Rtp.h#L12.

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

### Filtering network interfaces

This is useful to reduce candidate gathering time when it is known for certain network interfaces to not work well. A sample callback is available in `Common.c`. The `iceSetInterfaceFilterFunc` in `KvsRtcConfiguration` must be set to the required callback. In the sample, it can be done this way in `initializePeerConnection()`: 
`configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = sampleFilterNetworkInterfaces`

### Building on MacOS M1
When building on MacOS M1, if the build fails while trying to build OpenSSL or Websockets, run the following command:
`cmake .. -DBUILD_OPENSSL_PLATFORM=darwin64-arm64-cc`

### Building on 32 bit Raspbian GNU/Linux 11

To build on a 32-bit Raspbian GNU/Linux 11 on 64-bit hardware, the OpenSSL library must be manually configured. This is due to the OpenSSL autoconfiguration script detecting 64-bit hardware and emitting 64-bit ARM assembly instructions which are not allowed in 32-bit executables. A 32-bit ARM version of OpenSSL can be configured by setting 32-bit ARM platform:
`cmake .. -DBUILD_OPENSSL_PLATFORM=linux-armv4`

### KVS Threadpool
Starting version 1.10.0, threadpool usage provides latency improvements in connection establishment. Note that increasing the number of minimum threads can increase stack memory usage. So, ensure to increase with caution.

The threadpool is disabled by default. To enable it, set the following CMake argument when building the SDK:
`cmake .. -DENABLE_KVS_THREADPOOL=ON`

By default, the threadpool starts with 3 threads that it will increase up to the maximum of 10 and decrease back down to the minimum of 3 as needed. To change these values to better match the resources of your use-case, you can set the following environment variables:
1. `export AWS_KVS_WEBRTC_THREADPOOL_MIN_THREADS=<value>`
2. `export AWS_KVS_WEBRTC_THREADPOOL_MAX_THREADS=<value>`

### Set up TWCC
TWCC is a mechanism in WebRTC designed to enhance the performance and reliability of real-time communication over the Internet. TWCC addresses the challenges of network congestion by providing detailed feedback on the transport of packets across the network, enabling adaptive bitrate control and optimization of 
media streams in real-time. This feedback mechanism is crucial for maintaining high-quality audio and video communication, as it allows senders to adjust their transmission strategies based on comprehensive information about packet losses, delays, and jitter experienced across the entire transport path.
The importance of TWCC in WebRTC lies in its ability to ensure efficient use of available network bandwidth while minimizing the negative impacts of network congestion. By monitoring the delivery of packets across the network, TWCC helps identify bottlenecks and adjust the media transmission rates accordingly. 
This dynamic approach to congestion control is essential for preventing degradation in call quality, such as pixelation, stuttering, or drops in audio and video streams, especially in environments with fluctuating network conditions. To learn more about TWCC, you can refer to the [RFC draft](https://datatracker.ietf.org/doc/html/draft-holmer-rmcat-transport-wide-cc-extensions-01)

In order to enable TWCC usage in the SDK, 2 things need to be set up:

1. Set the `disableSenderSideBandwidthEstimation` to FALSE. In our samples, the value is set using `enableTwcc` flag in `pSampleConfiguration`

```c
pSampleConfiguration->enableTwcc = TRUE; // to enable TWCC
pSampleConfiguration->enableTwcc = FALSE; // to disable TWCC
configuration.kvsRtcConfiguration.disableSenderSideBandwidthEstimation = !pSampleConfiguration->enableTwcc;
```

2. Set the callback that will have the business logic to modify the bitrate based on packet loss information. The callback can be set using `peerConnectionOnSenderBandwidthEstimation()`.

```c
CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                                     sampleSenderBandwidthEstimationHandler));
```

By default, our SDK enables TWCC listener. The SDK has a sample implementation to integrate TWCC into the Gstreamer pipeline via the `sampleSenderBandwidthEstimationHandler` callback. To get more details, look for this specific callback.


### Setting ICE related timeouts

There are some default timeout values set for different steps in ICE in the [KvsRtcConfiguration](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/structKvsRtcConfiguration.html). These are configurable in the application. While the defaults are generous, there could be applications that might need more flexibility to improve chances of connection establishment because of poor network. 

You can find the default setting in the logs:
```
2024-01-08 19:43:44.433 INFO    iceAgentValidateKvsRtcConfig():
	iceLocalCandidateGatheringTimeout: 10000 ms
	iceConnectionCheckTimeout: 12000 ms
	iceCandidateNominationTimeout: 12000 ms
	iceConnectionCheckPollingInterval: 50 ms
```
Let us look into when each of these could be changed:
1. `iceCandidateNominationTimeout`: Say the connection with host/srflx could not be established and TURN seems to be the only resort. Let us assume it takes about 15 seconds to gather the first local relay candidate, the application could set the timeout to a value more than 15 seconds to ensure candidate pairs with the local relay candidate are tried for success. If the value is set to less than 15 seconds in this case, the SDK would lose out on trying a potential candidate pair leading to connection establishment failure
2. `iceLocalCandidateGatheringTimeout`: Say the host candidates would not work and srflx/relay candidates need to be tried. Due to poor network, it is anticipated the candidates are gathered slowly and the application does not want to spend more than 20 seconds on this step. The goal is to try all possible candidate pairs. Increasing the timeout helps in giving some more time to gather more potential candidates to try for connection. Also note, this parameter increase would not make a difference in the situation unless `iceCandidateNominationTimeout` > `iceLocalCandidateGatheringTimeout` since nomination step should also be given time to work with the new candidates
3. `iceConnectionCheckTimeout`: It is useful to increase this timeout in unstable/slow network where the packet exchange takes time and hence the binding request/response. Essentially, increasing it will allow atleast one candidate pair to be tried for nomination by the other peer. 
4. `iceConnectionCheckPollingInterval`: This value is set to a default of 50 ms per [spec](https://datatracker.ietf.org/doc/html/rfc8445#section-14.2). Changing this would change the frequency of connectivity checks and essentially, the ICE state machine transitions. Decreasing the value could help in faster connection establishment in a reliable high performant network setting with good system resources. Increasing the value could help in reducing the network load, however, the connection establishment could slow down. Unless there is a strong reasoning, it is **NOT** recommended to deviate from spec/default.

## Documentation
All Public APIs are documented in our [Include.h](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h), we also generate a [Doxygen](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-c/) each commit for easier navigation.

Refer to [related](#related) for more about WebRTC and KVS.

## Development

If you would like to contribute to the development of this project, please base your pull requests off of the `origin/develop` branch, and to the `origin/develop` branch. Commits from `develop` will be merged into main periodically as a part of each release cycle.

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
