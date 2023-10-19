#include "Samples.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

extern PSampleConfiguration gSampleConfiguration;

// #define VERBOSE

GstFlowReturn on_new_sample(GstElement* sink, gpointer data, UINT64 trackid)
{
    GstBuffer* buffer;
    STATUS retStatus = STATUS_SUCCESS;
    BOOL isDroppable, delta;
    GstFlowReturn ret = GST_FLOW_OK;
    GstSample* sample = NULL;
    GstMapInfo info;
    GstSegment* segment;
    GstClockTime buf_pts;
    Frame frame;
    STATUS status;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) data;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PRtcRtpTransceiver pRtcRtpTransceiver = NULL;
    UINT32 i;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "NULL sample configuration");

    info.data = NULL;
    sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

    buffer = gst_sample_get_buffer(sample);
    isDroppable = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED) || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY) ||
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
            DLOGE("[KVS GStreamer Master] Frame contains invalid PTS dropping the frame");
        }

        if (!(gst_buffer_map(buffer, &info, GST_MAP_READ))) {
            DLOGE("[KVS GStreamer Master] on_new_sample(): Gst buffer mapping failed");
            goto CleanUp;
        }

        frame.trackId = trackid;
        frame.duration = 0;
        frame.version = FRAME_CURRENT_VERSION;
        frame.size = (UINT32) info.size;
        frame.frameData = (PBYTE) info.data;

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];
            frame.index = (UINT32) ATOMIC_INCREMENT(&pSampleStreamingSession->frameIndex);

            if (trackid == DEFAULT_AUDIO_TRACK_ID) {
                pRtcRtpTransceiver = pSampleStreamingSession->pAudioRtcRtpTransceiver;
                frame.presentationTs = pSampleStreamingSession->audioTimestamp;
                frame.decodingTs = frame.presentationTs;
                pSampleStreamingSession->audioTimestamp +=
                    SAMPLE_AUDIO_FRAME_DURATION; // assume audio frame size is 20ms, which is default in opusenc
            } else {
                pRtcRtpTransceiver = pSampleStreamingSession->pVideoRtcRtpTransceiver;
                frame.presentationTs = pSampleStreamingSession->videoTimestamp;
                frame.decodingTs = frame.presentationTs;
                pSampleStreamingSession->videoTimestamp += SAMPLE_VIDEO_FRAME_DURATION; // assume video fps is 25
            }
            status = writeFrame(pRtcRtpTransceiver, &frame);
            if (status != STATUS_SRTP_NOT_READY_YET && status != STATUS_SUCCESS) {
#ifdef VERBOSE
                DLOGE("writeFrame() failed with 0x%08x", status);
#endif
            } else if (status == STATUS_SUCCESS && pSampleStreamingSession->firstFrame) {
                PROFILE_WITH_START_TIME(pSampleStreamingSession->offerReceiveTime, "Time to first frame");
                pSampleStreamingSession->firstFrame = FALSE;
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
    }

CleanUp:

    if (info.data != NULL) {
        gst_buffer_unmap(buffer, &info);
    }

    if (sample != NULL) {
        gst_sample_unref(sample);
    }

    if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        ret = GST_FLOW_EOS;
    }

    return ret;
}

GstFlowReturn on_new_sample_video(GstElement* sink, gpointer data)
{
    return on_new_sample(sink, data, DEFAULT_VIDEO_TRACK_ID);
}

GstFlowReturn on_new_sample_audio(GstElement* sink, gpointer data)
{
    return on_new_sample(sink, data, DEFAULT_AUDIO_TRACK_ID);
}

