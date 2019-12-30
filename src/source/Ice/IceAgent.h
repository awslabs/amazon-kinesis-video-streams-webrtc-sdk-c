/*******************************************
IceAgent internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_ICE_AGENT__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_ICE_AGENT__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define KVS_ICE_MAX_CANDIDATE_PAIR_COUNT                                1024
#define KVS_ICE_MAX_REMOTE_CANDIDATE_COUNT                              100
#define KVS_ICE_MAX_LOCAL_CANDIDATE_COUNT                               100
#define KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT          10 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KVS_ICE_CONNECTIVITY_CHECK_TIMEOUT                              10 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT                            10 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KVS_ICE_SEND_KEEP_ALIVE_INTERVAL                                15 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KVS_ICE_CANDIDATE_PAIR_GENERATION_TIMEOUT                       1 * HUNDREDS_OF_NANOS_IN_A_MINUTE

// Ta in https://tools.ietf.org/html/rfc8445
#define KVS_ICE_CONNECTION_CHECK_POLLING_INTERVAL                       50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define KVS_ICE_STATE_NEW_TIMER_POLLING_INTERVAL                        100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL                      1 * HUNDREDS_OF_NANOS_IN_A_SECOND

// Disconnection timeout should be as long as KVS_ICE_SEND_KEEP_ALIVE_INTERVAL because peer can just be receiving
// media and not sending anything back except keep alives
#define KVS_ICE_ENTER_STATE_DISCONNECTION_GRACE_PERIOD                  15 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define KVS_ICE_ENTER_STATE_FAILED_GRACE_PERIOD                         30 * HUNDREDS_OF_NANOS_IN_A_SECOND

#define STUN_HEADER_MAGIC_BYTE_OFFSET                                   4

#define KVS_ICE_MAX_ICE_SERVERS 3

// https://tools.ietf.org/html/rfc5245#section-4.1.2.1
#define ICE_PRIORITY_HOST_CANDIDATE_TYPE_PREFERENCE                     126
#define ICE_PRIORITY_SERVER_REFLEXIVE_CANDIDATE_TYPE_PREFERENCE         100
#define ICE_PRIORITY_PEER_REFLEXIVE_CANDIDATE_TYPE_PREFERENCE           110
#define ICE_PRIORITY_RELAYED_CANDIDATE_TYPE_PREFERENCE                  0
#define ICE_PRIORITY_LOCAL_PREFERENCE                                   65535

#define ICE_STUN_DEFAULT_PORT                                           3478
#define ICE_MAX_CONNECTIVITY_REQUEST_COUNT                              20

#define ICE_URL_PREFIX_STUN                                             "stun:"
#define ICE_URL_PREFIX_TURN                                             "turn:"
#define ICE_URL_PREFIX_TURN_SECURE                                      "turns:"

#define IS_STUN_PACKET(pBuf)                    (getInt32(*(PUINT32)((pBuf) + STUN_HEADER_MAGIC_BYTE_OFFSET)) == STUN_HEADER_MAGIC_COOKIE)

#define IS_CANN_PAIR_SENDING_FROM_RELAYED(p)                            ((p)->local->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED)

#define KVS_ICE_DEFAULT_TURN_PROTOCOL                                   KVS_SOCKET_PROTOCOL_TCP

typedef enum {
    ICE_CANDIDATE_TYPE_HOST             = 0,
    ICE_CANDIDATE_TYPE_PEER_REFLEXIVE   = 1,
    ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE = 2,
    ICE_CANDIDATE_TYPE_RELAYED          = 3,
} ICE_CANDIDATE_TYPE;

typedef enum {
    ICE_CANDIDATE_STATE_NEW,
    ICE_CANDIDATE_STATE_VALID,
    ICE_CANDIDATE_STATE_INVALID,
} ICE_CANDIDATE_STATE;

typedef enum {
    ICE_CANDIDATE_PAIR_STATE_FROZEN         = 0,
    ICE_CANDIDATE_PAIR_STATE_WAITING        = 1,
    ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS    = 2,
    ICE_CANDIDATE_PAIR_STATE_SUCCEEDED      = 3,
    ICE_CANDIDATE_PAIR_STATE_FAILED         = 4,
} ICE_CANDIDATE_PAIR_STATE;

typedef VOID (*IceInboundPacketFunc)(UINT64, PBYTE, UINT32);
typedef VOID (*IceConnectionStateChangedFunc)(UINT64, UINT64);
typedef VOID (*IceNewLocalCandidateFunc)(UINT64, PCHAR);

typedef struct {
    UINT64 customData;
    IceInboundPacketFunc inboundPacketFn;
    IceConnectionStateChangedFunc connectionStateChangedFn;
    IceNewLocalCandidateFunc newLocalCandidateFn;
} IceAgentCallbacks, *PIceAgentCallbacks;

typedef struct {
    ICE_CANDIDATE_TYPE iceCandidateType;
    KvsIpAddress ipAddress;
    PSocketConnection pSocketConnection;
    ICE_CANDIDATE_STATE state;
    UINT32 priority;            // stun priority attribute takes UINT32
    UINT32 iceServerIndex;
    UINT32 foundation;
} IceCandidate, *PIceCandidate;

typedef struct {
    PIceCandidate local;
    PIceCandidate remote;
    BOOL nominated;
    UINT64 priority;
    ICE_CANDIDATE_PAIR_STATE state;
    PTransactionIdStore pTransactionIdStore;
    UINT64 lastDataSentTime;
} IceCandidatePair, *PIceCandidatePair;

typedef struct {
    volatile ATOMIC_BOOL agentStartGathering;

    CHAR localUsername[MAX_ICE_CONFIG_USER_NAME_LEN + 1];
    CHAR localPassword[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
    CHAR remoteUsername[MAX_ICE_CONFIG_USER_NAME_LEN + 1];
    CHAR remotePassword[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
    CHAR combinedUserName[MAX_ICE_CONFIG_USER_NAME_LEN + 1];

    PDoubleList localCandidates;
    PDoubleList remoteCandidates;
    // store PIceCandidatePair which will be immediately checked for connectivity when the timer is fired.
    PStackQueue triggeredCheckQueue;

    PIceCandidatePair candidatePairs[KVS_ICE_MAX_CANDIDATE_PAIR_COUNT];
    UINT32 candidatePairCount;
    PConnectionListener pConnectionListener;
    volatile BOOL agentStarted;
    BOOL isControlling;
    UINT64 tieBreaker;

    MUTEX lock;

    // Current ice agent state
    UINT64 iceAgentState;
    UINT32 iceAgentStateTimerCallback;
    UINT32 keepAliveTimerCallback;
    // The state machine
    PStateMachine pStateMachine;
    STATUS iceAgentStatus;
    UINT64 stateEndTime;
    UINT64 candidateGenerationEndTime;
    PIceCandidatePair pDataSendingIceCandidatePair;

    IceAgentCallbacks iceAgentCallbacks;
    UINT64 customData; // customData for iceAgentCallbacks

    IceServer iceServers[KVS_ICE_MAX_ICE_SERVERS];
    UINT32 iceServersCount;

    UINT32 foundationCounter;

    struct __TurnConnection* pTurnConnection;

    TIMER_QUEUE_HANDLE timerQueueHandle;

    UINT64 lastDataReceivedTime;
    BOOL detectedDisconnection;
    UINT64 disconnectionGracePeriodEndTime;

    ICE_TRANSPORT_POLICY iceTransportPolicy;
} IceAgent, *PIceAgent;


//////////////////////////////////////////////
// internal functions
//////////////////////////////////////////////

/**
 * allocate the IceAgent struct and store username and password
 *
 * @param - PCHAR - IN - username
 * @param - PCHAR - IN - password
 * @param - UINT64 - IN - customData
 * @param - PIceAgentCallbacks - IN - callback for inbound packets
 * @param - PRtcConfiguration - IN - RtcConfig
 * @param - PIceAgent* - OUT - the created IceAgent struct
 *
 * @return - STATUS - status of execution
 */
