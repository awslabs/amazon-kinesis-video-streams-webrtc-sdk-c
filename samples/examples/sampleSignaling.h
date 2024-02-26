#ifndef KVS_SDK_SIGNALING_H
#define KVS_SDK_SIGNALING_H


#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

typedef struct {
    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PAwsCredentialProvider pCredentialProvider;
    SIGNALING_CLIENT_HANDLE signalingClientHandle;
} SignalingCtx, *PSignalingCtx;

#ifdef __cplusplus
}
#endif
#endif //KVS_SDK_SIGNALING_H
