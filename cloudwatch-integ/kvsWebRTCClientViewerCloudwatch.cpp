#include <aws/core/Aws.h>
#include "../samples/Samples.h"
#include "Cloudwatch.h"

extern PSampleConfiguration gSampleConfiguration;

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

STATUS publishStatsForCanary(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL sampleConfigurationObjLocked = FALSE;
    BOOL statsLocked = FALSE;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "Sample config object not set up");

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        sampleConfigurationObjLocked = TRUE;
    }

    pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[0];
    acquireMetricsCtx(pSampleStreamingSession);
    CHK_WARN(pSampleStreamingSession != NULL || pSampleStreamingSession->pStatsCtx != NULL, STATUS_NULL_ARG, "Stats ctx object not set up");
    MUTEX_LOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    statsLocked = TRUE;
    pSampleStreamingSession->pStatsCtx->kvsRtcStats.requestedTypeOfStats = RTC_STATS_TYPE_INBOUND_RTP;
    if (!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->pSampleConfiguration->appTerminateFlag)) {
        CHK_LOG_ERR(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection,
                                                pSampleStreamingSession->pVideoRtcRtpTransceiver,
                                                &pSampleStreamingSession->pStatsCtx->kvsRtcStats));
        populateIncomingRtpMetricsContext(pSampleStreamingSession);
        CppInteg::Cloudwatch::getInstance().monitoring.pushInboundRtpStats(
                &pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx);
    } else {
        retStatus = STATUS_TIMER_QUEUE_STOP_SCHEDULING;
    }
CleanUp:
    if(statsLocked) {
        MUTEX_UNLOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    }
    releaseMetricsCtx(pSampleStreamingSession);
    if (sampleConfigurationObjLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }
    return retStatus;
}

STATUS publishEndToEndMetrics(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL sampleConfigurationObjLocked = FALSE;
    BOOL statsLocked = FALSE;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "Sample config object not set up");

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        sampleConfigurationObjLocked = TRUE;
    }

    pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[0];
    acquireMetricsCtx(pSampleStreamingSession);
    CHK_WARN(pSampleStreamingSession != NULL || pSampleStreamingSession->pStatsCtx != NULL, STATUS_NULL_ARG, "Stats ctx object not set up");
    MUTEX_LOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    statsLocked = TRUE;

    if (!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->pSampleConfiguration->appTerminateFlag)) {
        CppInteg::Cloudwatch::getInstance().monitoring.pushEndToEndMetrics(
                &pSampleStreamingSession->pStatsCtx->endToEndMetricsCtx);
    } else {
            retStatus = STATUS_TIMER_QUEUE_STOP_SCHEDULING;
    }
CleanUp:
    if(statsLocked) {
        MUTEX_UNLOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    }
    releaseMetricsCtx(pSampleStreamingSession);
    if (sampleConfigurationObjLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }
    return STATUS_SUCCESS;
}