STATUS createIceAgent(PCHAR, PCHAR, UINT64, PIceAgentCallbacks, PRtcConfiguration, TIMER_QUEUE_HANDLE, PConnectionListener, PIceAgent*);

/**
 * deallocate the PIceAgent object and all its resources.
 *
 * @return - STATUS - status of execution
 */
STATUS freeIceAgent(PIceAgent*);

/**
 * if PIceCandidate doesnt exist already in remoteCandidates, create a copy and add to remoteCandidates
 *
 * @param - PIceAgent - IN - IceAgent object
 * @param - PIceCandidate - IN - new remote candidate to add
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentAddRemoteCandidate(PIceAgent, PCHAR);

/**
 * Initiates stun commuinication with remote candidates.
 *
 * @param - PIceAgent - IN - IceAgent object
 * @param - PCHAR - IN - remote username
 * @param - PCHAR - IN - remote password
 * @param - BOOL - IN - is controlling agent
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentStartAgent(PIceAgent, PCHAR, PCHAR, BOOL);

/**
 * Initiates candidate gathering
 *
 * @param - PIceAgent - IN - IceAgent object
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentStartGathering(PIceAgent);

/**
 * Serialize a candidate for Trickle ICE or exchange via SDP
 *
 * @param - PIceAgent - IN - IceAgent object
 * @param - PCHAR - OUT - Destination buffer
 * @param - UINT32 - OUT - Size of destination buffer
 *
 * @return - STATUS - status of execution
 */
