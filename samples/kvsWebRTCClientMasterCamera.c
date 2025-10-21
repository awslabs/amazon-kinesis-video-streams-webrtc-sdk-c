#include "Samples.h"
#include "CameraInput.h"

extern PSampleConfiguration gSampleConfiguration;

// カメラコンテキストのグローバル変数
static CameraContext gCameraContext = {0};

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    PCHAR pCameraDevice = DEFAULT_CAMERA_DEVICE;
    SignalingClientMetrics signalingClientMetrics;
    signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;
    RTC_CODEC audioCodec = RTC_CODEC_OPUS;
    RTC_CODEC videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    UINT32 videoWidth = 640;
    UINT32 videoHeight = 480;

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

    // コマンドライン引数の解析
    if (argc > 2) {
        pCameraDevice = argv[2];
    }
    if (argc > 3) {
        videoWidth = (UINT32) STRTOUL(argv[3], NULL, 10);
        if (videoWidth == 0) videoWidth = 640;
    }
    if (argc > 4) {
        videoHeight = (UINT32) STRTOUL(argv[4], NULL, 10);
        if (videoHeight == 0) videoHeight = 480;
    }

    DLOGI("[KVS Master Camera] Using camera device: %s, resolution: %dx%d", pCameraDevice, videoWidth, videoHeight);

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    // カメラを初期化
    CHK_STATUS(initializeCamera(&gCameraContext, pCameraDevice, videoWidth, videoHeight));
    
    // カメラの能力を確認
    CHK_STATUS(getCameraCapabilities(&gCameraContext));
    CHK_STATUS(listSupportedFormats(&gCameraContext));
    
    // H.264またはMJPEG対応チェック
    CameraFormat format = detectBestCameraFormat(&gCameraContext);
    CHK_ERR(format == CAMERA_FORMAT_H264 || format == CAMERA_FORMAT_MJPEG, STATUS_INVALID_OPERATION,
            "Camera does not support H.264 or MJPEG format. Please use a compatible camera.");
    
    if (format == CAMERA_FORMAT_H264) {
        DLOGI("[KVS Master Camera] Using direct H.264 format");
    } else if (format == CAMERA_FORMAT_MJPEG) {
        DLOGI("[KVS Master Camera] Using MJPEG format with H.264 extraction");
    }

    // 映像のみのハンドラーを設定（音声は無効化）
    pSampleConfiguration->audioSource = NULL;  // 音声無効
    pSampleConfiguration->videoSource = sendCameraVideoPackets;  // カメラ用の関数を使用
    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveVideoOnlyFrame;
    pSampleConfiguration->videoCodec = videoCodec;

    // RTPローリングバッファサイズを設定（映像のみ）
    pSampleConfiguration->videoRollingBufferDurationSec = 3;
    pSampleConfiguration->videoRollingBufferBitratebps = 1.4 * 1024 * 1024;

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    DLOGI("[KVS Master Camera] Finished setting handlers");

    // KVS WebRTCを初期化
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Master Camera] KVS WebRTC initialization completed successfully");

    // カメラストリーミングを開始
    CHK_STATUS(startCameraStreaming(&gCameraContext));
    DLOGI("[KVS Master Camera] Camera streaming started");

    PROFILE_CALL_WITH_START_END_T_OBJ(
        retStatus = initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID), pSampleConfiguration->signalingClientMetrics.signalingStartTime,
        pSampleConfiguration->signalingClientMetrics.signalingEndTime, pSampleConfiguration->signalingClientMetrics.signalingCallTime,
        "Initialize signaling client and connect to the signaling channel");

    DLOGI("[KVS Master Camera] Channel %s set up done ", pChannelName);

    // 終了まで待機
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[KVS Master Camera] Streaming session terminated");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Master Camera] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Master Camera] Cleaning up....");
    
    // カメラを停止・終了
    stopCameraStreaming(&gCameraContext);
    terminateCamera(&gCameraContext);

    if (pSampleConfiguration != NULL) {
        // 終了シーケンスを開始
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
        }

        retStatus = signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);
        if (retStatus == STATUS_SUCCESS) {
            logSignalingClientStats(&signalingClientMetrics);
        } else {
            DLOGE("[KVS Master Camera] signalingClientGetMetrics() operation returned status code: 0x%08x", retStatus);
        }
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Master Camera] freeSignalingClient(): operation returned status code: 0x%08x", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Master Camera] freeSampleConfiguration(): operation returned status code: 0x%08x", retStatus);
        }
    }
    DLOGI("[KVS Master Camera] Cleanup done");
    CHK_LOG_ERR(retStatus);

    RESET_INSTRUMENTED_ALLOCATORS();

    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

