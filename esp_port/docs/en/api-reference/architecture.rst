Architecture Overview
=====================

This section provides a comprehensive view of the different WebRTC architectures supported by the ESP-IDF SDK.

WebRTC Classic Mode
-------------------

Classic mode runs both signaling and media streaming on a single ESP32 device, providing the simplest setup for WebRTC applications.

.. uml:: ../../webrtc_classic.puml
   :caption: WebRTC Classic Mode - Single Device Architecture
   :align: center
   :width: 100%

**Key Characteristics:**

* **Single Device**: All WebRTC functionality on one ESP32
* **AWS KVS Integration**: Uses Amazon Kinesis Video Streams for signaling
* **Complete Solution**: Handles both signaling and media streaming
* **Simple Setup**: Easiest to configure and deploy
* **Best For**: Prototyping, simple applications, single-device solutions

WebRTC Split Mode
-----------------

Split mode distributes signaling and media streaming across two ESP32 devices, optimizing for power efficiency and performance.

.. uml:: ../../webrtc_split.puml
   :caption: WebRTC Split Mode - Dual Device Architecture
   :align: center
   :width: 100%

**Key Characteristics:**

* **Dual Device**: ESP32-C6 for signaling, ESP32-P4 for streaming
* **Power Optimized**: Streaming device can sleep when not active
* **Performance Focused**: Each chip optimized for its specific role
* **Bridge Communication**: UART/SPI coordination between devices
* **Best For**: Battery-powered applications, high-performance streaming

ESP Camera Device Lifecycle
---------------------------

Complete operational flow for ESP32 camera devices using WebRTC, showing the full lifecycle from initialization to streaming.

.. note::

   The full device lifecycle flow has moved to :doc:`../flows`.

* **Error Recovery**: Robust handling of connection failures
* **Best For**: Production camera applications, IoT devices

Architecture Comparison
-----------------------

======================= =================== =================== ===================
Feature                 Classic Mode        Split Mode          ESP Camera
======================= =================== =================== ===================
**Device Count**        1 (ESP32)           2 (C6 + P4)         1 (ESP32)
**Signaling**           On-device           ESP32-C6            AppRTC/KVS
**Media Streaming**     On-device           ESP32-P4            On-device
**Power Efficiency**    Standard            Optimized           Standard
**Setup Complexity**    Simple              Complex             Medium
**Best Use Case**       General purpose     Battery-powered     Camera applications
**AWS Requirement**     Yes (KVS)           Yes (KVS)           Optional
======================= =================== =================== ===================

Choosing the Right Architecture
-------------------------------

**Use Classic Mode When:**

* Prototyping or proof-of-concept development
* Simple single-device applications
* Power consumption is not critical
* Easy setup and maintenance is priority

**Use Split Mode When:**

* Battery-powered or power-constrained applications
* High-performance streaming requirements
* Advanced power management needed
* Willing to manage dual-device complexity

**Use ESP Camera Approach When:**

* Building dedicated camera applications
* Need robust lifecycle management
* Require multi-viewer support
* Want production-ready camera solutions

Development Workflow
--------------------

**1. Start with Classic Mode**
  - Quickest to implement and test
  - Understand WebRTC fundamentals
  - Validate core functionality

**2. Consider Split Mode for Optimization**
  - When power efficiency becomes critical
  - For high-performance requirements
  - In battery-powered applications

**3. Implement ESP Camera Features**
  - For production camera applications
  - When lifecycle management is important
  - For robust multi-viewer scenarios

Next Steps
----------

* **Getting Started**: See :doc:`examples` for hands-on tutorials
* **API Reference**: Check :doc:`../c-api-reference/index` for detailed API documentation
* **Custom Implementation**: Review :doc:`webrtc_config` for configuration options