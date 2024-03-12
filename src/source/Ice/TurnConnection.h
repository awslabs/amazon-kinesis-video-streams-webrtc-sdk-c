/*******************************************
TurnConnection internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
#define TURN_REQUEST_TRANSPORT_UDP               17
#define TURN_REQUEST_TRANSPORT_TCP               6
#define DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS 600
// required by rfc5766 to be 300s
#define TURN_PERMISSION_LIFETIME                 (300 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_TIMER_INTERVAL_BEFORE_READY (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY  (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_SEND_REFRESH_INVERVAL       (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// turn state timeouts
#define DEFAULT_TURN_GET_CREDENTIAL_TIMEOUT    (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_ALLOCATION_TIMEOUT        (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_BIND_CHANNEL_TIMEOUT      (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_CLEAN_UP_TIMEOUT          (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define DEFAULT_TURN_ALLOCATION_REFRESH_GRACE_PERIOD (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_PERMISSION_REFRESH_GRACE_PERIOD (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define MAX_TURN_CHANNEL_DATA_MESSAGE_SIZE                4 + 65536 /* header + data */
#define DEFAULT_TURN_MESSAGE_SEND_CHANNEL_DATA_BUFFER_LEN MAX_TURN_CHANNEL_DATA_MESSAGE_SIZE
#define DEFAULT_TURN_MESSAGE_RECV_CHANNEL_DATA_BUFFER_LEN MAX_TURN_CHANNEL_DATA_MESSAGE_SIZE
#define DEFAULT_TURN_CHANNEL_DATA_BUFFER_SIZE             512
#define DEFAULT_TURN_MAX_PEER_COUNT                       32
#define MAX_TURN_PROFILE_LOG_DESC_LEN                     256

// all turn channel numbers must be greater than 0x4000 and less than 0x7FFF
#define TURN_CHANNEL_BIND_CHANNEL_NUMBER_BASE (UINT16) 0x4000

// 2 byte channel number 2 data byte size
#define TURN_DATA_CHANNEL_SEND_OVERHEAD  4
#define TURN_DATA_CHANNEL_MSG_FIRST_BYTE 0x40

#define TURN_STATE_MACHINE_NAME (PCHAR) "TURN"

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

typedef STATUS (*RelayAddressAvailableFunc)(UINT64, PKvsIpAddress, PSocketConnection);
typedef STATUS (*TurnStateFailedFunc)(PSocketConnection, UINT64);

typedef enum {
    TURN_PEER_CONN_STATE_CREATE_PERMISSION,
    TURN_PEER_CONN_STATE_BIND_CHANNEL,
    TURN_PEER_CONN_STATE_READY,
    TURN_PEER_CONN_STATE_FAILED,
} TURN_PEER_CONNECTION_STATE;

typedef enum {
    TURN_CONNECTION_DATA_TRANSFER_MODE_SEND_INDIDATION,
    TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL,
} TURN_CONNECTION_DATA_TRANSFER_MODE;

typedef struct {
    PBYTE data;
    UINT32 size;
    KvsIpAddress senderAddr;
} TurnChannelData, *PTurnChannelData;

typedef struct {
    UINT64 customData;
    RelayAddressAvailableFunc relayAddressAvailableFn;
    TurnStateFailedFunc turnStateFailedFn;
} TurnConnectionCallbacks, *PTurnConnectionCallbacks;

typedef struct {
    KvsIpAddress address;
    KvsIpAddress xorAddress;
    /*
     * Steps to create a turn channel for a peer:
     *     - create permission
     *     - channel bind
     *     - ready to send data
     */
    TURN_PEER_CONNECTION_STATE connectionState;
    PTransactionIdStore pTransactionIdStore;
    UINT16 channelNumber;
    UINT64 permissionExpirationTime;
    BOOL ready;
    BOOL firstTimeCreatePermReq;
    BOOL firstTimeCreatePermResponse;
    UINT64 createPermissionStartTime;
    UINT64 createPermissionTime;
    BOOL firstTimeBindChannelReq;
    BOOL firstTimeBindChannelResponse;
    UINT64 bindChannelStartTime;
    UINT64 bindChannelTime;
} TurnPeer, *PTurnPeer;

typedef struct {
    UINT64 getCredentialsStartTime;
    UINT64 getCredentialsTime;
    UINT64 createAllocationStartTime;
    UINT64 createAllocationTime;
} TurnProfileDiagnostics, *PTurnProfileDiagnostics;

typedef struct __TurnConnection TurnConnection;
struct __TurnConnection {
    volatile ATOMIC_BOOL stopTurnConnection;
    /* shutdown is complete when turn socket is closed */
    volatile ATOMIC_BOOL shutdownComplete;
    volatile ATOMIC_BOOL hasAllocation;
    volatile SIZE_T timerCallbackId;

