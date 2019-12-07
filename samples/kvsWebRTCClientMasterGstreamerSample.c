#include "Samples.h"
#include "gstSample.h"

GstFlowReturn on_new_sample(GstElement *sink, gpointer data, UINT64 trackid)
{
    GstBuffer *buffer;
    BOOL isDroppable, delta;
    GstFlowReturn ret = GST_FLOW_OK;
    GstSample *sample = NULL;
    GstMapInfo info;
    GstSegment *segment;
    GstClockTime buf_pts;
    Frame frame;
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) data;
    PRtcRtpTransceiver pRtcRtpTransceiver;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    info.data = NULL;
    sample = gst_app_sink_pull_sample(GST_APP_SINK (sink));

    buffer = gst_sample_get_buffer(sample);
    isDroppable = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED) ||
                  GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY) ||
                  (GST_BUFFER_FLAGS(buffer) == GST_BUFFER_FLAG_DISCONT) ||
                  (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT) && GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)) ||
                  // drop if buffer contains header only and has invalid timestamp
                  !GST_BUFFER_PTS_IS_VALID(buffer);

    if (!isDroppable) {
        delta = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        frame.flags = delta ? FRAME_FLAG_NONE : FRAME_FLAG_KEY_FRAME;

        // convert from segment timestamp to running time in live mode.
        segment = gst_sample_get_segment(sample);
        buf_pts = gst_segment_to_running_time(segment, GST_FORMAT_TIME, buffer->pts);
        if (!GST_CLOCK_TIME_IS_VALID(buf_pts)) {
            DLOGD("Frame contains invalid PTS dropping the frame.");
            CHK(FALSE, retStatus);
        }

        CHK(gst_buffer_map(buffer, &info, GST_MAP_READ), retStatus);

        frame.trackId = trackid;
        frame.size = (UINT32) info.size;
        frame.duration = 0;
        if (trackid == DEFAULT_AUDIO_TRACK_ID) {
            pRtcRtpTransceiver = pSampleConfiguration->pAudioRtcRtpTransceiver;
            frame.presentationTs = pSampleConfiguration->audioTimestamp;
            frame.decodingTs = frame.presentationTs;
            pSampleConfiguration->audioTimestamp += SAMPLE_AUDIO_FRAME_DURATION; // assume audio frame size is 20ms, which is default in opusenc

        } else {
            pRtcRtpTransceiver = pSampleConfiguration->pVideoRtcRtpTransceiver;
            frame.presentationTs = pSampleConfiguration->videoTimestamp;
            frame.decodingTs = frame.presentationTs;
            pSampleConfiguration->videoTimestamp += SAMPLE_VIDEO_FRAME_DURATION; // assume video fps is 30
        }
        frame.version = FRAME_CURRENT_VERSION;
        frame.frameData = (PBYTE) info.data;
        frame.index = (UINT32) ATOMIC_INCREMENT(&pSampleConfiguration->frameIndex);

        retStatus = writeFrame(pRtcRtpTransceiver, &frame);
        if (STATUS_FAILED(retStatus)) {
            DLOGW("writeFrame failed with 0x%08x", retStatus);
        }
    }

CleanUp:

    if (info.data != NULL) {
        gst_buffer_unmap(buffer, &info);
    }

    if (sample != NULL) {
        gst_sample_unref(sample);
    }

    if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag)) {
        ret = GST_FLOW_EOS;
    }

    return ret;
}

GstFlowReturn on_new_sample_video(GstElement *sink, gpointer data) {
    return on_new_sample(sink, data, DEFAULT_VIDEO_TRACK_ID);
}

GstFlowReturn on_new_sample_audio(GstElement *sink, gpointer data) {
    return on_new_sample(sink, data, DEFAULT_AUDIO_TRACK_ID);
}

