WebRTC Configuration
====================

This guide explains how to configure the WebRTC SDK for ESP-IDF using the actual ``app_webrtc_config_t`` structure.

Configuration Structure
-----------------------

The WebRTC SDK uses the ``app_webrtc_config_t`` structure to configure all aspects of WebRTC functionality:

.. code-block:: c

   typedef struct {
       // Signaling configuration
       webrtc_signaling_client_if_t *signaling_client_if;  // Signaling client interface
       void *signaling_cfg;                  // Signaling-specific configuration (opaque pointer)

       // WebRTC configuration
       webrtc_signaling_channel_role_type_t role_type;    // Role type (master initiates, viewer receives)
       bool trickleIce;                         // Whether to use trickle ICE
       bool useTurn;                            // Whether to use TURN servers
       uint32_t logLevel;                       // Log level
       bool signaling_only;                      // If TRUE, disable media streaming components to save memory

       // Media configuration
       app_webrtc_rtc_codec_t audioCodec;       // Audio codec to use
       app_webrtc_rtc_codec_t videoCodec;       // Video codec to use
       app_webrtc_streaming_media_t mediaType;   // Media type (audio-only, video-only, or both)

       // Media capture interfaces
       void* video_capture;                      // Video capture interface
       void* audio_capture;                      // Audio capture interface

       // Media player interfaces
       void* video_player;                       // Video player interface
       void* audio_player;                       // Audio player interface

       // Media reception
       bool receive_media;                       // Whether to receive media
   } app_webrtc_config_t;

Basic Configuration
-------------------

Here's a basic example of configuring the WebRTC SDK using the default configuration:

.. code-block:: c

   #include "app_webrtc.h"

   // Start with default configuration
   app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

   // Configure signaling (example using KVS signaling)
   webrtcConfig.signaling_client_if = kvs_signaling_client_if_get();
   webrtcConfig.signaling_cfg = &kvs_signaling_cfg; // Your KVS configuration

   // Set role type
   webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;

   // Configure media interfaces
   webrtcConfig.video_capture = media_stream_get_video_capture_if();
   webrtcConfig.audio_capture = media_stream_get_audio_capture_if();
   webrtcConfig.video_player = media_stream_get_video_player_if();
   webrtcConfig.audio_player = media_stream_get_audio_player_if();

   // Initialize the WebRTC application
   WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
   if (status == WEBRTC_STATUS_SUCCESS) {
       app_webrtc_run();
   }

Signaling Configuration Examples
----------------------------------

The SDK supports different signaling protocols through pluggable interfaces:

AWS KVS Signaling
^^^^^^^^^^^^^^^^^

.. code-block:: c

   // Configure for AWS KVS signaling
   kvs_signaling_config_t kvsConfig = {
       .pChannelName = "my-esp32-channel",
       .useIotCredentials = TRUE,
       .iotCoreCredentialEndpoint = "your-credential-endpoint",
       .iotCoreCert = "/spiffs/cert.pem",
       .iotCorePrivateKey = "/spiffs/private.key",
       .iotCoreRoleAlias = "your-role-alias",
       .iotCoreThingName = "your-thing-name",
       .awsRegion = "us-west-2",
       .caCertPath = "/spiffs/ca.pem"
   };

   webrtcConfig.signaling_client_if = kvs_signaling_client_if_get();
   webrtcConfig.signaling_cfg = &kvsConfig;

AppRTC Signaling
^^^^^^^^^^^^^^^^

.. code-block:: c

   // Configure for AppRTC signaling (browser-compatible)
   apprtc_signaling_config_t apprtc_config = {
       .serverUrl = NULL,  // Use default AppRTC server
       .roomId = NULL,     // Generated automatically
       .autoConnect = false,
       .connectionTimeout = 30000,
       .logLevel = 3
   };

   webrtcConfig.signaling_client_if = apprtc_signaling_client_if_get();
   webrtcConfig.signaling_cfg = &apprtc_config;

Bridge Signaling (Split Mode)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   // Configure for bridge signaling (split mode streaming device)
   bridge_signaling_config_t bridgeConfig = {
       .client_id = "streaming_client",
       .log_level = 2
   };

   webrtcConfig.signaling_client_if = getBridgeSignalingClientInterface();
   webrtcConfig.signaling_cfg = &bridgeConfig;

Media Configuration
-------------------

Configure media codecs and types:

.. code-block:: c

   // Configure codecs
   webrtcConfig.audioCodec = APP_WEBRTC_RTC_CODEC_OPUS;
   webrtcConfig.videoCodec = APP_WEBRTC_RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;

   // Configure media type
   webrtcConfig.mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;  // Both audio and video
   // webrtcConfig.mediaType = APP_WEBRTC_MEDIA_VIDEO_ONLY; // Video only
   // webrtcConfig.mediaType = APP_WEBRTC_MEDIA_AUDIO_ONLY; // Audio only

   // Enable media reception
   webrtcConfig.receive_media = true;

Role Configuration
------------------

Configure the WebRTC role (Master vs Viewer):

.. code-block:: c

   // Master role (initiates connection, sends offer)
   webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;

   // Viewer role (receives offer, sends answer)
   webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;

