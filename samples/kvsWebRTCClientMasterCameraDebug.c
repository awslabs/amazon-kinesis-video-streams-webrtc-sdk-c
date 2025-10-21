#include "Samples.h"
#include "CameraInput.h"

// カメラ診断用のデバッグバージョン
INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pCameraDevice = DEFAULT_CAMERA_DEVICE;
    CameraContext cameraContext = {0};

    if (argc > 1) {
        pCameraDevice = argv[1];
    }

    printf("=== Camera Debug Information ===\n");
    printf("Device: %s\n\n", pCameraDevice);

#ifdef __linux__
    // デバイスを開く
    cameraContext.fd = open(pCameraDevice, O_RDWR | O_NONBLOCK);
    if (cameraContext.fd < 0) {
        printf("ERROR: Failed to open camera device: %s\n", pCameraDevice);
        printf("Error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // デバイス能力を確認
    struct v4l2_capability cap;
    if (ioctl(cameraContext.fd, VIDIOC_QUERYCAP, &cap) == 0) {
        printf("Device Capabilities:\n");
        printf("  Driver: %s\n", cap.driver);
        printf("  Card: %s\n", cap.card);
        printf("  Bus info: %s\n", cap.bus_info);
        printf("  Version: %u.%u.%u\n", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
        printf("  Capabilities: 0x%08x\n", cap.capabilities);
        
        if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
            printf("  - Video capture: YES\n");
        } else {
            printf("  - Video capture: NO\n");
        }
        
        if (cap.capabilities & V4L2_CAP_STREAMING) {
            printf("  - Streaming I/O: YES\n");
        } else {
            printf("  - Streaming I/O: NO\n");
        }
    } else {
        printf("ERROR: Failed to query device capabilities\n");
    }

    printf("\nSupported Formats:\n");
    struct v4l2_fmtdesc fmt;
    UINT32 index = 0;
    BOOL h264Found = FALSE;
    BOOL mjpegFound = FALSE;

    while (TRUE) {
        MEMSET(&fmt, 0, sizeof(fmt));
        fmt.index = index;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ioctl(cameraContext.fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
            if (errno == EINVAL) {
                break; // 全フォーマットを列挙完了
            } else {
                printf("ERROR: Failed to enumerate format %d\n", index);
                break;
            }
        }

        printf("  Format %d: %s (0x%08x)\n", index, fmt.description, fmt.pixelformat);
        
        if (fmt.pixelformat == V4L2_PIX_FMT_H264) {
            h264Found = TRUE;
            printf("    *** H.264 FORMAT FOUND! ***\n");
        }
        if (fmt.pixelformat == V4L2_PIX_FMT_MJPEG) {
            mjpegFound = TRUE;
            printf("    *** MJPEG FORMAT FOUND ***\n");
        }
        
        index++;
    }

    printf("\nFormat Summary:\n");
    printf("  H.264 support: %s\n", h264Found ? "YES" : "NO");
    printf("  MJPEG support: %s\n", mjpegFound ? "YES" : "NO");

    // 現在のフォーマットを確認
    printf("\nCurrent Format:\n");
    struct v4l2_format current_fmt;
    MEMSET(&current_fmt, 0, sizeof(current_fmt));
    current_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(cameraContext.fd, VIDIOC_G_FMT, &current_fmt) == 0) {
        printf("  Width: %d\n", current_fmt.fmt.pix.width);
        printf("  Height: %d\n", current_fmt.fmt.pix.height);
        printf("  Pixel Format: 0x%08x\n", current_fmt.fmt.pix.pixelformat);
        printf("  Field: %d\n", current_fmt.fmt.pix.field);
        printf("  Bytes per line: %d\n", current_fmt.fmt.pix.bytesperline);
        printf("  Size image: %d\n", current_fmt.fmt.pix.sizeimage);
        
        // フォーマットを文字列で表示
        char fourcc[5] = {0};
        fourcc[0] = (current_fmt.fmt.pix.pixelformat >> 0) & 0xFF;
        fourcc[1] = (current_fmt.fmt.pix.pixelformat >> 8) & 0xFF;
        fourcc[2] = (current_fmt.fmt.pix.pixelformat >> 16) & 0xFF;
        fourcc[3] = (current_fmt.fmt.pix.pixelformat >> 24) & 0xFF;
        printf("  Format (FourCC): '%s'\n", fourcc);
    } else {
        printf("ERROR: Failed to get current format\n");
    }

    // H.264フォーマットを試してみる
    if (h264Found) {
        printf("\nTesting H.264 Format:\n");
        struct v4l2_format test_fmt;
        MEMSET(&test_fmt, 0, sizeof(test_fmt));
        test_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        test_fmt.fmt.pix.width = 640;
        test_fmt.fmt.pix.height = 480;
        test_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
        test_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (ioctl(cameraContext.fd, VIDIOC_S_FMT, &test_fmt) == 0) {
            printf("  H.264 format set successfully!\n");
            printf("  Actual width: %d\n", test_fmt.fmt.pix.width);
            printf("  Actual height: %d\n", test_fmt.fmt.pix.height);
            printf("  Actual format: 0x%08x\n", test_fmt.fmt.pix.pixelformat);
        } else {
            printf("  Failed to set H.264 format: %s\n", strerror(errno));
        }
    }

    // MJPEGフォーマットも試してみる
    if (mjpegFound) {
        printf("\nTesting MJPEG Format:\n");
        struct v4l2_format test_fmt;
        MEMSET(&test_fmt, 0, sizeof(test_fmt));
        test_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        test_fmt.fmt.pix.width = 640;
        test_fmt.fmt.pix.height = 480;
        test_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        test_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (ioctl(cameraContext.fd, VIDIOC_S_FMT, &test_fmt) == 0) {
            printf("  MJPEG format set successfully!\n");
            printf("  Actual width: %d\n", test_fmt.fmt.pix.width);
            printf("  Actual height: %d\n", test_fmt.fmt.pix.height);
            printf("  Actual format: 0x%08x\n", test_fmt.fmt.pix.pixelformat);
        } else {
            printf("  Failed to set MJPEG format: %s\n", strerror(errno));
        }
    }

    close(cameraContext.fd);

    printf("\n=== Debug Complete ===\n");

    if (h264Found) {
        printf("\n✅ Your camera supports H.264! The issue might be in the configuration.\n");
        printf("Try running the configure script again:\n");
        printf("  ./scripts/configure_h264_camera.sh -d %s\n", pCameraDevice);
    } else if (mjpegFound) {
        printf("\n⚠️ Your camera supports MJPEG but not H.264.\n");
        printf("You may need a different camera or use MJPEG with software encoding.\n");
    } else {
        printf("\n❌ Your camera doesn't support H.264 or MJPEG.\n");
        printf("Please check if you have a UVC H.264 compatible camera.\n");
    }

#else
    printf("This debug tool only works on Linux with V4L2 support.\n");
#endif

    return EXIT_SUCCESS;
}