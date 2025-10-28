#ifndef __V4L2_LOOPBACK_OUTPUT_H__
#define __V4L2_LOOPBACK_OUTPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Samples.h"

#ifdef __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

#define MAX_V4L2_LOOPBACK_DEVICE_PATH 256
#define DEFAULT_V4L2_LOOPBACK_DEVICE "/dev/video4"

typedef struct {
    INT32 fd;
    CHAR devicePath[MAX_V4L2_LOOPBACK_DEVICE_PATH];
    UINT32 width;
    UINT32 height;
    UINT32 pixelFormat;
    BOOL isActive;
    MUTEX writeMutex;
} V4L2LoopbackContext, *PV4L2LoopbackContext;

// V4L2 Loopback出力の初期化・終了
STATUS initializeV4L2LoopbackOutput(PV4L2LoopbackContext pContext, PCHAR devicePath, UINT32 width, UINT32 height);
STATUS terminateV4L2LoopbackOutput(PV4L2LoopbackContext pContext);

// フレーム出力
STATUS writeFrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pFrameData, UINT32 frameSize);

// H.264をYUV420Pに変換してV4L2 Loopbackに出力
STATUS writeH264FrameToV4L2Loopback(PV4L2LoopbackContext pContext, PBYTE pH264Data, UINT32 h264Size);

#ifdef __cplusplus
}
#endif

#endif /* __V4L2_LOOPBACK_OUTPUT_H__ */