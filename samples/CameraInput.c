#include "CameraInput.h"

#ifdef __linux__

STATUS initializeCamera(PCameraContext pCameraContext, PCHAR devicePath, UINT32 width, UINT32 height)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    UINT32 i;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(devicePath != NULL, STATUS_NULL_ARG, "Device path is NULL");

    // デバイスパスをコピー
    STRNCPY(pCameraContext->devicePath, devicePath, MAX_CAMERA_DEVICE_PATH - 1);
    pCameraContext->devicePath[MAX_CAMERA_DEVICE_PATH - 1] = '\0';
    
    pCameraContext->width = width;
    pCameraContext->height = height;
    pCameraContext->bufferCount = MAX_CAMERA_BUFFERS;
    ATOMIC_STORE_BOOL(&pCameraContext->isStreaming, FALSE);
    ATOMIC_STORE_BOOL(&pCameraContext->terminateFlag, FALSE);
    
    // H.264抽出器を初期化
    CHK_STATUS(initH264Extractor(&pCameraContext->h264Extractor));

    // デバイスを開く
    pCameraContext->fd = open(devicePath, O_RDWR | O_NONBLOCK);
    CHK_ERR(pCameraContext->fd >= 0, STATUS_OPEN_FILE_FAILED, "Failed to open camera device: %s", devicePath);

    // デバイス能力を確認
    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_QUERYCAP, &cap) == 0, STATUS_INVALID_OPERATION, "Failed to query device capabilities");
    CHK_ERR(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE, STATUS_INVALID_OPERATION, "Device does not support video capture");
    CHK_ERR(cap.capabilities & V4L2_CAP_STREAMING, STATUS_INVALID_OPERATION, "Device does not support streaming");

    DLOGI("[Camera] Device: %s, Driver: %s, Card: %s", devicePath, cap.driver, cap.card);

    // 最適なフォーマットを検出
    pCameraContext->format = detectBestCameraFormat(pCameraContext);
    
    // フォーマットを設定
    MEMSET(&fmt, 0, SIZEOF(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    
    // 検出されたフォーマットに応じてピクセルフォーマットを設定
    switch (pCameraContext->format) {
        case CAMERA_FORMAT_H264:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
            DLOGI("[Camera] Using H.264 format");
            break;
        case CAMERA_FORMAT_MJPEG:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
            DLOGI("[Camera] Using MJPEG format (will extract H.264)");
            break;
        default:
            CHK_ERR(FALSE, STATUS_INVALID_OPERATION, "No suitable format found (H.264 or MJPEG required)");
            break;
    }

    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_S_FMT, &fmt) == 0, STATUS_INVALID_OPERATION, "Failed to set format");

    // 実際に設定されたフォーマットを確認
    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_G_FMT, &fmt) == 0, STATUS_INVALID_OPERATION, "Failed to get format");
    
    pCameraContext->width = fmt.fmt.pix.width;
    pCameraContext->height = fmt.fmt.pix.height;
    pCameraContext->pixelFormat = fmt.fmt.pix.pixelformat;

    DLOGI("[Camera] Format set: %dx%d, pixelformat: 0x%x",
          pCameraContext->width, pCameraContext->height, pCameraContext->pixelFormat);

    // バッファを要求
    MEMSET(&req, 0, SIZEOF(req));
    req.count = MAX_CAMERA_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_REQBUFS, &req) == 0, STATUS_INVALID_OPERATION, "Failed to request buffers");
    CHK_ERR(req.count >= 2, STATUS_INVALID_OPERATION, "Insufficient buffer memory");

    pCameraContext->bufferCount = req.count;
    DLOGI("[Camera] Allocated %d buffers", pCameraContext->bufferCount);

    // バッファメモリを割り当て
    pCameraContext->buffers = (PVOID*) MEMCALLOC(pCameraContext->bufferCount, SIZEOF(PVOID));
    CHK_ERR(pCameraContext->buffers != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate buffer array");

    pCameraContext->bufferLengths = (UINT32*) MEMCALLOC(pCameraContext->bufferCount, SIZEOF(UINT32));
    CHK_ERR(pCameraContext->bufferLengths != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate buffer length array");

    // 各バッファをメモリマップ
    for (i = 0; i < pCameraContext->bufferCount; ++i) {
        MEMSET(&buf, 0, SIZEOF(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_QUERYBUF, &buf) == 0, STATUS_INVALID_OPERATION, "Failed to query buffer %d", i);

        pCameraContext->bufferLengths[i] = buf.length;
        pCameraContext->buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, pCameraContext->fd, buf.m.offset);
        
        CHK_ERR(pCameraContext->buffers[i] != MAP_FAILED, STATUS_INVALID_OPERATION, "Failed to mmap buffer %d", i);
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        terminateCamera(pCameraContext);
    }

    return retStatus;
}

