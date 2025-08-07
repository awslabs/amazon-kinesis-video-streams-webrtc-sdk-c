Solutions Offered
=================

Choose the deployment that best fits your product needs. The SDK supports three configurations that balance simplicity, power, and flexibility.

Single‑chip Solution (Option 1)
-------------------------------

- Complete firmware including signaling and media streaming on a single chip (ESP32‑S3/ESP32, etc.)
- Simplest setup and fastest path to streaming

Dual‑chip with Network Co‑processor (Option 2)
----------------------------------------------

- ESP32‑P4: Handles WebRTC peer connection and streaming; runs alongside RainMaker, Matter, etc.
- ESP32‑C6: Works as a network co‑processor; typically pre‑built and rarely modified

.. figure:: ../_static/camera_sdk_solution_1-2.svg
   :alt: ESP WebRTC Camera SDK Solution Options (Option 1 and 2)
   :align: center
   :width: 100%

Dual‑chip with WebRTC and LWIP Split (Option 3)
-----------------------------------------------

- Signaling and media streaming are split across ESP32‑P4 and ESP32‑C6
- ESP32‑P4: WebRTC peer connection and streaming; can be pre‑built
- ESP32‑C6: Signaling backend (KVS, RainMaker, Matter, or custom)
- Power‑optimized: ESP32‑P4 can deep sleep while ESP32‑C6 maintains signaling and wakes streaming on demand

.. figure:: ../_static/camera_sdk_solution_3.svg
   :alt: ESP WebRTC Camera SDK Solution Option 3
   :align: center
   :width: 80%

Key Features
------------

- Multiple signaling options: Amazon Kinesis Video Streams, ESP RainMaker, Master/Viewer, or custom
- Flexible architecture: single‑chip, dual‑chip, and network co‑processor configurations
- Real‑time streaming: high‑quality video/audio over WebRTC
- Multi‑platform: ESP32, ESP32‑S3, ESP32‑C3, ESP32‑C6, ESP32‑P4
- Power optimization: advanced power management with split‑mode architectures
- Camera integration: seamless support for ESP32 camera modules
- Pre‑compiled firmware: ready‑to‑use firmware for quick bring‑up
