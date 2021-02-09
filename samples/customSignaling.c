#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include "Samples.h"

struct MySession {
    RtcConfiguration rtcConfig;
    PRtcPeerConnection pPeerConnection;

    RtcMediaStreamTrack videoTrack;
    PRtcRtpTransceiver transceiver;
    RTC_PEER_CONNECTION_STATE connectionState;

    BOOL iceGatheringDone;
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

VOID onConnectionStateChange_(UINT64 session64, RTC_PEER_CONNECTION_STATE state)
{
    printf("onConnectionStateChange: %s\n", connection_state_to_string(state));
    session.connectionState = state;
}

void onRemoteMessage(UINT64 session64, PRtcDataChannel pRtcDataChannel, BOOL binary, PBYTE data, UINT32 len)
{
    char text[128] = {0};
    snprintf(text, MIN(len, 127) + 1, "%s", data);
    printf("onRemoteMessage %s %s %s\n", pRtcDataChannel->name, binary ? "bin" : "text", text);
    dataChannelSend(pRtcDataChannel, FALSE, "pong", 4);
}

void onRemoteDataChannel(UINT64 session64, PRtcDataChannel pRtcDataChannel)
{
    printf("remote data channel '%s'\n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, session64, onRemoteMessage);
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
    session.rtcConfig.kvsRtcConfiguration.iceConnectionCheckTimeout = (60 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    UINT64 session64 = (UINT64) &session;
    STRNCPY(session.rtcConfig.iceServers[0].urls, "stun:stun.l.google.com:19302", MAX_ICE_CONFIG_URI_LEN);

    CHK_STATUS(createPeerConnection(&session.rtcConfig, &session.pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(session.pPeerConnection, session64, onIceCandidate));
    CHK_STATUS(peerConnectionOnConnectionStateChange(session.pPeerConnection, session64, onConnectionStateChange_));
    CHK_STATUS(peerConnectionOnDataChannel(session.pPeerConnection, session64, onRemoteDataChannel));
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
    CHK_STATUS(setLocalDescription(session.pPeerConnection, &answerSdp));

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
        SampleConfiguration config = {0};
        config.streamingSessionListReadLock = MUTEX_CREATE(FALSE);
        SampleStreamingSession sampleStreamingSession = {0};
        sampleStreamingSession.pVideoRtcRtpTransceiver = session.transceiver;
        config.streamingSessionCount = 1;
        config.sampleStreamingSessionList[0] = &sampleStreamingSession;
        sendVideoPackets(&config);
    }

    return 0;
CleanUp:
    return retStatus;
}