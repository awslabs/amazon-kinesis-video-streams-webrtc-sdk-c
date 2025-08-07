WebRTC Signaling
================

This section describes the signaling process used by the Amazon Kinesis Video Streams WebRTC SDK for ESP-IDF.

Signaling Overview
------------------

WebRTC signaling is the process of coordinating communication between peers. In the context of Amazon Kinesis Video Streams, signaling is used to establish a connection between a WebRTC client (ESP32 device) and the Kinesis Video Streams service. The signaling process involves exchanging session descriptions, ICE candidates, and other metadata necessary to establish a peer-to-peer connection.

Signaling Architecture
----------------------

The signaling architecture in the SDK consists of the following components:

* **Signaling Client**: Handles the communication with the Amazon Kinesis Video Streams signaling service.
* **Signaling Channel**: A Kinesis Video Streams resource that acts as a rendezvous point for WebRTC clients.
* **Signaling Callbacks**: User-defined callbacks that are invoked when signaling events occur.
* **Credential Provider**: Provides AWS credentials for authenticating with the signaling service.

Signaling Flow
--------------

The signaling flow in the SDK follows these steps:

1. **Create Signaling Client**: Initialize the signaling client with configuration parameters.
2. **Connect to Signaling Channel**: Establish a connection to the Kinesis Video Streams signaling channel.
3. **Exchange Session Descriptions**: Send and receive SDP (Session Description Protocol) messages.
4. **Exchange ICE Candidates**: Send and receive ICE (Interactive Connectivity Establishment) candidates.
5. **Establish Peer Connection**: Use the exchanged information to establish a peer connection.

Signaling API Usage
-------------------

The signaling API is designed to be simple and easy to use. Here's a basic example of how to use the signaling API:

.. code-block:: c

    // Initialize signaling client
    SignalingClientInfo clientInfo;
    SignalingClientCallbacks callbacks;
    SignalingClientMetrics metrics;
    SignalingClientConfig config;

    // Set client info
    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_DEBUG;
    STRCPY(clientInfo.clientId, CLIENT_ID);

    // Set callbacks
    callbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    callbacks.customData = (UINT64) pSampleConfiguration;
    callbacks.messageReceivedFn = on_msg_received;
    callbacks.errorReportFn = onSignalingClientError;
    callbacks.stateChangeFn = onSignalingClientStateChange;

    // Set configuration
    config.version = SIGNALING_CLIENT_CONFIG_CURRENT_VERSION;
    config.channelInfo.version = SIGNALING_CHANNEL_INFO_CURRENT_VERSION;
    config.channelInfo.pChannelName = pChannelName;
    config.channelInfo.pChannelArn = pChannelArn;
    config.channelInfo.pRegion = pRegion;
    config.channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    config.channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    config.clientInfo = &clientInfo;
    config.pCredentialProvider = pCredentialProvider;

    // Create signaling client
    createSignalingClientSync(&config, &callbacks, &metrics, &signalingHandle);

    // Connect to signaling channel
    connectSignalingChannel(signalingHandle);

Callback Implementation
-----------------------

The signaling API requires several callbacks to be implemented:

.. code-block:: c

    // Message received callback
    VOID on_msg_received(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
    {
        PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

        switch (pReceivedSignalingMessage->messageType) {
            case SIGNALING_MESSAGE_TYPE_OFFER:
                // Handle offer
                handleOffer(pSampleConfiguration, pReceivedSignalingMessage);
                break;
            case SIGNALING_MESSAGE_TYPE_ANSWER:
                // Handle answer
                handleAnswer(pSampleConfiguration, pReceivedSignalingMessage);
                break;
            case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
                // Handle ICE candidate
                handleIceCandidate(pSampleConfiguration, pReceivedSignalingMessage);
                break;
            default:
                // Unhandled message type
                break;
        }
    }

    // State change callback
    VOID onSignalingClientStateChange(UINT64 customData, SIGNALING_CLIENT_STATE state)
    {
        PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

        switch (state) {
            case SIGNALING_CLIENT_STATE_READY:
                // Signaling client is ready
                break;
            case SIGNALING_CLIENT_STATE_CONNECTED:
                // Signaling client is connected
                break;
            case SIGNALING_CLIENT_STATE_DISCONNECTED:
                // Signaling client is disconnected
                break;
            default:
                // Unhandled state
                break;
        }
    }

    // Error callback
    VOID onSignalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
    {
        PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

        // Handle error
        printf("Signaling client error: %.*s\n", msgLen, msg);
    }

Master and Viewer Roles
-----------------------

The SDK supports two roles for signaling:

* **Master**: The master role is typically used by the device that initiates the WebRTC connection. In a Kinesis Video Streams context, this is often the device that is streaming video.
* **Viewer**: The viewer role is typically used by the device that receives the WebRTC connection. In a Kinesis Video Streams context, this is often the device that is viewing the video stream.

The role is specified when creating the signaling client:

.. code-block:: c

    // For master role
    config.channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;

    // For viewer role
    config.channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;

Split Mode Signaling
--------------------

In split mode, signaling is handled by a separate device (typically a network coprocessor). The main processor and the network coprocessor communicate using a custom protocol over a serial interface. The network coprocessor handles all signaling with the Kinesis Video Streams service, while the main processor handles media streaming.

To use split mode signaling:

1. Configure the main processor to communicate with the network coprocessor.
2. Configure the network coprocessor to handle signaling with Kinesis Video Streams.
3. Implement the custom protocol for communication between the two processors.

Detailed documentation on split mode signaling can be found in the ESP-IDF examples directory.

Troubleshooting Signaling Issues
--------------------------------

Common signaling issues and their solutions:

1. **Connection Failures**: Ensure that the device has internet connectivity and that the AWS credentials are valid.
2. **Authentication Errors**: Verify that the AWS credentials have the necessary permissions to access the Kinesis Video Streams service.
3. **Timeout Errors**: Check network connectivity and increase timeout values if necessary.
4. **ICE Connection Issues**: Ensure that the device is not behind a restrictive firewall that blocks UDP traffic.
5. **SDP Negotiation Failures**: Check that the SDP messages are being properly exchanged and that the WebRTC configuration is compatible.

For more detailed troubleshooting, enable debug logging and monitor the signaling client state changes.

.. Signaling API Reference
.. -----------------------

.. .. include-build-file:: inc/WebRtcSignaling.inc
