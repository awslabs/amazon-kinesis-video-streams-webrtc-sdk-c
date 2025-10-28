#include "Samples.h"
#include "CameraInput.h"
#include "V4L2LoopbackOutput.h"

// デュアルカメラ管理用の定数
#define MAX_DUAL_CAMERAS 2
#define MAX_CHANNELS_PER_CAMERA 4
#define DEFAULT_MAIN_BITRATE 500000   // 500kbps
#define DEFAULT_SUB_BITRATE 200000    // 200kbps
#define DEFAULT_MAIN_FPS 30
#define DEFAULT_SUB_FPS 15

// カメラ情報構造体
typedef struct {
    PCHAR devicePath;
    CameraContext cameraContext;
    BOOL isActive;
    UINT32 channelCount;
    PSampleConfiguration channels[MAX_CHANNELS_PER_CAMERA];
    PCHAR channelNames[MAX_CHANNELS_PER_CAMERA];
    UINT32 bitrates[MAX_CHANNELS_PER_CAMERA];
    UINT32 fpsList[MAX_CHANNELS_PER_CAMERA];
    UINT64 frameDurations[MAX_CHANNELS_PER_CAMERA];
    TID mediaSenderTid;
    V4L2LoopbackContext v4l2Output;  // V4L2 Loopback出力
    PCHAR v4l2OutputPath;            // V4L2 Loopback出力デバイスパス
} DualCameraContext, *PDualCameraContext;

// グローバル変数
static DualCameraContext gCameras[MAX_DUAL_CAMERAS];
static UINT32 gActiveCameraCount = 0;
static MUTEX gCameraMutex;
static volatile ATOMIC_BOOL gAppTerminateFlag = FALSE;