PVOID sendGstreamerAudioVideo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *appsinkVideo = NULL, *appsinkAudio = NULL, *pipeline = NULL;
    GstBus *bus;
    GstMessage *msg;
    GError *error = NULL;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Use x264enc as its available on mac, pi, ubuntu and windows
    // mac pipeline fails if resolution is not 720p
    //
    // For alaw
    // audiotestsrc ! audio/x-raw, rate=8000, channels=1, format=S16LE, layout=interleaved ! alawenc ! appsink sync=TRUE emit-signals=TRUE name=appsink-audio
    //
    // For VP8
    // videotestsrc ! video/x-raw,width=1280,height=720,framerate=30/1 ! vp8enc error-resilient=partitions keyframe-max-dist=10 auto-alt-ref=true cpu-used=5 deadline=1 ! appsink sync=TRUE emit-signals=TRUE name=appsink-video

    switch (pSampleConfiguration->mediaType) {
        case SAMPLE_STREAMING_VIDEO_ONLY:
            pipeline = gst_parse_launch(
                    "autovideosrc num-buffers=1000 ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=30/1 ! x264enc bframes=0 speed-preset=veryfast key-int-max=30 bitrate=512 ! "
                    "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE emit-signals=TRUE name=appsink-video",
                    &error);
            break;

        case SAMPLE_STREAMING_AUDIO_VIDEO:
            pipeline = gst_parse_launch(
                    "autovideosrc num-buffers=1000 ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=30/1 ! x264enc bframes=0 speed-preset=veryfast key-int-max=30 bitrate=512 ! "
                            "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE emit-signals=TRUE name=appsink-video autoaudiosrc num-buffers=1000 ! "
                            "queue leaky=2 max-size-buffers=400 ! audioconvert ! audioresample ! opusenc ! audio/x-opus,rate=48000,channels=2 ! appsink sync=TRUE emit-signals=TRUE name=appsink-audio",
                    &error);
            break;
    }

    CHK_ERR(pipeline != NULL, STATUS_INTERNAL_ERROR, "Failed to launch gstreamer");

    appsinkVideo = gst_bin_get_by_name(GST_BIN(pipeline), "appsink-video");
    appsinkAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsink-audio");
    CHK_ERR(appsinkVideo != NULL || appsinkAudio != NULL, STATUS_INTERNAL_ERROR, "cant find appsink");

    if (appsinkVideo != NULL) {
        g_signal_connect(appsinkVideo, "new-sample", G_CALLBACK(on_new_sample_video), (gpointer) pSampleConfiguration);
    }

    if (appsinkAudio != NULL) {
        g_signal_connect(appsinkAudio, "new-sample", G_CALLBACK(on_new_sample_audio), (gpointer) pSampleConfiguration);
    }

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
        gst_message_unref(msg);
    }
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);

CleanUp:

    if (error != NULL) {
        DLOGE("%s", error->message);
        g_clear_error (&error);
    }

    CHK_LOG_ERR_NV(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

VOID onGstAudioFrameReady(UINT64 customData, PFrame pFrame) {
    GstFlowReturn ret;
    GstBuffer *buffer;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    PGstAppSrcs pGstAppSrcs = (PGstAppSrcs) pSampleConfiguration->customData;

    if (pGstAppSrcs->pGstAudioAppSrc != NULL) {
        /* Create a new empty buffer */
        buffer = gst_buffer_new_and_alloc(pFrame->size);
        gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size);

        /* Push the buffer into the appsrc */
        g_signal_emit_by_name(pGstAppSrcs->pGstAudioAppSrc, "push-buffer", buffer, &ret);

        /* Free the buffer now that we are done with it */
        gst_buffer_unref(buffer);
    }
}

PVOID receiveGstreamerAudioVideo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *appsrcVideo = NULL, *appsrcAudio = NULL, *pipeline = NULL;
    GstBus *bus;
    GstMessage *msg;
    GError *error = NULL;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    PGstAppSrcs pGstAppSrcs = (PGstAppSrcs) pSampleConfiguration->customData;
    gchar *videoDescription = "", *audioDescription = "", *audioVideoDescription;

    CHK(pSampleConfiguration != NULL && pGstAppSrcs != NULL, STATUS_NULL_ARG);

    pGstAppSrcs->pGstAudioAppSrc = NULL;
    //TODO: Wire video up with gstream pipeline

    switch (pSampleConfiguration->pAudioRtcRtpTransceiver->receiver.track.codec) {
        case RTC_CODEC_OPUS:
            audioDescription = "appsrc name=appsrc-audio ! opusparse ! decodebin ! autoaudiosink";
            break;

        case RTC_CODEC_MULAW:
        case RTC_CODEC_ALAW:
            audioDescription = "appsrc name=appsrc-audio ! rawaudioparse ! decodebin ! autoaudiosink";
            break;
        default:
            break;
    }

    audioVideoDescription = g_strjoin(" ", audioDescription, videoDescription, NULL);

    pipeline = gst_parse_launch(audioVideoDescription, &error);

    g_free(audioVideoDescription);

    CHK_ERR(pipeline != NULL, STATUS_INTERNAL_ERROR, "Failed to launch gstreamer");

    appsrcVideo = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-video");

    appsrcAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-audio");

    CHK_ERR(appsrcVideo != NULL || appsrcAudio != NULL, STATUS_INTERNAL_ERROR, "cant find appssrc");

    pGstAppSrcs->pGstVideoAppSrc = appsrcVideo;
    pGstAppSrcs->pGstAudioAppSrc = appsrcAudio;

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
        gst_message_unref(msg);
    }
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);

