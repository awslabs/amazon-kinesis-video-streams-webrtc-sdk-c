#include "Samples.h"

extern PSampleConfiguration gSampleConfiguration;
static UINT64 videoTimestamp = 0;

#ifdef ENABLE_DATA_CHANNEL

// onMessage callback for a message received by the viewer on a data channel
VOID dataChannelOnMessageCallback(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pDataChannel);
    if (isBinary) {
        DLOGI("DataChannel Binary Message");
    } else {
        DLOGI("DataChannel String Message: %.*s", pMessageLen, pMessage);
    }
}

// onOpen callback for the onOpen event of a viewer created data channel
VOID dataChannelOnOpenCallback(UINT64 customData, PRtcDataChannel pDataChannel)
{
    STATUS retStatus = STATUS_SUCCESS;
    DLOGI("New DataChannel has been opened %s ", pDataChannel->name);
    dataChannelOnMessage(pDataChannel, customData, dataChannelOnMessageCallback);
    ATOMIC_INCREMENT((PSIZE_T) customData);
    // Sending first message to the master over the data channel
    retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) VIEWER_DATA_CHANNEL_MESSAGE, STRLEN(VIEWER_DATA_CHANNEL_MESSAGE));
    if (retStatus != STATUS_SUCCESS) {
        DLOGI("[KVS Viewer] dataChannelSend(): operation returned status code: 0x%08x ", retStatus);
    }
}
#endif

VOID onGstVideoFrameReadyViewer(UINT64 customData, PFrame pFrame)
{
    GstFlowReturn ret;
    GstBuffer* buffer;
    GstElement* appsrcVideo = (GstElement*) customData;
    if (!appsrcVideo) {
        DLOGE("Null");
    }
    buffer = gst_buffer_new_allocate(NULL, pFrame->size, NULL);
    if (!buffer) {
        DLOGE("Buffer allocation failed");
        return;
    }

    DLOGI("Frame size: %d, %llu", pFrame->size, videoTimestamp);

    GST_BUFFER_DTS(buffer) = videoTimestamp;
    GST_BUFFER_PTS(buffer) = videoTimestamp;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 25);

    videoTimestamp += 40000000;

    if (gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size) != pFrame->size) {
        DLOGE("Buffer fill did not complete correctly");
        gst_buffer_unref(buffer);
        return;
    }
    g_signal_emit_by_name(appsrcVideo, "push-buffer", buffer, &ret);
    if (ret != GST_FLOW_OK) {
        DLOGE("Error pushing buffer: %s", gst_flow_get_name(ret));
    }
    gst_buffer_unref(buffer);
}

VOID onGstAudioFrameReadyViewer(UINT64 customData, PFrame pFrame)
{
    GstFlowReturn ret;
    GstBuffer* buffer;
    GstElement* appsrcAudio = (GstElement*) customData;
    int sample_rate = 48000; // Hz
    int num_channels = 2;
    int bits_per_sample = 16; // For example, 16-bit audio
    int byte_rate = (sample_rate * num_channels * bits_per_sample) / 8;
    if (!appsrcAudio) {
        DLOGE("Null");
    }
    buffer = gst_buffer_new_allocate(NULL, pFrame->size, NULL);
    if (!buffer) {
        DLOGE("Buffer allocation failed");
        return;
    }

    DLOGI("Frame size: %d, %llu", pFrame->size, videoTimestamp);

    GST_BUFFER_DTS(buffer) = videoTimestamp;
    GST_BUFFER_PTS(buffer) = videoTimestamp;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(pFrame->size, GST_SECOND, byte_rate);

    if (gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size) != pFrame->size) {
        DLOGE("Buffer fill did not complete correctly");
        gst_buffer_unref(buffer);
        return;
    }
    g_signal_emit_by_name(appsrcAudio, "push-buffer", buffer, &ret);
    if (ret != GST_FLOW_OK) {
        DLOGE("Error pushing buffer: %s", gst_flow_get_name(ret));
    }
    gst_buffer_unref(buffer);
}

