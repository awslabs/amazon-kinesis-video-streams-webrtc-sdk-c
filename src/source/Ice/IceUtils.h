/*******************************************
Ice Utils internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_ICE_UTILS__
#define __KINESIS_VIDEO_WEBRTC_ICE_UTILS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT 20
#define MAX_STORED_TRANSACTION_ID_COUNT         100

#define ICE_STUN_DEFAULT_PORT 3478

#define ICE_URL_PREFIX_STUN        "stun:"
#define ICE_URL_PREFIX_TURN        "turn:"
#define ICE_URL_PREFIX_TURN_SECURE "turns:"
#define ICE_URL_TRANSPORT_UDP      "transport=udp"
#define ICE_URL_TRANSPORT_TCP      "transport=tcp"

#define ICE_TRANSPORT_TYPE_UDP "udp"
#define ICE_TRANSPORT_TYPE_TCP "tcp"
#define ICE_TRANSPORT_TYPE_TLS "tls"

/**
 * Ring buffer storing transactionIds
 */
typedef struct {
    UINT32 maxTransactionIdsCount;
    UINT32 nextTransactionIdIndex;
    UINT32 earliestTransactionIdIndex;
    UINT32 transactionIdCount;
    PBYTE transactionIds;
} TransactionIdStore, *PTransactionIdStore;

STATUS createTransactionIdStore(UINT32, PTransactionIdStore*);
STATUS freeTransactionIdStore(PTransactionIdStore*);
VOID transactionIdStoreInsert(PTransactionIdStore, PBYTE);
VOID transactionIdStoreRemove(PTransactionIdStore, PBYTE);
BOOL transactionIdStoreHasId(PTransactionIdStore, PBYTE);
VOID transactionIdStoreClear(PTransactionIdStore);

STATUS iceUtilsGenerateTransactionId(PBYTE, UINT32);

// Stun packaging and sending functions
STATUS iceUtilsPackageStunPacket(PStunPacket, PBYTE, UINT32, PBYTE, PUINT32);
STATUS iceUtilsSendStunPacket(PStunPacket, PBYTE, UINT32, PKvsIpAddress, PSocketConnection, struct __TurnConnection*, BOOL);
STATUS iceUtilsSendData(PBYTE, UINT32, PKvsIpAddress, PSocketConnection, struct __TurnConnection*, BOOL);

typedef struct {
    BOOL isTurn;
    BOOL isSecure;
    CHAR url[MAX_ICE_CONFIG_URI_LEN + 1];
    KvsIpAddress ipAddress;
    CHAR username[MAX_ICE_CONFIG_USER_NAME_LEN + 1];
    CHAR credential[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
    KVS_SOCKET_PROTOCOL transport;
    IceServerSetIpFunc setIpFn;
} IceServer, *PIceServer;

STATUS parseIceServer(PIceServer, PCHAR, PCHAR, PCHAR);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_ICE_UTILS__ */
