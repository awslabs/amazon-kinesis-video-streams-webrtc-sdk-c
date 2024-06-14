#define LOG_CLASS "WebRtcSamples"
#include "../Samples.h"

PSampleConfiguration gSampleConfiguration = NULL;


STATUS terminate(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    DLOGI("Terminating the app");
    if (gSampleConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gSampleConfiguration->interrupted, TRUE);
        ATOMIC_STORE_BOOL(&gSampleConfiguration->appTerminateFlag, TRUE);
        CVAR_BROADCAST(gSampleConfiguration->cvar);
    }
    return STATUS_SUCCESS;
}

VOID sigintHandler(INT32 sigNum)
{
    UNUSED_PARAM(sigNum);
    if (gSampleConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gSampleConfiguration->interrupted, TRUE);
        CVAR_BROADCAST(gSampleConfiguration->cvar);
    }
}

STATUS signalingCallFailed(STATUS status)
{
    return (STATUS_SIGNALING_GET_TOKEN_CALL_FAILED == status || STATUS_SIGNALING_DESCRIBE_CALL_FAILED == status ||
            STATUS_SIGNALING_CREATE_CALL_FAILED == status || STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED == status ||
            STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED == status || STATUS_SIGNALING_CONNECT_CALL_FAILED == status ||
            STATUS_SIGNALING_DESCRIBE_MEDIA_CALL_FAILED == status);
}

VOID onConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE newState)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    DLOGI("New connection state %u", newState);

    switch (newState) {
        case RTC_PEER_CONNECTION_STATE_CONNECTED:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);
            if (pSampleConfiguration->enableIceStats) {
                CHK_LOG_ERR(logSelectedIceCandidatesInformation(pSampleStreamingSession));
            }
            break;
        case RTC_PEER_CONNECTION_STATE_FAILED:
            // explicit fallthrough
        case RTC_PEER_CONNECTION_STATE_CLOSED:
            // explicit fallthrough
        case RTC_PEER_CONNECTION_STATE_DISCONNECTED:
            DLOGD("p2p connection disconnected");
            ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);
            pSampleConfiguration->storageDisconnectedTime = GETTIME();
            // explicit fallthrough
        default:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);

            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    UNUSED_PARAM(customData);
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStateStr;

    signalingClientGetStateString(state, &pStateStr);

    DLOGV("Signaling client state changed to %d - '%s'", state, pStateStr);

    // Return success to continue
    return retStatus;
}

STATUS signalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    DLOGW("Signaling client generated an error 0x%08x - '%.*s'", status, msgLen, msg);

    // We will force re-create the signaling client on the following errors
    if (status == STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED || status == STATUS_SIGNALING_RECONNECT_FAILED) {
        ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);
    }

    return STATUS_SUCCESS;
}

BOOL sampleFilterNetworkInterfaces(UINT64 customData, PCHAR networkInt)
{
    UNUSED_PARAM(customData);
    BOOL useInterface = FALSE;
    if (STRNCMP(networkInt, (PCHAR) "eth0", ARRAY_SIZE("eth0")) == 0) {
        useInterface = TRUE;
    }
    DLOGD("%s %s", networkInt, (useInterface) ? ("allowed. Candidates to be gathered") : ("blocked. Candidates will not be gathered"));
    return useInterface;
}

PVOID asyncGetIceConfigInfo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    AsyncGetIceStruct* data = (AsyncGetIceStruct*) args;
    PIceConfigInfo pIceConfigInfo = NULL;
    UINT32 uriCount = 0;
    UINT32 i = 0, maxTurnServer = 1;

    if (data != NULL) {
        /* signalingClientGetIceConfigInfoCount can return more than one turn server. Use only one to optimize
         * candidate gathering latency. But user can also choose to use more than 1 turn server. */
        for (uriCount = 0, i = 0; i < maxTurnServer; i++) {
            /*
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
             * over DTLS yet. if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
             *
             * It's recommended to not pass too many TURN iceServers to configuration because it will slow down ice gathering in non-trickle mode.
             */
            CHK_STATUS(signalingClientGetIceConfigInfo(data->signalingClientHandle, i, &pIceConfigInfo));
            CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
            uriCount += pIceConfigInfo->uriCount;
            CHK_STATUS(addConfigToServerList(&(data->pRtcPeerConnection), pIceConfigInfo));
        }
    }
    *(data->pUriCount) += uriCount;