// 関数プロトタイプ
STATUS initializeDualCameras(PCHAR* devicePaths, PCHAR* v4l2OutputPaths, UINT32* widths, UINT32* heights, UINT32 cameraCount);
STATUS terminateDualCameras(VOID);
PVOID sendDualCameraVideoPackets(PVOID args);
STATUS addCameraChannel(UINT32 cameraIndex, PCHAR channelName, UINT32 targetBitrate, UINT32 targetFps);
PVOID sampleReceiveVideoOnlyFrameDual(PVOID args);

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR devicePaths[MAX_DUAL_CAMERAS] = {"/dev/video2", "/dev/video0"}; // 画像に合わせた順序
    PCHAR v4l2OutputPaths[MAX_DUAL_CAMERAS] = {"/dev/video5", "/dev/video4"}; // V4L2 Loopback出力
    UINT32 widths[MAX_DUAL_CAMERAS] = {640, 640};
    UINT32 heights[MAX_DUAL_CAMERAS] = {480, 480};
    UINT32 cameraCount = 2;
    UINT32 i, j;
    SignalingClientMetrics signalingClientMetrics;

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

    // ヘルプ表示
    if (argc > 1 && (STRCMP(argv[1], "-h") == 0 || STRCMP(argv[1], "--help") == 0)) {
        printf("Usage: %s [camera1_device] [camera2_device] [width1] [height1] [width2] [height2]\n", argv[0]);
        printf("\n");
        printf("Arguments:\n");
        printf("  camera1_device : First camera device path (default: /dev/video2)\n");
        printf("  camera2_device : Second camera device path (default: /dev/video0)\n");
        printf("  width1         : First camera width in pixels (default: 640)\n");
        printf("  height1        : First camera height in pixels (default: 480)\n");
        printf("  width2         : Second camera width in pixels (default: 640)\n");
        printf("  height2        : Second camera height in pixels (default: 480)\n");
        printf("\n");
        printf("V4L2 Loopback Output:\n");
        printf("  Camera1 → /dev/video5 (v4l2loopback)\n");
        printf("  Camera2 → /dev/video4 (v4l2loopback)\n");
        printf("\n");
        printf("Examples:\n");
        printf("  %s                                    # Use defaults\n", argv[0]);
        printf("  %s /dev/video2 /dev/video0 1280 720 640 480  # HD + SD\n", argv[0]);
        printf("\n");
        printf("Channels created:\n");
        printf("  Camera1_MainStream   : From first camera + V4L2 output (/dev/video5)\n");
        printf("  Camera2_MainStream   : From second camera + V4L2 output (/dev/video4)\n");
        printf("\n");
        return EXIT_SUCCESS;
    }

    // コマンドライン引数の解析
    if (argc > 1) {
        devicePaths[0] = argv[1];
    }
    if (argc > 2) {
        devicePaths[1] = argv[2];
    }
    if (argc > 3) {
        widths[0] = (UINT32) STRTOUL(argv[3], NULL, 10);
        if (widths[0] == 0) widths[0] = 640;
    }
    if (argc > 4) {
        heights[0] = (UINT32) STRTOUL(argv[4], NULL, 10);
        if (heights[0] == 0) heights[0] = 480;
    }
    if (argc > 5) {
        widths[1] = (UINT32) STRTOUL(argv[5], NULL, 10);
        if (widths[1] == 0) widths[1] = 640;
    }
    if (argc > 6) {
        heights[1] = (UINT32) STRTOUL(argv[6], NULL, 10);
        if (heights[1] == 0) heights[1] = 480;
    }

    DLOGI("[KVS Dual Camera] Configuring dual camera setup:");
    for (i = 0; i < cameraCount; i++) {
        DLOGI("[KVS Dual Camera] - Camera %d: %s (%dx%d) → V4L2 output: %s",
              i + 1, devicePaths[i], widths[i], heights[i], v4l2OutputPaths[i]);
    }

    // ミューテックスを初期化
    CHK_STATUS(MUTEX_CREATE(&gCameraMutex));

    // デュアルカメラを初期化
    CHK_STATUS(initializeDualCameras(devicePaths, v4l2OutputPaths, widths, heights, cameraCount));

    // 各カメラにメインチャンネルのみを追加
    for (i = 0; i < gActiveCameraCount; i++) {
        CHAR mainChannelName[64];
        
        SNPRINTF(mainChannelName, SIZEOF(mainChannelName), "Camera%d_MainStream", i + 1);
        
        CHK_STATUS(addCameraChannel(i, mainChannelName, DEFAULT_MAIN_BITRATE, DEFAULT_MAIN_FPS));
    }

    // KVS WebRTCを初期化
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Dual Camera] KVS WebRTC initialization completed successfully");

    // 各カメラのストリーミングを開始
    for (i = 0; i < gActiveCameraCount; i++) {
        CHK_STATUS(startCameraStreaming(&gCameras[i].cameraContext));
        DLOGI("[KVS Dual Camera] Camera %d streaming started", i + 1);
    }

    // 各チャンネルのシグナリングを初期化
    for (i = 0; i < gActiveCameraCount; i++) {
        for (j = 0; j < gCameras[i].channelCount; j++) {
            if (gCameras[i].channels[j] != NULL) {
                PROFILE_CALL_WITH_START_END_T_OBJ(
                    retStatus = initSignaling(gCameras[i].channels[j], SAMPLE_MASTER_CLIENT_ID), 
                    gCameras[i].channels[j]->signalingClientMetrics.signalingStartTime,
                    gCameras[i].channels[j]->signalingClientMetrics.signalingEndTime, 
                    gCameras[i].channels[j]->signalingClientMetrics.signalingCallTime,
                    "Initialize signaling client and connect to the signaling channel");
                
                if (STATUS_FAILED(retStatus)) {
                    DLOGE("[KVS Dual Camera] Failed to initialize signaling for channel %s: 0x%08x", 
                          gCameras[i].channelNames[j], retStatus);
                    continue;
                }
                
                DLOGI("[KVS Dual Camera] Channel %s set up done", gCameras[i].channelNames[j]);
            }
        }
    }

    // 終了まで待機
    DLOGI("[KVS Dual Camera] All channels initialized. Waiting for termination...");
    while (!ATOMIC_LOAD_BOOL(&gAppTerminateFlag)) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Dual Camera] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Dual Camera] Cleaning up....");
    
    // 終了フラグを設定
    ATOMIC_STORE_BOOL(&gAppTerminateFlag, TRUE);
    
    // デュアルカメラを終了
    terminateDualCameras();

    // ミューテックスを破棄
    MUTEX_FREE(gCameraMutex);

    DLOGI("[KVS Dual Camera] Cleanup done");
    CHK_LOG_ERR(retStatus);

    RESET_INSTRUMENTED_ALLOCATORS();

    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

