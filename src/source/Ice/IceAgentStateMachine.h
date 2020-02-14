/*******************************************
Signaling State Machine internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_ICE_STATE_MACHINE__
#define __KINESIS_VIDEO_WEBRTC_ICE_STATE_MACHINE__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Ice states definitions
 *
 * ICE_AGENT_STATE_NONE:                Dummy state
 * ICE_AGENT_STATE_NEW:                 State at creation
 * ICE_AGENT_STATE_GATHERING:           Gathering local candidates
 * ICE_AGENT_STATE_CHECK_CONNECTION:    Checking candidate pair connectivity
 * ICE_AGENT_STATE_CONNECTED:           At least one working candidate pair
 * ICE_AGENT_STATE_NOMINATING:          Waiting for connectivity check to succeed for the nominated cadidate pair
 * ICE_AGENT_STATE_READY:               Selected candidate pair is now final
 * ICE_AGENT_STATE_DISCONNECTED:        Lost connection after ICE_AGENT_STATE_READY
 * ICE_AGENT_STATE_FAILED:
 */
#define ICE_AGENT_STATE_NONE                            ((UINT64) 0)
#define ICE_AGENT_STATE_NEW                             ((UINT64) (1 << 0))
#define ICE_AGENT_STATE_GATHERING                       ((UINT64) (1 << 1))
#define ICE_AGENT_STATE_WAITING_REMOTE_CREDENTIAL       ((UINT64) (1 << 2))
#define ICE_AGENT_STATE_CHECK_CONNECTION                ((UINT64) (1 << 3))
#define ICE_AGENT_STATE_CONNECTED                       ((UINT64) (1 << 4))
#define ICE_AGENT_STATE_NOMINATING                      ((UINT64) (1 << 5))
#define ICE_AGENT_STATE_READY                           ((UINT64) (1 << 6))
#define ICE_AGENT_STATE_DISCONNECTED                    ((UINT64) (1 << 7))
#define ICE_AGENT_STATE_FAILED                          ((UINT64) (1 << 8))

#define ICE_AGENT_STATE_NONE_STR                        (PCHAR) "ICE_AGENT_STATE_NONE"
#define ICE_AGENT_STATE_NEW_STR                         (PCHAR) "ICE_AGENT_STATE_NEW"
#define ICE_AGENT_STATE_GATHERING_STR                   (PCHAR) "ICE_AGENT_STATE_GATHERING"
#define ICE_AGENT_STATE_WAITING_REMOTE_CREDENTIAL_STR	(PCHAR) "ICE_AGENT_STATE_WAITING_REMOTE_CREDENTIAL"
#define ICE_AGENT_STATE_CHECK_CONNECTION_STR            (PCHAR) "ICE_AGENT_STATE_CHECK_CONNECTION"
#define ICE_AGENT_STATE_CONNECTED_STR                   (PCHAR) "ICE_AGENT_STATE_CONNECTED"
#define ICE_AGENT_STATE_NOMINATING_STR                  (PCHAR) "ICE_AGENT_STATE_NOMINATING"
#define ICE_AGENT_STATE_READY_STR                       (PCHAR) "ICE_AGENT_STATE_READY"
#define ICE_AGENT_STATE_DISCONNECTED_STR                (PCHAR) "ICE_AGENT_STATE_DISCONNECTED"
#define ICE_AGENT_STATE_FAILED_STR                      (PCHAR) "ICE_AGENT_STATE_FAILED"

// Whether to step the state machine
STATUS stepIceAgentStateMachine(PIceAgent);
STATUS acceptIceAgentMachineState(PIceAgent, UINT64);
STATUS iceAgentStateMachineCheckDisconnection(PIceAgent, PUINT64);
PCHAR iceAgentStateToString(UINT64);

/**
 * Signaling state machine callbacks
 */
STATUS fromNewIceAgentState(UINT64, PUINT64);
STATUS executeNewIceAgentState(UINT64, UINT64);
STATUS fromGatheringIceAgentState(UINT64, PUINT64);
STATUS executeGatheringIceAgentState(UINT64, UINT64);
STATUS fromWaitingRemoteCredentialIceAgentState(UINT64, PUINT64);
STATUS executeWaitingRemoteCredentialIceAgentState(UINT64, UINT64);
STATUS fromCheckConnectionIceAgentState(UINT64, PUINT64);
STATUS executeCheckConnectionIceAgentState(UINT64, UINT64);
STATUS fromConnectedIceAgentState(UINT64, PUINT64);
STATUS executeConnectedIceAgentState(UINT64, UINT64);
STATUS fromNominatingIceAgentState(UINT64, PUINT64);
STATUS executeNominatingIceAgentState(UINT64, UINT64);
STATUS fromReadyIceAgentState(UINT64, PUINT64);
STATUS executeReadyIceAgentState(UINT64, UINT64);
STATUS fromDisconnectedIceAgentState(UINT64, PUINT64);
STATUS executeDisconnectedIceAgentState(UINT64, UINT64);
STATUS fromFailedIceAgentState(UINT64, PUINT64);
STATUS executeFailedIceAgentState(UINT64, UINT64);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_ICE_STATE_MACHINE__ */