CleanUp:
    SAFE_MEMFREE(data);
    CHK_LOG_ERR(retStatus);
    return NULL;
}

STATUS initializePeerConnection(PSampleConfiguration pSampleConfiguration, PRtcPeerConnection* ppRtcPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcConfiguration configuration;
#ifndef ENABLE_KVS_THREADPOOL
    UINT32 i, j, maxTurnServer = 1;
    PIceConfigInfo pIceConfigInfo;
    UINT32 uriCount = 0;
#endif
    UINT64 data;
    PRtcCertificate pRtcCertificate = NULL;

    CHK(pSampleConfiguration != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set this to custom callback to enable filtering of interfaces
    configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = NULL;

    // disable TWCC
    configuration.kvsRtcConfiguration.disableSenderSideBandwidthEstimation = !(pSampleConfiguration->enableTwcc);
    DLOGI("TWCC is : %s", configuration.kvsRtcConfiguration.disableSenderSideBandwidthEstimation ? "Disabled" : "Enabled");

    // Set the ICE mode explicitly

    if (pSampleConfiguration->forceTurn) {
        configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
    } else {
        configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;
    }

    configuration.kvsRtcConfiguration.enableIceStats = pSampleConfiguration->enableIceStats;
    // Set the  STUN server
    PCHAR pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pSampleConfiguration->channelInfo.pRegion, "cn-")) {
        pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, pSampleConfiguration->channelInfo.pRegion,
             pKinesisVideoStunUrlPostFix);

    // Check if we have any pregenerated certs and use them
    // NOTE: We are running under the config lock
    retStatus = stackQueueDequeue(pSampleConfiguration->pregeneratedCertificates, &data);
    CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_NOT_FOUND, retStatus);

    if (retStatus == STATUS_NOT_FOUND) {
        retStatus = STATUS_SUCCESS;
    } else {
        // Use the pre-generated cert and get rid of it to not reuse again
        pRtcCertificate = (PRtcCertificate) data;
        configuration.certificates[0] = *pRtcCertificate;
    }

    CHK_STATUS(createPeerConnection(&configuration, ppRtcPeerConnection));

    if (pSampleConfiguration->useTurn) {
#ifdef ENABLE_KVS_THREADPOOL
        pSampleConfiguration->iceUriCount = 1;
        AsyncGetIceStruct* pAsyncData = NULL;

        pAsyncData = (AsyncGetIceStruct*) MEMCALLOC(1, SIZEOF(AsyncGetIceStruct));
        pAsyncData->signalingClientHandle = pSampleConfiguration->signalingClientHandle;
        pAsyncData->pRtcPeerConnection = *ppRtcPeerConnection;
        pAsyncData->pUriCount = &(pSampleConfiguration->iceUriCount);
        CHK_STATUS(peerConnectionAsync(asyncGetIceConfigInfo, (PVOID) pAsyncData));
#else

        /* signalingClientGetIceConfigInfoCount can return more than one turn server. Use only one to optimize
         * candidate gathering latency. But user can also choose to use more than 1 turn server. */
        for (uriCount = 0, i = 0; i < maxTurnServer; i++) {
            /*
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
             * over DTLS yet. if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
             *
             * It's recommended to not pass too many TURN iceServers to configuration because it will slow down ice gathering in non-trickle mode.
             */
            CHK_STATUS(signalingClientGetIceConfigInfo(pSampleConfiguration->signalingClientHandle, i, &pIceConfigInfo));
            CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
            uriCount += pIceConfigInfo->uriCount;
            CHK_STATUS(addConfigToServerList(ppRtcPeerConnection, pIceConfigInfo));
        }
        pSampleConfiguration->iceUriCount = uriCount + 1;
#endif
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Free the certificate which can be NULL as we no longer need it and won't reuse
    freeRtcCertificate(pRtcCertificate);

    LEAVES();
    return retStatus;
}

