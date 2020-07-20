/*******************************************
Shared include file for the samples
*******************************************/
#ifndef __KINESIS_VIDEO_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#define NUMBER_OF_H264_FRAME_FILES               403
#define NUMBER_OF_OPUS_FRAME_FILES               618
#define DEFAULT_FPS_VALUE                        25
#define DEFAULT_MAX_CONCURRENT_STREAMING_SESSION 10

#define SAMPLE_MASTER_CLIENT_ID "ProducerMaster"
#define SAMPLE_VIEWER_CLIENT_ID "ConsumerViewer"
#define SAMPLE_CHANNEL_NAME     (PCHAR) "ScaryTestChannel"

#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)

#define ASYNC_ICE_CONFIG_INFO_WAIT_TIMEOUT (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define ICE_CONFIG_INFO_POLL_PERIOD        (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define CA_CERT_PEM_FILE_EXTENSION ".pem"

#define FILE_LOGGING_BUFFER_SIZE (100 * 1024)
#define MAX_NUMBER_OF_LOG_FILES  5
typedef enum {
    SAMPLE_STREAMING_VIDEO_ONLY,
    SAMPLE_STREAMING_AUDIO_VIDEO,
} SampleStreamingMediaType;

typedef struct __SampleStreamingSession SampleStreamingSession;
typedef struct __SampleStreamingSession* PSampleStreamingSession;

typedef struct {
    volatile ATOMIC_BOOL appTerminateFlag;
    volatile ATOMIC_BOOL interrupted;
    volatile ATOMIC_BOOL mediaThreadStarted;
    volatile ATOMIC_BOOL updatingSampleStreamingSessionList;
    volatile ATOMIC_BOOL recreateSignalingClient;
    volatile SIZE_T streamingSessionListReadingThreadCount;
    BOOL useTestSrc;
    ChannelInfo channelInfo;
    PCHAR pCaCertPath;
    PAwsCredentialProvider pCredentialProvider;
    SIGNALING_CLIENT_HANDLE signalingClientHandle;
    PBYTE pAudioFrameBuffer;
    UINT32 audioBufferSize;
    PBYTE pVideoFrameBuffer;
    UINT32 videoBufferSize;
    TID videoSenderTid;
    TID audioSenderTid;
    SampleStreamingMediaType mediaType;
    startRoutine audioSource;
    startRoutine videoSource;
    startRoutine receiveAudioVideoSource;
    RtcOnDataChannel onDataChannel;

    MUTEX sampleConfigurationObjLock;
    CVAR cvar;
    BOOL trickleIce;
    BOOL useTurn;
    BOOL enableFileLogging;
    UINT64 customData;
    PSampleStreamingSession sampleStreamingSessionList[DEFAULT_MAX_CONCURRENT_STREAMING_SESSION];
    UINT32 streamingSessionCount;
    UINT32 iceUriCount;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
} SampleConfiguration, *PSampleConfiguration;

typedef VOID (*StreamSessionShutdownCallback)(UINT64, PSampleStreamingSession);

struct __SampleStreamingSession {
    volatile ATOMIC_BOOL terminateFlag;
    volatile ATOMIC_BOOL candidateGatheringDone;
    volatile ATOMIC_BOOL peerIdReceived;
    volatile ATOMIC_BOOL sdpOfferAnswerExchanged;
    volatile SIZE_T frameIndex;
    PRtcPeerConnection pPeerConnection;
    PRtcRtpTransceiver pVideoRtcRtpTransceiver;
    PRtcRtpTransceiver pAudioRtcRtpTransceiver;
    RtcSessionDescriptionInit answerSessionDescriptionInit;
    PSampleConfiguration pSampleConfiguration;
    UINT32 audioTimestamp;
    UINT32 videoTimestamp;
    CHAR peerId[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    TID receiveAudioVideoSenderTid;
    UINT64 firstSdpMsgReceiveTime;

    // this is called when the SampleStreamingSession is being freed
    StreamSessionShutdownCallback shutdownCallback;
    UINT64 shutdownCallbackCustomData;
};

VOID sigintHandler(INT32);
STATUS readFrameFromDisk(PBYTE, PUINT32, PCHAR);
PVOID sendVideoPackets(PVOID);
PVOID sendAudioPackets(PVOID);
PVOID sendGstreamerAudioVideo(PVOID);
PVOID sampleReceiveAudioFrame(PVOID args);
STATUS createSampleConfiguration(PCHAR, SIGNALING_CHANNEL_ROLE_TYPE, BOOL, BOOL, PSampleConfiguration*);
STATUS freeSampleConfiguration(PSampleConfiguration*);
STATUS viewerMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS signalingClientStateChanged(UINT64, SIGNALING_CLIENT_STATE);
STATUS masterMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS handleAnswer(PSampleConfiguration, PSampleStreamingSession, PSignalingMessage);
STATUS handleOffer(PSampleConfiguration, PSampleStreamingSession, PSignalingMessage);
STATUS handleRemoteCandidate(PSampleStreamingSession, PSignalingMessage);
STATUS initializePeerConnection(PSampleConfiguration, PRtcPeerConnection*);
STATUS lookForSslCert(PSampleConfiguration*);
STATUS createSampleStreamingSession(PSampleConfiguration, PCHAR, BOOL, PSampleStreamingSession*);
STATUS freeSampleStreamingSession(PSampleStreamingSession*);
STATUS streamingSessionOnShutdown(PSampleStreamingSession, UINT64, StreamSessionShutdownCallback);
STATUS respondWithAnswer(PSampleStreamingSession);
STATUS resetSampleConfigurationState(PSampleConfiguration);
VOID sampleFrameHandler(UINT64, PFrame);
VOID sampleBandwidthEstimationHandler(UINT64, DOUBLE);
VOID onDataChannel(UINT64, PRtcDataChannel);
VOID onConnectionStateChange(UINT64, RTC_PEER_CONNECTION_STATE);
STATUS sessionCleanupWait(PSampleConfiguration);
STATUS awaitGetIceConfigInfoCount(SIGNALING_CLIENT_HANDLE, PUINT32);
STATUS logSignalingClientStats(PSignalingClientMetrics);
STATUS logSelectedIceCandidatesInformation(PSampleStreamingSession);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_SAMPLE_INCLUDE__ */