PVOID sendCameraVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    RtcEncoderStats encoderStats;
    Frame frame;
    PBYTE pFrameData = NULL;
    UINT32 frameSize = 0;
    STATUS status;
    UINT32 i;
    UINT64 startTime, lastFrameTime, elapsed;
    
    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master Camera] Streaming session is NULL");

    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    // エンコーダー統計情報を設定
    encoderStats.width = gCameraContext.width;
    encoderStats.height = gCameraContext.height;
    encoderStats.targetBitrate = 1000000; // 1Mbps

    DLOGI("[KVS Master Camera] Starting camera video streaming thread");

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag) && 
           !ATOMIC_LOAD_BOOL(&gCameraContext.terminateFlag)) {
        
        // カメラからH.264フレームを取得（MJPEG抽出も含む）
        retStatus = captureH264Frame(&gCameraContext, &pFrameData, &frameSize);
        if (STATUS_FAILED(retStatus)) {
            if (retStatus == STATUS_OPERATION_TIMED_OUT) {
                DLOGV("[KVS Master Camera] Frame capture timeout, continuing...");
                continue;
            } else if (retStatus == STATUS_NO_MORE_DATA_AVAILABLE) {
                DLOGV("[KVS Master Camera] No H.264 data in frame, continuing...");
                continue;
            } else {
                DLOGE("[KVS Master Camera] Failed to capture H.264 frame: 0x%08x", retStatus);
                break;
            }
        }

        // フレームサイズが0の場合はスキップ
        if (frameSize == 0 || pFrameData == NULL) {
            continue;
        }

        // フレームバッファを再割り当て（必要に応じて）
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            CHK_ERR(pSampleConfiguration->pVideoFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, 
                    "[KVS Master Camera] Failed to allocate video frame buffer");
            pSampleConfiguration->videoBufferSize = frameSize;
        }

        // H.264データをコピー（MJPEGの場合は既に抽出済み）
        MEMCPY(pSampleConfiguration->pVideoFrameBuffer, pFrameData, frameSize);
        
        // 注意: captureH264Frame()で取得したデータは内部バッファなので、releaseFrame()は不要

        // フレーム構造体を設定
        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;
        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        // 全てのストリーミングセッションにフレームを送信
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
            encoderStats.encodeTimeMsec = 4; // エンコード時間を更新
            updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &encoderStats);
            if (status != STATUS_SRTP_NOT_READY_YET && status != STATUS_SUCCESS) {
                DLOGV("writeFrame() failed with 0x%08x", status);
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        // フレームレート調整のためのスリープ
        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:
    DLOGI("[KVS Master Camera] Closing camera video thread");
    CHK_LOG_ERR(retStatus);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveVideoOnlyFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Master Camera] Streaming session is NULL");
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleVideoFrameHandler));
    // 音声は設定しない（映像のみ）

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;
    CHK_ERR(pSize != NULL, STATUS_NULL_ARG, "[KVS Master Camera] Invalid file size");
    size = *pSize;
    // サイズを取得してフレームに読み込み
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));
CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

PVOID sampleReceiveAudioVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Master Camera] Streaming session is NULL");
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleVideoFrameHandler));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleAudioFrameHandler));

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}