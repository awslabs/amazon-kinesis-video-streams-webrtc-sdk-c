#ifndef __KINESIS_VIDEO_SAMPLE_GST_MEDIA_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_GST_MEDIA_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Samples.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

GstFlowReturn on_new_sample(GstElement*, gpointer, UINT64);
GstFlowReturn on_new_sample_video(GstElement*, gpointer);
GstFlowReturn on_new_sample_audio(GstElement*, gpointer);

PVOID sendGstreamerAudioVideo(PVOID);

STATUS useGstreamer(PSampleConfiguration, INT32, CHAR**);
STATUS parseSrcType(PSampleConfiguration pSampleConfiguration, INT32 argc, CHAR** argv, INT32 startIndex);

#ifdef __cplusplus
}
#endif

#endif // __KINESIS_VIDEO_SAMPLE_GST_MEDIA_INCLUDE__