STATUS terminateCamera(PCameraContext pCameraContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");

    // ストリーミングを停止
    if (ATOMIC_LOAD_BOOL(&pCameraContext->isStreaming)) {
        stopCameraStreaming(pCameraContext);
    }
    
    // H.264抽出器を解放
    freeH264Extractor(&pCameraContext->h264Extractor);

    // バッファのメモリマップを解除
    if (pCameraContext->buffers != NULL) {
        for (i = 0; i < pCameraContext->bufferCount; ++i) {
            if (pCameraContext->buffers[i] != NULL && pCameraContext->buffers[i] != MAP_FAILED) {
                munmap(pCameraContext->buffers[i], pCameraContext->bufferLengths[i]);
            }
        }
        MEMFREE(pCameraContext->buffers);
        pCameraContext->buffers = NULL;
    }

    if (pCameraContext->bufferLengths != NULL) {
        MEMFREE(pCameraContext->bufferLengths);
        pCameraContext->bufferLengths = NULL;
    }

    // デバイスを閉じる
    if (pCameraContext->fd >= 0) {
        close(pCameraContext->fd);
        pCameraContext->fd = -1;
    }

CleanUp:
    return retStatus;
}

STATUS startCameraStreaming(PCameraContext pCameraContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    UINT32 i;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(!ATOMIC_LOAD_BOOL(&pCameraContext->isStreaming), STATUS_INVALID_OPERATION, "Camera is already streaming");

    // バッファをキューに追加
    for (i = 0; i < pCameraContext->bufferCount; ++i) {
        MEMSET(&buf, 0, SIZEOF(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_QBUF, &buf) == 0, STATUS_INVALID_OPERATION, "Failed to queue buffer %d", i);
    }

    // ストリーミング開始
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_STREAMON, &type) == 0, STATUS_INVALID_OPERATION, "Failed to start streaming");

    ATOMIC_STORE_BOOL(&pCameraContext->isStreaming, TRUE);
    DLOGI("[Camera] Streaming started");

CleanUp:
    return retStatus;
}

STATUS stopCameraStreaming(PCameraContext pCameraContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    enum v4l2_buf_type type;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");

    if (ATOMIC_LOAD_BOOL(&pCameraContext->isStreaming)) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(pCameraContext->fd, VIDIOC_STREAMOFF, &type) == 0) {
            ATOMIC_STORE_BOOL(&pCameraContext->isStreaming, FALSE);
            DLOGI("[Camera] Streaming stopped");
        } else {
            DLOGE("[Camera] Failed to stop streaming: %s", strerror(errno));
            retStatus = STATUS_INVALID_OPERATION;
        }
    }

CleanUp:
    return retStatus;
}

STATUS captureFrame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_buffer buf;
    fd_set fds;
    struct timeval tv;
    INT32 r;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(ppFrameData != NULL, STATUS_NULL_ARG, "Frame data pointer is NULL");
    CHK_ERR(pFrameSize != NULL, STATUS_NULL_ARG, "Frame size pointer is NULL");
    CHK_ERR(ATOMIC_LOAD_BOOL(&pCameraContext->isStreaming), STATUS_INVALID_OPERATION, "Camera is not streaming");

    // selectを使用してフレームの準備を待つ
    FD_ZERO(&fds);
    FD_SET(pCameraContext->fd, &fds);

    tv.tv_sec = 2;  // 2秒のタイムアウト
    tv.tv_usec = 0;

    r = select(pCameraContext->fd + 1, &fds, NULL, NULL, &tv);
    CHK_ERR(r > 0, STATUS_OPERATION_TIMED_OUT, "Timeout waiting for frame");
    CHK_ERR(r != -1, STATUS_INVALID_OPERATION, "Select error: %s", strerror(errno));

    // バッファをデキュー
    MEMSET(&buf, 0, SIZEOF(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_DQBUF, &buf) == 0, STATUS_INVALID_OPERATION, "Failed to dequeue buffer");

    // フレームデータを返す
    *ppFrameData = (PBYTE) pCameraContext->buffers[buf.index];
    *pFrameSize = buf.bytesused;

