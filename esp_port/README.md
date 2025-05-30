# ESP-IDF Port of Amazon Kinesis Video Streams WebRTC SDK

This directory contains the ESP-IDF port of the Amazon Kinesis Video Streams WebRTC SDK. The port is organized as a collection of ESP-IDF components that can be used in ESP-IDF projects.

## Directory Structure

```
esp_port/
├── components/             # ESP-IDF components
│   ├── api_call/           # API call handling component
│   ├── credential/         # Credential management
│   ├── esp_hosted/         # ESP hosted functionality
│   ├── esp_usrsctp/        # SCTP protocol implementation for ESP
│   ├── esp_webrtc_utils/   # WebRTC utilities for ESP
│   ├── esp_wifi_remote/    # Remote WiFi functionality
│   ├── kvs_utils/          # KVS utility functions
│   ├── kvs_webrtc/         # Main KVS WebRTC component
│   ├── libllhttp/          # HTTP parsing library
│   ├── libsrtp2/           # SRTP (Secure Real-time Transport Protocol) library
│   ├── libwebsockets/      # WebSocket library
│   ├── media_stream/       # Component responsible for capturing video/audio and playing it
│   ├── network_coprocessor/ # Network coprocessor support
│   ├── patches/            # ESP-IDF patches need to be applied using git am
│   └── state_machine/      # State machine implementation
├── docs/                   # Some puml diagrams demonstrating different WebRTC scenarios
├── examples/               # Example applications
└── README.md               # This README
```

## Operational Modes

The WebRTC SDK supports two operational modes for different hardware configurations:

### Classic Mode
In classic mode, signaling and streaming are performed on the same chip. This mode can be implemented as:
- **Single Chip Solution**: All WebRTC functionality runs on a single ESP chip (e.g., ESP32-WROVER-KIT, ESP32-S3-EYE)
- **Dual Chip Solution**:
  - Host processor (e.g., ESP32-P4) handles all signaling and streaming
  - Wi-Fi coprocessor (e.g., ESP32-C6) transparently forwards network traffic to the main processor

The `webrtc_classic` example demonstrates this functionality.

### Split Mode
Split mode is designed for dual chip solutions, dividing responsibilities between processors:
- **Signaling Processor**: Network coprocessor (ESP32-C6) handles signaling with KVS
- **Streaming Processor**: Main processor (ESP32-P4) handles media streaming

 - More about this in later sections

## Setup

### Clone the project

Please clone this project using the following git command:

```bash
git clone --recursive <git url>
```

If you've already cloned it without `--recursive` switch do submodule update.

```bash
cd </cloned/dir/path/>
git submodule update --init
```

### Install the ESP-IDF

Please follow the [Espressif instructions](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html) to set up the IDF environment.

Clone the IDF branch with `release/v5.4`:

```bash
git clone -b release/v5.4 --recursive https://github.com/espressif/esp-idf.git esp-idf
```

### Apply ESP-IDF Patches (Required Step)

Before building for ESP32/ESP-IDF, you must apply the required patches to your ESP-IDF source tree.
The patches are located in [`esp_port/patches/`](./patches/):

Patch on `ets_sys.h` files which suppress redefinition of `STATUS`:
```bash
cd $IDF_PATH
git am -i <path-to-sdk>/esp_port/patches/0001-ets_sys-Fix-for-STATUS-define.patch
cd -
```

Patch on IDF, required for esp_hosted to work correctly:
```bash
cd $IDF_PATH
git am -i <path-to-sdk>/esp_port/patches/0002-Fixes-for-IDF-deep-sleep-and-lwip_split_for_esp_host.patch
cd -
```

### Install the tools and set the environment. For Linux/unix this looks like

```bash
export IDF_PATH=</path/to/esp-idf>
cd $IDF_PATH
./install.sh
. ./export.sh
```

### Install pkg-config

```bash
sudo apt-get install pkg-config
```

## Demonstration

You can find three examples present under `examples` directory.
The main logical steps involved in WebRTC are signalling and streaming. Signalling is something which deals with authentication and negotiation with signalling server. Streaming is only started once signalling is successful. Based on where the signalling and streaming are performed, there are two modes of operation.

