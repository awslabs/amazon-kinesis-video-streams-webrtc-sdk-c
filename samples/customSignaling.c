#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>

#define DEFAULT_FPS_VALUE           25
#define NUMBER_OF_H264_FRAME_FILES  1500
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)

struct MySession {
    volatile ATOMIC_BOOL appTerminateFlag;
    RtcConfiguration rtcConfig;
    PRtcPeerConnection pPeerConnection;
    UINT64 u64_node;

    RtcMediaStreamTrack videoTrack;
    PRtcRtpTransceiver transceiver;
    RTC_PEER_CONNECTION_STATE connectionState;

    BOOL iceGatheringDone;
    PBYTE pVideoFrameBuffer;
    UINT32 videoBufferSize;
    MUTEX streamingSessionListReadLock;
    UINT32 streamingSessionCount;
};

static const char* ConnectionStateNames[] = {
    "RTC_PEER_CONNECTION_STATE_NONE",      "RTC_PEER_CONNECTION_STATE_NEW",          "RTC_PEER_CONNECTION_STATE_CONNECTING",
    "RTC_PEER_CONNECTION_STATE_CONNECTED", "RTC_PEER_CONNECTION_STATE_DISCONNECTED", "RTC_PEER_CONNECTION_STATE_FAILED",
    "RTC_PEER_CONNECTION_STATE_CLOSED",    "RTC_PEER_CONNECTION_TOTAL_STATE_COUNT",  NULL};

static const char* connection_state_to_string(RTC_PEER_CONNECTION_STATE state)
{
    BOOL rangeok = state >= RTC_PEER_CONNECTION_STATE_NONE && state <= RTC_PEER_CONNECTION_TOTAL_STATE_COUNT;
    return rangeok ? ConnectionStateNames[state] : "RTC_PEER_CONNECTION_TOTAL_STATE_UNKNOWN";
}

// https://stackoverflow.com/questions/39546500/how-to-make-scanf-to-read-more-than-4095-characters-given-as-input
int clear_icanon(void)
{
    struct termios settings = {0};
    if (tcgetattr(STDIN_FILENO, &settings) < 0) {
        perror("error in tcgetattr");
        return 0;
    }

    settings.c_lflag &= ~ICANON;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) < 0) {
        perror("error in tcsetattr");
        return 0;
    }
    return 1;
}

static struct MySession session = {0};
VOID onIceCandidate(UINT64 session64, PCHAR candidate)
{
    STATUS retStatus = 0;

    printf("onIceCandidate: %s\n", candidate != NULL ? candidate : "NULL");
    if (candidate == NULL) {
        RtcSessionDescriptionInit answerSdp = {0};
        CHK_STATUS(createAnswer(session.pPeerConnection, &answerSdp));
        CHK_STATUS(setLocalDescription(session.pPeerConnection, &answerSdp));
        printf("answer: '%s'\n", answerSdp.sdp);
        char json[8192] = {0};
        UINT32 sz = 8192;
        CHK_STATUS(serializeSessionDescriptionInit(&answerSdp, json, &sz));
        printf("---- Please copy and send this message to the other peer ----\n");
        printf("%s\n", json);
        session.iceGatheringDone = TRUE;
    }
CleanUp:
    if (STATUS_FAILED(retStatus))
        printf("onIceCandidate failed 0x%x\n", retStatus);
}

VOID onConnectionStateChange(UINT64 session64, RTC_PEER_CONNECTION_STATE state)
{
    printf("onConnectionStateChange: %s\n", connection_state_to_string(state));
    session.connectionState = state;
}

void onRemoteMessage(UINT64 session64, PRtcDataChannel pRtcDataChannel, BOOL binary, PBYTE data, UINT32 len)
{
    char text[128] = {0};
    snprintf(text, MIN(len, 128), "%s", data);
    printf("onRemoteMessage %s %s %s\n", pRtcDataChannel->name, binary ? "bin" : "text", text);
    dataChannelSend(pRtcDataChannel, FALSE, "pong", 4);
}

