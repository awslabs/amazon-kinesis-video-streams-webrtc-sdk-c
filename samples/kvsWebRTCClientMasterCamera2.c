#include "Samples.h"
#include "CameraInput.h"

extern PSampleConfiguration gSampleConfiguration;

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR devicePath = "/dev/video2";  // デフォルトはvideo2
    UINT32 width = 640, height = 480;
    SignalingClientMetrics signalingClientMetrics;
    PCHAR channelName = "Camera2_MainStream";  // Camera2専用チャンネル名

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

    // ヘルプ表示
    if (argc > 1 && (STRCMP(argv[1], "-h") == 0 || STRCMP(argv[1], "--help") == 0)) {
        printf("Usage: %s [device_path] [width] [height]\n", argv[0]);
        printf("\n");
        printf("Arguments:\n");
        printf("  device_path : Camera device path (default: /dev/video2)\n");
        printf("  width       : Video width in pixels (default: 640)\n");
        printf("  height      : Video height in pixels (default: 480)\n");
        printf("\n");
        printf("Examples:\n");
        printf("  %s                           # Use defaults\n", argv[0]);
        printf("  %s /dev/video2 1280 720      # HD resolution\n", argv[0]);
        printf("\n");
        printf("Channel: %s\n", channelName);
        printf("\n");
        return EXIT_SUCCESS;
    }

    // コマンドライン引数の解析
    if (argc > 1) {
        devicePath = argv[1];
    }
    if (argc > 2) {
        width = (UINT32) STRTOUL(argv[2], NULL, 10);
        if (width == 0) width = 640;
    }
    if (argc > 3) {
        height = (UINT32) STRTOUL(argv[3], NULL, 10);
        if (height == 0) height = 480;
    }

    DLOGI("[KVS Camera2] Starting Camera2 WebRTC client");
    DLOGI("[KVS Camera2] Device: %s, Resolution: %dx%d, Channel: %s", 
          devicePath, width, height, channelName);

    // KVS WebRTCを初期化
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Camera2] KVS WebRTC initialization completed successfully");

    // サンプル設定を作成
    CHK_STATUS(createSampleConfiguration(channelName, 
                                       SIGNALING_CHANNEL_ROLE_TYPE_MASTER, 
                                       TRUE, TRUE, logLevel, 
                                       &gSampleConfiguration));

    // カメラを初期化
    CHK_STATUS(initializeCamera(&gSampleConfiguration->cameraContext, devicePath, width, height));
    
    // カメラの能力を確認
    CHK_STATUS(getCameraCapabilities(&gSampleConfiguration->cameraContext));
    CHK_STATUS(listSupportedFormats(&gSampleConfiguration->cameraContext));
    
    // H.264またはMJPEG対応チェック
    CameraFormat format = detectBestCameraFormat(&gSampleConfiguration->cameraContext);
    if (format != CAMERA_FORMAT_H264 && format != CAMERA_FORMAT_MJPEG) {
        DLOGE("[KVS Camera2] Camera does not support H.264 or MJPEG format");
        CHK_STATUS(STATUS_INVALID_OPERATION);
    }
    
    if (format == CAMERA_FORMAT_H264) {
        DLOGI("[KVS Camera2] Using direct H.264 format");
    } else if (format == CAMERA_FORMAT_MJPEG) {
        DLOGI("[KVS Camera2] Using MJPEG format with H.264 extraction");
    }

    // カメラストリーミングを開始
    CHK_STATUS(startCameraStreaming(&gSampleConfiguration->cameraContext));
    DLOGI("[KVS Camera2] Camera streaming started");

    // シグナリングを初期化
    PROFILE_CALL_WITH_START_END_T_OBJ(
        retStatus = initSignaling(gSampleConfiguration, SAMPLE_MASTER_CLIENT_ID), 
        gSampleConfiguration->signalingClientMetrics.signalingStartTime,
        gSampleConfiguration->signalingClientMetrics.signalingEndTime, 
        gSampleConfiguration->signalingClientMetrics.signalingCallTime,
        "Initialize signaling client and connect to the signaling channel");
    
    if (STATUS_FAILED(retStatus)) {
        DLOGE("[KVS Camera2] Failed to initialize signaling: 0x%08x", retStatus);
        goto CleanUp;
    }
    
    DLOGI("[KVS Camera2] Channel %s set up done", channelName);

    // 終了まで待機
    DLOGI("[KVS Camera2] Channel initialized. Waiting for termination...");
    while (!ATOMIC_LOAD_BOOL(&gSampleConfiguration->appTerminateFlag)) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Camera2] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Camera2] Cleaning up....");
    
    if (gSampleConfiguration != NULL) {
        // 終了フラグを設定
        ATOMIC_STORE_BOOL(&gSampleConfiguration->appTerminateFlag, TRUE);
        
        // カメラストリーミングを停止
        stopCameraStreaming(&gSampleConfiguration->cameraContext);
        terminateCamera(&gSampleConfiguration->cameraContext);

        // メディア送信スレッドの終了を待つ
        if (gSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(gSampleConfiguration->mediaSenderTid, NULL);
        }

        // シグナリングクライアントの統計情報を取得
        retStatus = signalingClientGetMetrics(gSampleConfiguration->signalingClientHandle, &signalingClientMetrics);
        if (retStatus == STATUS_SUCCESS) {
            logSignalingClientStats(&signalingClientMetrics);
        }

        // シグナリングクライアントを解放
        retStatus = freeSignalingClient(&gSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Camera2] freeSignalingClient(): 0x%08x", retStatus);
        }

        // サンプル設定を解放
        retStatus = freeSampleConfiguration(&gSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Camera2] freeSampleConfiguration(): 0x%08x", retStatus);
        }
    }

    DLOGI("[KVS Camera2] Cleanup done");
    CHK_LOG_ERR(retStatus);

    RESET_INSTRUMENTED_ALLOCATORS();

    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}