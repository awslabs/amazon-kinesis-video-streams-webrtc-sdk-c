Flows by Mode
=============

This section shows the high‑level data and control flows for the offered solutions in Classic and Split modes.

Classic Mode Flow
-----------------

**Single ESP32 device** handles both signaling and streaming.

.. uml:: ../webrtc_classic.puml
   :caption: WebRTC Classic Mode Architecture & Flow
   :align: center
   :width: 100%

Split Mode Flow
---------------

**Dual ESP32 devices** — ESP32‑C6 for signaling (always‑on) and ESP32‑P4 for streaming (power‑optimized, sleep‑capable).

.. uml:: ../webrtc_split.puml
   :caption: WebRTC Split Mode Architecture & Flow
   :align: center
   :width: 100%

ESP Camera Lifecycle (Optional)
-------------------------------

The complete operational lifecycle for an ESP32 camera device using WebRTC.

.. uml:: ../camera_device.puml
   :caption: ESP Camera WebRTC Lifecycle
   :align: center
   :width: 100%