    // realm attribute in Allocation response
    CHAR turnRealm[STUN_MAX_REALM_LEN + 1];
    BYTE turnNonce[STUN_MAX_NONCE_LEN];
    UINT16 nonceLen;
    BYTE longTermKey[KVS_MD5_DIGEST_LENGTH];
    BOOL credentialObtained;
    BOOL relayAddressReported;

    PSocketConnection pControlChannel;

    TurnPeer turnPeerList[DEFAULT_TURN_MAX_PEER_COUNT];
    UINT32 turnPeerCount;

    TIMER_QUEUE_HANDLE timerQueueHandle;

    IceServer turnServer;

    MUTEX lock;
    MUTEX sendLock;
    CVAR freeAllocationCvar;

    UINT64 state;

    UINT64 stateTimeoutTime;

    STATUS errorStatus;

    PStunPacket pTurnPacket;
    PStunPacket pTurnCreatePermissionPacket;
    PStunPacket pTurnChannelBindPacket;
    PStunPacket pTurnAllocationRefreshPacket;

    KvsIpAddress hostAddress;

    KvsIpAddress relayAddress;

    PConnectionListener pConnectionListener;

    TURN_CONNECTION_DATA_TRANSFER_MODE dataTransferMode;
    KVS_SOCKET_PROTOCOL protocol;

    TurnConnectionCallbacks turnConnectionCallbacks;

    PBYTE sendDataBuffer;
    UINT32 dataBufferSize;

    PBYTE recvDataBuffer;
    UINT32 recvDataBufferSize;
    UINT32 currRecvDataLen;
    // when a complete channel data have been assembled in recvDataBuffer, move it to completeChannelDataBuffer
    // to make room for subsequent partial channel data.
    PBYTE completeChannelDataBuffer;

    UINT64 allocationExpirationTime;
    UINT64 nextAllocationRefreshTime;

    UINT64 currentTimerCallingPeriod;
    BOOL deallocatePacketSent;
    TurnProfileDiagnostics turnProfileDiagnostics;
    PStateMachine pStateMachine;
};
typedef struct __TurnConnection* PTurnConnection;

STATUS createTurnConnection(PIceServer, TIMER_QUEUE_HANDLE, TURN_CONNECTION_DATA_TRANSFER_MODE, KVS_SOCKET_PROTOCOL, PTurnConnectionCallbacks,
                            PSocketConnection, PConnectionListener, PTurnConnection*);
STATUS freeTurnConnection(PTurnConnection*);
STATUS turnConnectionAddPeer(PTurnConnection, PKvsIpAddress);
STATUS turnConnectionSendData(PTurnConnection, PBYTE, UINT32, PKvsIpAddress);
STATUS turnConnectionStart(PTurnConnection);
STATUS turnConnectionShutdown(PTurnConnection, UINT64);
BOOL turnConnectionIsShutdownComplete(PTurnConnection);
BOOL turnConnectionGetRelayAddress(PTurnConnection, PKvsIpAddress);
STATUS turnConnectionRefreshAllocation(PTurnConnection);
STATUS turnConnectionRefreshPermission(PTurnConnection, PBOOL);
STATUS turnConnectionFreePreAllocatedPackets(PTurnConnection);

// used for state machine
UINT64 turnConnectionGetTime(UINT64);

STATUS turnConnectionUpdateNonce(PTurnConnection);
STATUS turnConnectionTimerCallback(UINT32, UINT64, UINT64);
STATUS turnConnectionGetLongTermKey(PCHAR, PCHAR, PCHAR, PBYTE, UINT32);
STATUS turnConnectionPackageTurnAllocationRequest(PCHAR, PCHAR, PBYTE, UINT16, UINT32, PStunPacket*);

STATUS turnConnectionIncomingDataHandler(PTurnConnection, PBYTE, UINT32, PKvsIpAddress, PKvsIpAddress, PTurnChannelData, PUINT32);

STATUS turnConnectionHandleStun(PTurnConnection, PBYTE, UINT32);
STATUS turnConnectionHandleStunError(PTurnConnection, PBYTE, UINT32);
STATUS turnConnectionHandleChannelData(PTurnConnection, PBYTE, UINT32, PTurnChannelData, PUINT32, PUINT32);
STATUS turnConnectionHandleChannelDataTcpMode(PTurnConnection, PBYTE, UINT32, PTurnChannelData, PUINT32, PUINT32);
VOID turnConnectionFatalError(PTurnConnection, STATUS);

PTurnPeer turnConnectionGetPeerWithChannelNumber(PTurnConnection, UINT16);
PTurnPeer turnConnectionGetPeerWithIp(PTurnConnection, PKvsIpAddress);

STATUS checkTurnPeerConnections(PTurnConnection);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__ */
