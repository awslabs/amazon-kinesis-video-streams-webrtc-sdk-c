/*******************************************
Ice Utils internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_ICE_UTILS__
#define __KINESIS_VIDEO_WEBRTC_ICE_UTILS__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT                         20
#define MAX_STORED_TRANSACTION_ID_COUNT                                 100

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
BOOL transactionIdStoreHasId(PTransactionIdStore, PBYTE);
VOID transactionIdStoreClear(PTransactionIdStore);

STATUS iceUtilsGenerateTransactionId(PBYTE, UINT32);

// Stun packaging and sending functions
STATUS iceUtilsPackageStunPacket(PStunPacket, PBYTE, UINT32, PBYTE, PUINT32);
STATUS iceUtilsSendStunPacket(PStunPacket, PBYTE, UINT32, PKvsIpAddress, PSocketConnection, struct __TurnConnection*, BOOL);

STATUS populateIpFromString(PKvsIpAddress, PCHAR);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_ICE_UTILS__ */
