#include "Samples.h"

STATUS sendTestFrames(PRtcRtpTransceiver);

INT32 main(INT32 argc, CHAR *argv[]) {
    STATUS retStatus = STATUS_SUCCESS;
    PRtcPeerConnection pRtcPeerConnection = NULL;
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    CHAR buff[50000] = {0}, buff2[50000] = {0};
    UINT32 buffSize = SIZEOF(buff), buff2Size = SIZEOF(buff2);
    RtcConfiguration configuration;
    RtcMediaStreamTrack videoTrack;
    PRtcRtpTransceiver pVideoRtcRtpTransceiver;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    CHK_STATUS(initKvsWebRtc());

    CHK_STATUS(createPeerConnection(&configuration, &pRtcPeerConnection));
    CHK_STATUS(addSupportedCodec(pRtcPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));

    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pRtcPeerConnection, &videoTrack, NULL, &pVideoRtcRtpTransceiver));

    CHK_STATUS(base64Decode(argv[1], STRLEN(argv[1]), (PBYTE) buff, &buffSize));
    CHK_STATUS(deserializeSessionDescriptionInit(buff, STRLEN(buff), &rtcSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit));

    CHK_STATUS(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit));
    CHK_STATUS(setLocalDescription(pRtcPeerConnection, &rtcSessionDescriptionInit));

    buffSize = SIZEOF(buff);
    CHK_STATUS(serializeSessionDescriptionInit(&rtcSessionDescriptionInit, buff, &buffSize));
    CHK_STATUS(base64Encode(buff, buffSize - 1, buff2, &buff2Size));

    printf("\n\n %s \n\n", buff2);

    CHK_STATUS(sendTestFrames(pVideoRtcRtpTransceiver));

CleanUp:
    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath) {
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;

    CHK(pSize != NULL, STATUS_NULL_ARG);
    size = *pSize;

    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));

CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

STATUS sendTestFrames(PRtcRtpTransceiver pRtcRtpTransceiver) {
    STATUS retStatus = STATUS_SUCCESS;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 i;
    STATUS status;

    frame.presentationTs = 0;
    frame.frameData = MEMALLOC(500000);

    while (TRUE) {
        // ./h264SampleFrames/frame-281.h264
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%03d.h264", fileIndex);
        // SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-281.h264");
        frame.size = 500000;

        CHK_STATUS(readFrameFromDisk(NULL, &frame.size, filePath));
        CHK_STATUS(readFrameFromDisk(frame.frameData, &frame.size, filePath));

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        CHK_STATUS(writeFrame(pRtcRtpTransceiver, &frame));
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION);
    }

CleanUp:
    return retStatus;

}