STATUS initializeDualCameras(PCHAR* devicePaths, PCHAR* v4l2OutputPaths, UINT32* widths, UINT32* heights, UINT32 cameraCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK_ERR(devicePaths != NULL, STATUS_NULL_ARG, "Device paths array is NULL");
    CHK_ERR(v4l2OutputPaths != NULL, STATUS_NULL_ARG, "V4L2 output paths array is NULL");
    CHK_ERR(widths != NULL, STATUS_NULL_ARG, "Widths array is NULL");
    CHK_ERR(heights != NULL, STATUS_NULL_ARG, "Heights array is NULL");
    CHK_ERR(cameraCount > 0 && cameraCount <= MAX_DUAL_CAMERAS, STATUS_INVALID_ARG,
            "Invalid camera count: %u", cameraCount);

    // カメラ配列を初期化
    MEMSET(gCameras, 0, SIZEOF(gCameras));
    gActiveCameraCount = 0;

    for (i = 0; i < cameraCount; i++) {
        // デバイスパスをコピー
        UINT32 pathLen = STRLEN(devicePaths[i]);
        gCameras[i].devicePath = (PCHAR) MEMALLOC(pathLen + 1);
        CHK_ERR(gCameras[i].devicePath != NULL, STATUS_NOT_ENOUGH_MEMORY,
                "Failed to allocate device path for camera %u", i);
        STRCPY(gCameras[i].devicePath, devicePaths[i]);

        // V4L2出力パスをコピー
        UINT32 v4l2PathLen = STRLEN(v4l2OutputPaths[i]);
        gCameras[i].v4l2OutputPath = (PCHAR) MEMALLOC(v4l2PathLen + 1);
        CHK_ERR(gCameras[i].v4l2OutputPath != NULL, STATUS_NOT_ENOUGH_MEMORY,
                "Failed to allocate V4L2 output path for camera %u", i);
        STRCPY(gCameras[i].v4l2OutputPath, v4l2OutputPaths[i]);

        // カメラを初期化
        retStatus = initializeCamera(&gCameras[i].cameraContext, devicePaths[i], widths[i], heights[i]);
        if (STATUS_FAILED(retStatus)) {
            DLOGE("[KVS Dual Camera] Failed to initialize camera %d (%s): 0x%08x", 
                  i + 1, devicePaths[i], retStatus);
            gCameras[i].isActive = FALSE;
            continue;
        }
        
        // カメラの能力を確認
        CHK_STATUS(getCameraCapabilities(&gCameras[i].cameraContext));
        CHK_STATUS(listSupportedFormats(&gCameras[i].cameraContext));
        
        // H.264またはMJPEG対応チェック
        CameraFormat format = detectBestCameraFormat(&gCameras[i].cameraContext);
        if (format != CAMERA_FORMAT_H264 && format != CAMERA_FORMAT_MJPEG) {
            DLOGE("[KVS Dual Camera] Camera %d does not support H.264 or MJPEG format", i + 1);
            gCameras[i].isActive = FALSE;
            continue;
        }
        
        if (format == CAMERA_FORMAT_H264) {
            DLOGI("[KVS Dual Camera] Camera %d using direct H.264 format", i + 1);
        } else if (format == CAMERA_FORMAT_MJPEG) {
            DLOGI("[KVS Dual Camera] Camera %d using MJPEG format with H.264 extraction", i + 1);
        }

        // V4L2 Loopback出力を初期化
        retStatus = initializeV4L2LoopbackOutput(&gCameras[i].v4l2Output,
                                               v4l2OutputPaths[i], widths[i], heights[i]);
        if (STATUS_FAILED(retStatus)) {
            DLOGW("[KVS Dual Camera] Failed to initialize V4L2 loopback output for camera %d: 0x%08x",
                  i + 1, retStatus);
            // V4L2 Loopback失敗は致命的ではないので続行
        }

        gCameras[i].isActive = TRUE;
        gCameras[i].channelCount = 0;
        gCameras[i].mediaSenderTid = INVALID_TID_VALUE;
        
        DLOGI("[KVS Dual Camera] Camera %u (%s → %s) initialized successfully",
              i + 1, devicePaths[i], v4l2OutputPaths[i]);
    }

    // アクティブなカメラ数を設定
    for (i = 0; i < cameraCount; i++) {
        if (gCameras[i].isActive) {
            gActiveCameraCount++;
        }
    }

    CHK_ERR(gActiveCameraCount > 0, STATUS_INVALID_OPERATION, 
            "No cameras were successfully initialized");

    DLOGI("[KVS Dual Camera] Successfully initialized %u out of %u cameras", 
          gActiveCameraCount, cameraCount);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        terminateDualCameras();
    }

    return retStatus;
}

