/*******************************************
TurnConnection internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
#define TURN_REQUEST_TRANSPORT_UDP                                      17
#define TURN_REQUEST_TRANSPORT_TCP                                      6
#define DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS                        600
// required by rfc5766 to be 300s
#define TURN_PERMISSION_LIFETIME                                        (300 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_TIMER_INTERVAL_BEFORE_READY                        (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY                         (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_SEND_REFRESH_INVERVAL                              (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// turn state timeouts
#define DEFAULT_TURN_GET_CREDENTIAL_TIMEOUT                             (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_ALLOCATION_TIMEOUT                                 (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT                          (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_BIND_CHANNEL_TIMEOUT                               (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_CLEAN_UP_TIMEOUT                                   (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/*
 * if no application data is sent through turn for this much time then we assume that a better connection is found
 * and initiate turn clean up.
 */
#define DEFAULT_TURN_START_CLEAN_UP_TIMEOUT                             (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_ALLOCATION_REFRESH_GRACE_PERIOD                    (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_TURN_PERMISSION_REFRESH_GRACE_PERIOD                    (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define DEFAULT_TURN_MESSAGE_SEND_CHANNEL_DATA_BUFFER_LEN               (10 * 1024)
#define DEFAULT_TURN_MESSAGE_RECV_CHANNEL_DATA_BUFFER_LEN               (10 * 1024)

// all turn channel numbers must be greater than 0x4000 and less than 0x7FFF
#define TURN_CHANNEL_BIND_CHANNEL_NUMBER_BASE                           (UINT16) 0x4000

// 2 byte channel number 2 data byte size
#define TURN_DATA_CHANNEL_SEND_OVERHEAD                                 4
#define TURN_DATA_CHANNEL_MSG_FIRST_BYTE                                0x40

#define TURN_STATE_NEW_STR                                              (PCHAR) "TURN_STATE_NEW"
#define TURN_STATE_GET_CREDENTIALS_STR                                  (PCHAR) "TURN_STATE_GET_CREDENTIALS"
#define TURN_STATE_ALLOCATION_STR                                       (PCHAR) "TURN_STATE_ALLOCATION"
#define TURN_STATE_CREATE_PERMISSION_STR                                (PCHAR) "TURN_STATE_CREATE_PERMISSION"
#define TURN_STATE_BIND_CHANNEL_STR                                     (PCHAR) "TURN_STATE_BIND_CHANNEL"
#define TURN_STATE_READY_STR                                            (PCHAR) "TURN_STATE_READY"
#define TURN_STATE_CLEAN_UP_STR                                         (PCHAR) "TURN_STATE_CLEAN_UP"
#define TURN_STATE_FAILED_STR                                           (PCHAR) "TURN_STATE_FAILED"

typedef STATUS (*RelayAddressAvailableFunc)(UINT64, PKvsIpAddress, PSocketConnection);

typedef enum {
    TURN_STATE_NEW,
    TURN_STATE_GET_CREDENTIALS,
    TURN_STATE_ALLOCATION,
    TURN_STATE_CREATE_PERMISSION,
    TURN_STATE_BIND_CHANNEL,
    TURN_STATE_READY,
    TURN_STATE_CLEAN_UP,
    TURN_STATE_FAILED,
} TURN_CONNECTION_STATE;

typedef enum {
    TURN_PEER_CONN_STATE_NEW,
    TURN_PEER_CONN_STATE_BIND_CHANNEL,
    TURN_PEER_CONN_STATE_READY,
    TURN_PEER_CONN_STATE_FAILED,
} TURN_PEER_CONNECTION_STATE;

typedef enum {
    TURN_CONNECTION_DATA_TRANSFER_MODE_SEND_INDIDATION,
    TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL,
} TURN_CONNECTION_DATA_TRANSFER_MODE;

typedef struct {
    UINT64 customData;
    RelayAddressAvailableFunc relayAddressAvailableFn;
    ConnectionDataAvailableFunc applicationDataAvailableFn;
} TurnConnectionCallbacks, *PTurnConnectionCallbacks;

typedef struct {
    KvsIpAddress address;
    KvsIpAddress xorAddress;
    TURN_PEER_CONNECTION_STATE connectionState;
    PTransactionIdStore pTransactionIdStore;
    UINT16 channelNumber;
    UINT64 permissionExpirationTime;
    BOOL ready;
} TurnPeer, *PTurnPeer;

