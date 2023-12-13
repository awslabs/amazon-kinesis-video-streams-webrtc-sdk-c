/*******************************************
Signaling State Machine internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_SIGNALING_STATE_MACHINE__
#define __KINESIS_VIDEO_WEBRTC_SIGNALING_STATE_MACHINE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Signaling states definitions
 */
#define SIGNALING_STATE_NONE                   ((UINT64) 0)
#define SIGNALING_STATE_NEW                    ((UINT64) (1 << 0))
#define SIGNALING_STATE_GET_TOKEN              ((UINT64) (1 << 1))
#define SIGNALING_STATE_DESCRIBE               ((UINT64) (1 << 2))
#define SIGNALING_STATE_CREATE                 ((UINT64) (1 << 3))
#define SIGNALING_STATE_GET_ENDPOINT           ((UINT64) (1 << 4))
#define SIGNALING_STATE_GET_ICE_CONFIG         ((UINT64) (1 << 5))
#define SIGNALING_STATE_READY                  ((UINT64) (1 << 6))
#define SIGNALING_STATE_CONNECT                ((UINT64) (1 << 7))
#define SIGNALING_STATE_CONNECTED              ((UINT64) (1 << 8))
#define SIGNALING_STATE_DISCONNECTED           ((UINT64) (1 << 9))
#define SIGNALING_STATE_DELETE                 ((UINT64) (1 << 10))
#define SIGNALING_STATE_DELETED                ((UINT64) (1 << 11))
#define SIGNALING_STATE_DESCRIBE_MEDIA         ((UINT64) (1 << 12))
#define SIGNALING_STATE_JOIN_SESSION           ((UINT64) (1 << 13))
#define SIGNALING_STATE_JOIN_SESSION_WAITING   ((UINT64) (1 << 14))
#define SIGNALING_STATE_JOIN_SESSION_CONNECTED ((UINT64) (1 << 15))

// Indicates infinite retries
#define INFINITE_RETRY_COUNT_SENTINEL 0

// Whether to step the state machine
STATUS signalingStateMachineIterator(PSignalingClient, UINT64, UINT64);

STATUS acceptSignalingStateMachineState(PSignalingClient, UINT64);
SIGNALING_CLIENT_STATE getSignalingStateFromStateMachineState(UINT64);

/**
 * Signaling state machine callbacks
 */
STATUS fromNewSignalingState(UINT64, PUINT64);
STATUS executeNewSignalingState(UINT64, UINT64);
STATUS fromGetTokenSignalingState(UINT64, PUINT64);
STATUS executeGetTokenSignalingState(UINT64, UINT64);
STATUS fromDescribeSignalingState(UINT64, PUINT64);
STATUS executeDescribeSignalingState(UINT64, UINT64);
STATUS fromDescribeMediaStorageConfState(UINT64, PUINT64);
STATUS executeDescribeMediaStorageConfState(UINT64, UINT64);
STATUS fromCreateSignalingState(UINT64, PUINT64);
STATUS executeCreateSignalingState(UINT64, UINT64);
STATUS fromGetEndpointSignalingState(UINT64, PUINT64);
STATUS executeGetEndpointSignalingState(UINT64, UINT64);
STATUS fromGetIceConfigSignalingState(UINT64, PUINT64);
STATUS executeGetIceConfigSignalingState(UINT64, UINT64);
STATUS fromReadySignalingState(UINT64, PUINT64);
STATUS executeReadySignalingState(UINT64, UINT64);
STATUS fromConnectSignalingState(UINT64, PUINT64);
STATUS executeConnectSignalingState(UINT64, UINT64);
STATUS fromJoinStorageSessionState(UINT64, PUINT64);
STATUS executeJoinStorageSessionState(UINT64, UINT64);
STATUS fromJoinStorageSessionWaitingState(UINT64, PUINT64);
STATUS executeJoinStorageSessionWaitingState(UINT64, UINT64);
STATUS fromJoinStorageSessionConnectedState(UINT64, PUINT64);
STATUS executeJoinStorageSessionConnectedState(UINT64, UINT64);
STATUS fromConnectedSignalingState(UINT64, PUINT64);
STATUS executeConnectedSignalingState(UINT64, UINT64);
STATUS fromDisconnectedSignalingState(UINT64, PUINT64);
STATUS executeDisconnectedSignalingState(UINT64, UINT64);
STATUS fromDeleteSignalingState(UINT64, PUINT64);
STATUS executeDeleteSignalingState(UINT64, UINT64);
STATUS fromDeletedSignalingState(UINT64, PUINT64);
STATUS executeDeletedSignalingState(UINT64, UINT64);

STATUS defaultSignalingStateTransitionHook(UINT64, PUINT64);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_SIGNALING_STATE_MACHINE__ */