VOID videoFrameHandler(UINT64 customData, PFrame pFrame)
{
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    PStatsCtx pStatsCtx = pSampleStreamingSession->pStatsCtx;
    // Parse packet and ad e2e metrics
    PBYTE frameDataPtr = pFrame->frameData + ANNEX_B_NALU_SIZE;
    UINT32 rawPacketSize = 0;
    // Get size of hex encoded data
    hexDecode((PCHAR) frameDataPtr, pFrame->size - ANNEX_B_NALU_SIZE, NULL, &rawPacketSize);
    PBYTE rawPacket = (PBYTE) MEMCALLOC(1, (rawPacketSize * SIZEOF(BYTE)));
    hexDecode((PCHAR) frameDataPtr, pFrame->size - ANNEX_B_NALU_SIZE, rawPacket, &rawPacketSize);

    // Extract the timestamp field from raw packet
    frameDataPtr = rawPacket;
    UINT64 receivedTs = getUnalignedInt64BigEndian((PINT64)(frameDataPtr));
    frameDataPtr += SIZEOF(UINT64);
    UINT32 receivedSize = getUnalignedInt32BigEndian((PINT32)(frameDataPtr));

    if(pSampleStreamingSession == NULL || pStatsCtx == NULL) {
        DLOGW("Streaming session freed. Doing nothing");
        return;
    }
    MUTEX_LOCK(pStatsCtx->statsUpdateLock);
    if(pStatsCtx == NULL) {
        DLOGW("pStatsCtx freed");
        return;
    }
    pStatsCtx->endToEndMetricsCtx.frameLatencyAvg =
            EMA_ACCUMULATOR_GET_NEXT(pStatsCtx->endToEndMetricsCtx.frameLatencyAvg, GETTIME() - receivedTs);

    // Do a size match of the raw packet. Since raw packet does not contain the NALu, the
    // comparison would be rawPacketSize + ANNEX_B_NALU_SIZE and the received size
    if (rawPacketSize + ANNEX_B_NALU_SIZE == receivedSize) {
        pStatsCtx->endToEndMetricsCtx.sizeMatchAvg = EMA_ACCUMULATOR_GET_NEXT(pSampleStreamingSession->pStatsCtx->endToEndMetricsCtx.sizeMatchAvg, 1);
    } else {
        pStatsCtx->endToEndMetricsCtx.sizeMatchAvg = EMA_ACCUMULATOR_GET_NEXT(pSampleStreamingSession->pStatsCtx->endToEndMetricsCtx.sizeMatchAvg, 0);
    }
    SAFE_MEMFREE(rawPacket);
    MUTEX_UNLOCK(pStatsCtx->statsUpdateLock);
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
    CHAR clientId[256];
    PCHAR region;
    CHAR channelName[MAX_CHANNEL_NAME_LEN];
    PCHAR channelNamePrefix;
    UINT32 e2eTimerId = MAX_UINT32;
    UINT32 terminateId = MAX_UINT32;
    UINT32 inboundTimerId = MAX_UINT32;
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        SET_INSTRUMENTED_ALLOCATORS();
        UINT32 logLevel = setLogLevel();

#ifndef _WIN32
        signal(SIGINT, sigintHandler);
#endif
        if (USE_IOT) {
            PCHAR pChannelName;
            CHK_ERR((pChannelName = GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set since USE_IOT is enabled");
            STRNCPY(channelName, pChannelName, SIZEOF(channelName));
        } else {
            channelNamePrefix = argc > 1 ? argv[1] : CHANNEL_NAME_PREFIX;
            SNPRINTF(channelName, SIZEOF(channelName), CHANNEL_NAME_TEMPLATE, channelNamePrefix, RUNNER_LABEL);
        }
        CHK_STATUS(createSampleConfiguration(channelName, SIGNALING_CHANNEL_ROLE_TYPE_VIEWER, USE_TRICKLE_ICE, USE_TURN, logLevel, &pSampleConfiguration));
        CHK_STATUS(setUpCredentialProvider(pSampleConfiguration, USE_IOT));
        pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
        pSampleConfiguration->audioCodec = AUDIO_CODEC;
        pSampleConfiguration->videoCodec = VIDEO_CODEC;
        pSampleConfiguration->forceTurn = FORCE_TURN_ONLY;
        pSampleConfiguration->enableMetrics = ENABLE_METRICS;
        pSampleConfiguration->receiveAudioVideoSource = NULL;

        // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
        CHK_STATUS(initKvsWebRtc());
        DLOGI("[KVS Viewer] KVS WebRTC initialization completed successfully");

        if(ENABLE_DATA_CHANNEL) {
            pSampleConfiguration->onDataChannel = onDataChannel;
        }

        if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
            region = (PCHAR) DEFAULT_AWS_REGION;
        }
        CppInteg::Cloudwatch::init(channelName, region, FALSE, FALSE);

        SNPRINTF(clientId, SIZEOF(clientId), "%s_%u", SAMPLE_VIEWER_CLIENT_ID, RAND() % MAX_UINT32);
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

        CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, videoFrameHandler));

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
        if(ENABLE_DATA_CHANNEL) {
            PRtcDataChannel pDataChannel = NULL;
            PRtcPeerConnection pPeerConnection = pSampleStreamingSession->pPeerConnection;
            SIZE_T datachannelLocalOpenCount = 0;

            // Creating a new datachannel on the peer connection of the existing sample streaming session
            CHK_STATUS(createDataChannel(pPeerConnection, channelName, NULL, &pDataChannel));
            DLOGI("[KVS Viewer] Creating data channel...completed");

            // Setting a callback for when the data channel is open
            CHK_STATUS(dataChannelOnOpen(pDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback));
            DLOGI("[KVS Viewer] Data Channel open now...");
        }

        CHK_STATUS(timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, END_TO_END_METRICS_INVOCATION_PERIOD, END_TO_END_METRICS_INVOCATION_PERIOD,
                                      publishEndToEndMetrics, (UINT64) pSampleConfiguration, &e2eTimerId));
        CHK_STATUS(timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, RUN_TIME, TIMER_QUEUE_SINGLE_INVOCATION_PERIOD, terminate,
                                      (UINT64) pSampleConfiguration, &terminateId));
        CHK_STATUS(timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, END_TO_END_METRICS_INVOCATION_PERIOD, END_TO_END_METRICS_INVOCATION_PERIOD,
                                      publishStatsForCanary, (UINT64) pSampleConfiguration, &inboundTimerId));
        // Block until interrupted
        while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted) && !ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag)) {
            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
        }
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

        // Free all timer created here that belong to SampleConfiguration timer handle before invoking freeSampleConfiguration
        if (IS_VALID_TIMER_QUEUE_HANDLE(pSampleConfiguration->timerQueueHandle)) {
            if (inboundTimerId != MAX_UINT32) {
                retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, inboundTimerId,
                                                  (UINT64) pSampleConfiguration);
                if (STATUS_FAILED(retStatus)) {
                    DLOGE("Failed to cancel inbound stats timer with: 0x%08x", retStatus);
                }
                inboundTimerId = MAX_UINT32;
            }

            if (e2eTimerId != MAX_UINT32) {
                retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, e2eTimerId,
                                                  (UINT64) pSampleConfiguration);
                if (STATUS_FAILED(retStatus)) {
                    DLOGE("Failed to cancel e2e timer with: 0x%08x", retStatus);
                }
                e2eTimerId = MAX_UINT32;
            }

            if (terminateId != MAX_UINT32) {
                retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, terminateId,
                                                  (UINT64) pSampleConfiguration);
                if (STATUS_FAILED(retStatus)) {
                    DLOGE("Failed to cancel terminate timer with: 0x%08x", retStatus);
                }
                terminateId = MAX_UINT32;
            }
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Viewer] freeSampleConfiguration(): operation returned status code: 0x%08x ", retStatus);
        }
    }
    DLOGI("[KVS Viewer] Cleanup done");

    retStatus = RESET_INSTRUMENTED_ALLOCATORS();
    DLOGI("All SDK allocations freed? %s..0x%08x", retStatus == STATUS_SUCCESS ? "Yes" : "No", retStatus);

    CppInteg::Cloudwatch::getInstance().monitoring.pushExitStatus(retStatus);
    CppInteg::Cloudwatch::deinit();
    Aws::ShutdownAPI(options);

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}