typedef struct __TurnConnection TurnConnection;
struct __TurnConnection {

    // realm attribute in Allocation response
    CHAR turnRealm[STUN_MAX_REALM_LEN + 1];
    BYTE turnNonce[STUN_MAX_NONCE_LEN];
    UINT16 nonceLen;
    BYTE longTermKey[MD5_DIGEST_LENGTH];
    BOOL credentialObtained;

    PSocketConnection pControlChannel;

    PDoubleList turnPeerList;

    TIMER_QUEUE_HANDLE timerQueueHandle;

    IceServer turnServer;

    MUTEX lock;

    MUTEX sendLock;

    volatile TURN_CONNECTION_STATE state;

    UINT64 stateTimeoutTime;

    STATUS errorStatus;

    UINT32 timerCallbackId;

    PStunPacket pTurnPacket;
    PStunPacket pTurnCreatePermissionPacket;
    PStunPacket pTurnChannelBindPacket;
    PStunPacket pTurnAllocationRefreshPacket;

    KvsIpAddress hostAddress;

    KvsIpAddress relayAddress;
    BOOL relayAddressReceived;

    PConnectionListener pConnectionListener;

    TURN_CONNECTION_DATA_TRANSFER_MODE dataTransferMode;
    KVS_SOCKET_PROTOCOL protocol;

    TurnConnectionCallbacks turnConnectionCallbacks;

    PBYTE sendDataBuffer;
    UINT32 dataBufferSize;

    PBYTE recvDataBuffer;
    UINT32 recvDataBufferSize;
    UINT32 currRecvDataLen;

    UINT64 lastApplicationDataSentTime;
    BOOL allocationFreed;

    UINT64 allocationExpirationTime;
    UINT64 nextAllocationRefreshTime;

    UINT64 currentTimerCallingPeriod;

    KvsRtcConfiguration kvsRtcConfiguration;
};
typedef struct __TurnConnection* PTurnConnection;

typedef struct {
    UINT64 customData;
    PTurnConnection pTurnConnection;
    PBYTE pData;
    UINT32 dataSize;
    UINT16 channelNumber;
    KvsIpAddress address;
} ConnectionDataAvailableWrapper, *PConnectionDataAvailableWrapper;

STATUS createTurnConnection(PIceServer, TIMER_QUEUE_HANDLE, PConnectionListener, TURN_CONNECTION_DATA_TRANSFER_MODE,
                            KVS_SOCKET_PROTOCOL, PTurnConnectionCallbacks, KvsRtcConfiguration, PTurnConnection*);
STATUS freeTurnConnection(PTurnConnection*);
STATUS turnConnectionAddPeer(PTurnConnection, PKvsIpAddress);
STATUS turnConnectionSendData(PTurnConnection, PBYTE, UINT32, PKvsIpAddress);
STATUS turnConnectionStart(PTurnConnection);
STATUS turnConnectionRefreshAllocation(PTurnConnection);
STATUS turnConnectionRefreshPermission(PTurnConnection, PBOOL);
STATUS turnConnectionFreePreAllocatedPackets(PTurnConnection);

STATUS turnConnectionStepState(PTurnConnection);
STATUS turnConnectionCheckTurnBeingUsed(PTurnConnection);
STATUS turnConnectionDeliverChannelData(PTurnConnection, PBYTE, UINT32);
STATUS turnConnectionTimerCallback(UINT32, UINT64, UINT64);
STATUS turnConnectionGetLongTermKey(PCHAR, PCHAR, PCHAR, PBYTE, UINT32);
STATUS turnConnectionPackageTurnAllocationRequest(PCHAR, PCHAR, PBYTE, UINT16, UINT32, PStunPacket*);
PCHAR turnConnectionGetStateStr(TURN_CONNECTION_STATE);

STATUS turnConnectionIncomingDataHandler(UINT64, PSocketConnection, PBYTE, UINT32,
                                         PKvsIpAddress, PKvsIpAddress);

STATUS turnConnectionHandleStun(PTurnConnection, PSocketConnection, PBYTE, UINT32);
STATUS turnConnectionHandleStunError(PTurnConnection, PSocketConnection, PBYTE, UINT32);
STATUS turnConnectionHandleChannelDataTcpMode(PTurnConnection, PBYTE, UINT32);
PVOID turnDataAvailableWrapper(PVOID);
#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_TURN_CONNECTION__ */
