#ifndef __MJPEG_H264_EXTRACTOR_H__
#define __MJPEG_H264_EXTRACTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Samples.h"

#define MAX_H264_BUFFER_SIZE (1024 * 1024)  // 1MB
#define MAX_SPS_PPS_SIZE 256

typedef struct {
    PBYTE spsData;
    UINT32 spsSize;
    PBYTE ppsData;
    UINT32 ppsSize;
    PBYTE h264Buffer;
    UINT32 h264BufferSize;
    BOOL hasSps;
    BOOL hasPps;
} H264ExtractorContext, *PH264ExtractorContext;

// H.264抽出器の初期化・終了
STATUS initH264Extractor(PH264ExtractorContext pContext);
STATUS freeH264Extractor(PH264ExtractorContext pContext);

// MJPEG内からH.264を抽出
STATUS extractH264FromMjpeg(PH264ExtractorContext pContext, 
                           PBYTE pMjpegData, UINT32 mjpegSize,
                           PBYTE* ppH264Data, PUINT32 pH264Size);

// ヘルパー関数
BOOL isStartCode3(PBYTE pData, UINT32 size);
BOOL isStartCode4(PBYTE pData, UINT32 size);
UINT8 getNalUnitType(PBYTE pNalData);

#ifdef __cplusplus
}
#endif

#endif /* __MJPEG_H264_EXTRACTOR_H__ */