#include "V4L2LoopbackOutput.h"

#ifdef __linux__

STATUS initializeV4L2LoopbackOutput(PV4L2LoopbackContext pContext, PCHAR devicePath, UINT32 width, UINT32 height)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_format fmt;
    struct v4l2_capability cap;

    CHK_ERR(pContext != NULL, STATUS_NULL_ARG, "V4L2 Loopback context is NULL");
    CHK_ERR(devicePath != NULL, STATUS_NULL_ARG, "Device path is NULL");

    // コンテキストを初期化
    MEMSET(pContext, 0, SIZEOF(V4L2LoopbackContext));
    
    // デバイスパスをコピー
    STRNCPY(pContext->devicePath, devicePath, MAX_V4L2_LOOPBACK_DEVICE_PATH - 1);
    pContext->devicePath[MAX_V4L2_LOOPBACK_DEVICE_PATH - 1] = '\0';
    
    pContext->width = width;
    pContext->height = height;
    pContext->fd = -1;
    pContext->isActive = FALSE;
    
    // ミューテックスを初期化
    CHK_STATUS(MUTEX_CREATE(&pContext->writeMutex));

    // デバイスを開く
    pContext->fd = open(devicePath, O_RDWR);
    CHK_ERR(pContext->fd >= 0, STATUS_OPEN_FILE_FAILED, 
            "Failed to open V4L2 loopback device: %s", devicePath);

    // デバイス能力を確認
    if (ioctl(pContext->fd, VIDIOC_QUERYCAP, &cap) == 0) {
        DLOGI("[V4L2 Loopback] Device: %s, Driver: %s, Card: %s", 
              devicePath, cap.driver, cap.card);
        
        if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
            DLOGW("[V4L2 Loopback] Device does not support video output, trying anyway");
        }
    } else {
        DLOGW("[V4L2 Loopback] Failed to query device capabilities, trying anyway");
    }

    // フォーマットを設定（MJPEG）
    MEMSET(&fmt, 0, SIZEOF(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;  // MJPEG
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = 0;  // MJPEGは可変長
    fmt.fmt.pix.sizeimage = width * height * 2;  // MJPEG推定サイズ

    if (ioctl(pContext->fd, VIDIOC_S_FMT, &fmt) == 0) {
        // 実際に設定されたフォーマットを確認
        if (ioctl(pContext->fd, VIDIOC_G_FMT, &fmt) == 0) {
            pContext->width = fmt.fmt.pix.width;
            pContext->height = fmt.fmt.pix.height;
            pContext->pixelFormat = fmt.fmt.pix.pixelformat;
            
            DLOGI("[V4L2 Loopback] Format set: %dx%d, pixelformat: 0x%x (%c%c%c%c)",
                  pContext->width, pContext->height, pContext->pixelFormat,
                  (pContext->pixelFormat >> 0) & 0xFF,
                  (pContext->pixelFormat >> 8) & 0xFF,
                  (pContext->pixelFormat >> 16) & 0xFF,
                  (pContext->pixelFormat >> 24) & 0xFF);
        }
    } else {
        DLOGW("[V4L2 Loopback] Failed to set format, device may not support format setting");
    }

    pContext->isActive = TRUE;
    DLOGI("[V4L2 Loopback] Initialized successfully: %s (%dx%d)", 
          devicePath, pContext->width, pContext->height);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        terminateV4L2LoopbackOutput(pContext);
    }

    return retStatus;
}

STATUS terminateV4L2LoopbackOutput(PV4L2LoopbackContext pContext)
{
    if (pContext != NULL) {
        pContext->isActive = FALSE;
        
        // デバイスを閉じる
        if (pContext->fd >= 0) {
            close(pContext->fd);
            pContext->fd = -1;
        }
        
        // ミューテックスを破棄
        MUTEX_FREE(pContext->writeMutex);
        
        DLOGI("[V4L2 Loopback] Terminated: %s", pContext->devicePath);
    }
    
    return STATUS_SUCCESS;
}

