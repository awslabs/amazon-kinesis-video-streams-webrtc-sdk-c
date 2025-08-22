Getting Started
===============

This guide will help you get started with the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF, featuring the new simplified API with smart defaults and pluggable architecture.

Prerequisites
-------------

Before you begin, ensure you have the following:

* ESP-IDF v5.4 or later installed and configured
* An AWS account with access to Kinesis Video Streams
* One of the supported ESP32 development boards:

  * ESP32-P4-Function-EV-Board
  * ESP32-S3-EYE
  * ESP32-WROVER-KIT
  * Other ESP32 variants (with appropriate camera modules)

Setup Steps
-----------

Clone the Repository
^^^^^^^^^^^^^^^^^^^^

Clone the WebRTC SDK repository with all submodules:

.. code-block:: bash

   git clone --recursive <git url>
   cd amazon-kinesis-video-streams-webrtc-sdk-c

If you've already cloned without ``--recursive``, update the submodules:

.. code-block:: bash

   git submodule update --init

Install the ESP-IDF
^^^^^^^^^^^^^^^^^^^^

Please follow the `Espressif instructions <https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html>`_ to set up the IDF environment.

Clone the IDF branch with ``release/v5.4``:

.. code-block:: bash

   git clone -b release/v5.4 --recursive https://github.com/espressif/esp-idf.git esp-idf

Apply ESP-IDF Patches (Required Step)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Before building for ESP32/ESP-IDF, you must apply the required patches to your ESP-IDF source tree.
The patches are located in ``esp_port/patches/``:

Patch on ``ets_sys.h`` files which suppress redefinition of ``STATUS``:

.. code-block:: bash

   cd $IDF_PATH
   git am -i <path-to-sdk>/esp_port/patches/0001-ets_sys-Fix-for-STATUS-define.patch
   cd -

Patch on IDF, required for esp_hosted to work correctly:

.. code-block:: bash

   cd $IDF_PATH
   git am -i <path-to-sdk>/esp_port/patches/0002-Fixes-for-IDF-deep-sleep-and-lwip_split_for_esp_host.patch
   cd -

Install the tools and set the environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For Linux/Unix:

.. code-block:: bash

   export IDF_PATH=</path/to/esp-idf>
   cd $IDF_PATH
   ./install.sh
   . ./export.sh

For Windows, follow the instructions in the `ESP-IDF Windows Setup Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html>`_.

Configure AWS Credentials
^^^^^^^^^^^^^^^^^^^^^^^^^

You can use either direct AWS credentials, IoT Core credentials, or ESP RainMaker integration for simplified credential management:

Option A: Direct AWS Credentials
""""""""""""""""""""""""""""""""

Set up your AWS credentials in the project configuration:

.. code-block:: bash

   cd esp_port/examples/webrtc_classic
   idf.py menuconfig

Navigate to "Example Configuration" and set:

* AWS_ACCESS_KEY_ID
* AWS_SECRET_ACCESS_KEY
* AWS_DEFAULT_REGION
* AWS_KVS_CHANNEL

Option B: IoT Core Credentials
""""""""""""""""""""""""""""""

Enable IoT Core credentials in menuconfig and place your certificates in the appropriate directory:

1. Enable IoT credentials in menuconfig:

   .. code-block:: bash

      idf.py menuconfig

   Navigate to "Example Configuration" → "Enable IoT Core Credentials"

2. Place your certificates in the SPIFFS image directory:

   .. code-block:: none

      esp_port/examples/app_common/spiffs_image/certs/

   Required files:

   * certificate.pem
   * private.key
   * cacert.pem

Option C: ESP RainMaker Integration (Recommended)
"""""""""""""""""""""""""""""""""""""""""""""""""

For the best experience with ESP32 camera devices, use ESP RainMaker integration which provides:

* **Simplified AWS Credentials**: Automatic credential management using ``esp_rmaker_get_aws_security_token()``
* **Centralized Device Management**: Control camera settings through ESP RainMaker mobile app
* **Optimized Memory Usage**: Credentials allocated from external RAM with proper cleanup
* **Reduced Code Complexity**: No manual certificate handling required

See the `kvs_webrtc_camera example <https://github.com/espressif/esp-rainmaker/tree/master/examples/kvs_webrtc_camera>`_ in the ESP RainMaker repository for complete implementation.

The RainMaker integration features:

.. code-block:: c

   // Simplified camera device creation (includes name and channel parameters)
   esp_rmaker_device_t *device = esp_rmaker_camera_device_create("Camera", NULL);

   // Automatic AWS region detection
   char *aws_region = esp_rmaker_get_aws_region();

   // Streamlined credential retrieval
   esp_rmaker_aws_credentials_t *credentials = esp_rmaker_get_aws_security_token("esp-videostream-v1-NodeRole");

Build and Flash
^^^^^^^^^^^^^^^

Build and flash the example for your specific board:

.. code-block:: bash

   # Set the target
   idf.py set-target esp32p4  # or esp32s3, esp32

   # Build
   idf.py build

   # Flash and monitor
   idf.py -p [PORT] flash monitor

Running Your First WebRTC Application
-------------------------------------

All examples now use the **new simplified API** with smart defaults. Instead of 12+ configuration fields, you only need to set 4 essential fields.

Classic Mode Example
^^^^^^^^^^^^^^^^^^^^

The ``webrtc_classic`` example demonstrates a complete WebRTC application running on a single ESP32 device using the simplified API:

.. code-block:: c

   // Simplified 4-line configuration
   app_webrtc_config_t config = APP_WEBRTC_CONFIG_DEFAULT();
   config.signaling_client_if = kvs_signaling_client_if_get();
   config.peer_connection_if = kvs_peer_connection_if_get();
   config.video_capture = media_stream_get_video_capture_if();
   // Smart defaults: MASTER role, H.264+OPUS, trickle ICE, TURN

After flashing:

1. The device will connect to Wi-Fi with the configured credentials
2. It will establish a connection to AWS KVS using smart defaults
3. When a viewer connects to the channel, video streaming will begin automatically

To view the stream:

1. Open the `KVS WebRTC Test Page <https://awslabs.github.io/amazon-kinesis-video-streams-webrtc-sdk-js/examples/index.html>`_
2. Enter your AWS credentials
3. Enter the same channel name configured in your ESP32 application
4. Click "Start Viewer" to view the stream

Split Mode Example
^^^^^^^^^^^^^^^^^^

For split mode operation with ESP32-P4 and ESP32-C6:

1. Build and flash the ``signaling_only`` example to ESP32-C6:

   .. code-block:: bash

      cd esp_port/examples/signaling_only
      idf.py set-target esp32c6
      idf.py build
      idf.py -p [C6_PORT] flash monitor

2. Build and flash the ``streaming_only`` example to ESP32-P4:

   .. code-block:: bash

      cd esp_port/examples/streaming_only
      idf.py set-target esp32p4
      idf.py build
      idf.py -p [P4_PORT] flash monitor

3. View the stream using the same method as the classic example

Next Steps
----------

* Explore the :doc:`api-reference/media_stream` guide to customize video and audio settings
* Check the :doc:`c-api-reference/index` for detailed API documentation
* See the :doc:`api-reference/examples` section for more advanced usage scenarios