WebRTC Options
--------------

Configure WebRTC protocol options:

.. code-block:: c

   // ICE configuration
   webrtcConfig.trickleIce = true;  // Use trickle ICE for faster connection
   webrtcConfig.useTurn = true;     // Use TURN servers for NAT traversal

   // Logging
   webrtcConfig.logLevel = 3;       // INFO level (0=SILENT, 1=FATAL, 2=ERROR, 3=WARN, 4=INFO, 5=DEBUG, 6=VERBOSE)

Special Configuration Modes
---------------------------

Signaling-Only Mode
^^^^^^^^^^^^^^^^^^^^

For split-mode signaling devices that don't handle media:

.. code-block:: c

   // Configure for signaling-only mode (saves memory)
   webrtcConfig.signaling_only = true;

   // No media interfaces needed
   webrtcConfig.video_capture = NULL;
   webrtcConfig.audio_capture = NULL;
   webrtcConfig.video_player = NULL;
   webrtcConfig.audio_player = NULL;
   webrtcConfig.receive_media = false;

Custom Media Interfaces
^^^^^^^^^^^^^^^^^^^^^^^

You can provide custom media capture/playback implementations:

.. code-block:: c

   // Use custom video capture interface
   my_video_capture_t *my_video = my_video_capture_create();
   webrtcConfig.video_capture = my_video;

   // Use standard audio capture
   webrtcConfig.audio_capture = media_stream_get_audio_capture_if();

Complete Configuration Examples
--------------------------------

ESP Camera Example (AppRTC)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From the ``esp_camera`` example:

.. code-block:: c

   #include "app_webrtc.h"
   #include "apprtc_signaling.h"
   #include "media_stream.h"

   void esp_webrtc_camera_init(void)
   {
       // Configure AppRTC signaling
       apprtc_signaling_config_t apprtc_config = {
           .serverUrl = NULL,  // Use default AppRTC server
           .roomId = NULL,     // Will be set based on role type
           .autoConnect = false,
           .connectionTimeout = 30000,
           .logLevel = 3
       };

       // Configure WebRTC app
       app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

       // Set signaling interface to AppRTC
       webrtcConfig.signaling_client_if = apprtc_signaling_client_if_get();
       webrtcConfig.signaling_cfg = &apprtc_config;

       // Configure role and media
       webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
       webrtcConfig.video_capture = media_stream_get_video_capture_if();
       webrtcConfig.audio_capture = media_stream_get_audio_capture_if();
       webrtcConfig.video_player = media_stream_get_video_player_if();
       webrtcConfig.audio_player = media_stream_get_audio_player_if();
       webrtcConfig.mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
       webrtcConfig.receive_media = true;

       // Initialize and run
       WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
       if (status == WEBRTC_STATUS_SUCCESS) {
           app_webrtc_run();
       }
   }

Signaling-Only Example (Split Mode)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From the ``signaling_only`` example:

.. code-block:: c

   #include "app_webrtc.h"
   #include "kvs_signaling.h"

   void signaling_only_init(void)
   {
       // Configure KVS signaling
       kvs_signaling_config_t kvsConfig = {
           .pChannelName = "my-esp32-channel",
           .useIotCredentials = TRUE,
           .iotCoreCredentialEndpoint = "your-credential-endpoint",
           .iotCoreCert = "/spiffs/cert.pem",
           .iotCorePrivateKey = "/spiffs/private.key",
           .iotCoreRoleAlias = "your-role-alias",
           .iotCoreThingName = "your-thing-name",
           .awsRegion = "us-west-2"
       };

       // Configure WebRTC for signaling-only mode
       app_webrtc_config_t webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
       webrtcConfig.signaling_client_if = kvs_signaling_client_if_get();
       webrtcConfig.signaling_cfg = &kvsConfig;
       webrtcConfig.role_type = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
       webrtcConfig.signaling_only = true; // Disable streaming components to save memory

       // No media interfaces needed for signaling-only mode
       webrtcConfig.video_capture = NULL;
       webrtcConfig.audio_capture = NULL;
       webrtcConfig.video_player = NULL;
       webrtcConfig.audio_player = NULL;
       webrtcConfig.receive_media = false;

       // Initialize and run
       WEBRTC_STATUS status = app_webrtc_init(&webrtcConfig);
       if (status == WEBRTC_STATUS_SUCCESS) {
           app_webrtc_run();
       }
   }

Configuration Best Practices
-----------------------------

1. **Always start with defaults**: Use ``WEBRTC_APP_CONFIG_DEFAULT()`` and modify only what you need.

2. **Validate interfaces**: Check that media interfaces are not NULL before assigning them.

3. **Choose appropriate role**: Use MASTER for devices that initiate connections, VIEWER for receiving devices.

4. **Memory optimization**: Use ``signaling_only = true`` for devices that only handle signaling.

5. **Error handling**: Always check the return status of ``app_webrtc_init()`` and ``app_webrtc_run()``.

See Also
--------

- :doc:`examples` - Complete working examples
- :doc:`webrtc_signaling` - Signaling interface details
- :doc:`media_stream` - Media capture/playback interfaces
- :doc:`../c-api-reference/kvs_signaling` - KVS signaling API reference