CleanUp:
    return retStatus;
}

STATUS releaseFrame(PCameraContext pCameraContext, PBYTE pFrameData)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_buffer buf;
    UINT32 i;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(pFrameData != NULL, STATUS_NULL_ARG, "Frame data is NULL");

    // フレームデータに対応するバッファインデックスを見つける
    for (i = 0; i < pCameraContext->bufferCount; ++i) {
        if (pCameraContext->buffers[i] == pFrameData) {
            break;
        }
    }
    CHK_ERR(i < pCameraContext->bufferCount, STATUS_INVALID_ARG, "Invalid frame data pointer");

    // バッファを再キュー
    MEMSET(&buf, 0, SIZEOF(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_QBUF, &buf) == 0, STATUS_INVALID_OPERATION, "Failed to requeue buffer");

CleanUp:
    return retStatus;
}

STATUS getCameraCapabilities(PCameraContext pCameraContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_capability cap;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(pCameraContext->fd >= 0, STATUS_INVALID_OPERATION, "Camera device not opened");

    CHK_ERR(ioctl(pCameraContext->fd, VIDIOC_QUERYCAP, &cap) == 0, STATUS_INVALID_OPERATION, "Failed to query capabilities");

    DLOGI("[Camera] Driver: %s", cap.driver);
    DLOGI("[Camera] Card: %s", cap.card);
    DLOGI("[Camera] Bus info: %s", cap.bus_info);
    DLOGI("[Camera] Version: %u.%u.%u", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
    DLOGI("[Camera] Capabilities: 0x%08x", cap.capabilities);

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        DLOGI("[Camera] - Video capture supported");
    }
    if (cap.capabilities & V4L2_CAP_STREAMING) {
        DLOGI("[Camera] - Streaming I/O supported");
    }

CleanUp:
    return retStatus;
}

STATUS listSupportedFormats(PCameraContext pCameraContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct v4l2_fmtdesc fmt;
    UINT32 index = 0;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(pCameraContext->fd >= 0, STATUS_INVALID_OPERATION, "Camera device not opened");

    DLOGI("[Camera] Supported formats:");
    
    while (TRUE) {
        MEMSET(&fmt, 0, SIZEOF(fmt));
        fmt.index = index;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ioctl(pCameraContext->fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
            if (errno == EINVAL) {
                break; // 全フォーマットを列挙完了
            } else {
                CHK_ERR(FALSE, STATUS_INVALID_OPERATION, "Failed to enumerate format %d", index);
            }
        }

        DLOGI("[Camera] - Format %d: %s (0x%08x)", index, fmt.description, fmt.pixelformat);
        index++;
    }

CleanUp:
    return retStatus;
}

BOOL isCameraH264Capable(PCameraContext pCameraContext)
{
    struct v4l2_fmtdesc fmt;
    UINT32 index = 0;

    if (pCameraContext == NULL || pCameraContext->fd < 0) {
        return FALSE;
    }

    while (TRUE) {
        MEMSET(&fmt, 0, SIZEOF(fmt));
        fmt.index = index;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ioctl(pCameraContext->fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
            break;
        }

        if (fmt.pixelformat == V4L2_PIX_FMT_H264) {
            return TRUE;
        }
        index++;
    }

    return FALSE;
}

