/***************************************************
TURN Connections State Machine internal include file
***************************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_TURN_STATE_MACHINE__
#define __KINESIS_VIDEO_WEBRTC_TURN_STATE_MACHINE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TURN states definitions
 *
 * TURN_STATE_NONE:                        Dummy state
 * TURN_STATE_NEW:                         State at creation
 * TURN_STATE_CHECK_SOCKET_CONNECTION:
 * TURN_STATE_GET_CREDENTIALS:
 * TURN_STATE_ALLOCATION:
 * TURN_STATE_CREATE_PERMISSION:
 * TURN_STATE_BIND_CHANNEL:
 * TURN_STATE_READY:
 * TURN_STATE_CLEAN_UP:
 * TURN_STATE_FAILED:
 */

#define TURN_STATE_NONE                    ((UINT64) 0)        // 0x000000000
#define TURN_STATE_NEW                     ((UINT64) (1 << 0)) // 0x000000001
#define TURN_STATE_CHECK_SOCKET_CONNECTION ((UINT64) (1 << 1)) // 0x000000002
#define TURN_STATE_GET_CREDENTIALS         ((UINT64) (1 << 2)) // 0x000000004
#define TURN_STATE_ALLOCATION              ((UINT64) (1 << 3)) // 0x000000008
#define TURN_STATE_CREATE_PERMISSION       ((UINT64) (1 << 4)) // 0x000000010
#define TURN_STATE_BIND_CHANNEL            ((UINT64) (1 << 5)) // 0x000000020
#define TURN_STATE_READY                   ((UINT64) (1 << 6)) // 0x000000040
#define TURN_STATE_CLEAN_UP                ((UINT64) (1 << 7)) // 0x000000080
#define TURN_STATE_FAILED                  ((UINT64) (1 << 8)) // 0x000000100

#define TURN_STATE_NONE_STR                    (PCHAR) "TURN_STATE_NONE"
#define TURN_STATE_NEW_STR                     (PCHAR) "TURN_STATE_NEW"
#define TURN_STATE_CHECK_SOCKET_CONNECTION_STR (PCHAR) "TURN_STATE_CHECK_SOCKET_CONNECTION"
#define TURN_STATE_GET_CREDENTIALS_STR         (PCHAR) "TURN_STATE_GET_CREDENTIALS"
#define TURN_STATE_ALLOCATION_STR              (PCHAR) "TURN_STATE_ALLOCATION"
#define TURN_STATE_CREATE_PERMISSION_STR       (PCHAR) "TURN_STATE_CREATE_PERMISSION"
#define TURN_STATE_BIND_CHANNEL_STR            (PCHAR) "TURN_STATE_BIND_CHANNEL"
#define TURN_STATE_READY_STR                   (PCHAR) "TURN_STATE_READY"
#define TURN_STATE_CLEAN_UP_STR                (PCHAR) "TURN_STATE_CLEAN_UP"
#define TURN_STATE_FAILED_STR                  (PCHAR) "TURN_STATE_FAILED"
#define TURN_STATE_UNKNOWN_STR                 (PCHAR) "TURN_STATE_UNKNOWN"

// Whether to step the state machine
STATUS stepTurnConnectionStateMachine(PTurnConnection);
STATUS acceptTurnMachineState(PTurnConnection, UINT64);
PCHAR turnStateGetStateStr(UINT64 state);

/**
 * Turn state machine callbacks
 */
STATUS fromNewTurnState(UINT64, PUINT64);
STATUS executeNewTurnState(UINT64, UINT64);
STATUS fromCheckSocketConnectionTurnState(UINT64, PUINT64);
STATUS executeCheckSocketConnectionTurnState(UINT64, UINT64);
STATUS fromGetCredentialsTurnState(UINT64, PUINT64);
STATUS executeGetCredentialsTurnState(UINT64, UINT64);
STATUS fromAllocationTurnState(UINT64, PUINT64);
STATUS executeAllocationTurnState(UINT64, UINT64);
STATUS fromCreatePermissionTurnState(UINT64, PUINT64);
STATUS executeCreatePermissionTurnState(UINT64, UINT64);
STATUS fromBindChannelTurnState(UINT64, PUINT64);
STATUS executeBindChannelTurnState(UINT64, UINT64);
STATUS fromReadyTurnState(UINT64, PUINT64);
STATUS executeReadyTurnState(UINT64, UINT64);
STATUS fromCleanUpTurnState(UINT64, PUINT64);
STATUS executeCleanUpTurnState(UINT64, UINT64);
STATUS fromFailedTurnState(UINT64, PUINT64);
STATUS executeFailedTurnState(UINT64, UINT64);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_TURN_STATE_MACHINE__ */