STATUS createSampleStreamingSession(PSampleConfiguration pSampleConfiguration, PCHAR peerId, BOOL isMaster,
                                    PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcMediaStreamTrack videoTrack, audioTrack;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    RtcRtpTransceiverInit audioRtpTransceiverInit;
    RtcRtpTransceiverInit videoRtpTransceiverInit;

    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    CHK(pSampleConfiguration != NULL && ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    CHK((isMaster && peerId != NULL) || !isMaster, STATUS_INVALID_ARG);

    pSampleStreamingSession = (PSampleStreamingSession) MEMCALLOC(1, SIZEOF(SampleStreamingSession));
    pSampleStreamingSession->firstFrame = TRUE;
    pSampleStreamingSession->offerReceiveTime = GETTIME();
    CHK(pSampleStreamingSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    if (isMaster) {
        STRCPY(pSampleStreamingSession->peerId, peerId);
    } else {
        STRCPY(pSampleStreamingSession->peerId, SAMPLE_VIEWER_CLIENT_ID);
    }
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->peerIdReceived, TRUE);

    pSampleStreamingSession->pAudioRtcRtpTransceiver = NULL;
    pSampleStreamingSession->pVideoRtcRtpTransceiver = NULL;

    pSampleStreamingSession->pSampleConfiguration = pSampleConfiguration;
    pSampleStreamingSession->rtpMetricsHistory.prevTs = GETTIME();

    // if we're the viewer, we control the trickle ice mode
    pSampleStreamingSession->remoteCanTrickleIce = !isMaster && pSampleConfiguration->trickleIce;

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, FALSE);
    if (pSampleConfiguration->enableTwcc) {
        pSampleStreamingSession->twccMetadata.updateLock = MUTEX_CREATE(TRUE);
    }

    if (pSampleConfiguration->enableMetrics) {
        CHK_STATUS(setupMetricsCtx(pSampleStreamingSession));
    }

    // Flag to enable SDK to calculate selected ice server, local, remote and candidate pair stats.
    pSampleConfiguration->enableIceStats = FALSE;

    pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionStartTime =
            GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    CHK_STATUS(initializePeerConnection(pSampleConfiguration, &pSampleStreamingSession->pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onIceCandidateHandler));
    CHK_STATUS(
        peerConnectionOnConnectionStateChange(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onConnectionStateChange));

#ifdef ENABLE_DATA_CHANNEL
    if (pSampleConfiguration->onDataChannel != NULL) {
        CHK_STATUS(peerConnectionOnDataChannel(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                               pSampleConfiguration->onDataChannel));
    }
#endif

    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, pSampleConfiguration->videoCodec));
    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, pSampleConfiguration->audioCodec));

    // Add a SendRecv Transceiver of type video
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = pSampleConfiguration->videoCodec;
    videoRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    videoRtpTransceiverInit.rollingBufferDurationSec = 3;
    // Considering 4 Mbps for 720p (which is what our samples use). This is for H.264.
    // The value could be different for other codecs.
    videoRtpTransceiverInit.rollingBufferBitratebps = 4 * 1024 * 1024;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &videoTrack, &videoRtpTransceiverInit,
                              &pSampleStreamingSession->pVideoRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));

    // Add a SendRecv Transceiver of type audio
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = pSampleConfiguration->audioCodec;
    audioRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audioRtpTransceiverInit.rollingBufferDurationSec = 3;
    // For opus, the bitrate could be between 6 Kbps to 510 Kbps
    audioRtpTransceiverInit.rollingBufferBitratebps = 510 * 1024;
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &audioTrack, &audioRtpTransceiverInit,
                              &pSampleStreamingSession->pAudioRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));
    // twcc bandwidth estimation
    if (pSampleConfiguration->enableTwcc) {
        CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                                             sampleSenderBandwidthEstimationHandler));
    }
    pSampleStreamingSession->startUpLatency = 0;

CleanUp:

    if (STATUS_FAILED(retStatus) && pSampleStreamingSession != NULL) {
        freeSampleStreamingSession(&pSampleStreamingSession);
        pSampleStreamingSession = NULL;
    }

    if (ppSampleStreamingSession != NULL) {
        *ppSampleStreamingSession = pSampleStreamingSession;
    }

    return retStatus;
}