STATUS iceCandidateSerialize(PIceCandidate, PCHAR, PUINT32);

/**
 * Send data through selected connection. PIceAgent has to be in ICE_AGENT_CONNECTION_STATE_CONNECTED state.
 *
 * @param - PIceAgent - IN - IceAgent object
 * @param - PBYTE - IN - buffer storing the data to be sent
 * @param - UINT32 - IN - length of data
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentSendPacket(PIceAgent, PBYTE, UINT32);

/**
 * gather local ip addresses and create a udp port. If port creation succeeded then create a new candidate
 * and store it in localCandidates. Ips that are already a local candidate will not be added again.
 *
 * @param - PIceAgent - IN - IceAgent object
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentGatherLocalCandidate(PIceAgent);

/**
 * Starting from given index, fillout PSdpMediaDescription->sdpAttributes with serialize local candidate strings.
 *
 * @param - PIceAgent - IN - IceAgent object
 * @param - PSdpMediaDescription - IN - PSdpMediaDescription object whose sdpAttributes will be filled with local candidate strings
 * @param - UINT32 - IN - buffer length of pSdpMediaDescription->sdpAttributes[index].attributeValue
 * @param - PUINT32 - IN - starting index in sdpAttributes
 *
 * @return - STATUS - status of execution
 */
STATUS iceAgentPopulateSdpMediaDescriptionCandidates(PIceAgent, PSdpMediaDescription, UINT32, PUINT32);

STATUS iceAgentReportNewLocalCandidate(PIceAgent, PIceCandidate);

// Incoming data handling functions
STATUS newRelayCandidateHandler(UINT64, PKvsIpAddress, PSocketConnection);
STATUS incomingDataHandler(UINT64, PSocketConnection, PBYTE, UINT32, PKvsIpAddress, PKvsIpAddress);
STATUS handleStunPacket(PIceAgent, PBYTE, UINT32, PSocketConnection, PKvsIpAddress, PKvsIpAddress);

// IceCandidate functions
STATUS updateCandidateAddress(PIceCandidate, PKvsIpAddress);
STATUS findCandidateWithIp(PKvsIpAddress, PDoubleList, PIceCandidate*);
STATUS findCandidateWithSocketConnection(PSocketConnection, PDoubleList, PIceCandidate*);

// IceCandidatePair functions
STATUS createIceCandidatePairs(PIceAgent, PIceCandidate, BOOL);
STATUS sortIceCandidatePairs(PIceAgent);
STATUS findIceCandidatePairWithLocalConnectionHandleAndRemoteAddr(PIceAgent, PSocketConnection, PKvsIpAddress, BOOL, PIceCandidatePair*);
STATUS pruneUnconnectedIceCandidatePair(PIceAgent);
STATUS pruneIceCandidatePairWithLocalCandidate(PIceAgent, PIceCandidate);
STATUS iceCandidatePairCheckConnection(PStunPacket, PIceAgent, PIceCandidatePair);

// timer callbacks. timer callbacks are interlocked by time queue lock.
STATUS iceAgentStateNewTimerCallback(UINT32, UINT64, UINT64);
STATUS iceAgentStateGatheringTimerCallback(UINT32, UINT64, UINT64);
STATUS iceAgentStateCheckConnectionTimerCallback(UINT32, UINT64, UINT64);
STATUS iceAgentSendKeepAliveTimerCallback(UINT32, UINT64, UINT64);
STATUS iceAgentStateNominatingTimerCallback(UINT32, UINT64, UINT64);
STATUS iceAgentStateReadyTimerCallback(UINT32, UINT64, UINT64);

STATUS iceAgentNominateCandidatePair(PIceAgent);
STATUS iceAgentCheckPeerReflexiveCandidate(PIceAgent, PKvsIpAddress, UINT32, BOOL, PSocketConnection);
STATUS iceAgentFatalError(PIceAgent, STATUS);

UINT32 computeCandidatePriority(PIceCandidate);
UINT64 computeCandidatePairPriority(PIceCandidatePair, BOOL);
PCHAR iceAgentGetCandidateTypeStr(ICE_CANDIDATE_TYPE);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_ICE_AGENT__ */