PVOID sendGstreamerAudioVideo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *appsinkVideo = NULL, *appsinkAudio = NULL, *pipeline = NULL;
    GstBus* bus;
    GstMessage* msg;
    GError* error = NULL;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Gstreamer Master] Streaming session is NULL");

    /**
     * Use x264enc as its available on mac, pi, ubuntu and windows
     * mac pipeline fails if resolution is not 720p
     *
     * For alaw
     * audiotestsrc is-live=TRUE ! queue leaky=2 max-size-buffers=400 ! audioconvert ! audioresample !
     * audio/x-raw, rate=8000, channels=1, format=S16LE, layout=interleaved ! alawenc ! appsink sync=TRUE emit-signals=TRUE name=appsink-audio
     *
     * For VP8
     * videotestsrc is-live=TRUE ! video/x-raw,width=1280,height=720,framerate=30/1 !
     * vp8enc error-resilient=partitions keyframe-max-dist=10 auto-alt-ref=true cpu-used=5 deadline=1 !
     * appsink sync=TRUE emit-signals=TRUE name=appsink-video
     *
     * Raspberry Pi Hardware Encode Example
     * "v4l2src device=\"/dev/video0\" ! queue ! v4l2convert ! "
     * "video/x-raw,format=I420,width=640,height=480,framerate=30/1 ! "
     * "v4l2h264enc ! "
     * "h264parse ! "
     * "video/x-h264,stream-format=byte-stream,alignment=au,width=640,height=480,framerate=30/1,profile=baseline,level=(string)4 ! "
     * "appsink sync=TRUE emit-signals=TRUE name=appsink-video"
     */

    CHAR rtspPipeLineBuffer[RTSP_PIPELINE_MAX_CHAR_COUNT];

    switch (pSampleConfiguration->mediaType) {
        case SAMPLE_STREAMING_VIDEO_ONLY:
            switch (pSampleConfiguration->srcType) {
                case TEST_SOURCE: {
                    pipeline =
                        gst_parse_launch("videotestsrc is-live=TRUE ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=25/1 ! "
                                         "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                         "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE emit-signals=TRUE "
                                         "name=appsink-video",
                                         &error);
                    break;
                }
                case DEVICE_SOURCE: {
                    pipeline = gst_parse_launch("autovideosrc ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=25/1 ! "
                                                "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                                "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE "
                                                "emit-signals=TRUE name=appsink-video",
                                                &error);
                    break;
                }
                case RTSP_SOURCE: {
                    UINT16 stringOutcome = snprintf(rtspPipeLineBuffer, RTSP_PIPELINE_MAX_CHAR_COUNT,
                                                    "uridecodebin uri=%s ! "
                                                    "videoconvert ! "
                                                    "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                                    "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! queue ! "
                                                    "appsink sync=TRUE emit-signals=TRUE name=appsink-video ",
                                                    pSampleConfiguration->rtspUri);

                    if (stringOutcome > RTSP_PIPELINE_MAX_CHAR_COUNT) {
                        printf("[KVS GStreamer Master] ERROR: rtsp uri entered exceeds maximum allowed length set by RTSP_PIPELINE_MAX_CHAR_COUNT\n");
                        goto CleanUp;
                    }
                    pipeline = gst_parse_launch(rtspPipeLineBuffer, &error);

                    break;
                }
            }
            break;

        case SAMPLE_STREAMING_AUDIO_VIDEO:
            switch (pSampleConfiguration->srcType) {
                case TEST_SOURCE: {
                    pipeline =
                        gst_parse_launch("videotestsrc is-live=TRUE ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=25/1 ! "
                                         "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                         "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE "
                                         "emit-signals=TRUE name=appsink-video audiotestsrc is-live=TRUE ! "
                                         "queue leaky=2 max-size-buffers=400 ! audioconvert ! audioresample ! opusenc ! "
                                         "audio/x-opus,rate=48000,channels=2 ! appsink sync=TRUE emit-signals=TRUE name=appsink-audio",
                                         &error);
                    break;
                }
                case DEVICE_SOURCE: {
                    pipeline =
                        gst_parse_launch("autovideosrc ! queue ! videoconvert ! video/x-raw,width=1280,height=720,framerate=25/1 ! "
                                         "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                         "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! appsink sync=TRUE emit-signals=TRUE "
                                         "name=appsink-video autoaudiosrc ! "
                                         "queue leaky=2 max-size-buffers=400 ! audioconvert ! audioresample ! opusenc ! "
                                         "audio/x-opus,rate=48000,channels=2 ! appsink sync=TRUE emit-signals=TRUE name=appsink-audio",
                                         &error);
                    break;
                }
                case RTSP_SOURCE: {
                    UINT16 stringOutcome = snprintf(rtspPipeLineBuffer, RTSP_PIPELINE_MAX_CHAR_COUNT,
                                                    "uridecodebin uri=%s name=src ! videoconvert ! "
                                                    "x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency ! "
                                                    "video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! queue ! "
                                                    "appsink sync=TRUE emit-signals=TRUE name=appsink-video "
                                                    "src. ! audioconvert ! "
                                                    "audioresample ! opusenc ! audio/x-opus,rate=48000,channels=2 ! queue ! "
                                                    "appsink sync=TRUE emit-signals=TRUE name=appsink-audio",
                                                    pSampleConfiguration->rtspUri);

                    if (stringOutcome > RTSP_PIPELINE_MAX_CHAR_COUNT) {
                        printf("[KVS GStreamer Master] ERROR: rtsp uri entered exceeds maximum allowed length set by RTSP_PIPELINE_MAX_CHAR_COUNT\n");
                        goto CleanUp;
                    }
                    pipeline = gst_parse_launch(rtspPipeLineBuffer, &error);

                    break;
                }
            }
            break;
    }

    CHK_ERR(pipeline != NULL, STATUS_NULL_ARG, "[KVS Gstreamer Master] Pipeline is NULL");

    appsinkVideo = gst_bin_get_by_name(GST_BIN(pipeline), "appsink-video");
    appsinkAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsink-audio");

    if (!(appsinkVideo != NULL || appsinkAudio != NULL)) {
        printf("[KVS GStreamer Master] sendGstreamerAudioVideo(): cant find appsink, operation returned status code: 0x%08x \n",
               STATUS_INTERNAL_ERROR);
        goto CleanUp;
    }

    if (appsinkVideo != NULL) {
        g_signal_connect(appsinkVideo, "new-sample", G_CALLBACK(on_new_sample_video), (gpointer) pSampleConfiguration);
    }
    if (appsinkAudio != NULL) {
        g_signal_connect(appsinkAudio, "new-sample", G_CALLBACK(on_new_sample_audio), (gpointer) pSampleConfiguration);
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
        gst_message_unref(msg);
    }
    if (bus != NULL) {
        gst_object_unref(bus);
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    if (pipeline != NULL) {
        gst_object_unref(pipeline);
    }
    if (appsinkAudio != NULL) {
        gst_object_unref(appsinkAudio);
    }
    if (appsinkVideo != NULL) {
        gst_object_unref(appsinkVideo);
    }

CleanUp:

    if (error != NULL) {
        DLOGE("%s", error->message);
        g_clear_error(&error);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

VOID onGstAudioFrameReady(UINT64 customData, PFrame pFrame)
{
    GstFlowReturn ret;
    GstBuffer* buffer;
    GstElement* appsrcAudio = (GstElement*) customData;

    /* Create a new empty buffer */
    buffer = gst_buffer_new_and_alloc(pFrame->size);
    gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size);

    /* Push the buffer into the appsrc */
    g_signal_emit_by_name(appsrcAudio, "push-buffer", buffer, &ret);

    /* Free the buffer now that we are done with it */
    gst_buffer_unref(buffer);
}

VOID onSampleStreamingSessionShutdown(UINT64 customData, PSampleStreamingSession pSampleStreamingSession)
{
    (void) (pSampleStreamingSession);
    GstElement* appsrc = (GstElement*) customData;
    GstFlowReturn ret;

    g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
}

PVOID receiveGstreamerAudioVideo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *pipeline = NULL, *appsrcAudio = NULL;
    GstBus* bus;
    GstMessage* msg;
    GError* error = NULL;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    gchar *videoDescription = "", *audioDescription = "", *audioVideoDescription;

    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Gstreamer Master] Sample streaming session is NULL");

    // TODO: For video
    switch (pSampleStreamingSession->pAudioRtcRtpTransceiver->receiver.track.codec) {
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

    appsrcAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-audio");
    CHK_ERR(appsrcAudio != NULL, STATUS_INTERNAL_ERROR, "[KVS Gstreamer Master] Cannot find appsrc");

    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) appsrcAudio, onGstAudioFrameReady));

    CHK_STATUS(streamingSessionOnShutdown(pSampleStreamingSession, (UINT64) appsrcAudio, onSampleStreamingSessionShutdown));
    g_free(audioVideoDescription);

    CHK_ERR(pipeline != NULL, STATUS_INTERNAL_ERROR, "[KVS Gstreamer Master] Pipeline is NULL");

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
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