BOOL isCameraMjpegCapable(PCameraContext pCameraContext)
{
    struct v4l2_fmtdesc fmt;
    UINT32 index = 0;

    if (pCameraContext == NULL || pCameraContext->fd < 0) {
        return FALSE;
    }

    while (TRUE) {
        MEMSET(&fmt, 0, SIZEOF(fmt));
        fmt.index = index;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ioctl(pCameraContext->fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
            break;
        }

        if (fmt.pixelformat == V4L2_PIX_FMT_MJPEG) {
            return TRUE;
        }
        index++;
    }

    return FALSE;
}

CameraFormat detectBestCameraFormat(PCameraContext pCameraContext)
{
    if (pCameraContext == NULL || pCameraContext->fd < 0) {
        return CAMERA_FORMAT_UNKNOWN;
    }

    // H.264を最優先
    if (isCameraH264Capable(pCameraContext)) {
        return CAMERA_FORMAT_H264;
    }

    // 次にMJPEG（H.264抽出可能）
    if (isCameraMjpegCapable(pCameraContext)) {
        return CAMERA_FORMAT_MJPEG;
    }

    return CAMERA_FORMAT_UNKNOWN;
}

STATUS captureH264Frame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pRawFrameData = NULL;
    UINT32 rawFrameSize = 0;
    PBYTE pH264Data = NULL;
    UINT32 h264Size = 0;

    CHK_ERR(pCameraContext != NULL, STATUS_NULL_ARG, "Camera context is NULL");
    CHK_ERR(ppFrameData != NULL, STATUS_NULL_ARG, "Frame data pointer is NULL");
    CHK_ERR(pFrameSize != NULL, STATUS_NULL_ARG, "Frame size pointer is NULL");

    // まず生フレームを取得
    CHK_STATUS(captureFrame(pCameraContext, &pRawFrameData, &rawFrameSize));

    if (pCameraContext->format == CAMERA_FORMAT_H264) {
        // 直接H.264の場合はそのまま返す
        *ppFrameData = pRawFrameData;
        *pFrameSize = rawFrameSize;
    } else if (pCameraContext->format == CAMERA_FORMAT_MJPEG) {
        // MJPEGからH.264を抽出
        retStatus = extractH264FromMjpeg(&pCameraContext->h264Extractor,
                                       pRawFrameData, rawFrameSize,
                                       &pH264Data, &h264Size);
        
        // 生フレームを解放
        releaseFrame(pCameraContext, pRawFrameData);
        
        if (STATUS_SUCCEEDED(retStatus) && pH264Data != NULL && h264Size > 0) {
            *ppFrameData = pH264Data;
            *pFrameSize = h264Size;
        } else {
            *ppFrameData = NULL;
            *pFrameSize = 0;
            if (STATUS_SUCCEEDED(retStatus)) {
                retStatus = STATUS_NO_MORE_DATA_AVAILABLE; // H.264データが見つからない
            }
        }
    } else {
        releaseFrame(pCameraContext, pRawFrameData);
        CHK_ERR(FALSE, STATUS_INVALID_OPERATION, "Unsupported camera format");
    }

CleanUp:
    return retStatus;
}

#else // !__linux__

// Linux以外のプラットフォーム用のスタブ実装
STATUS initializeCamera(PCameraContext pCameraContext, PCHAR devicePath, UINT32 width, UINT32 height)
{
    DLOGE("[Camera] V4L2 camera input is only supported on Linux");
    return STATUS_NOT_IMPLEMENTED;
}

STATUS terminateCamera(PCameraContext pCameraContext)
{
    return STATUS_SUCCESS;
}

STATUS startCameraStreaming(PCameraContext pCameraContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS stopCameraStreaming(PCameraContext pCameraContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS captureFrame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS releaseFrame(PCameraContext pCameraContext, PBYTE pFrameData)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS getCameraCapabilities(PCameraContext pCameraContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

STATUS listSupportedFormats(PCameraContext pCameraContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

BOOL isCameraH264Capable(PCameraContext pCameraContext)
{
    return FALSE;
}

BOOL isCameraMjpegCapable(PCameraContext pCameraContext)
{
    return FALSE;
}

CameraFormat detectBestCameraFormat(PCameraContext pCameraContext)
{
    return CAMERA_FORMAT_UNKNOWN;
}

STATUS captureH264Frame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize)
{
    return STATUS_NOT_IMPLEMENTED;
}

#endif // __linux__