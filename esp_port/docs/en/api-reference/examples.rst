Examples
========

The Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF includes several example applications that demonstrate different use cases and operational modes.

.. note::
   **📖 Complete Documentation**: Each example includes a comprehensive README with step-by-step instructions. The documentation below provides links to these detailed guides.

.. note::
   For architectures and flows, see :doc:`../solutions` and :doc:`../flows`.

Quick Start Resources
---------------------

Before diving into examples, check out these essential guides:

========================== ================================================
Resource                   Description
========================== ================================================
**Main README**            Project overview & getting started (`README.md`_)
**API Usage Guide**        Complete API reference & patterns (`API_USAGE.md`_)
**Custom Signaling**       How to implement custom signaling (`CUSTOM_SIGNALING.md`_)
**Components Guide**       ESP-IDF components documentation (`components/README.md`_)
========================== ================================================

.. _README.md: https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/README.md
.. _API_USAGE.md: https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/API_USAGE.md
.. _CUSTOM_SIGNALING.md: https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/CUSTOM_SIGNALING.md
.. _components/README.md: https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/components/README.md

Example Quick Reference
-----------------------

==================== =================================================================
Example              Purpose & Documentation
==================== =================================================================
**webrtc_classic**   AWS KVS single-device (`README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/webrtc_classic/README.md>`__)
**esp_camera**       AppRTC browser-compatible (`README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/esp_camera/README.md>`__)
**streaming_only**   Split-mode media device (`README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/streaming_only/README.md>`__)
**signaling_only**   Split-mode signaling device (`README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/signaling_only/README.md>`__)
==================== =================================================================

Detailed Example Documentation
------------------------------

Each example includes comprehensive documentation with step-by-step instructions, configuration details, and troubleshooting guides. Click the README links in the table above for complete details.

Quick Overview
~~~~~~~~~~~~~~

**WebRTC Classic** (`webrtc_classic README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/webrtc_classic/README.md>`__)
  🏆 **AWS KVS Streaming** - Complete single-device implementation with AWS Kinesis Video Streams integration. Auto-connects to AWS on startup.

**ESP Camera** (`esp_camera README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/esp_camera/README.md>`__)
  🌐 **AppRTC Compatible** - Browser-compatible WebRTC camera using AppRTC protocol. No AWS account needed.

**Streaming Only** (`streaming_only README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/streaming_only/README.md>`__)
  📱 **Split Mode Streaming** - Power-optimized streaming device for ESP32-P4. Handles media streaming in dual-chip configurations.

**Signaling Only** (`signaling_only README <https://github.com/aws-samples/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/esp_port/examples/signaling_only/README.md>`__)
  📡 **Split Mode Signaling** - Always-on communication hub for ESP32-C6. Maintains AWS KVS connection while streaming device sleeps.

Example Comparison
------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Feature
     - WebRTC Classic
     - Streaming Only
     - Signaling Only
     - ESP WebRTC Camera
   * - Signaling
     - ✓
     - ✗
     - ✓
     - ✓
   * - Media Streaming
     - ✓
     - ✓
     - ✗
     - ✓
   * - Recommended Hardware
     - ESP32-S3
     - ESP32-P4
     - ESP32-C6
     - ESP32-S3
   * - Memory Usage
     - Medium
     - Medium
     - Low
     - High
   * - Split Mode Compatible
     - ✗
     - ✓
     - ✓
     - ✗