VOID onSampleStreamingSessionShutdown(UINT64 customData, PSampleStreamingSession pSampleStreamingSession)
{
    (void) (pSampleStreamingSession);
    GstElement* pipeline = (GstElement*) customData;

    gst_element_send_event(pipeline, gst_event_new_eos());
}

PVOID receiveGstreamerAudioVideoFromMaster(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *pipeline = NULL, *appsrcAudio = NULL, *appsrcVideo = NULL;
    GstBus* bus;
    GstMessage* msg;
    GError* error = NULL;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    gchar *videoDescription = "", *audioDescription = "", *audioVideoDescription;
    
    gst_init(NULL, NULL);

    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Viewer] Sample streaming session is NULL");

    switch (pSampleStreamingSession->pVideoRtcRtpTransceiver->receiver.track.codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            videoDescription = "appsrc name=appsrc-video ! capsfilter caps=video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline,width=1920,height=1080 ! queue ! h264parse ! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv";
            break;

        case RTC_CODEC_VP8:
            videoDescription = "appsrc name=appsrc-video ! matroskademux ! vp8dec ! decodebin ! autovideosink";
            break;

        case RTC_CODEC_H265:
            videoDescription = "appsrc name=appsrc-video ! capsfilter caps=video/x-h265,stream-format=byte-stream,alignment=au,profile=main,width=1280,height=720 ! queue ! h265parse ! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv ";
            break;

        default:
            break;
    }

    switch (pSampleStreamingSession->pAudioRtcRtpTransceiver->receiver.track.codec) {
        case RTC_CODEC_OPUS:
            audioDescription = "appsrc name=appsrc-audio ! capsfilter caps=audio/x-opus,rate=48000,channels=2,channel-mapping-family=0 ! queue ! opusparse ! queue ! mux.";
            break;

        case RTC_CODEC_MULAW:
        case RTC_CODEC_ALAW:
            audioDescription = "appsrc name=appsrc-audio ! capsfilter caps=audio/x-opus,rate=48000,channels=2 ! queue ! rawaudioparse ! queue ! mux.";
            break;

        case RTC_CODEC_AAC:
            audioDescription = "appsrc name=appsrc-audio ! capsfilter caps=audio/mpeg,mpegversion=4,stream-format=adts,base-profile=lc,channels=2,rate=48000 ! queue ! aacparse ! mux.";
            break;

        default:
            break;
    }

    audioVideoDescription = g_strjoin(" ", videoDescription, audioDescription, NULL);

    pipeline = gst_parse_launch(audioVideoDescription, &error);
    CHK_ERR(pipeline != NULL, STATUS_INTERNAL_ERROR, "[KVS Viewer] Pipeline is NULL");

    appsrcVideo = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-video");
    CHK_ERR(appsrcVideo != NULL, STATUS_INTERNAL_ERROR, "[KVS Viewer] Cannot find appsrc video");

    appsrcAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-audio");
    CHK_ERR(appsrcAudio != NULL, STATUS_INTERNAL_ERROR, "[KVS Viewer] Cannot find appsrc audio");

    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) appsrcVideo, onGstVideoFrameReadyViewer));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) appsrcAudio, onGstAudioFrameReadyViewer));
    CHK_STATUS(streamingSessionOnShutdown(pSampleStreamingSession, (UINT64) pipeline, onSampleStreamingSessionShutdown));
    g_free(audioVideoDescription);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    CHK_ERR(bus != NULL, STATUS_INTERNAL_ERROR, "[KVS Viewer] Bus is NULL");
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &error, NULL);
                DLOGE("Error received: %s", error->message);
                g_error_free(error);
                break;
            case GST_MESSAGE_EOS:
                DLOGI("End of stream");
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }
    if (bus != NULL) {
        gst_object_unref(bus);
    }
    if (pipeline != NULL) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    if (appsrcAudio != NULL) {
        gst_object_unref(appsrcAudio);
    }
    if (appsrcVideo != NULL) {
        gst_object_unref(appsrcVideo);
    }