STATUS freeSampleStreamingSession(PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PSampleConfiguration pSampleConfiguration;

    CHK(ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    pSampleStreamingSession = *ppSampleStreamingSession;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, retStatus);
    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;

    DLOGD("Freeing streaming session with peer id: %s ", pSampleStreamingSession->peerId);

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);

    if (pSampleStreamingSession->shutdownCallback != NULL) {
        pSampleStreamingSession->shutdownCallback(pSampleStreamingSession->shutdownCallbackCustomData, pSampleStreamingSession);
    }

    if (IS_VALID_TID_VALUE(pSampleStreamingSession->receiveAudioVideoSenderTid)) {
        THREAD_JOIN(pSampleStreamingSession->receiveAudioVideoSenderTid, NULL);
    }

    // De-initialize the session stats timer if there are no active sessions
    // NOTE: we need to perform this under the lock which might be acquired by
    // the running thread but it's OK as it's re-entrant
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    if (pSampleConfiguration->iceCandidatePairStatsTimerId != MAX_UINT32 && pSampleConfiguration->streamingSessionCount == 0 &&
        IS_VALID_TIMER_QUEUE_HANDLE(pSampleConfiguration->timerQueueHandle)) {
        CHK_LOG_ERR(timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->iceCandidatePairStatsTimerId,
                                          (UINT64) pSampleConfiguration));
        pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
    }
    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    if (pSampleConfiguration->enableTwcc) {
        if (IS_VALID_MUTEX_VALUE(pSampleStreamingSession->twccMetadata.updateLock)) {
            MUTEX_FREE(pSampleStreamingSession->twccMetadata.updateLock);
        }
    }

    if(pSampleConfiguration->enableMetrics) {
        CHK_LOG_ERR(freeMetricsCtx(&pSampleStreamingSession->pStatsCtx));
    }
    CHK_LOG_ERR(closePeerConnection(pSampleStreamingSession->pPeerConnection));
    CHK_LOG_ERR(freePeerConnection(&pSampleStreamingSession->pPeerConnection));
    SAFE_MEMFREE(pSampleStreamingSession);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS streamingSessionOnShutdown(PSampleStreamingSession pSampleStreamingSession, UINT64 customData,
                                  StreamSessionShutdownCallback streamSessionShutdownCallback)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleStreamingSession != NULL && streamSessionShutdownCallback != NULL, STATUS_NULL_ARG);

    pSampleStreamingSession->shutdownCallbackCustomData = customData;
    pSampleStreamingSession->shutdownCallback = streamSessionShutdownCallback;

CleanUp:

    return retStatus;
}

VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame)
{
    UNUSED_PARAM(customData);
    DLOGV("Video Frame received. TrackId: %" PRIu64 ", Size: %u, Flags %u", pFrame->trackId, pFrame->size, pFrame->flags);
}

VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame)
{
    UNUSED_PARAM(customData);
    DLOGV("Audio Frame received. TrackId: %" PRIu64 ", Size: %u, Flags %u", pFrame->trackId, pFrame->size, pFrame->flags);
}

VOID sampleFrameHandler(UINT64 customData, PFrame pFrame)
{
    UNUSED_PARAM(customData);
    DLOGV("Video Frame received. TrackId: %" PRIu64 ", Size: %u, Flags %u", pFrame->trackId, pFrame->size, pFrame->flags);
}

VOID sampleBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate)
{
    UNUSED_PARAM(customData);
    DLOGV("received bitrate suggestion: %f", maximumBitrate);
}

// Sample callback for TWCC. Average packet is calculated with exponential moving average (EMA). If average packet lost is <= 5%,
// the current bitrate is increased by 5%. If more than 5%, the current bitrate
// is reduced by percent lost. Bitrate update is allowed every second and is increased/decreased upto the limits
VOID sampleSenderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt,
                                            UINT64 duration)
{
    UNUSED_PARAM(duration);
    UINT64 videoBitrate, audioBitrate;
    UINT64 currentTimeMs, timeDiff;
    UINT32 lostPacketsCnt = txPacketsCnt - rxPacketsCnt;
    DOUBLE percentLost = (DOUBLE) ((txPacketsCnt > 0) ? (lostPacketsCnt * 100 / txPacketsCnt) : 0.0);
    SampleStreamingSession* pSampleStreamingSession = (SampleStreamingSession*) customData;

    if (pSampleStreamingSession == NULL) {
        DLOGW("Invalid streaming session (NULL object)");
        return;
    }

    // Calculate packet loss
    pSampleStreamingSession->twccMetadata.averagePacketLoss =
        EMA_ACCUMULATOR_GET_NEXT(pSampleStreamingSession->twccMetadata.averagePacketLoss, ((DOUBLE) percentLost));

    currentTimeMs = GETTIME();
    timeDiff = currentTimeMs - pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs;
    if (timeDiff < TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS) {
        // Too soon for another adjustment
        return;
    }

    MUTEX_LOCK(pSampleStreamingSession->twccMetadata.updateLock);
    videoBitrate = pSampleStreamingSession->twccMetadata.currentVideoBitrate;
    audioBitrate = pSampleStreamingSession->twccMetadata.currentAudioBitrate;

    if (pSampleStreamingSession->twccMetadata.averagePacketLoss <= 5) {
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        videoBitrate = (UINT64) MIN(videoBitrate * 1.05, MAX_VIDEO_BITRATE_KBPS);
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        audioBitrate = (UINT64) MIN(audioBitrate * 1.05, MAX_AUDIO_BITRATE_BPS);
    } else {
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        videoBitrate = (UINT64) MAX(videoBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_VIDEO_BITRATE_KBPS);
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        audioBitrate = (UINT64) MAX(audioBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_AUDIO_BITRATE_BPS);
    }

    // Update the session with the new bitrate and adjustment time
    pSampleStreamingSession->twccMetadata.newVideoBitrate = videoBitrate;
    pSampleStreamingSession->twccMetadata.newAudioBitrate = audioBitrate;
    MUTEX_UNLOCK(pSampleStreamingSession->twccMetadata.updateLock);

    pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs = currentTimeMs;

    DLOGI("Adjustment made: average packet loss = %.2f%%, timediff: %llu ms", pSampleStreamingSession->twccMetadata.averagePacketLoss, timeDiff);
    DLOGI("Suggested video bitrate %u kbps, suggested audio bitrate: %u bps, sent: %u bytes %u packets received: %u bytes %u packets in %lu msec",
          videoBitrate, audioBitrate, txBytes, txPacketsCnt, rxBytes, rxPacketsCnt, duration / 10000ULL);
}

STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, UINT32 logLevel,
                                 PSampleConfiguration* ppSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pIotCoreCredentialEndPoint, pIotCoreCert, pIotCorePrivateKey, pIotCoreRoleAlias, pIotCoreCertificateId, pIotCoreThingName;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSampleConfiguration = (PSampleConfiguration) MEMCALLOC(1, SIZEOF(SampleConfiguration))), STATUS_NOT_ENOUGH_MEMORY);

    // If the env is set, we generate normal log files apart from filtered profile log files
    // If not set, we generate only the filtered profile log files
    if (NULL != GETENV(ENABLE_FILE_LOGGING)) {
        retStatus = createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, TRUE, LOG_LEVEL_PROFILE, NULL);

        if (retStatus != STATUS_SUCCESS) {
            DLOGW("[KVS Master] createFileLogger(): operation returned status code: 0x%08x", retStatus);
        } else {
            pSampleConfiguration->enableFileLogging = TRUE;
        }
    } else {
        retStatus = createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, FALSE, LOG_LEVEL_PROFILE, NULL);

        if (retStatus != STATUS_SUCCESS) {
            DLOGW("[KVS Master] createFileLogger(): operation returned status code: 0x%08x", retStatus);
        } else {
            pSampleConfiguration->enableFileLogging = TRUE;
        }
    }

    if ((pSampleConfiguration->channelInfo.pRegion = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        pSampleConfiguration->channelInfo.pRegion = DEFAULT_AWS_REGION;
    }

    pSampleConfiguration->mediaSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pSampleConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
    pSampleConfiguration->cvar = CVAR_CREATE();
    pSampleConfiguration->streamingSessionListReadLock = MUTEX_CREATE(FALSE);
    pSampleConfiguration->signalingSendMessageLock = MUTEX_CREATE(FALSE);
    pSampleConfiguration->forceTurn = FALSE;

    /* This is ignored for master. Master can extract the info from offer. Viewer has to know if peer can trickle or
     * not ahead of time. */

    pSampleConfiguration->trickleIce = trickleIce;
    pSampleConfiguration->useTurn = useTurn;
    pSampleConfiguration->enableSendingMetricsToViewerViaDc = FALSE;
    pSampleConfiguration->receiveAudioVideoSource = NULL;

    pSampleConfiguration->channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    DLOGI("Channel name: %s", channelName);
    pSampleConfiguration->channelInfo.pChannelName = channelName;

    pSampleConfiguration->channelInfo.pKmsKeyId = NULL;
    pSampleConfiguration->channelInfo.tagCount = 0;
    pSampleConfiguration->channelInfo.pTags = NULL;
    pSampleConfiguration->channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pSampleConfiguration->channelInfo.channelRoleType = roleType;
    pSampleConfiguration->channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_FILE;
    pSampleConfiguration->channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    pSampleConfiguration->channelInfo.asyncIceServerConfig = TRUE; // has no effect
    pSampleConfiguration->channelInfo.retry = TRUE;
    pSampleConfiguration->channelInfo.reconnect = TRUE;
    pSampleConfiguration->channelInfo.messageTtl = 0; // Default is 60 seconds

    pSampleConfiguration->signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    pSampleConfiguration->signalingClientCallbacks.errorReportFn = signalingClientError;
    pSampleConfiguration->signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    pSampleConfiguration->signalingClientCallbacks.customData = (UINT64) pSampleConfiguration;

    pSampleConfiguration->clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    pSampleConfiguration->clientInfo.loggingLevel = logLevel;
    pSampleConfiguration->clientInfo.cacheFilePath = NULL; // Use the default path
    pSampleConfiguration->clientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;
    pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
    pSampleConfiguration->pregenerateCertTimerId = MAX_UINT32;
    pSampleConfiguration->signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    // Flag to enable SDK to calculate selected ice server, local, remote and candidate pair stats.
    pSampleConfiguration->enableIceStats = FALSE;

    // Flag to enable/disable TWCC
    pSampleConfiguration->enableTwcc = TRUE;

    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);

    CHK_STATUS(timerQueueCreate(&pSampleConfiguration->timerQueueHandle));

    CHK_STATUS(stackQueueCreate(&pSampleConfiguration->pregeneratedCertificates));

    // Start the cert pre-gen timer callback