### `classic_mode`
Typically, WebRTC can work on single chip solution or dual chip solution. In this mode, signalling and streaming are done without splitting the functionality, on the same chip.
- The example `webrtc_classic` can be built and flashed on the board to use this functionality.
- Single Chip
  - This can be used on any ESP board from mentioned: ESP32-WROVER-KIT, ESP32-S3-EYE. These dev boards have the Wi-Fi capabilities built in. With some modifications, it should be possible to extend this support to other dev boards and chipsets.
- Dual Chip
  - Host or main processor (e.g., ESP32-P4) runs the network stack and responsible for handling signalling and streaming.
  - Wi-Fi capability is provided by the co-processor chipset (on board ESP32-C6), which transperantly forwards all incoming frames to main processor for processing.
  - The [network_adapter](examples/network_adapter/README.md) example can be used to build and flash the co-processor.

### `split_mode`
split mode is generally referred on dual chip solution. In this mode, Signalling and Streaming roles are split on different chips. Main  processor is responsible of streaming and the network co-processor is responsible for signalling.
- `signaling_only`: special application discussed [below](#signalling-and-streaming-split-on-esp32-p4-and-esp32-c6). <br>
    Signalling only binary flashed on network co-processor, i.e. ESP32-C6
- `streaming_only`: special app discussed [below](#signalling-and-streaming-split-on-esp32-p4-and-esp32-c6).<br>
    Streaming only binary flashed on main processor, i.e. ESP32-P4

### Signalling and Streaming Split on ESP32-P4 and ESP32-C6
- Espressif's innovative solution, `esp_hosted` allows us to run network stack on both the co-processor and host. Network stack running on both the processors, share same IP but different, non-overlapping port numbers.
- `ESP32-P4 Function_EV_Board` is equipped with on-board main processor, `ESP32-P4` and on-board network co-processor, `ESP32-C6`.
- Signalling functionality of WebRTC would run on network co-processor, i.e., `ESP32-C6`.
- Once the streaming request is received by `ESP32-C6`, `ESP32-P4` is notified to start the streaming.
- This gives us two advantages:
  - Significant power savings <br>
  ESP32-P4 is a powerful and fast MCU. But at the same time, consumes higher power.
  By default, Keeping `ESP32-P4` in `deep sleep` mode and only wake-up once streaming request is received at `ESP32-C6`, would save a lot of power.
  - Instant wake-up times <br>
  Since network stack is running on both the sides, ESP32-P4 can instantly get IP address after waking up.

### Using the `split_mode`
- Similar to `classic_mode` on ESP32-P4, this can be built and flashed.
  - Build the `streaming_only` application and flash it on ESP32-P4
  - Build the `signalling_only` application and flash it on ESP32-C6
  - That's it, you can now start viewer from the [WebRTC Test webpage](https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html).
    - You can observe that, the ESP32-C6 is connected to the signalling server and relays only absolutely necessary messages to ESP32-P4. Also, it forwards messages from ESP32-P4 to the signalling server.
    - The final RTP and SCTP sessions are directly established from the ESP32-P4.
- Please follow [streaming_only](examples/streaming_only/README.md) and [signalling_only](examples/signalling_only/README.md) for more in-depth understanding of how this works.


## BUILD

### Configure the project
Go to the example directory and select the target using following command:

```bash
# The non-split legacy webrtc app
cd examples/webrtc_classic

# for esp32
idf.py set-target esp32

# for esp32s3
idf.py set-target esp32s3

# for esp32p4
idf.py set-target esp32p4
```

Use menuconfig of ESP-IDF to configure the project.

```bash
idf.py menuconfig
```

- These parameters under Example Configuration Options must be set:

  - ESP_WIFI_SSID
  - ESP_WIFI_PASSWORD
  - ESP_MAXIMUM_RETRY
  - AWS_ACCESS_KEY_ID
  - AWS_SECRET_ACCESS_KEY
  - AWS_DEFAULT_REGION
  - AWS_KVS_CHANNEL
  - AWS_KVS_LOG_LEVEL

- You may override the above settings in `main/app_main.c` file of the example also.

### Using IoT Credentials
It is also possible to use AWS IoT credentials instead of Access token.
- For this, please find and set `CONFIG_IOT_CORE_ENABLE_CREDENTIALS` via menuconfig.
- Put the certificates under [examples/app_common/spiffs_image/certs](examples/app_common/spiffs_image/certs/) directory.
- To generate these certificates, please take a look at [this](../scripts/generate-iot-credential.sh) script.

- Find the detailed info on KVS specific setup [here](../README#setup-iot).

## License

This project is licensed under the Apache-2.0 License.
