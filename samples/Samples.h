/*******************************************
Shared include file for the samples
*******************************************/
#ifndef __KINESIS_VIDEO_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_INCLUDE__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, samples_i, 1) // for byte alignment

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#define NUMBER_OF_H264_FRAME_FILES                                              403
#define NUMBER_OF_OPUS_FRAME_FILES                                              618
#define DEFAULT_FPS_VALUE                                                       25
#define DEFAULT_TERMINATION_POLLING_INTERVAL                                    5 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KINESIS_VIDEO_BETA_CONTROL_PLANE_URL                                    "https://beta.kinesisvideo.us-west-2.amazonaws.com"
#define KINESIS_VIDEO_BETA_STUN_URL                                             "stun:stun.beta.kinesisvideo.us-west-2.amazonaws.com:443"

#define SAMPLE_MASTER_CLIENT_ID                                                 "ProducerMaster"
#define SAMPLE_VIEWER_CLIENT_ID                                                 "ConsumerViewer"
#define SAMPLE_CHANNEL_NAME                                                     (PCHAR) "ScaryTestChannel"

#define SAMPLE_AUDIO_FRAME_DURATION                                             (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_VIDEO_FRAME_DURATION                                             (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)

typedef enum {
    SAMPLE_STREAMING_VIDEO_ONLY,
    SAMPLE_STREAMING_AUDIO_VIDEO,
} SampleStreamingMediaType;

typedef struct {
    volatile ATOMIC_BOOL terminateFlag;
    volatile ATOMIC_BOOL peerIdReceived;
    volatile ATOMIC_BOOL peerConnectionStarted;
    volatile ATOMIC_BOOL interrupted;
    volatile ATOMIC_BOOL candidateGatheringDone;
    volatile SIZE_T frameIndex;
    PCHAR pCaCertPath;
    PCHAR pRegion;
    PAwsCredentialProvider pCredentialProvider;
    PRtcPeerConnection pPeerConnection;
    PRtcRtpTransceiver pVideoRtcRtpTransceiver;
    PRtcRtpTransceiver pAudioRtcRtpTransceiver;
    RtcSessionDescriptionInit answerSessionDescriptionInit;
    SIGNALING_CLIENT_HANDLE signalingClientHandle;
    CHAR peerId[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    PBYTE pAudioFrameBuffer;
    UINT32 audioBufferSize;
    PBYTE pVideoFrameBuffer;
    UINT32 videoBufferSize;
    TID replyTid;
    TID videoSenderTid;
    TID audioSenderTid;
    TID receiveAudioVideoSenderTid;
    SampleStreamingMediaType mediaType;
    startRoutine audioSource;
    startRoutine videoSource;
    startRoutine receiveAudioVideoSource;
    RtcOnDataChannel onDataChannel;
    UINT32 audioTimestamp;
    UINT32 videoTimestamp;
    MUTEX sampleConfigurationObjLock;
    CVAR cvar;
    BOOL trickleIce;
    BOOL useTurn;
    UINT64 customData;
} SampleConfiguration, *PSampleConfiguration;

STATUS readFrameFromDisk(PBYTE, PUINT32, PCHAR);
PVOID sendVideoPackets(PVOID);
PVOID sendAudioPackets(PVOID);
PVOID sendGstreamerAudioVideo(PVOID);
STATUS createSampleConfiguration(PSampleConfiguration*, BOOL, BOOL, BOOL);
STATUS freeSampleConfiguration(PSampleConfiguration*);
STATUS viewerMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS signalingClientStateChanged(UINT64, SIGNALING_CLIENT_STATE);
STATUS masterMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS handleAnswer(PSampleConfiguration, PSignalingMessage);
STATUS handleOffer(PSampleConfiguration, PSignalingMessage);
STATUS handleRemoteCandidate(PSampleConfiguration, PSignalingMessage);
STATUS initializePeerConnection(PSampleConfiguration);
STATUS respondWithAnswer(PSampleConfiguration);
STATUS resetSampleConfigurationState(PSampleConfiguration);
VOID sampleFrameHandler(UINT64, PFrame);
VOID onDataChannel(UINT64, PRtcDataChannel);
VOID onConnectionStateChange(UINT64, RTC_PEER_CONNECTION_STATE);

#pragma pack(pop, samples_i)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_SAMPLE_INCLUDE__ */