#ifdef SAMPLE_PRE_GENERATE_CERT
    CHK_LOG_ERR(retStatus =
                    timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, 0, SAMPLE_PRE_GENERATE_CERT_PERIOD, pregenerateCertTimerCallback,
                                       (UINT64) pSampleConfiguration, &pSampleConfiguration->pregenerateCertTimerId));
#endif

    pSampleConfiguration->iceUriCount = 0;

    CHK_STATUS(stackQueueCreate(&pSampleConfiguration->pPendingSignalingMessageForRemoteClient));
    CHK_STATUS(hashTableCreateWithParams(SAMPLE_HASH_TABLE_BUCKET_COUNT, SAMPLE_HASH_TABLE_BUCKET_LENGTH,
                                         &pSampleConfiguration->pRtcPeerConnectionForRemoteClient));
CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeSampleConfiguration(&pSampleConfiguration);
    }

    if (ppSampleConfiguration != NULL) {
        *ppSampleConfiguration = pSampleConfiguration;
    }

    return retStatus;
}

STATUS initSignaling(PSampleConfiguration pSampleConfiguration, PCHAR clientId)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientMetrics signalingClientMetrics = pSampleConfiguration->signalingClientMetrics;
    pSampleConfiguration->signalingClientCallbacks.messageReceivedFn = signalingMessageReceived;
    STRCPY(pSampleConfiguration->clientInfo.clientId, clientId);
    CHK_STATUS(createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                         &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                         &pSampleConfiguration->signalingClientHandle));

    // Enable the processing of the messages
    CHK_STATUS(signalingClientFetchSync(pSampleConfiguration->signalingClientHandle));

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif

    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);

    // Logging this here since the logs in signaling library do not get routed to file
    DLOGP("[Signaling Get token] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getTokenCallTime);
    DLOGP("[Signaling Describe] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.describeCallTime);
    DLOGP("[Signaling Describe Media] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.describeMediaCallTime);
    DLOGP("[Signaling Create Channel] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.createCallTime);
    DLOGP("[Signaling Get endpoint] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getEndpointCallTime);
    DLOGP("[Signaling Get ICE config] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getIceConfigCallTime);
    DLOGP("[Signaling Connect] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.connectCallTime);
    if (signalingClientMetrics.signalingClientStats.joinSessionCallTime != 0) {
        DLOGP("[Signaling Join Session] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.joinSessionCallTime);
    }
    DLOGP("[Signaling create client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.createClientTime);
    DLOGP("[Signaling fetch client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.fetchClientTime);
    DLOGP("[Signaling connect client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.connectClientTime);
    pSampleConfiguration->signalingClientMetrics = signalingClientMetrics;
    gSampleConfiguration = pSampleConfiguration;
CleanUp:
    return retStatus;
}

STATUS logSignalingClientStats(PSignalingClientMetrics pSignalingClientMetrics)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSignalingClientMetrics != NULL, STATUS_NULL_ARG);
    DLOGD("Signaling client connection duration: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.connectionDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    DLOGD("Number of signaling client API errors: %d", pSignalingClientMetrics->signalingClientStats.numberOfErrors);
    DLOGD("Number of runtime errors in the session: %d", pSignalingClientMetrics->signalingClientStats.numberOfRuntimeErrors);
    DLOGD("Signaling client uptime: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.connectionDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    // This gives the EMA of the createChannel, describeChannel, getChannelEndpoint and deleteChannel calls
    DLOGD("Control Plane API call latency: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.cpApiCallLatency / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    // This gives the EMA of the getIceConfig() call.
    DLOGD("Data Plane API call latency: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.dpApiCallLatency / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    DLOGD("API call retry count: %d", pSignalingClientMetrics->signalingClientStats.apiCallRetryCount);
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS pregenerateCertTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    BOOL locked = FALSE;
    UINT32 certCount;
    PRtcCertificate pRtcCertificate = NULL;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] pregenerateCertTimerCallback(): Passed argument is NULL");

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        locked = TRUE;
    }

    // Quick check if there is anything that needs to be done.
    CHK_STATUS(stackQueueGetCount(pSampleConfiguration->pregeneratedCertificates, &certCount));
    CHK(certCount != MAX_RTCCONFIGURATION_CERTIFICATES, retStatus);

    // Generate the certificate with the keypair
    CHK_STATUS(createRtcCertificate(&pRtcCertificate));

    // Add to the stack queue
    CHK_STATUS(stackQueueEnqueue(pSampleConfiguration->pregeneratedCertificates, (UINT64) pRtcCertificate));

    DLOGV("New certificate has been pre-generated and added to the queue");

    // Reset it so it won't be freed on exit
    pRtcCertificate = NULL;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

CleanUp:

    if (pRtcCertificate != NULL) {
        freeRtcCertificate(pRtcCertificate);
    }

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS freeSampleConfiguration(PSampleConfiguration* ppSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration;
    UINT32 i;
    UINT64 data;
    StackQueueIterator iterator;
    BOOL locked = FALSE;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = *ppSampleConfiguration;

    CHK(pSampleConfiguration != NULL, retStatus);

    if (IS_VALID_TIMER_QUEUE_HANDLE(pSampleConfiguration->timerQueueHandle)) {
        if (pSampleConfiguration->iceCandidatePairStatsTimerId != MAX_UINT32) {
            retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->iceCandidatePairStatsTimerId,
                                              (UINT64) pSampleConfiguration);
            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to cancel stats timer with: 0x%08x", retStatus);
            }
            pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
        }

        if (pSampleConfiguration->pregenerateCertTimerId != MAX_UINT32) {
            retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->pregenerateCertTimerId,
                                              (UINT64) pSampleConfiguration);
            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to cancel certificate pre-generation timer with: 0x%08x", retStatus);
            }
            pSampleConfiguration->pregenerateCertTimerId = MAX_UINT32;
        }

        timerQueueFree(&pSampleConfiguration->timerQueueHandle);
    }

    if (pSampleConfiguration->pPendingSignalingMessageForRemoteClient != NULL) {
        // Iterate and free all the pending queues
        stackQueueGetIterator(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeMessageQueue((PPendingMessageQueue) data);
        }

        stackQueueClear(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, FALSE);
        stackQueueFree(pSampleConfiguration->pPendingSignalingMessageForRemoteClient);
        pSampleConfiguration->pPendingSignalingMessageForRemoteClient = NULL;
    }

    hashTableClear(pSampleConfiguration->pRtcPeerConnectionForRemoteClient);
    hashTableFree(pSampleConfiguration->pRtcPeerConnectionForRemoteClient);

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = TRUE;
    }

    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        if (pSampleConfiguration->enableIceStats) {
            CHK_LOG_ERR(gatherIceServerStats(pSampleConfiguration->sampleStreamingSessionList[i]));
        }
        freeSampleStreamingSession(&pSampleConfiguration->sampleStreamingSessionList[i]);
    }
    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }
    deinitKvsWebRtc();

    SAFE_MEMFREE(pSampleConfiguration->pVideoFrameBuffer);
    SAFE_MEMFREE(pSampleConfiguration->pAudioFrameBuffer);

    if (IS_VALID_CVAR_VALUE(pSampleConfiguration->cvar) && IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        CVAR_BROADCAST(pSampleConfiguration->cvar);
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        MUTEX_FREE(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->streamingSessionListReadLock)) {
        MUTEX_FREE(pSampleConfiguration->streamingSessionListReadLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->signalingSendMessageLock)) {
        MUTEX_FREE(pSampleConfiguration->signalingSendMessageLock);
    }

    if (IS_VALID_CVAR_VALUE(pSampleConfiguration->cvar)) {
        CVAR_FREE(pSampleConfiguration->cvar);
    }

    if (pSampleConfiguration->useIot) {
        freeIotCredentialProvider(&pSampleConfiguration->pCredentialProvider);
    } else {
        freeStaticCredentialProvider(&pSampleConfiguration->pCredentialProvider);
    }

    if (pSampleConfiguration->pregeneratedCertificates != NULL) {
        stackQueueGetIterator(pSampleConfiguration->pregeneratedCertificates, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeRtcCertificate((PRtcCertificate) data);
        }

        CHK_LOG_ERR(stackQueueClear(pSampleConfiguration->pregeneratedCertificates, FALSE));
        CHK_LOG_ERR(stackQueueFree(pSampleConfiguration->pregeneratedCertificates));
        pSampleConfiguration->pregeneratedCertificates = NULL;
    }
    if (pSampleConfiguration->enableFileLogging) {
        freeFileLogger();
    }
    SAFE_MEMFREE(*ppSampleConfiguration);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS sessionCleanupWait(PSampleConfiguration pSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    UINT32 i, clientIdHash;
    BOOL sampleConfigurationObjLockLocked = FALSE, streamingSessionListReadLockLocked = FALSE, peerConnectionFound = FALSE, sessionFreed = FALSE;
    SIGNALING_CLIENT_STATE signalingClientState;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted)) {
        // Keep the main set of operations interlocked until cvar wait which would atomically unlock
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = TRUE;

        // scan and cleanup terminated streaming session
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->sampleStreamingSessionList[i]->terminateFlag)) {
                pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = TRUE;

                // swap with last element and decrement count
                pSampleConfiguration->streamingSessionCount--;
                pSampleConfiguration->sampleStreamingSessionList[i] =
                    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount];

                // Remove from the hash table
                clientIdHash = COMPUTE_CRC32((PBYTE) pSampleStreamingSession->peerId, (UINT32) STRLEN(pSampleStreamingSession->peerId));
                CHK_STATUS(hashTableContains(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));
                if (peerConnectionFound) {
                    CHK_STATUS(hashTableRemove(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash));
                }

                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = FALSE;

                CHK_STATUS(freeSampleStreamingSession(&pSampleStreamingSession));
                sessionFreed = TRUE;
            }
        }

        if (sessionFreed && pSampleConfiguration->channelInfo.useMediaStorage && !ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient)) {
            // In the WebRTC Media Storage Ingestion Case the backend will terminate the session after
            // 1 hour.  The SDK needs to make a new JoinSession Call in order to receive a new
            // offer from the backend.  We will create a new sample streaming session upon receipt of the
            // offer.  The signalingClientConnectSync call will result in a JoinSession API call being made.
            CHK_STATUS(signalingClientDisconnectSync(pSampleConfiguration->signalingClientHandle));
            CHK_STATUS(signalingClientFetchSync(pSampleConfiguration->signalingClientHandle));
            CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));
            sessionFreed = FALSE;
        }

        // Check if we need to re-create the signaling client on-the-fly
        if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient)) {
            retStatus = signalingClientFetchSync(pSampleConfiguration->signalingClientHandle);
            if (STATUS_SUCCEEDED(retStatus)) {
                // Re-set the variable again
                ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
            } else if (signalingCallFailed(retStatus)) {
                printf("[KVS Common] recreating Signaling Client\n");
                freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
                createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                          &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                          &pSampleConfiguration->signalingClientHandle);
            }
        }

        // Check the signaling client state and connect if needed
        if (IS_VALID_SIGNALING_CLIENT_HANDLE(pSampleConfiguration->signalingClientHandle)) {
            CHK_STATUS(signalingClientGetCurrentState(pSampleConfiguration->signalingClientHandle, &signalingClientState));
            if (signalingClientState == SIGNALING_CLIENT_STATE_READY) {
                UNUSED_PARAM(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));
            }
        }

        // Check if any lingering pending message queues
        CHK_STATUS(removeExpiredMessageQueues(pSampleConfiguration->pPendingSignalingMessageForRemoteClient));

        // periodically wake up and clean up terminated streaming session
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, SAMPLE_SESSION_CLEANUP_WAIT_PERIOD);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = FALSE;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (sampleConfigurationObjLockLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (streamingSessionListReadLockLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
    }

    LEAVES();
    return retStatus;
}