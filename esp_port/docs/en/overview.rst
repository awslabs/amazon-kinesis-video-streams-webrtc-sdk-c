Overview
========

This section provides an overview of the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF, featuring the new simplified API with pluggable architecture.

Software Stack Overview
-----------------------

This SDK is an ESP‑IDF based solution. The software layers are organized as follows:

1. **ESP‑IDF (Foundation Layer)**: RTOS, drivers, networking (LWIP), peripherals
2. **Kinesis Video Streams WebRTC SDK for C and Core Components (Middle Layer)**:
   - Ported KVS WebRTC SDK for C
   - Signaling client(s), media pipeline, peer connection, utilities
3. **Application Components and Examples (Top Layer)**:
   - Example apps (classic/split), camera integrations, and auxiliary libraries

Architecture
------------

.. figure:: ../_static/webrtc_architecture.svg
   :alt: WebRTC Software Architecture (Component Blocks)
   :align: center
   :width: 100%

Core Components
---------------

The SDK features a **pluggable architecture** with the following core components:

* **Simplified API Layer (app_webrtc.h)**: 4-line configuration with smart defaults and advanced override APIs
* **Signaling Interfaces**: Pluggable signaling implementations (KVS, AppRTC, Bridge, Custom)
* **Peer Connection Interfaces**: Pluggable peer connection implementations (KVS WebRTC, Bridge-only, Custom)
* **Media Pipeline**: Standardized capture/player interfaces for audio and video data
* **ESP-IDF Integration**: Deep integration with ESP-IDF components for Wi-Fi, camera, and audio drivers

New Simplified API Benefits
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The new API design provides:

* **Smart Defaults**: MASTER role, H.264+OPUS codecs, trickle ICE, TURN servers enabled
* **Auto-Detection**: Automatically detects signaling-only vs streaming modes based on provided interfaces
* **Pluggable**: Mix and match signaling and peer connection implementations
* **Backward Compatible**: Advanced users can override any default with dedicated APIs

Next Steps
----------

For deployment options and topologies, see:

- :doc:`solutions` — single‑chip and dual‑chip options
- :doc:`api-reference/architecture` — Classic vs Split architectural diagrams
- :doc:`flows` — Classic and Split flow diagrams

RainMaker Integration
---------------------

The SDK can be integrated with ESP RainMaker, allowing for:

* Remote control of WebRTC streaming through the RainMaker cloud service
* Management of WebRTC configuration through RainMaker parameters
* Integration with other RainMaker-enabled devices and services

This integration enables seamless incorporation of WebRTC streaming capabilities into your IoT ecosystem.

Supported Hardware
------------------

The SDK supports the following ESP32 hardware platforms:

* ESP32
* ESP32-S3
* ESP32-C3
* ESP32-C6
* ESP32-P4

For optimal performance, ESP32-S3 or ESP32-P4 is recommended for video streaming applications due to their enhanced processing capabilities and hardware acceleration features.

Memory Requirements
-------------------

The memory requirements for the SDK vary depending on the operational mode and features used:

* **Classic Mode**: Requires approximately 400KB of RAM and 1.2MB of flash memory.
* **Split Mode**: Requires approximately 250KB of RAM and 800KB of flash memory.
* **With Video Streaming**: Additional 100-200KB of RAM depending on resolution and frame rate.
* **With Audio Streaming**: Additional 50-100KB of RAM.

These requirements can be optimized based on your specific use case and configuration.
