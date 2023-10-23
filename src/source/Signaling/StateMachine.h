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
#define SIGNALING_STATE_NONE           ((UINT64) 0) // 0
#define SIGNALING_STATE_NEW            ((UINT64) (1 << 0)) // 1 -> 0x01
#define SIGNALING_STATE_GET_TOKEN      ((UINT64) (1 << 1)) // 2 -> 0x02
#define SIGNALING_STATE_DESCRIBE       ((UINT64) (1 << 2)) // 4 -> 0x04
#define SIGNALING_STATE_CREATE         ((UINT64) (1 << 3)) // 8 -> 0x08
#define SIGNALING_STATE_GET_ENDPOINT   ((UINT64) (1 << 4)) // 16 -> 0x10
#define SIGNALING_STATE_GET_ICE_CONFIG ((UINT64) (1 << 5)) // 32 -> 0x20
#define SIGNALING_STATE_READY          ((UINT64) (1 << 6)) // 64 -> 0x40
#define SIGNALING_STATE_CONNECT        ((UINT64) (1 << 7)) // 128 -> 0x80
#define SIGNALING_STATE_CONNECTED      ((UINT64) (1 << 8)) // 256 -> 0x100
#define SIGNALING_STATE_DISCONNECTED   ((UINT64) (1 << 9)) // 512 -> 0x200
#define SIGNALING_STATE_DELETE         ((UINT64) (1 << 10)) // 1024 -> 0x400
#define SIGNALING_STATE_DELETED        ((UINT64) (1 << 11)) // 2048 -> 0x800

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