STATUS addCameraChannel(UINT32 cameraIndex, PCHAR channelName, UINT32 targetBitrate, UINT32 targetFps)
{
    STATUS retStatus = STATUS_SUCCESS;
    RTC_CODEC audioCodec = RTC_CODEC_OPUS;
    RTC_CODEC videoCodec = 1; // RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE
    UINT32 channelIndex;

    CHK_ERR(cameraIndex < MAX_DUAL_CAMERAS, STATUS_INVALID_ARG, "Invalid camera index: %u", cameraIndex);
    CHK_ERR(gCameras[cameraIndex].isActive, STATUS_INVALID_OPERATION, "Camera %u is not active", cameraIndex);
    CHK_ERR(gCameras[cameraIndex].channelCount < MAX_CHANNELS_PER_CAMERA, STATUS_INVALID_OPERATION, 
            "Camera %u has reached maximum channel count", cameraIndex);
    CHK_ERR(channelName != NULL, STATUS_NULL_ARG, "Channel name is NULL");

    channelIndex = gCameras[cameraIndex].channelCount;

    // チャンネル名をコピー
    UINT32 nameLen = STRLEN(channelName);
    gCameras[cameraIndex].channelNames[channelIndex] = (PCHAR) MEMALLOC(nameLen + 1);
    CHK_ERR(gCameras[cameraIndex].channelNames[channelIndex] != NULL, STATUS_NOT_ENOUGH_MEMORY, 
            "Failed to allocate channel name");
    STRCPY(gCameras[cameraIndex].channelNames[channelIndex], channelName);

    // 設定値を保存
    gCameras[cameraIndex].bitrates[channelIndex] = targetBitrate;
    gCameras[cameraIndex].fpsList[channelIndex] = targetFps;
    gCameras[cameraIndex].frameDurations[channelIndex] = HUNDREDS_OF_NANOS_IN_A_SECOND / targetFps;

    // サンプル設定を作成
    CHK_STATUS(createSampleConfiguration(channelName, 
                                       SIGNALING_CHANNEL_ROLE_TYPE_MASTER, 
                                       TRUE, TRUE, setLogLevel(), 
                                       &gCameras[cameraIndex].channels[channelIndex]));

    // 映像のみのハンドラーを設定
    gCameras[cameraIndex].channels[channelIndex]->audioSource = NULL;  // 音声無効
    gCameras[cameraIndex].channels[channelIndex]->videoSource = sendDualCameraVideoPackets;
    gCameras[cameraIndex].channels[channelIndex]->receiveAudioVideoSource = sampleReceiveVideoOnlyFrameDual;
    gCameras[cameraIndex].channels[channelIndex]->videoCodec = videoCodec;
    gCameras[cameraIndex].channels[channelIndex]->audioCodec = audioCodec;

    // RTPローリングバッファサイズを設定
    gCameras[cameraIndex].channels[channelIndex]->videoRollingBufferDurationSec = 3;
    gCameras[cameraIndex].channels[channelIndex]->videoRollingBufferBitratebps = targetBitrate * 1.4;

#ifdef ENABLE_DATA_CHANNEL
    gCameras[cameraIndex].channels[channelIndex]->onDataChannel = onDataChannel;
#endif
    gCameras[cameraIndex].channels[channelIndex]->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    
    // カスタムデータとしてカメラインデックスとチャンネルインデックスを設定
    gCameras[cameraIndex].channels[channelIndex]->customData = (cameraIndex << 16) | channelIndex;

    gCameras[cameraIndex].channelCount++;

    DLOGI("[KVS Dual Camera] Camera %u channel %u (%s) added: bitrate=%u, fps=%u", 
          cameraIndex + 1, channelIndex, channelName, targetBitrate, targetFps);

CleanUp:
    return retStatus;
}

