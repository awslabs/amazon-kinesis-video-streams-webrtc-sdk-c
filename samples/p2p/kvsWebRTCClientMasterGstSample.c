#include "../common/Samples.h"
#include "../common/GstMedia.h"

extern PSampleConfiguration gSampleConfiguration;

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    RTC_CODEC audioCodec = RTC_CODEC_OPUS;
    RTC_CODEC videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

    signal(SIGINT, sigintHandler);

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = argc > 1 ? argv[1] : GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    if (argc > 3 && STRCMP(argv[3], "testsrc") == 0) {
        if (argc > 4) {
            if (!STRCMP(argv[4], AUDIO_CODEC_NAME_OPUS)) {
                audioCodec = RTC_CODEC_OPUS;
            }
        }

        if (argc > 5) {
            if (!STRCMP(argv[5], VIDEO_CODEC_NAME_H265)) {
                videoCodec = RTC_CODEC_H265;
            }
        }
    }

    pSampleConfiguration->videoSource = sendGstreamerAudioVideo;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    pSampleConfiguration->audioCodec = audioCodec;
    pSampleConfiguration->videoCodec = videoCodec;

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
                DLOGI("[KVS GStreamer Master] No RTSP source URI included. Defaulting to device source");
                DLOGI("[KVS GStreamer Master] Usage: ./kvsWebrtcClientMasterGstSample <channel name> audio-video rtspsrc rtsp://<rtsp uri>"
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
        DLOGI("[KVS GStreamer Master] Using device source in GStreamer");
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
