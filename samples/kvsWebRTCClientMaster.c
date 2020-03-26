#include <getopt.h>
#include "Samples.h"

extern PSampleConfiguration gSampleConfiguration;


static PCHAR gChannelName = SAMPLE_CHANNEL_NAME;
static BOOL gUseTrickleIce = TRUE;
static BOOL gUseTurn = TRUE;
static UINT32 gDebugLevel = LOG_LEVEL_INFO;


// #define VERBOSE

static void usage()
{
    printf("Run a sample code.\n\n");
    printf("  -h, --help\t\tPrint this message.\n");
    printf("  -c, --channel-name\tCreate channel. Default: %s\n", SAMPLE_CHANNEL_NAME);
    printf("  -i, --ice\t\tEnable trickle ice or not. Default: enable. enable or disable\n");
    printf("  -t, --turn\t\tEnable turn or not. Default: enable. enable or disable\n");
    printf("  -d, --debug-level\tSetup debug level.\n");
    printf("  -v, --version\t\tPrint version information and exit.\n");
    printf("\n");
}

static void parse_opts(int argc, char **argv)
{
    static struct option longopts[] = {
        {"help", no_argument, NULL, 'h'},
        {"channel-name", required_argument, NULL, 'c'},
        {"ice", required_argument, NULL, 't'},
        {"turn", required_argument, NULL, 'r'},
        {"debug-level", required_argument, NULL, 'd'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };
    int c;

    const char* opts_spec = "hc:i:t:d:v";

    while (1) {
        c = getopt_long(argc, argv, opts_spec, longopts, (int *) 0);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage();
            exit(0);
        case 'c':
            gChannelName = optarg;
            printf("%s\n", gChannelName);
            break;
        case 'i':
            if(STRCMP(optarg, "enable")==0){
                printf("enable the functionality of trickle ice\n");
                gUseTrickleIce = TRUE; 
            }else if (STRCMP(optarg, "disable")==0){
                printf("disable the functionality of trickle ice\n");
                gUseTrickleIce = FALSE;
            }else{
                printf("invalid parameters\n");
            }
            break;
        case 't':
            if(STRCMP(optarg, "enable")==0){
                printf("enable the functionality of turn\n");
                gUseTurn = TRUE; 
            }else if (STRCMP(optarg, "disable")==0){
                printf("disable the functionality of turn\n");
                gUseTurn = FALSE;
            }else{
                printf("invalid parameters\n");
            }
            break;
        case 'd':
            gDebugLevel = STRTOUL (optarg, NULL, 0);
            if(gDebugLevel>=1 && gDebugLevel<=7 && setenv("AWS_KVS_LOG_LEVEL", optarg, 1)==0){
                printf("set the debug level as %d\n", gDebugLevel);
            }else{
                printf("invalid debug level value: %d\n", gDebugLevel);
                gDebugLevel = LOG_LEVEL_INFO;
            }
            break;
        case 'v':
            printf("master version: %d.%d\n", SAMPLE_VERSION_MAJOR, SAMPLE_VERSION_MINOR);
            exit(0);
        default:
            usage();
            exit(2);
        }
    }
}