PVOID sendDualCameraVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT32 cameraIndex, channelIndex;
    PDualCameraContext pCamera = NULL;
    RtcEncoderStats encoderStats;
    Frame frame;
    PBYTE pFrameData = NULL;
    UINT32 frameSize = 0;
    STATUS status;
    UINT32 i;
    UINT64 startTime, lastFrameTime, elapsed;
    UINT64 frameDuration;
    
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "Sample configuration is NULL");
    
    // カスタムデータからカメラインデックスとチャンネルインデックスを取得
    cameraIndex = (UINT32) (pSampleConfiguration->customData >> 16);
    channelIndex = (UINT32) (pSampleConfiguration->customData & 0xFFFF);
    
    CHK_ERR(cameraIndex < MAX_DUAL_CAMERAS, STATUS_INVALID_ARG, "Invalid camera index: %u", cameraIndex);
    CHK_ERR(channelIndex < MAX_CHANNELS_PER_CAMERA, STATUS_INVALID_ARG, "Invalid channel index: %u", channelIndex);
    
    pCamera = &gCameras[cameraIndex];
    CHK_ERR(pCamera->isActive, STATUS_INVALID_OPERATION, "Camera %u is not active", cameraIndex);

    frameDuration = pCamera->frameDurations[channelIndex];

    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));
    
    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    // エンコーダー統計情報を設定
    encoderStats.width = pCamera->cameraContext.width;
    encoderStats.height = pCamera->cameraContext.height;
    encoderStats.targetBitrate = pCamera->bitrates[channelIndex];

    DLOGI("[KVS Dual Camera] Starting video streaming thread for camera %u channel %s", 
          cameraIndex + 1, pCamera->channelNames[channelIndex]);
    DLOGI("[KVS Dual Camera] Camera %u channel %s: resolution=%dx%d, bitrate=%u, fps=%u", 
          cameraIndex + 1, pCamera->channelNames[channelIndex], 
          pCamera->cameraContext.width, pCamera->cameraContext.height, 
          pCamera->bitrates[channelIndex], pCamera->fpsList[channelIndex]);

    UINT32 frameCount = 0;
    UINT64 lastLogTime = GETTIME();

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag) &&
           !ATOMIC_LOAD_BOOL(&pCamera->cameraContext.terminateFlag) &&
           !ATOMIC_LOAD_BOOL(&gAppTerminateFlag)) {
        
        // まず生のMJPEGフレームを取得（V4L2 Loopback用）
        PBYTE pRawFrameData = NULL;
        UINT32 rawFrameSize = 0;
        
        retStatus = captureFrame(&pCamera->cameraContext, &pRawFrameData, &rawFrameSize);
        if (STATUS_FAILED(retStatus)) {
            if (retStatus == STATUS_OPERATION_TIMED_OUT) {
                DLOGV("[KVS Dual Camera] Camera %u: Frame capture timeout", cameraIndex + 1);
                continue;
            } else {
                DLOGE("[KVS Dual Camera] Camera %u: Failed to capture raw frame: 0x%08x",
                      cameraIndex + 1, retStatus);
                break;
            }
        }

        // 生フレームサイズが0の場合はスキップ
        if (rawFrameSize == 0 || pRawFrameData == NULL) {
            DLOGV("[KVS Dual Camera] Camera %u: Empty raw frame received", cameraIndex + 1);
            if (pRawFrameData != NULL) {
                releaseFrame(&pCamera->cameraContext, pRawFrameData);
            }
            continue;
        }

        // V4L2 Loopbackに生のMJPEGフレームを出力（各カメラのメインストリーム）
        if (pCamera->v4l2Output.isActive) {
            STATUS v4l2Status = writeFrameToV4L2Loopback(&pCamera->v4l2Output, pRawFrameData, rawFrameSize);
            if (STATUS_FAILED(v4l2Status)) {
                DLOGV("[KVS Dual Camera] V4L2 loopback write failed: 0x%08x", v4l2Status);
            }
        }

        // WebRTC配信用にH.264を抽出
        if (pCamera->cameraContext.format == CAMERA_FORMAT_H264) {
            // 直接H.264の場合はそのまま使用
            pFrameData = pRawFrameData;
            frameSize = rawFrameSize;
        } else if (pCamera->cameraContext.format == CAMERA_FORMAT_MJPEG) {
            // MJPEGからH.264を抽出
            retStatus = extractH264FromMjpeg(&pCamera->cameraContext.h264Extractor,
                                           pRawFrameData, rawFrameSize,
                                           &pFrameData, &frameSize);
            
            if (STATUS_FAILED(retStatus) || pFrameData == NULL || frameSize == 0) {
                DLOGV("[KVS Dual Camera] Camera %u: H.264 extraction failed or no data", cameraIndex + 1);
                releaseFrame(&pCamera->cameraContext, pRawFrameData);
                continue;
            }
        } else {
            DLOGE("[KVS Dual Camera] Camera %u: Unsupported format", cameraIndex + 1);
            releaseFrame(&pCamera->cameraContext, pRawFrameData);
            break;
        }

        // H.264フレームサイズが0の場合はスキップ
        if (frameSize == 0 || pFrameData == NULL) {
            DLOGV("[KVS Dual Camera] Camera %u: Empty H.264 frame", cameraIndex + 1);
            releaseFrame(&pCamera->cameraContext, pRawFrameData);
            continue;
        }

        frameCount++;
        DLOGV("[KVS Dual Camera] Camera %u channel %s: Frame #%u captured (%u bytes)", 
              cameraIndex + 1, pCamera->channelNames[channelIndex], frameCount, frameSize);
        
        // 定期的にフレーム統計をログ出力
        UINT64 currentTime = GETTIME();
        if (currentTime - lastLogTime >= 10 * HUNDREDS_OF_NANOS_IN_A_SECOND) { // 10秒ごと
            DLOGI("[KVS Dual Camera] Camera %u channel %s: %u frames in last 10 seconds", 
                  cameraIndex + 1, pCamera->channelNames[channelIndex], frameCount);
            frameCount = 0;
            lastLogTime = currentTime;
        }

        // フレームバッファを再割り当て（必要に応じて）
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            CHK_ERR(pSampleConfiguration->pVideoFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, 
                    "Failed to allocate video frame buffer for camera %u", cameraIndex + 1);
            pSampleConfiguration->videoBufferSize = frameSize;
        }

        // H.264データをコピー
        MEMCPY(pSampleConfiguration->pVideoFrameBuffer, pFrameData, frameSize);
        
        // 生フレームを解放
        releaseFrame(&pCamera->cameraContext, pRawFrameData);
        
        // フレーム構造体を設定
        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;
        frame.presentationTs += frameDuration;

        // 全てのストリーミングセッションにフレームを送信
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, 
                                       "Time to first frame");
                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
            encoderStats.encodeTimeMsec = 4;
            updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &encoderStats);
            if (status != STATUS_SRTP_NOT_READY_YET && status != STATUS_SUCCESS) {
                DLOGV("[KVS Dual Camera] Camera %u channel %s: writeFrame() failed with 0x%08x", 
                      cameraIndex + 1, pCamera->channelNames[channelIndex], status);
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        // フレームレート調整のためのスリープ
        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(frameDuration - elapsed % frameDuration);
        lastFrameTime = GETTIME();
    }

