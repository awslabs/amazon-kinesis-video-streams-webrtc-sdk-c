#define LOG_CLASS "WebRtcSamples"
#include "Samples.h"

PDemoConfiguration gDemoConfiguration = NULL;

VOID sigintHandler(INT32 sigNum)
{
    UNUSED_PARAM(sigNum);
    if (gDemoConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gDemoConfiguration->interrupted, TRUE);
        CVAR_BROADCAST(gDemoConfiguration->cvar);
    }
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;
    CHK_ERR(pSize != NULL, STATUS_NULL_ARG, "[KVS Master] Invalid file size");
    size = *pSize;
    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));
CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

static STATUS setUpLogging(PDemoConfiguration pDemoConfiguration)
{
    PCHAR pLogLevel;
    UINT32 logLevel = LOG_LEVEL_DEBUG;
    STATUS retStatus = STATUS_SUCCESS;
    if (NULL == (pLogLevel = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pLogLevel, NULL, 10, &logLevel) ||
        logLevel < LOG_LEVEL_VERBOSE || logLevel > LOG_LEVEL_SILENT) {
        logLevel = LOG_LEVEL_WARN;
    }
    SET_LOGGER_LOG_LEVEL(logLevel);

    if (NULL != GETENV(ENABLE_FILE_LOGGING)) {
        CHK_LOG_ERR(createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, TRUE, LOG_LEVEL_PROFILE, NULL));
        pDemoConfiguration->enableFileLogging = TRUE;
    } else {
        CHK_LOG_ERR(createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, FALSE, LOG_LEVEL_PROFILE, NULL));
        pDemoConfiguration->enableFileLogging = TRUE;
    }
    pDemoConfiguration->logLevel = logLevel;
CleanUp:
    return retStatus;
}