CleanUp:
    if (error != NULL) {
        DLOGE("[KVS Viewer] %s", error->message);
        g_clear_error(&error);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    UINT32 buffLen = 0;
    SignalingMessage message;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL locked = FALSE;
    PCHAR pChannelName;
    CHAR clientId[256];

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = argc > 1 ? argv[1] : GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_VIEWER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    pSampleConfiguration->receiveAudioVideoSource = receiveGstreamerAudioVideoFromMaster;

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Viewer] KVS WebRTC initialization completed successfully");

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif

    SPRINTF(clientId, "%s_%u", SAMPLE_VIEWER_CLIENT_ID, RAND() % MAX_UINT32);
    CHK_STATUS(initSignaling(pSampleConfiguration, clientId));
    DLOGI("[KVS Viewer] Signaling client connection established");

    // Initialize streaming session
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;
    CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, NULL, FALSE, &pSampleStreamingSession));
    DLOGI("[KVS Viewer] Creating streaming session...completed");
    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    offerSessionDescriptionInit.useTrickleIce = pSampleStreamingSession->remoteCanTrickleIce;
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    DLOGI("[KVS Viewer] Completed setting local description");

    if (!pSampleConfiguration->trickleIce) {
        DLOGI("[KVS Viewer] Non trickle ice. Wait for Candidate collection to complete");
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = TRUE;

        while (!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->candidateGatheringDone)) {
            CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag), STATUS_OPERATION_TIMED_OUT,
                     "application terminated and candidate gathering still not done");
            CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
        }

        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = FALSE;

        DLOGI("[KVS Viewer] Candidate collection completed");
    }

    CHK_STATUS(createOffer(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    DLOGI("[KVS Viewer] Offer creation successful");

    DLOGI("[KVS Viewer] Generating JSON of session description....");
    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, NULL, &buffLen));

    if (buffLen >= SIZEOF(message.payload)) {
        DLOGE("[KVS Viewer] serializeSessionDescriptionInit(): operation returned status code: 0x%08x ", STATUS_INVALID_OPERATION);
        retStatus = STATUS_INVALID_OPERATION;
        goto CleanUp;
    }

    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, SAMPLE_MASTER_CLIENT_ID);
    message.payloadLen = (buffLen / SIZEOF(CHAR)) - 1;
    message.correlationId[0] = '\0';

    CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message));
#ifdef ENABLE_DATA_CHANNEL
    PRtcDataChannel pDataChannel = NULL;
    PRtcPeerConnection pPeerConnection = pSampleStreamingSession->pPeerConnection;
    SIZE_T datachannelLocalOpenCount = 0;

    // Creating a new datachannel on the peer connection of the existing sample streaming session
    CHK_STATUS(createDataChannel(pPeerConnection, pChannelName, NULL, &pDataChannel));
    DLOGI("[KVS Viewer] Creating data channel...completed");

    // Setting a callback for when the data channel is open
    CHK_STATUS(dataChannelOnOpen(pDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback));
    DLOGI("[KVS Viewer] Data Channel open now...");
#endif

    // Block until interrupted
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted) && !ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag)) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Viewer] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Viewer] Cleaning up....");

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (pSampleConfiguration->enableFileLogging) {
        freeFileLogger();
    }
    if (pSampleConfiguration != NULL) {
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Viewer] freeSignalingClient(): operation returned status code: 0x%08x ", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Viewer] freeSampleConfiguration(): operation returned status code: 0x%08x ", retStatus);
        }
    }
    DLOGI("[KVS Viewer] Cleanup done");

    RESET_INSTRUMENTED_ALLOCATORS();

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}