STATUS writeFrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pFrameData, UINT32 frameSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    ssize_t bytesWritten;

    CHK_ERR(pContext != NULL, STATUS_NULL_ARG, "V4L2 Loopback context is NULL");
    CHK_ERR(pFrameData != NULL, STATUS_NULL_ARG, "Frame data is NULL");
    CHK_ERR(frameSize > 0, STATUS_INVALID_ARG, "Frame size is zero");
    CHK_ERR(pContext->isActive, STATUS_INVALID_OPERATION, "V4L2 Loopback is not active");

    MUTEX_LOCK(pContext->writeMutex);
    
    // フレームデータを書き込み
    bytesWritten = write(pContext->fd, pFrameData, frameSize);
    
    if (bytesWritten != (ssize_t)frameSize) {
        if (bytesWritten < 0) {
            DLOGE("[V4L2 Loopback] Write failed: %s", strerror(errno));
            retStatus = STATUS_WRITE_TO_FILE_FAILED;
        } else {
            DLOGW("[V4L2 Loopback] Partial write: %zd/%u bytes", bytesWritten, frameSize);
            retStatus = STATUS_WRITE_TO_FILE_FAILED;
        }
    } else {
        DLOGV("[V4L2 Loopback] Frame written successfully: %u bytes", frameSize);
    }
    
    MUTEX_UNLOCK(pContext->writeMutex);

CleanUp:
    return retStatus;
}

// 簡易的なH.264からYUV420P変換（実際にはFFmpegやGStreamerを使用することを推奨）
STATUS writeH264FrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pH264Data, UINT32 h264Size)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pYuvData = NULL;
    UINT32 yuvSize;
    
    CHK_ERR(pContext != NULL, STATUS_NULL_ARG, "V4L2 Loopback context is NULL");
    CHK_ERR(pH264Data != NULL, STATUS_NULL_ARG, "H264 data is NULL");
    CHK_ERR(h264Size > 0, STATUS_INVALID_ARG, "H264 size is zero");
    CHK_ERR(pContext->isActive, STATUS_INVALID_OPERATION, "V4L2 Loopback is not active");

    // YUV420Pのサイズを計算
    yuvSize = (pContext->width * pContext->height * 3) / 2;
    
    // YUVバッファを割り当て
    pYuvData = (PBYTE) MEMALLOC(yuvSize);
    CHK_ERR(pYuvData != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate YUV buffer");
    
    // 注意: これは簡易的な実装です
    // 実際のプロダクションでは、FFmpegやGStreamerを使用してH.264をYUV420Pに変換する必要があります
    // ここでは、テスト用にグレーフレームを生成します
    
    // Y平面（輝度）: グレー値128で埋める
    MEMSET(pYuvData, 128, pContext->width * pContext->height);
    
    // U平面（色差）: 128で埋める（グレー）
    MEMSET(pYuvData + (pContext->width * pContext->height), 128, 
           (pContext->width * pContext->height) / 4);
    
    // V平面（色差）: 128で埋める（グレー）
    MEMSET(pYuvData + (pContext->width * pContext->height) + ((pContext->width * pContext->height) / 4), 128, 
           (pContext->width * pContext->height) / 4);
    
    // TODO: 実際のH.264デコード処理をここに実装
    // 現在は警告メッセージを出力
    static BOOL warningShown = FALSE;
    if (!warningShown) {
        DLOGW("[V4L2 Loopback] H.264 to YUV conversion not implemented - outputting test pattern");
        DLOGW("[V4L2 Loopback] For production use, integrate FFmpeg or GStreamer for proper H.264 decoding");
        warningShown = TRUE;
    }
    
    // YUVフレームをV4L2 Loopbackに書き込み
    CHK_STATUS(writeFrameToV4L2Loopback(pContext, pYuvData, yuvSize));

CleanUp:
    if (pYuvData != NULL) {
        MEMFREE(pYuvData);
    }
    
    return retStatus;
}

#else // !__linux__

// Linux以外のプラットフォーム用のスタブ実装
STATUS initializeV4L2LoopbackOutput(PV4L2LoopbackContext pContext, PCHAR devicePath, UINT32 width, UINT32 height)
{
    DLOGE("[V4L2 Loopback] V4L2 loopback output is only supported on Linux");
    return STATUS_NOT_IMPLEMENTED;
}

STATUS terminateV4L2LoopbackOutput(PV4L2LoopbackContext pContext)
{
    return STATUS_SUCCESS;
}

STATUS writeFrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pFrameData, UINT32 frameSize)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS writeH264FrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pH264Data, UINT32 h264Size)
{
    return STATUS_NOT_IMPLEMENTED;
}

#endif // __linux__