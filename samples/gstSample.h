/*******************************************
Shared include file for the gst samples
*******************************************/
#ifndef __KINESIS_VIDEO_GST_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_GST_SAMPLE_INCLUDE__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

// For tight packing
#pragma pack(push, samples_i, 1) // for byte alignment

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

typedef struct __GstAppSrcs {
    GstElement*  pGstAudioAppSrc;
    GstElement*  pGstVideoAppSrc;
} GstAppSrcs, *PGstAppSrcs;

#pragma pack(pop, samples_i)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_GST_SAMPLE_INCLUDE__ */