static STATUS setUpCredentialProvider(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken;
#ifdef IOT_CORE_ENABLE_CREDENTIALS
    PCHAR pIotCoreCredentialEndPoint, pIotCoreCert, pIotCorePrivateKey, pIotCoreRoleAlias, pIotCoreCertificateId, pIotCoreThingName;
    CHK_ERR((pIotCoreCredentialEndPoint = GETENV(IOT_CORE_CREDENTIAL_ENDPOINT)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_CREDENTIAL_ENDPOINT must be set");
    CHK_ERR((pIotCoreCert = GETENV(IOT_CORE_CERT)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_CERT must be set");
    CHK_ERR((pIotCorePrivateKey = GETENV(IOT_CORE_PRIVATE_KEY)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_PRIVATE_KEY must be set");
    CHK_ERR((pIotCoreRoleAlias = GETENV(IOT_CORE_ROLE_ALIAS)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_ROLE_ALIAS must be set");
    CHK_ERR((pIotCoreThingName = GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    CHK_ERR((pAccessKey = GETENV(ACCESS_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_ACCESS_KEY_ID must be set");
    CHK_ERR((pSecretKey = GETENV(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_SECRET_ACCESS_KEY must be set");
#endif

    pSessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
    if (pSessionToken != NULL && IS_EMPTY_STRING(pSessionToken)) {
        DLOGW("Session token is set but its value is empty. Ignoring.");
        pSessionToken = NULL;
    }

    if ((pDemoConfiguration->appSignalingCtx.channelInfo.pRegion = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        pDemoConfiguration->appSignalingCtx.channelInfo.pRegion = DEFAULT_AWS_REGION;
    }

    CHK_STATUS(lookForSslCert(&pDemoConfiguration));

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_STATUS(createLwsIotCredentialProvider(pIotCoreCredentialEndPoint, pIotCoreCert, pIotCorePrivateKey, pDemoConfiguration->pCaCertPath,
                                              pIotCoreRoleAlias, pIotCoreThingName, &pDemoConfiguration->pCredentialProvider));
#else
    CHK_STATUS(createStaticCredentialProvider(pAccessKey, 0, pSecretKey, 0, pSessionToken, 0, MAX_UINT64, &pDemoConfiguration->pCredentialProvider));
#endif
CleanUp:
    return retStatus;
}

static STATUS setUpSignalingDefaults(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    pDemoConfiguration->appSignalingCtx.signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pDemoConfiguration->appSignalingCtx.channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    pDemoConfiguration->appSignalingCtx.channelInfo.pKmsKeyId = NULL;
    pDemoConfiguration->appSignalingCtx.channelInfo.tagCount = 0;
    pDemoConfiguration->appSignalingCtx.channelInfo.pTags = NULL;
    pDemoConfiguration->appSignalingCtx.channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pDemoConfiguration->appSignalingCtx.channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_FILE;
    pDemoConfiguration->appSignalingCtx.channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    pDemoConfiguration->appSignalingCtx.channelInfo.asyncIceServerConfig = TRUE; // has no effect
    pDemoConfiguration->appSignalingCtx.channelInfo.retry = TRUE;
    pDemoConfiguration->appSignalingCtx.channelInfo.reconnect = TRUE;
    pDemoConfiguration->appSignalingCtx.channelInfo.pCertPath = pDemoConfiguration->pCaCertPath;
    pDemoConfiguration->appSignalingCtx.channelInfo.messageTtl = 0; // Default is 60 seconds
    pDemoConfiguration->appSignalingCtx.channelInfo.channelRoleType = pDemoConfiguration->appConfigCtx.roleType;
    pDemoConfiguration->appSignalingCtx.channelInfo.pChannelName = pDemoConfiguration->appConfigCtx.pChannelName;

    pDemoConfiguration->appSignalingCtx.signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    pDemoConfiguration->appSignalingCtx.signalingClientCallbacks.errorReportFn = signalingClientError;
    pDemoConfiguration->appSignalingCtx.signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    pDemoConfiguration->appSignalingCtx.signalingClientCallbacks.customData = (UINT64) pDemoConfiguration;

    pDemoConfiguration->appSignalingCtx.clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    pDemoConfiguration->appSignalingCtx.clientInfo.loggingLevel = pDemoConfiguration->logLevel;
    pDemoConfiguration->appSignalingCtx.clientInfo.cacheFilePath = NULL; // Use the default path
    pDemoConfiguration->appSignalingCtx.clientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;
CleanUp:
    return retStatus;
}

static STATUS setUpDefaultsFn(UINT64 sampleConfigHandle, SIGNALING_CHANNEL_ROLE_TYPE roleType, INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PDemoConfiguration pDemoConfiguration = (PDemoConfiguration) sampleConfigHandle;
    /* This is ignored for master. Master can extract the info from offer. Viewer has to know if peer can trickle or
     * not ahead of time. */
    pDemoConfiguration->appConfigCtx.trickleIce = TRUE;
    pDemoConfiguration->appConfigCtx.useTurn = TRUE;
    pDemoConfiguration->appConfigCtx.enableSendingMetricsToViewerViaDc = FALSE;
    pDemoConfiguration->appConfigCtx.roleType = roleType;
    pDemoConfiguration->appSignalingCtx.channelInfo.useMediaStorage = FALSE;

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pDemoConfiguration->appConfigCtx.pChannelName = argc > 1 ? argv[1] : GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_THING_NAME must be set");
#else
    pDemoConfiguration->appConfigCtx.pChannelName = (argc > 1) ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    if ((pIotCoreCertificateId = GETENV(IOT_CORE_CERTIFICATE_ID)) != NULL) {
        pDemoConfiguration->channelInfo.pChannelName = pIotCoreCertificateId;
    }
#endif

#ifdef ENABLE_DATA_CHANNEL
    pDemoConfiguration->onDataChannel = onDataChannel;
#endif

CleanUp:
    return retStatus;
}

static STATUS setUpDemoDefaults(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    pDemoConfiguration->appMediaCtx.mediaSenderTid = INVALID_TID_VALUE;
    pDemoConfiguration->appMediaCtx.audioSenderTid = INVALID_TID_VALUE;
    pDemoConfiguration->appMediaCtx.videoSenderTid = INVALID_TID_VALUE;
    pDemoConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
    pDemoConfiguration->cvar = CVAR_CREATE();
    pDemoConfiguration->streamingSessionListReadLock = MUTEX_CREATE(FALSE);
    pDemoConfiguration->appSignalingCtx.signalingSendMessageLock = MUTEX_CREATE(FALSE);
    ATOMIC_STORE_BOOL(&pDemoConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pDemoConfiguration->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pDemoConfiguration->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pDemoConfiguration->appSignalingCtx.recreateSignalingClient, FALSE);
    ATOMIC_STORE_BOOL(&pDemoConfiguration->connected, FALSE);
    CHK_STATUS(stackQueueCreate(&pDemoConfiguration->pPendingSignalingMessageForRemoteClient));
    CHK_STATUS(doubleListCreate(&pDemoConfiguration->timerIdList));
    CHK_STATUS(timerQueueCreate(&pDemoConfiguration->timerQueueHandle));
    CHK_STATUS(hashTableCreateWithParams(SAMPLE_HASH_TABLE_BUCKET_COUNT, SAMPLE_HASH_TABLE_BUCKET_LENGTH,
                                         &pDemoConfiguration->pRtcPeerConnectionForRemoteClient));
    pDemoConfiguration->iceUriCount = 0;
CleanUp:
    return retStatus;
}

STATUS initializeConfiguration(PDemoConfiguration* ppDemoConfiguration, SIGNALING_CHANNEL_ROLE_TYPE roleType, INT32 argc, CHAR* argv[],
                               ParamsSetFn paramsSetFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDemoConfiguration pDemoConfiguration = NULL;

    CHK(ppDemoConfiguration != NULL, STATUS_NULL_ARG);

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

    SET_INSTRUMENTED_ALLOCATORS();
    CHK(NULL != (pDemoConfiguration = (PDemoConfiguration) MEMCALLOC(1, SIZEOF(DemoConfiguration))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(initKvsWebRtc());
    CHK_STATUS(setUpLogging(pDemoConfiguration));
    CHK_STATUS(setUpCredentialProvider(pDemoConfiguration));
    CHK_STATUS(setUpDemoDefaults(pDemoConfiguration));
    if (paramsSetFn == NULL) {
        pDemoConfiguration->configureAppFn = setUpDefaultsFn;
    } else {
        pDemoConfiguration->configureAppFn = paramsSetFn;
    }
    CHK_STATUS(pDemoConfiguration->configureAppFn((UINT64) pDemoConfiguration, roleType, argc, argv));
    CHK_STATUS(setUpSignalingDefaults(pDemoConfiguration));
CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeDemoConfiguration(&pDemoConfiguration);
    }
    if (ppDemoConfiguration != NULL) {
        *ppDemoConfiguration = pDemoConfiguration;
    }
    return retStatus;
}

STATUS initializeMediaSenders(PDemoConfiguration pDemoConfiguration, startRoutine audioSource, startRoutine videoSource)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    CHK(pDemoConfiguration != NULL && (audioSource != NULL && videoSource != NULL), STATUS_NULL_ARG);
    pDemoConfiguration->appMediaCtx.audioSource = audioSource;
    pDemoConfiguration->appMediaCtx.videoSource = videoSource;
    if (videoSource != NULL && audioSource == NULL) {
        pDemoConfiguration->appMediaCtx.mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    } else {
        pDemoConfiguration->appMediaCtx.mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    }
CleanUp:
    return retStatus;
}

STATUS initializeMediaReceivers(PDemoConfiguration pDemoConfiguration, startRoutine receiveAudioVideoSource)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pDemoConfiguration != NULL && receiveAudioVideoSource != NULL, STATUS_NULL_ARG);
    pDemoConfiguration->appMediaCtx.receiveAudioVideoSource = receiveAudioVideoSource;
CleanUp:
    return retStatus;
}

STATUS addTaskToTimerQueue(PDemoConfiguration pDemoConfiguration, PTimerTaskConfiguration pTimerTaskConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pDemoConfiguration != NULL && pTimerTaskConfiguration != NULL, STATUS_NULL_ARG);
    CHK_LOG_ERR(timerQueueAddTimer(pDemoConfiguration->timerQueueHandle, pTimerTaskConfiguration->startTime, pTimerTaskConfiguration->iterationTime,
                                   pTimerTaskConfiguration->timerCallbackFunc, pTimerTaskConfiguration->customData,
                                   &pTimerTaskConfiguration->timerId));
    if (pTimerTaskConfiguration->timerId != MAX_UINT32) {
        CHK_STATUS(doubleListInsertItemHead(pDemoConfiguration->timerIdList, (UINT64) pTimerTaskConfiguration->timerId));
    }
CleanUp:
    return retStatus;
}

STATUS enablePregenerateCertificate(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    TimerTaskConfiguration pregencertconfig;
    pregencertconfig.startTime = 0;
    pregencertconfig.iterationTime = SAMPLE_PRE_GENERATE_CERT_PERIOD;
    pregencertconfig.timerCallbackFunc = pregenerateCertTimerCallback;
    pregencertconfig.customData = (UINT64) pDemoConfiguration;
    CHK_STATUS(addTaskToTimerQueue(pDemoConfiguration, &pregencertconfig));
    CHK_STATUS(stackQueueCreate(&pDemoConfiguration->pregeneratedCertificates));
CleanUp:
    return retStatus;
}

STATUS signalingCallFailed(STATUS status)
{
    return (STATUS_SIGNALING_GET_TOKEN_CALL_FAILED == status || STATUS_SIGNALING_DESCRIBE_CALL_FAILED == status ||
            STATUS_SIGNALING_CREATE_CALL_FAILED == status || STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED == status ||
            STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED == status || STATUS_SIGNALING_CONNECT_CALL_FAILED == status ||
            STATUS_SIGNALING_DESCRIBE_MEDIA_CALL_FAILED == status);
}

STATUS logSelectedIceCandidatesInformation(PSampleStreamingSession pSampleStreamingSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcStats rtcMetrics;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_LOCAL_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &rtcMetrics));
    DLOGD("Local Candidate IP Address: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.address);
    DLOGD("Local Candidate type: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.candidateType);
    DLOGD("Local Candidate port: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.port);
    DLOGD("Local Candidate priority: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.priority);
    DLOGD("Local Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.protocol);
    DLOGD("Local Candidate relay protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.relayProtocol);
    DLOGD("Local Candidate Ice server source: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.url);

    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_REMOTE_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &rtcMetrics));
    DLOGD("Remote Candidate IP Address: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.address);
    DLOGD("Remote Candidate type: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.candidateType);
    DLOGD("Remote Candidate port: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.port);
    DLOGD("Remote Candidate priority: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.priority);
    DLOGD("Remote Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.protocol);
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS handleAnswer(PDemoConfiguration pDemoConfiguration, PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    UNUSED_PARAM(pDemoConfiguration);
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &answerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &answerSessionDescriptionInit));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

PVOID mediaSenderRoutine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDemoConfiguration pDemoConfiguration = (PDemoConfiguration) customData;
    CHK(pDemoConfiguration != NULL, STATUS_NULL_ARG);
    pDemoConfiguration->appMediaCtx.videoSenderTid = INVALID_TID_VALUE;
    pDemoConfiguration->appMediaCtx.audioSenderTid = INVALID_TID_VALUE;

    MUTEX_LOCK(pDemoConfiguration->sampleConfigurationObjLock);
    while (!ATOMIC_LOAD_BOOL(&pDemoConfiguration->connected) && !ATOMIC_LOAD_BOOL(&pDemoConfiguration->appTerminateFlag)) {
        CVAR_WAIT(pDemoConfiguration->cvar, pDemoConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    MUTEX_UNLOCK(pDemoConfiguration->sampleConfigurationObjLock);

    CHK(!ATOMIC_LOAD_BOOL(&pDemoConfiguration->appTerminateFlag), retStatus);

    if (pDemoConfiguration->appMediaCtx.videoSource != NULL) {
        THREAD_CREATE(&pDemoConfiguration->appMediaCtx.videoSenderTid, pDemoConfiguration->appMediaCtx.videoSource, (PVOID) pDemoConfiguration);
    }

    if (pDemoConfiguration->appMediaCtx.audioSource != NULL) {
        THREAD_CREATE(&pDemoConfiguration->appMediaCtx.audioSenderTid, pDemoConfiguration->appMediaCtx.audioSource, (PVOID) pDemoConfiguration);
    }

    if (pDemoConfiguration->appMediaCtx.videoSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pDemoConfiguration->appMediaCtx.videoSenderTid, NULL);
    }

    if (pDemoConfiguration->appMediaCtx.audioSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pDemoConfiguration->appMediaCtx.audioSenderTid, NULL);
    }

CleanUp:
    // clean the flag of the media thread.
    ATOMIC_STORE_BOOL(&pDemoConfiguration->mediaThreadStarted, FALSE);
    CHK_LOG_ERR(retStatus);
    return NULL;
}

STATUS handleOffer(PDemoConfiguration pDemoConfiguration, PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    NullableBool canTrickle;
    BOOL mediaThreadStarted;

    CHK(pDemoConfiguration != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    MEMSET(&pSampleStreamingSession->answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &offerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    canTrickle = canTrickleIceCandidates(pSampleStreamingSession->pPeerConnection);
    /* cannot be null after setRemoteDescription */
    CHECK(!NULLABLE_CHECK_EMPTY(canTrickle));
    pSampleStreamingSession->remoteCanTrickleIce = canTrickle.value;
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));

    /*
     * If remote support trickle ice, send answer now. Otherwise answer will be sent once ice candidate gathering is complete.
     */
    if (pSampleStreamingSession->remoteCanTrickleIce) {
        CHK_STATUS(createAnswer(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
        CHK_STATUS(respondWithAnswer(pSampleStreamingSession));
    }

    mediaThreadStarted = ATOMIC_EXCHANGE_BOOL(&pDemoConfiguration->mediaThreadStarted, TRUE);
    if (!mediaThreadStarted) {
        THREAD_CREATE(&pDemoConfiguration->appMediaCtx.mediaSenderTid, mediaSenderRoutine, (PVOID) pDemoConfiguration);
    }

    // The audio video receive routine should be per streaming session
    if (pDemoConfiguration->appMediaCtx.receiveAudioVideoSource != NULL) {
        THREAD_CREATE(&pSampleStreamingSession->receiveAudioVideoSenderTid, pDemoConfiguration->appMediaCtx.receiveAudioVideoSource,
                      (PVOID) pSampleStreamingSession);
    }
CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS sendSignalingMessage(PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PDemoConfiguration pDemoConfiguration;
    // Validate the input params
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pDemoConfiguration != NULL && pMessage != NULL, STATUS_NULL_ARG);

    pDemoConfiguration = pSampleStreamingSession->pDemoConfiguration;

    CHK(IS_VALID_MUTEX_VALUE(pDemoConfiguration->appSignalingCtx.signalingSendMessageLock) &&
            IS_VALID_SIGNALING_CLIENT_HANDLE(pDemoConfiguration->appSignalingCtx.signalingClientHandle),
        STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pDemoConfiguration->appSignalingCtx.signalingSendMessageLock);
    locked = TRUE;
    CHK_STATUS(signalingClientSendMessageSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle, pMessage));
    if (pMessage->messageType == SIGNALING_MESSAGE_TYPE_ANSWER) {
        CHK_STATUS(signalingClientGetMetrics(pDemoConfiguration->appSignalingCtx.signalingClientHandle,
                                             &pDemoConfiguration->appSignalingCtx.signalingClientMetrics));
        DLOGP("[Signaling offer received to answer sent time] %" PRIu64 " ms",
              pDemoConfiguration->appSignalingCtx.signalingClientMetrics.signalingClientStats.offerToAnswerTime);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleStreamingSession->pDemoConfiguration->appSignalingCtx.signalingSendMessageLock);
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS respondWithAnswer(PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingMessage message;
    UINT32 buffLen = MAX_SIGNALING_MESSAGE_LEN;

    CHK_STATUS(serializeSessionDescriptionInit(&pSampleStreamingSession->answerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRNCPY(message.peerClientId, pSampleStreamingSession->peerId, MAX_SIGNALING_CLIENT_ID_LEN);
    message.payloadLen = (UINT32) STRLEN(message.payload);
    // SNPRINTF appends null terminator, so we do not manually add it
    SNPRINTF(message.correlationId, MAX_CORRELATION_ID_LEN, "%llu_%llu", GETTIME(), ATOMIC_INCREMENT(&pSampleStreamingSession->correlationIdPostFix));
    DLOGD("Responding With Answer With correlationId: %s", message.correlationId);
    CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
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

STATUS initializePeerConnection(PDemoConfiguration pDemoConfiguration, PRtcPeerConnection* ppRtcPeerConnection)
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

    CHK(pDemoConfiguration != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set this to custom callback to enable filtering of interfaces
    configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = NULL;

    // Set the ICE mode explicitly
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    // Set the  STUN server
    PCHAR pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pDemoConfiguration->appSignalingCtx.channelInfo.pRegion, "cn-")) {
        pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL,
             pDemoConfiguration->appSignalingCtx.channelInfo.pRegion, pKinesisVideoStunUrlPostFix);

    // Check if we have any pregenerated certs and use them
    // NOTE: We are running under the config lock. This is also NULL if the enablePregenerateCertificate is not invoked
    if (pDemoConfiguration->pregeneratedCertificates != NULL) {
        retStatus = stackQueueDequeue(pDemoConfiguration->pregeneratedCertificates, &data);
        CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_NOT_FOUND, retStatus);

        if (retStatus == STATUS_NOT_FOUND) {
            retStatus = STATUS_SUCCESS;
        } else {
            // Use the pre-generated cert and get rid of it to not reuse again
            pRtcCertificate = (PRtcCertificate) data;
            configuration.certificates[0] = *pRtcCertificate;
        }
    }

    CHK_STATUS(createPeerConnection(&configuration, ppRtcPeerConnection));

    if (pDemoConfiguration->appConfigCtx.useTurn) {
#ifdef ENABLE_KVS_THREADPOOL
        pDemoConfiguration->iceUriCount = 1;
        AsyncGetIceStruct* pAsyncData = NULL;

        pAsyncData = (AsyncGetIceStruct*) MEMCALLOC(1, SIZEOF(AsyncGetIceStruct));
        pAsyncData->signalingClientHandle = pDemoConfiguration->appSignalingCtx.signalingClientHandle;
        pAsyncData->pRtcPeerConnection = *ppRtcPeerConnection;
        pAsyncData->pUriCount = &(pDemoConfiguration->iceUriCount);
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
            CHK_STATUS(signalingClientGetIceConfigInfo(pDemoConfiguration->signalingClientHandle, i, &pIceConfigInfo));
            CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
            uriCount += pIceConfigInfo->uriCount;
            CHK_STATUS(addConfigToServerList(ppRtcPeerConnection, pIceConfigInfo));
        }
        pDemoConfiguration->iceUriCount = uriCount + 1;
#endif
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Free the certificate which can be NULL as we no longer need it and won't reuse
    freeRtcCertificate(pRtcCertificate);

    LEAVES();
    return retStatus;
}

// Return ICE server stats for a specific streaming session
STATUS gatherIceServerStats(PSampleStreamingSession pSampleStreamingSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcStats rtcmetrics;
    UINT32 j = 0;
    rtcmetrics.requestedTypeOfStats = RTC_STATS_TYPE_ICE_SERVER;
    for (; j < pSampleStreamingSession->pDemoConfiguration->iceUriCount; j++) {
        rtcmetrics.rtcStatsObject.iceServerStats.iceServerIndex = j;
        CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &rtcmetrics));
        DLOGD("ICE Server URL: %s", rtcmetrics.rtcStatsObject.iceServerStats.url);
        DLOGD("ICE Server port: %d", rtcmetrics.rtcStatsObject.iceServerStats.port);
        DLOGD("ICE Server protocol: %s", rtcmetrics.rtcStatsObject.iceServerStats.protocol);
        DLOGD("Total requests sent:%" PRIu64, rtcmetrics.rtcStatsObject.iceServerStats.totalRequestsSent);
        DLOGD("Total responses received: %" PRIu64, rtcmetrics.rtcStatsObject.iceServerStats.totalResponsesReceived);
        DLOGD("Total round trip time: %" PRIu64 "ms",
              rtcmetrics.rtcStatsObject.iceServerStats.totalRoundTripTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS handleRemoteCandidate(PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceCandidateInit iceCandidate;
    CHK(pSampleStreamingSession != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);

    CHK_STATUS(deserializeRtcIceCandidateInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &iceCandidate));
    CHK_STATUS(addIceCandidate(pSampleStreamingSession->pPeerConnection, iceCandidate.candidate));

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS traverseDirectoryPEMFileScan(UINT64 customData, DIR_ENTRY_TYPES entryType, PCHAR fullPath, PCHAR fileName)
{
    UNUSED_PARAM(entryType);
    UNUSED_PARAM(fullPath);

    PCHAR certName = (PCHAR) customData;
    UINT32 fileNameLen = STRLEN(fileName);

    if (fileNameLen > ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1 &&
        (STRCMPI(CA_CERT_PEM_FILE_EXTENSION, &fileName[fileNameLen - ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1]) == 0)) {
        certName[0] = FPATHSEPARATOR;
        certName++;
        STRCPY(certName, fileName);
    }

    return STATUS_SUCCESS;
}

STATUS lookForSslCert(PDemoConfiguration* ppDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct stat pathStat;
    CHAR certName[MAX_PATH_LEN];
    PDemoConfiguration pDemoConfiguration = *ppDemoConfiguration;

    MEMSET(certName, 0x0, ARRAY_SIZE(certName));
    pDemoConfiguration->pCaCertPath = GETENV(CACERT_PATH_ENV_VAR);

    // if ca cert path is not set from the environment, try to use the one that cmake detected
    if (pDemoConfiguration->pCaCertPath == NULL) {
        CHK_ERR(STRNLEN(DEFAULT_KVS_CACERT_PATH, MAX_PATH_LEN) > 0, STATUS_INVALID_OPERATION, "No ca cert path given (error:%s)", strerror(errno));
        pDemoConfiguration->pCaCertPath = DEFAULT_KVS_CACERT_PATH;
    } else {
        // Check if the environment variable is a path
        CHK(0 == FSTAT(pDemoConfiguration->pCaCertPath, &pathStat), STATUS_DIRECTORY_ENTRY_STAT_ERROR);

        if (S_ISDIR(pathStat.st_mode)) {
            CHK_STATUS(traverseDirectory(pDemoConfiguration->pCaCertPath, (UINT64) &certName, /* iterate */ FALSE, traverseDirectoryPEMFileScan));

            if (certName[0] != 0x0) {
                STRCAT(pDemoConfiguration->pCaCertPath, certName);
            } else {
                DLOGW("Cert not found in path set...checking if CMake detected a path\n");
                CHK_ERR(STRNLEN(DEFAULT_KVS_CACERT_PATH, MAX_PATH_LEN) > 0, STATUS_INVALID_OPERATION, "No ca cert path given (error:%s)",
                        strerror(errno));
                DLOGI("CMake detected cert path\n");
                pDemoConfiguration->pCaCertPath = DEFAULT_KVS_CACERT_PATH;
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS initSignaling(PDemoConfiguration pDemoConfiguration, PCHAR clientId)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientMetrics signalingClientMetrics = pDemoConfiguration->appSignalingCtx.signalingClientMetrics;
    pDemoConfiguration->appSignalingCtx.signalingClientCallbacks.messageReceivedFn = signalingMessageReceived;
    STRCPY(pDemoConfiguration->appSignalingCtx.clientInfo.clientId, clientId);
    CHK_STATUS(createSignalingClientSync(&pDemoConfiguration->appSignalingCtx.clientInfo, &pDemoConfiguration->appSignalingCtx.channelInfo,
                                         &pDemoConfiguration->appSignalingCtx.signalingClientCallbacks, pDemoConfiguration->pCredentialProvider,
                                         &pDemoConfiguration->appSignalingCtx.signalingClientHandle));

    // Enable the processing of the messages
    CHK_STATUS(signalingClientFetchSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));

    CHK_STATUS(signalingClientConnectSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));

    signalingClientGetMetrics(pDemoConfiguration->appSignalingCtx.signalingClientHandle, &signalingClientMetrics);

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
    pDemoConfiguration->appSignalingCtx.signalingClientMetrics = signalingClientMetrics;
    gDemoConfiguration = pDemoConfiguration;
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

static STATUS cancelTimerTasks(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 timerId;
    CHK_STATUS(doubleListGetHeadNode(pDemoConfiguration->timerIdList, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &timerId));
        pCurNode = pCurNode->pNext;
        CHK_STATUS(timerQueueCancelTimer(pDemoConfiguration->timerQueueHandle, timerId, (UINT64) pDemoConfiguration));
    }
    CHK_STATUS(doubleListClear(pDemoConfiguration->timerIdList, FALSE));
CleanUp:
    return retStatus;
}

static STATUS freeDemoCredentialProvider(PDemoConfiguration pDemoConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_LOG_ERR(freeIotCredentialProvider(&pDemoConfiguration->pCredentialProvider));
#else
    CHK_LOG_ERR(freeStaticCredentialProvider(&pDemoConfiguration->pCredentialProvider));
#endif
CleanUp:
    return retStatus;
}

STATUS freeDemoConfiguration(PDemoConfiguration* ppDemoConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDemoConfiguration pDemoConfiguration;
    UINT32 i;
    UINT64 data;
    StackQueueIterator iterator;
    BOOL locked = FALSE;
    SignalingClientMetrics signalingClientMetrics;

    CHK(ppDemoConfiguration != NULL, STATUS_NULL_ARG);
    pDemoConfiguration = *ppDemoConfiguration;

    CHK(pDemoConfiguration != NULL, retStatus);

    // Kick of the termination sequence
    ATOMIC_STORE_BOOL(&pDemoConfiguration->appTerminateFlag, TRUE);

    if (pDemoConfiguration->appMediaCtx.mediaSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pDemoConfiguration->appMediaCtx.mediaSenderTid, NULL);
    }

    CHK_LOG_ERR(signalingClientGetMetrics(pDemoConfiguration->appSignalingCtx.signalingClientHandle, &signalingClientMetrics));
    CHK_LOG_ERR(logSignalingClientStats(&signalingClientMetrics));
    CHK_LOG_ERR(freeSignalingClient(&pDemoConfiguration->appSignalingCtx.signalingClientHandle));

    if (IS_VALID_TIMER_QUEUE_HANDLE(pDemoConfiguration->timerQueueHandle)) {
        cancelTimerTasks(pDemoConfiguration);
        timerQueueFree(&pDemoConfiguration->timerQueueHandle);
    }

    if (pDemoConfiguration->pPendingSignalingMessageForRemoteClient != NULL) {
        // Iterate and free all the pending queues
        stackQueueGetIterator(pDemoConfiguration->pPendingSignalingMessageForRemoteClient, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeMessageQueue((PPendingMessageQueue) data);
        }

        stackQueueClear(pDemoConfiguration->pPendingSignalingMessageForRemoteClient, FALSE);
        stackQueueFree(pDemoConfiguration->pPendingSignalingMessageForRemoteClient);
        pDemoConfiguration->pPendingSignalingMessageForRemoteClient = NULL;
    }

    hashTableClear(pDemoConfiguration->pRtcPeerConnectionForRemoteClient);
    hashTableFree(pDemoConfiguration->pRtcPeerConnectionForRemoteClient);

    if (IS_VALID_MUTEX_VALUE(pDemoConfiguration->sampleConfigurationObjLock)) {
        MUTEX_LOCK(pDemoConfiguration->sampleConfigurationObjLock);
        locked = TRUE;
    }

    for (i = 0; i < pDemoConfiguration->streamingSessionCount; ++i) {
        retStatus = gatherIceServerStats(pDemoConfiguration->sampleStreamingSessionList[i]);
        if (STATUS_FAILED(retStatus)) {
            DLOGW("Failed to ICE Server Stats for streaming session %d: %08x", i, retStatus);
        }
        freeSampleStreamingSession(&pDemoConfiguration->sampleStreamingSessionList[i]);
    }
    if (locked) {
        MUTEX_UNLOCK(pDemoConfiguration->sampleConfigurationObjLock);
    }
    deinitKvsWebRtc();

    SAFE_MEMFREE(pDemoConfiguration->appMediaCtx.pVideoFrameBuffer);
    SAFE_MEMFREE(pDemoConfiguration->appMediaCtx.pAudioFrameBuffer);

    if (IS_VALID_CVAR_VALUE(pDemoConfiguration->cvar) && IS_VALID_MUTEX_VALUE(pDemoConfiguration->sampleConfigurationObjLock)) {
        CVAR_BROADCAST(pDemoConfiguration->cvar);
        MUTEX_LOCK(pDemoConfiguration->sampleConfigurationObjLock);
        MUTEX_UNLOCK(pDemoConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pDemoConfiguration->sampleConfigurationObjLock)) {
        MUTEX_FREE(pDemoConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pDemoConfiguration->streamingSessionListReadLock)) {
        MUTEX_FREE(pDemoConfiguration->streamingSessionListReadLock);
    }

    if (IS_VALID_MUTEX_VALUE(pDemoConfiguration->appSignalingCtx.signalingSendMessageLock)) {
        MUTEX_FREE(pDemoConfiguration->appSignalingCtx.signalingSendMessageLock);
    }

    if (IS_VALID_CVAR_VALUE(pDemoConfiguration->cvar)) {
        CVAR_FREE(pDemoConfiguration->cvar);
    }

    CHK_LOG_ERR(freeDemoCredentialProvider(pDemoConfiguration));

    if (pDemoConfiguration->pregeneratedCertificates != NULL) {
        stackQueueGetIterator(pDemoConfiguration->pregeneratedCertificates, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeRtcCertificate((PRtcCertificate) data);
        }

        CHK_LOG_ERR(stackQueueClear(pDemoConfiguration->pregeneratedCertificates, FALSE));
        CHK_LOG_ERR(stackQueueFree(pDemoConfiguration->pregeneratedCertificates));
        pDemoConfiguration->pregeneratedCertificates = NULL;
    }
    if (pDemoConfiguration->enableFileLogging) {
        freeFileLogger();
    }
    SAFE_MEMFREE(*ppDemoConfiguration);

    retStatus = RESET_INSTRUMENTED_ALLOCATORS();
    DLOGI("All SDK allocations freed? %s..0x%08x", retStatus == STATUS_SUCCESS ? "Yes" : "No", retStatus);
CleanUp:

    LEAVES();
    return retStatus;
}

STATUS sessionCleanupWait(PDemoConfiguration pDemoConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    UINT32 i, clientIdHash;
    BOOL sampleConfigurationObjLockLocked = FALSE, streamingSessionListReadLockLocked = FALSE, peerConnectionFound = FALSE, sessionFreed = FALSE;
    SIGNALING_CLIENT_STATE signalingClientState;

    CHK(pDemoConfiguration != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pDemoConfiguration->interrupted)) {
        // Keep the main set of operations interlocked until cvar wait which would atomically unlock
        MUTEX_LOCK(pDemoConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = TRUE;

        // scan and cleanup terminated streaming session
        for (i = 0; i < pDemoConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pDemoConfiguration->sampleStreamingSessionList[i]->terminateFlag)) {
                pSampleStreamingSession = pDemoConfiguration->sampleStreamingSessionList[i];

                MUTEX_LOCK(pDemoConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = TRUE;

                // swap with last element and decrement count
                pDemoConfiguration->streamingSessionCount--;
                pDemoConfiguration->sampleStreamingSessionList[i] =
                    pDemoConfiguration->sampleStreamingSessionList[pDemoConfiguration->streamingSessionCount];

                // Remove from the hash table
                clientIdHash = COMPUTE_CRC32((PBYTE) pSampleStreamingSession->peerId, (UINT32) STRLEN(pSampleStreamingSession->peerId));
                CHK_STATUS(hashTableContains(pDemoConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));
                if (peerConnectionFound) {
                    CHK_STATUS(hashTableRemove(pDemoConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash));
                }

                MUTEX_UNLOCK(pDemoConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = FALSE;

                CHK_STATUS(freeSampleStreamingSession(&pSampleStreamingSession));
                sessionFreed = TRUE;
            }
        }

        if (sessionFreed && pDemoConfiguration->appSignalingCtx.channelInfo.useMediaStorage &&
            !ATOMIC_LOAD_BOOL(&pDemoConfiguration->appSignalingCtx.recreateSignalingClient)) {
            // In the WebRTC Media Storage Ingestion Case the backend will terminate the session after
            // 1 hour.  The SDK needs to make a new JoinSession Call in order to receive a new
            // offer from the backend.  We will create a new sample streaming session upon receipt of the
            // offer.  The signalingClientConnectSync call will result in a JoinSession API call being made.
            CHK_STATUS(signalingClientDisconnectSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));
            CHK_STATUS(signalingClientFetchSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));
            CHK_STATUS(signalingClientConnectSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));
            sessionFreed = FALSE;
        }

        // Check if we need to re-create the signaling client on-the-fly
        if (ATOMIC_LOAD_BOOL(&pDemoConfiguration->appSignalingCtx.recreateSignalingClient)) {
            retStatus = signalingClientFetchSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle);
            if (STATUS_SUCCEEDED(retStatus)) {
                // Re-set the variable again
                ATOMIC_STORE_BOOL(&pDemoConfiguration->appSignalingCtx.recreateSignalingClient, FALSE);
            } else if (signalingCallFailed(retStatus)) {
                printf("[KVS Common] recreating Signaling Client\n");
                freeSignalingClient(&pDemoConfiguration->appSignalingCtx.signalingClientHandle);
                createSignalingClientSync(&pDemoConfiguration->appSignalingCtx.clientInfo, &pDemoConfiguration->appSignalingCtx.channelInfo,
                                          &pDemoConfiguration->appSignalingCtx.signalingClientCallbacks, pDemoConfiguration->pCredentialProvider,
                                          &pDemoConfiguration->appSignalingCtx.signalingClientHandle);
            }
        }

        // Check the signaling client state and connect if needed
        if (IS_VALID_SIGNALING_CLIENT_HANDLE(pDemoConfiguration->appSignalingCtx.signalingClientHandle)) {
            CHK_STATUS(signalingClientGetCurrentState(pDemoConfiguration->appSignalingCtx.signalingClientHandle, &signalingClientState));
            if (signalingClientState == SIGNALING_CLIENT_STATE_READY) {
                UNUSED_PARAM(signalingClientConnectSync(pDemoConfiguration->appSignalingCtx.signalingClientHandle));
            }
        }

        // Check if any lingering pending message queues
        CHK_STATUS(removeExpiredMessageQueues(pDemoConfiguration->pPendingSignalingMessageForRemoteClient));

        // periodically wake up and clean up terminated streaming session
        CVAR_WAIT(pDemoConfiguration->cvar, pDemoConfiguration->sampleConfigurationObjLock, SAMPLE_SESSION_CLEANUP_WAIT_PERIOD);
        MUTEX_UNLOCK(pDemoConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = FALSE;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (sampleConfigurationObjLockLocked) {
        MUTEX_UNLOCK(pDemoConfiguration->sampleConfigurationObjLock);
    }

    if (streamingSessionListReadLockLocked) {
        MUTEX_UNLOCK(pDemoConfiguration->streamingSessionListReadLock);
    }

    LEAVES();
    return retStatus;
}

STATUS submitPendingIceCandidate(PPendingMessageQueue pPendingMessageQueue, PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL noPendingSignalingMessageForClient = FALSE;
    PReceivedSignalingMessage pReceivedSignalingMessage = NULL;
    UINT64 hashValue;

    CHK(pPendingMessageQueue != NULL && pPendingMessageQueue->messageQueue != NULL && pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    do {
        CHK_STATUS(stackQueueIsEmpty(pPendingMessageQueue->messageQueue, &noPendingSignalingMessageForClient));
        if (!noPendingSignalingMessageForClient) {
            hashValue = 0;
            CHK_STATUS(stackQueueDequeue(pPendingMessageQueue->messageQueue, &hashValue));
            pReceivedSignalingMessage = (PReceivedSignalingMessage) hashValue;
            CHK(pReceivedSignalingMessage != NULL, STATUS_INTERNAL_ERROR);
            if (pReceivedSignalingMessage->signalingMessage.messageType == SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE) {
                CHK_STATUS(handleRemoteCandidate(pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
            }
            SAFE_MEMFREE(pReceivedSignalingMessage);
        }
    } while (!noPendingSignalingMessageForClient);

    CHK_STATUS(freeMessageQueue(pPendingMessageQueue));

CleanUp:

    SAFE_MEMFREE(pReceivedSignalingMessage);
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS createMessageQueue(UINT64 hashValue, PPendingMessageQueue* ppPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;

    CHK(ppPendingMessageQueue != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pPendingMessageQueue = (PPendingMessageQueue) MEMCALLOC(1, SIZEOF(PendingMessageQueue))), STATUS_NOT_ENOUGH_MEMORY);
    pPendingMessageQueue->hashValue = hashValue;
    pPendingMessageQueue->createTime = GETTIME();
    CHK_STATUS(stackQueueCreate(&pPendingMessageQueue->messageQueue));

CleanUp:

    if (STATUS_FAILED(retStatus) && pPendingMessageQueue != NULL) {
        freeMessageQueue(pPendingMessageQueue);
        pPendingMessageQueue = NULL;
    }

    if (ppPendingMessageQueue != NULL) {
        *ppPendingMessageQueue = pPendingMessageQueue;
    }

    return retStatus;
}

STATUS freeMessageQueue(PPendingMessageQueue pPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;

    // free is idempotent
    CHK(pPendingMessageQueue != NULL, retStatus);

    if (pPendingMessageQueue->messageQueue != NULL) {
        stackQueueClear(pPendingMessageQueue->messageQueue, TRUE);
        stackQueueFree(pPendingMessageQueue->messageQueue);
    }

    MEMFREE(pPendingMessageQueue);

CleanUp:
    return retStatus;
}

STATUS getPendingMessageQueueForHash(PStackQueue pPendingQueue, UINT64 clientHash, BOOL remove, PPendingMessageQueue* ppPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    StackQueueIterator iterator;
    BOOL iterate = TRUE;
    UINT64 data;

    CHK(pPendingQueue != NULL && ppPendingMessageQueue != NULL, STATUS_NULL_ARG);

    CHK_STATUS(stackQueueGetIterator(pPendingQueue, &iterator));
    while (iterate && IS_VALID_ITERATOR(iterator)) {
        CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));
        CHK_STATUS(stackQueueIteratorNext(&iterator));

        pPendingMessageQueue = (PPendingMessageQueue) data;

        if (clientHash == pPendingMessageQueue->hashValue) {
            *ppPendingMessageQueue = pPendingMessageQueue;
            iterate = FALSE;

            // Check if the item needs to be removed
            if (remove) {
                // This is OK to do as we are terminating the iterator anyway
                CHK_STATUS(stackQueueRemoveItem(pPendingQueue, data));
            }
        }
    }

CleanUp:

    return retStatus;
}

STATUS removeExpiredMessageQueues(PStackQueue pPendingQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    UINT32 i, count;
    UINT64 data, curTime;

    CHK(pPendingQueue != NULL, STATUS_NULL_ARG);

    curTime = GETTIME();
    CHK_STATUS(stackQueueGetCount(pPendingQueue, &count));

    // Dequeue and enqueue in order to not break the iterator while removing an item
    for (i = 0; i < count; i++) {
        CHK_STATUS(stackQueueDequeue(pPendingQueue, &data));

        // Check for expiry
        pPendingMessageQueue = (PPendingMessageQueue) data;
        if (pPendingMessageQueue->createTime + SAMPLE_PENDING_MESSAGE_CLEANUP_DURATION < curTime) {
            // Message queue has expired and needs to be freed
            CHK_STATUS(freeMessageQueue(pPendingMessageQueue));
        } else {
            // Enqueue back again as it's still valued
            CHK_STATUS(stackQueueEnqueue(pPendingQueue, data));
        }
    }

CleanUp:

    return retStatus;
}
