#ifndef __CAMERA_INPUT_H__
#define __CAMERA_INPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Samples.h"
#include "MjpegH264Extractor.h"

#ifdef __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

#define MAX_CAMERA_DEVICE_PATH 256
#define MAX_CAMERA_BUFFERS 4
#define DEFAULT_CAMERA_DEVICE "/dev/video0"

typedef enum {
    CAMERA_FORMAT_H264,
    CAMERA_FORMAT_MJPEG,
    CAMERA_FORMAT_YUYV,
    CAMERA_FORMAT_UNKNOWN
} CameraFormat;

typedef struct {
    INT32 fd;
    CHAR devicePath[MAX_CAMERA_DEVICE_PATH];
    UINT32 width;
    UINT32 height;
    UINT32 pixelFormat;
    CameraFormat format;
    UINT32 bufferCount;
    PVOID* buffers;
    UINT32* bufferLengths;
    volatile ATOMIC_BOOL isStreaming;
    volatile ATOMIC_BOOL terminateFlag;
    H264ExtractorContext h264Extractor;  // MJPEG内H.264抽出用
} CameraContext, *PCameraContext;

// カメラ初期化・終了
STATUS initializeCamera(PCameraContext pCameraContext, PCHAR devicePath, UINT32 width, UINT32 height);
STATUS terminateCamera(PCameraContext pCameraContext);

// カメラストリーミング制御
STATUS startCameraStreaming(PCameraContext pCameraContext);
STATUS stopCameraStreaming(PCameraContext pCameraContext);

// フレーム取得
STATUS captureFrame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize);
STATUS releaseFrame(PCameraContext pCameraContext, PBYTE pFrameData);

// カメラ情報取得
STATUS getCameraCapabilities(PCameraContext pCameraContext);
STATUS listSupportedFormats(PCameraContext pCameraContext);

// フォーマット対応チェック
BOOL isCameraH264Capable(PCameraContext pCameraContext);
BOOL isCameraMjpegCapable(PCameraContext pCameraContext);
CameraFormat detectBestCameraFormat(PCameraContext pCameraContext);

// H.264データ取得（MJPEGからの抽出も含む）
STATUS captureH264Frame(PCameraContext pCameraContext, PBYTE* ppFrameData, PUINT32 pFrameSize);

#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_INPUT_H__ */