CleanUp:
    DLOGI("[KVS Dual Camera] Closing video thread for camera %u channel %s", 
          cameraIndex + 1, pCamera ? pCamera->channelNames[channelIndex] : "unknown");
    CHK_LOG_ERR(retStatus);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveVideoOnlyFrameDual(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "Streaming session is NULL");
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, 
                                (UINT64) pSampleStreamingSession, sampleVideoFrameHandler));

CleanUp:
    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS terminateDualCameras(VOID)
{
    UINT32 i, j;
    SignalingClientMetrics signalingClientMetrics;
    STATUS retStatus;

    for (i = 0; i < MAX_DUAL_CAMERAS; i++) {
        if (gCameras[i].isActive) {
            // カメラストリーミングを停止
            stopCameraStreaming(&gCameras[i].cameraContext);
            terminateCamera(&gCameras[i].cameraContext);

            // 各チャンネルを終了
            for (j = 0; j < gCameras[i].channelCount; j++) {
                if (gCameras[i].channels[j] != NULL) {
                    // 終了シーケンスを開始
                    ATOMIC_STORE_BOOL(&gCameras[i].channels[j]->appTerminateFlag, TRUE);

                    // メディア送信スレッドの終了を待つ
                    if (gCameras[i].channels[j]->mediaSenderTid != INVALID_TID_VALUE) {
                        THREAD_JOIN(gCameras[i].channels[j]->mediaSenderTid, NULL);
                    }

                    // シグナリングクライアントの統計情報を取得
                    retStatus = signalingClientGetMetrics(gCameras[i].channels[j]->signalingClientHandle, 
                                                        &signalingClientMetrics);
                    if (retStatus == STATUS_SUCCESS) {
                        logSignalingClientStats(&signalingClientMetrics);
                    }

                    // シグナリングクライアントを解放
                    retStatus = freeSignalingClient(&gCameras[i].channels[j]->signalingClientHandle);
                    if (retStatus != STATUS_SUCCESS) {
                        DLOGE("[KVS Dual Camera] freeSignalingClient() for camera %u channel %s: 0x%08x", 
                              i + 1, gCameras[i].channelNames[j], retStatus);
                    }

                    // サンプル設定を解放
                    retStatus = freeSampleConfiguration(&gCameras[i].channels[j]);
                    if (retStatus != STATUS_SUCCESS) {
                        DLOGE("[KVS Dual Camera] freeSampleConfiguration() for camera %u channel %s: 0x%08x", 
                              i + 1, gCameras[i].channelNames[j], retStatus);
                    }
                }

                // チャンネル名を解放
                if (gCameras[i].channelNames[j] != NULL) {
                    MEMFREE(gCameras[i].channelNames[j]);
                    gCameras[i].channelNames[j] = NULL;
                }
            }

            // デバイスパスを解放
            if (gCameras[i].devicePath != NULL) {
                MEMFREE(gCameras[i].devicePath);
                gCameras[i].devicePath = NULL;
            }

            gCameras[i].isActive = FALSE;
            gCameras[i].channelCount = 0;
        }
    }

    gActiveCameraCount = 0;
    return STATUS_SUCCESS;
}