INT32 main(INT32 argc, CHAR *argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    PSampleConfiguration pSampleConfiguration = NULL;
    parse_opts(argc, argv);
    signal(SIGINT, sigintHandler);

    // do tricketIce by default
    printf("[KVS Master] %s trickle ice\n", (gUseTrickleIce==TRUE)?"Enable":"Disable");
    printf("[KVS Master] %s turn\n", (gUseTurn==TRUE)?"Enable":"Disable");

    retStatus = createSampleConfiguration(gChannelName,
                                          SIGNALING_CHANNEL_ROLE_TYPE_MASTER,
                                          gUseTrickleIce,
                                          gUseTurn,
                                          &pSampleConfiguration);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] createSampleConfiguration(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Master] Created signaling channel %s\n", gChannelName);

    // Set the audio and video handlers
    pSampleConfiguration->audioSource = sendAudioPackets;
    pSampleConfiguration->videoSource = sendVideoPackets;
    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveAudioFrame;
    pSampleConfiguration->onDataChannel = onDataChannel;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    printf("[KVS Master] Finished setting audio and video handlers\n");

    // Check if the samples are present

    retStatus = readFrameFromDisk(NULL, &frameSize, "./h264SampleFrames/frame-001.h264");
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Checked sample video frame availability....available\n");

    retStatus = readFrameFromDisk(NULL, &frameSize, "./opusSampleFrames/sample-001.opus");
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Checked sample audio frame availability....available\n");

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    retStatus = initKvsWebRtc();
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] initKvsWebRtc(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] KVS WebRTC initialization completed successfully\n");

    pSampleConfiguration->signalingClientCallbacks.messageReceivedFn = masterMessageReceived;

    strcpy(pSampleConfiguration->clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

    retStatus = createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                          &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                          &pSampleConfiguration->signalingClientHandle);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] createSignalingClientSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;

    }
    printf("[KVS Master] Signaling client created successfully\n");

    // Enable the processing of the messages
    retStatus = signalingClientConnectSync(pSampleConfiguration->signalingClientHandle);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] signalingClientConnectSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Signaling client connection to socket established\n");

    gSampleConfiguration = pSampleConfiguration;

    printf("[KVS Master] Beginning audio-video streaming...check the stream over channel %s\n",
            gChannelName);

    // Checking for termination
    retStatus = sessionCleanupWait(pSampleConfiguration);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] sessionCleanupWait(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Master] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    printf("[KVS Master] Cleaning up....\n");

    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        // Join the threads
        if (pSampleConfiguration->videoSenderTid != (UINT64) NULL) {
           // Join the threads
           THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
        }

        if (pSampleConfiguration->audioSenderTid != (UINT64) NULL) {
            // Join the threads
            THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
        }

        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSignalingClient(): operation returned status code: 0x%08x", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSampleConfiguration(): operation returned status code: 0x%08x", retStatus);
        }
    }
    printf("[KVS Master] Cleanup done\n");
    return (INT32) retStatus;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;

    if(pSize == NULL) {
       printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
       goto CleanUp;
    }

    size = *pSize;

    // Get the size and read into frame
    retStatus = readFile(frameFilePath, TRUE, pFrame, &size);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFile(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

PVOID sendVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;

    if(pSampleConfiguration == NULL) {
        printf("[KVS Master] sendVideoPackets(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        snprintf(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%03d.h264", fileIndex);

        retStatus = readFrameFromDisk(NULL, &frameSize, filePath);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) realloc(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            if(pSampleConfiguration->pVideoFrameBuffer == NULL)
            {
                printf("[KVS Master] Video frame Buffer reallocation failed...%s (code %d)\n", strerror(errno), errno);
                printf("[KVS Master] realloc(): operation returned status code: 0x%08x \n", STATUS_NOT_ENOUGH_MEMORY);
                goto CleanUp;
            }

            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;

        retStatus = readFrameFromDisk(frame.frameData, &frameSize, filePath);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        if (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList)) {
            ATOMIC_INCREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
            for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
                if (status != STATUS_SUCCESS) {
                    #ifdef VERBOSE
                        printf("writeFrame() failed with 0x%08x", status);
                    #endif
                }
            }
            ATOMIC_DECREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
        }
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION);
    }

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 i;
    STATUS status;

    if(pSampleConfiguration == NULL) {
        printf("[KVS Master] sendAudioPackets(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;
        snprintf(filePath, MAX_PATH_LEN, "./opusSampleFrames/sample-%03d.opus", fileIndex);

        retStatus = readFrameFromDisk(NULL, &frameSize, filePath);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->audioBufferSize) {
            pSampleConfiguration->pAudioFrameBuffer = (uint8_t*) realloc(pSampleConfiguration->pAudioFrameBuffer, frameSize);
            if(pSampleConfiguration->pAudioFrameBuffer == NULL) {
                printf("[KVS Master] Audio frame Buffer reallocation failed...%s (code %d)\n", strerror(errno), errno);
                printf("[KVS Master] realloc(): operation returned status code: 0x%08x \n", STATUS_NOT_ENOUGH_MEMORY);
                goto CleanUp;
            }
        }

        frame.frameData = pSampleConfiguration->pAudioFrameBuffer;
        frame.size = frameSize;

        retStatus = readFrameFromDisk(frame.frameData, &frameSize, filePath);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;

        if (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList)) {
            ATOMIC_INCREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
            for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
                if (status != STATUS_SUCCESS) {
                    printf("writeFrame failed with 0x%08x", status);
                }
            }
            ATOMIC_DECREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
        }
        THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION);
    }

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveAudioFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    if(pSampleStreamingSession == NULL) {
        printf("[KVS Master] sampleReceiveAudioFrame(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    retStatus = transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                   (UINT64) pSampleStreamingSession,
                                   sampleFrameHandler);
    if(retStatus != STATUS_SUCCESS)
    {
        printf("[KVS Master] transceiverOnFrame(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}