CleanUp:
    if (error != NULL) {
        DLOGE("%s", error->message);
        g_clear_error (&error);
    }

    CHK_LOG_ERR_NV(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS createSampleGstAppSrcs(PGstAppSrcs* ppGstAppSrc)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstAppSrcs pGstAppSrcs = NULL;
    CHK(ppGstAppSrc != NULL, STATUS_NULL_ARG);
    CHK(NULL != (pGstAppSrcs = (PGstAppSrcs) MEMCALLOC(1, SIZEOF(GstAppSrcs))), STATUS_NOT_ENOUGH_MEMORY);
CleanUp:
    if (ppGstAppSrc != NULL) {
        *ppGstAppSrc = pGstAppSrcs;
    }
    return retStatus;
}

INT32 main(INT32 argc, CHAR *argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSampleConfiguration pSampleConfiguration = NULL;
    PGstAppSrcs pGstAppSrcs = NULL;

    // do trickle-ice by default
    CHK_STATUS(createSampleConfiguration(argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME,
            SIGNALING_CHANNEL_ROLE_TYPE_MASTER,
            TRUE,
            TRUE,
            &pSampleConfiguration));

    CHK_STATUS(createSampleGstAppSrcs(&pGstAppSrcs));

    pSampleConfiguration->videoSource = sendGstreamerAudioVideo;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    pSampleConfiguration->receiveAudioVideoSource = receiveGstreamerAudioVideo;
    pSampleConfiguration->customData = (UINT64) pGstAppSrcs;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    if (argc > 2) {
        if (STRCMP(argv[2], "video-only") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
        } else if (STRCMP(argv[2], "audio-video") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
        } else {
            DLOGD("Unrecognized streaming type. Default to video-only");
        }
    }

    switch (pSampleConfiguration->mediaType) {
        case SAMPLE_STREAMING_VIDEO_ONLY:
            DLOGD("streaming type video-only");
            break;
        case SAMPLE_STREAMING_AUDIO_VIDEO:
            DLOGD("streaming type audio-video");
            break;
    }

    // Initalize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.messageReceivedFn = masterMessageReceived;
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.customData = (UINT64) pSampleConfiguration;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

    CHK_STATUS(createSignalingClientSync(&clientInfo,
            &pSampleConfiguration->channelInfo,
            &signalingClientCallbacks,
            pSampleConfiguration->pCredentialProvider,
            &pSampleConfiguration->signalingClientHandle));

    // Initialize the peer connection
    CHK_STATUS(initializePeerConnection(pSampleConfiguration));

    transceiverOnFrame(pSampleConfiguration->pAudioRtcRtpTransceiver, (UINT64) pSampleConfiguration,
            onGstAudioFrameReady);

    // Enable the processing of the messages
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    // Block forever
    THREAD_SLEEP(MAX_UINT64);

    // Kick of the termination sequence
    ATOMIC_STORE_BOOL(&pSampleConfiguration->terminateFlag, TRUE);

    // Join the threads
    THREAD_JOIN(pSampleConfiguration->replyTid, NULL);
    THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (pSampleConfiguration != NULL) {
        CHK_LOG_ERR_NV(freePeerConnection(&pSampleConfiguration->pPeerConnection));
        CHK_LOG_ERR_NV(freeSignalingClient(&pSampleConfiguration->signalingClientHandle));
        CHK_LOG_ERR_NV(freeSampleConfiguration(&pSampleConfiguration));
    }

    if (pGstAppSrcs != NULL) {
        SAFE_MEMFREE(pGstAppSrcs);
    }

    return (INT32) retStatus;
}
