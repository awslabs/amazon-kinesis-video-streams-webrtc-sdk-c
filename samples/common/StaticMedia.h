#ifndef __KINESIS_VIDEO_SAMPLE_STATIC_MEDIA_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_STATIC_MEDIA_INCLUDE__

#include "Samples.h"

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STATUS useStaticFramePresets(PSampleConfiguration, RTC_CODEC);
STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath);
PVOID sendVideoPacketsFromDisk(PVOID args);
PVOID sendAudioPacketsFromDisk(PVOID args);

#ifdef __cplusplus
}
#endif

#endif // __KINESIS_VIDEO_SAMPLE_STATIC_MEDIA_INCLUDE__