CleanUp:
    if (error != NULL) {
        DLOGE("%s", error->message);
        g_clear_error(&error);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

    signal(SIGINT, sigintHandler);

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = getenv(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    pSampleConfiguration->videoSource = sendGstreamerAudioVideo;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    pSampleConfiguration->receiveAudioVideoSource = receiveGstreamerAudioVideo;

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif
    pSampleConfiguration->customData = (UINT64) pSampleConfiguration;
    pSampleConfiguration->srcType = DEVICE_SOURCE; // Default to device source (autovideosrc and autoaudiosrc)
    /* Initialize GStreamer */
    gst_init(&argc, &argv);
    DLOGI("[KVS Gstreamer Master] Finished initializing GStreamer and handlers");

    if (argc > 2) {
        if (STRCMP(argv[2], "video-only") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
            DLOGI("[KVS Gstreamer Master] Streaming video only");
        } else if (STRCMP(argv[2], "audio-video-storage") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
            pSampleConfiguration->channelInfo.useMediaStorage = TRUE;
            DLOGI("[KVS Gstreamer Master] Streaming audio and video");
        } else if (STRCMP(argv[2], "audio-video") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
            DLOGI("[KVS Gstreamer Master] Streaming audio and video");
        } else {
            DLOGI("[KVS Gstreamer Master] Unrecognized streaming type. Default to video-only");
        }
    } else {
        DLOGI("[KVS Gstreamer Master] Streaming video only");
    }

    if (argc > 3) {
        if (STRCMP(argv[3], "testsrc") == 0) {
            DLOGI("[KVS GStreamer Master] Using test source in GStreamer");
            pSampleConfiguration->srcType = TEST_SOURCE;
        } else if (STRCMP(argv[3], "devicesrc") == 0) {
            DLOGI("[KVS GStreamer Master] Using device source in GStreamer");
            pSampleConfiguration->srcType = DEVICE_SOURCE;
        } else if (STRCMP(argv[3], "rtspsrc") == 0) {
            DLOGI("[KVS GStreamer Master] Using RTSP source in GStreamer");
            if (argc < 5) {
                printf("[KVS GStreamer Master] No RTSP source URI included. Defaulting to device source");
                printf("[KVS GStreamer Master] Usage: ./kvsWebrtcClientMasterGstSample <channel name> audio-video rtspsrc rtsp://<rtsp uri>\n"
                       "or ./kvsWebrtcClientMasterGstSample <channel name> video-only rtspsrc <rtsp://<rtsp uri>");
                pSampleConfiguration->srcType = DEVICE_SOURCE;
            } else {
                pSampleConfiguration->srcType = RTSP_SOURCE;
                pSampleConfiguration->rtspUri = argv[4];
            }
        } else {
            DLOGI("[KVS Gstreamer Master] Unrecognized source type. Defaulting to device source in GStreamer");
        }
    } else {
        printf("[KVS GStreamer Master] Using device source in GStreamer\n");
    }

    switch (pSampleConfiguration->mediaType) {
        case SAMPLE_STREAMING_VIDEO_ONLY:
            DLOGI("[KVS GStreamer Master] streaming type video-only");
            break;
        case SAMPLE_STREAMING_AUDIO_VIDEO:
            DLOGI("[KVS GStreamer Master] streaming type audio-video");
            break;
    }

    // Initalize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS GStreamer Master] KVS WebRTC initialization completed successfully");

    CHK_STATUS(initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID));
    DLOGI("[KVS GStreamer Master] Channel %s set up done ", pChannelName);

    // Checking for termination
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[KVS GStreamer Master] Streaming session terminated");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS GStreamer Master] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS GStreamer Master] Cleaning up....");

    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
        }

        if (pSampleConfiguration->enableFileLogging) {
            freeFileLogger();
        }
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS GStreamer Master] freeSignalingClient(): operation returned status code: 0x%08x", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS GStreamer Master] freeSampleConfiguration(): operation returned status code: 0x%08x", retStatus);
        }
    }
    DLOGI("[KVS Gstreamer Master] Cleanup done");

    RESET_INSTRUMENTED_ALLOCATORS();

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}
