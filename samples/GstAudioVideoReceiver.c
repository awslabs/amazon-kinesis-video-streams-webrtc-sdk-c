#ifdef ENABLE_GST_SAMPLE_RECEIVER
#include "Samples.h"
#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/app/gstappsink.h>


// This function is a callback for the transceiver for every single video frame it receives
// It writes these frames to a buffer and pushes it to the `appsrcVideo` element of the 
// GStreamer pipeline created in `receiveGstreamerAudioVideo`. Any logic to modify / discard the frames would go here
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

    DLOGD("Frame size: %d, presentationTs: %llu", pFrame->size, pFrame->presentationTs);
    GST_BUFFER_PTS(buffer) = pFrame->presentationTs;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, DEFAULT_FPS_VALUE);
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

// This function is a callback for the transceiver for every single audio frame it receives
// It writes these frames to a buffer and pushes it to the `appsrcAudio` element of the 
// GStreamer pipeline created in `receiveGstreamerAudioVideo`. Any logic to modify / discard the frames would go here
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

    DLOGD("Audio Frame size: %d, presentationTs: %llu", pFrame->size, pFrame->presentationTs);
    GST_BUFFER_PTS(buffer) = pFrame->presentationTs;

    // Recalculate the byte-rate if not using the default values
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(pFrame->size, GST_SECOND, DEFAULT_AUDIO_BYTE_RATE);
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

// This function is a callback for the streaming session shutdown event. We send an eos to the pipeline to exit the 
// application using this.
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
    GstCaps *audiocaps, *videocaps;
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

    // It is advised to modify the pipeline and the caps as per the source of the media. Customers can also modify this pipeline to 
    // use any other sinks instead of `filesink` like `autovideosink` and `autoaudiosink`. The existing pipelines are not complex enough to
    // change cpas and properties dynamically, more complex logic may be needed to support the same.
    switch (pSampleStreamingSession->pVideoRtcRtpTransceiver->receiver.track.codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            videoDescription = "appsrc name=appsrc-video ! queue ! h264parse ! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv";
            videocaps =
                gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", "alignment", G_TYPE_STRING, "au", "profile",
                                    G_TYPE_STRING, "baseline", "height", G_TYPE_INT, DEFAULT_HEIGHT, "width", G_TYPE_INT, DEFAULT_WIDTH, NULL);
            break;

        case RTC_CODEC_H265:
            videoDescription = "appsrc name=appsrc-video ! queue ! h265parse ! queue ! matroskamux name=mux ! queue ! filesink location=video.mkv ";
            videocaps =
                gst_caps_new_simple("video/x-h265", "stream-format", G_TYPE_STRING, "byte-stream", "alignment", G_TYPE_STRING, "au", "profile",
                                    G_TYPE_STRING, "main", "height", G_TYPE_INT, DEFAULT_HEIGHT, "width", G_TYPE_INT, DEFAULT_WIDTH, NULL);
            break;

        // TODO: add a similar pipeline for VP8
        
        default:
            break;
    }

    if (pSampleConfiguration->mediaType == SAMPLE_STREAMING_AUDIO_VIDEO) {
        switch (pSampleStreamingSession->pAudioRtcRtpTransceiver->receiver.track.codec) {
            case RTC_CODEC_OPUS:
                audioDescription = "appsrc name=appsrc-audio ! queue ! opusparse ! queue ! mux.";
                audiocaps =
                    gst_caps_new_simple("audio/x-opus", "rate", G_TYPE_INT, DEFAULT_AUDIO_SAMPLE_RATE, "channel-mapping-family", G_TYPE_INT, 1, NULL);
                break;

            case RTC_CODEC_AAC:
                audioDescription = "appsrc name=appsrc-audio ! queue ! aacparse ! mux.";
                audiocaps = gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "rate", G_TYPE_INT, DEFAULT_AUDIO_SAMPLE_RATE, "channels",
                                                G_TYPE_INT, DEFAULT_AUDIO_CHANNELS, "stream-format", G_TYPE_STRING, "adts", "base-profile",
                                                G_TYPE_STRING, "lc", NULL);
                break;

            // TODO: make sure this pipeline works. Figure out the caps for this
            case RTC_CODEC_MULAW:
            case RTC_CODEC_ALAW:
                audioDescription = "appsrc name=appsrc-audio ! rawaudioparse ! decodebin ! autoaudiosink";
                break;
            
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
    g_object_set(G_OBJECT(appsrcVideo), "caps", videocaps, NULL);
    gst_caps_unref(videocaps);

    if (pSampleConfiguration->mediaType == SAMPLE_STREAMING_AUDIO_VIDEO) {
        appsrcAudio = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-audio");
        CHK_ERR(appsrcAudio != NULL, STATUS_INTERNAL_ERROR, "[KVS %s] Cannot find appsrc audio", roleType);
        CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) appsrcAudio, onGstAudioFrameReady));
        g_object_set(G_OBJECT(appsrcAudio), "caps", audiocaps, NULL);
        gst_caps_unref(audiocaps);
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
