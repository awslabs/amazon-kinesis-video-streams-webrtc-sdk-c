#ifdef ENABLE_GST_SAMPLE_RECEIVER
#include "Samples.h"
#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/app/gstappsink.h>

VOID onGstVideoFrameReady(UINT64 customData, PFrame pFrame)
{
    GstFlowReturn ret;
    GstBuffer* buffer;
    GstElement* appsrcVideo = (GstElement*) customData;
    if (!appsrcVideo) {
        DLOGE("Null");
    }
    buffer = gst_buffer_new_allocate(NULL, pFrame->size, NULL);
    if (!buffer) {
        DLOGE("Buffer allocation failed");
        return;
    }

    DLOGI("Frame size: %d, %llu", pFrame->size, pFrame->presentationTs);
    GST_BUFFER_PTS(buffer) = pFrame->presentationTs;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 25);
    if (gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size) != pFrame->size) {
        DLOGE("Buffer fill did not complete correctly");
        gst_buffer_unref(buffer);
        return;
    }
    g_signal_emit_by_name(appsrcVideo, "push-buffer", buffer, &ret);
    if (ret != GST_FLOW_OK) {
        DLOGE("Error pushing buffer: %s", gst_flow_get_name(ret));
    }
    gst_buffer_unref(buffer);
}

VOID onGstAudioFrameReady(UINT64 customData, PFrame pFrame)
{
    GstFlowReturn ret;
    GstBuffer* buffer;
    GstElement* appsrcAudio = (GstElement*) customData;
    if (!appsrcAudio) {
        DLOGE("Null");
    }
    buffer = gst_buffer_new_allocate(NULL, pFrame->size, NULL);
    if (!buffer) {
        DLOGE("Buffer allocation failed");
        return;
    }

    DLOGI("Audio Frame size: %d, %llu", pFrame->size, pFrame->presentationTs);
    GST_BUFFER_PTS(buffer) = pFrame->presentationTs;
    int sample_rate = 48000; // Hz
    int num_channels = 2;
    int bits_per_sample = 16; // For example, 16-bit audio
    int byte_rate = (sample_rate * num_channels * bits_per_sample) / 8;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(pFrame->size, GST_SECOND, byte_rate);
    if (gst_buffer_fill(buffer, 0, pFrame->frameData, pFrame->size) != pFrame->size) {
        DLOGE("Buffer fill did not complete correctly");
        gst_buffer_unref(buffer);
        return;
    }
    g_signal_emit_by_name(appsrcAudio, "push-buffer", buffer, &ret);
    if (ret != GST_FLOW_OK) {
        DLOGE("Error pushing buffer: %s", gst_flow_get_name(ret));
    }
    gst_buffer_unref(buffer);
}

VOID onSampleStreamingSessionShutdown(UINT64 customData, PSampleStreamingSession pSampleStreamingSession)
{
    (void) (pSampleStreamingSession);
    GstElement* pipeline = (GstElement*) customData;
    gst_element_send_event(pipeline, gst_event_new_eos());
}

PVOID receiveGstreamerAudioVideo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    GstElement *pipeline = NULL, *appsrcAudio = NULL, *appsrcVideo = NULL;
    GstBus* bus;
    GstMessage* msg;
    GError* error = NULL;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    PCHAR roleType;
    gchar *videoDescription = "", *audioDescription = "", *audioVideoDescription;

    if (pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_MASTER) {
        roleType = "Master";
    } else if (pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        roleType = "Viewer";
    }

    gst_init(NULL, NULL);

    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS %s] Sample streaming session is NULL", roleType);

    switch (pSampleStreamingSession->pVideoRtcRtpTransceiver->receiver.track.codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            videoDescription = "appsrc name=appsrc-video ! capsfilter "
                               "caps=video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline,width=1920,height=720 ! queue ! h264parse "
                               "! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv";
            break;

        case RTC_CODEC_H265:
            videoDescription = "appsrc name=appsrc-video ! capsfilter "
                               "caps=video/x-h265,stream-format=byte-stream,framerate=25/1,alignment=au,profile=main,width=1920,height=720 ! queue "
                               "! h265parse ! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv ";
            break;

        // TODO: add a case for vp8
        default:
            break;
    }

    if (pSampleConfiguration->mediaType == SAMPLE_STREAMING_AUDIO_VIDEO) {
        switch (pSampleStreamingSession->pAudioRtcRtpTransceiver->receiver.track.codec) {
            case RTC_CODEC_OPUS:
                audioDescription = "appsrc name=appsrc-audio ! capsfilter caps=audio/x-opus,rate=48000,channels=2 ! queue ! opusparse ! queue ! mux.";
                break;

            case RTC_CODEC_AAC:
                audioDescription = "appsrc name=appsrc-audio ! capsfilter "
                                   "caps=audio/mpeg,mpegversion=4,stream-format=adts,base-profile=lc,channels=2,rate=48000 ! queue ! aacparse ! mux.";
                break;

            // TODO: add a case for mulaw and alaw
            default:
                break;
        }
    }

    audioVideoDescription = g_strjoin(" ", videoDescription, audioDescription, NULL);

    pipeline = gst_parse_launch(audioVideoDescription, &error);
    CHK_ERR(pipeline != NULL, STATUS_INTERNAL_ERROR, "[KVS %s] Pipeline is NULL", roleType);

    appsrcVideo = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-video");
    CHK_ERR(appsrcVideo != NULL, STATUS_INTERNAL_ERROR, "[KVS %s] Cannot find appsrc video", roleType);
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) appsrcVideo, onGstVideoFrameReady));

    if (pSampleConfiguration->mediaType == SAMPLE_STREAMING_AUDIO_VIDEO) {
        appsrcAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-audio");
        CHK_ERR(appsrcAudio != NULL, STATUS_INTERNAL_ERROR, "[KVS %s] Cannot find appsrc audio", roleType);
        CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) appsrcAudio, onGstAudioFrameReady));
    }

    CHK_STATUS(streamingSessionOnShutdown(pSampleStreamingSession, (UINT64) pipeline, onSampleStreamingSessionShutdown));
    g_free(audioVideoDescription);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* block until error or EOS */
    bus = gst_element_get_bus(pipeline);
    CHK_ERR(bus != NULL, STATUS_INTERNAL_ERROR, "[KVS %s] Bus is NULL", roleType);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &error, NULL);
                DLOGE("Error received: %s", error->message);
                g_error_free(error);
                break;
            case GST_MESSAGE_EOS:
                DLOGI("End of stream");
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }
    if (bus != NULL) {
        gst_object_unref(bus);
    }
    if (pipeline != NULL) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    if (appsrcAudio != NULL) {
        gst_object_unref(appsrcAudio);
    }
    if (appsrcVideo != NULL) {
        gst_object_unref(appsrcVideo);
    }

CleanUp:
    if (error != NULL) {
        DLOGE("[KVS %s] %s", roleType, error->message);
        g_clear_error(&error);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}
#endif