void onRemoteDataChannel(UINT64 session64, PRtcDataChannel pRtcDataChannel)
{
    printf("remote data channel '%s'\n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, session64, onRemoteMessage);
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;

    if (pSize == NULL) {
        printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    size = *pSize;

    // Get the size and read into frame
    retStatus = readFile(frameFilePath, TRUE, pFrame, &size);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFile(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

PVOID sendVideoPackets()
{
    printf("sendVideoPackets\n");
    STATUS retStatus = STATUS_SUCCESS;
    RtcEncoderStats encoderStats;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT64 startTime, lastFrameTime, elapsed;
    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));

    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&session.appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        snprintf(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%04d.h264", fileIndex);

        retStatus = readFrameFromDisk(NULL, &frameSize, filePath);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // Re-alloc if needed
        if (frameSize > session.videoBufferSize) {
            session.pVideoFrameBuffer = (PBYTE) MEMREALLOC(session.pVideoFrameBuffer, frameSize);
            if (session.pVideoFrameBuffer == NULL) {
                printf("[KVS Master] Video frame Buffer reallocation failed...%s (code %d)\n", strerror(errno), errno);
                printf("[KVS Master] MEMREALLOC(): operation returned status code: 0x%08x \n", STATUS_NOT_ENOUGH_MEMORY);
                goto CleanUp;
            }

            session.videoBufferSize = frameSize;
        }

        frame.frameData = session.pVideoFrameBuffer;
        frame.size = frameSize;

        retStatus = readFrameFromDisk(frame.frameData, &frameSize, filePath);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // based on bitrate of samples/h264SampleFrames/frame-*
        encoderStats.width = 640;
        encoderStats.height = 480;
        encoderStats.targetBitrate = 262000;
        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        status = writeFrame(session.transceiver, &frame);
        encoderStats.encodeTimeMsec = 4; // update encode time to an arbitrary number to demonstrate stats update
        updateEncoderStats(session.transceiver, &encoderStats);

        if (status != STATUS_SRTP_NOT_READY_YET && status != STATUS_SUCCESS) {
            DLOGD("writeFrame() failed with 0x%08x\n", status);
        }

        // Adjust sleep in the case the sleep itself and writeFrame take longer than expected. Since sleep makes sure that the thread
        // will be paused at least until the given amount, we can assume that there's no too early frame scenario.
        // Also, it's very unlikely to have a delay greater than SAMPLE_VIDEO_FRAME_DURATION, so the logic assumes that this is always
        // true for simplicity.
        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    return (PVOID)(ULONG_PTR) retStatus;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_DEBUG);
    clear_icanon(); // Changes the input mode of terminal from canonical mode to non canonical mode to allow copy-paste of over 4096 bytes
    // equivalent to running "stty -icanon"

    STATUS retStatus = 0;
    CHK_STATUS(initKvsWebRtc());

    printf("---- Please paste in the message here from the other peer ----\n");
    RtcSessionDescriptionInit offerSdp = {SDP_TYPE_OFFER};

    char offer[8192] = {0};
    fgets(offer, 8192, stdin);
    CHK_STATUS(deserializeSessionDescriptionInit(offer, STRLEN(offer), &offerSdp));
    printf("%s\n", offerSdp.sdp);

    session.rtcConfig.kvsRtcConfiguration.iceLocalCandidateGatheringTimeout = (5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    session.rtcConfig.kvsRtcConfiguration.iceCandidateNominationTimeout = (120 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    session.u64_node = (UINT64) &session;
    STRNCPY(session.rtcConfig.iceServers[0].urls, "stun:stun.l.google.com:19302", MAX_ICE_CONFIG_URI_LEN);

    CHK_STATUS(createPeerConnection(&session.rtcConfig, &session.pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(session.pPeerConnection, session.u64_node, onIceCandidate));
    CHK_STATUS(peerConnectionOnConnectionStateChange(session.pPeerConnection, session.u64_node, onConnectionStateChange));
    CHK_STATUS(peerConnectionOnDataChannel(session.pPeerConnection, session.u64_node, onRemoteDataChannel));
    CHK_STATUS(addSupportedCodec(session.pPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));
    CHK_STATUS(addSupportedCodec(session.pPeerConnection, RTC_CODEC_OPUS));
    RtcRtpTransceiverInit trackinit = {RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY};
    session.videoTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    session.videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    STRNCPY(session.videoTrack.streamId, "streamId", MAX_MEDIA_STREAM_ID_LEN);
    STRNCPY(session.videoTrack.trackId, "trackId", MAX_MEDIA_STREAM_ID_LEN);

    CHK_STATUS(addTransceiver(session.pPeerConnection, &session.videoTrack, &trackinit, &session.transceiver));

    RtcSessionDescriptionInit answerSdp = {0};

    CHK_STATUS(setRemoteDescription(session.pPeerConnection, &offerSdp));
    //    CHK_STATUS(createAnswer(session.pPeerConnection, &answerSdp));
    CHK_STATUS(setLocalDescription(session.pPeerConnection, &answerSdp));
    //    printf("answer: '%s'\n", answerSdp.sdp);

    while (1) {
        if (!session.iceGatheringDone) {
            printf("gathering...\n");
        }
        if (session.connectionState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
            break;
        }
        sleep(1);
    }
    if (session.connectionState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
        // send frames
        sendVideoPackets();
    }

    return 0;
CleanUp:
    return retStatus;
}