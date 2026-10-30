/*******************************************
DTLS-in-STUN (SPED) internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_DTLS_IN_STUN__
#define __KINESIS_VIDEO_WEBRTC_DTLS_IN_STUN__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SPED (draft-uberti-tsvwg-warp, section 4.2) carries the DTLS handshake inside ICE connectivity checks: each Binding
 * Request and Response embeds one DTLS packet of the current flight, round-robin, and acknowledges the
 * embedded or plain DTLS packets received so far by their CRC-32. The controller below holds that state for
 * one ICE agent; the ICE agent and the DTLS session feed it. It takes its own lock and never calls out.
 */

// DTLS packets of one flight kept for embedding; a KVS flight is one or two packets at the SPED DTLS MTU
#define DTLS_IN_STUN_MAX_STASHED_PACKETS 4

// Largest DTLS packet that is embedded or taken out of a STUN message
#define DTLS_IN_STUN_MAX_PACKET_SIZE 1200

// Requests remembered for the ACK a response implicitly carries for the packet its request embedded
#define DTLS_IN_STUN_MAX_SENT_REQUESTS 16

// A DTLS record header is 13 bytes and its content type, the first byte, lies in [20, 63] (RFC 9443)
#define DTLS_IN_STUN_RECORD_HEADER_LEN     13
#define IS_DTLS_IN_STUN_PACKET(pData, len) ((len) >= DTLS_IN_STUN_RECORD_HEADER_LEN && (pData)[0] > 19 && (pData)[0] < 64)

typedef enum {
    // Not in use, or the peer turned out not to support it
    DTLS_IN_STUN_STATE_OFF,
    // Embedding, but nothing heard from the peer yet
    DTLS_IN_STUN_STATE_TENTATIVE,
    // The peer embeds or acknowledges too
    DTLS_IN_STUN_STATE_CONFIRMED,
    // The local DTLS handshake is done, the peer may still need our last flight
    DTLS_IN_STUN_STATE_PENDING,
    // Done for good
    DTLS_IN_STUN_STATE_COMPLETE,
} DTLS_IN_STUN_STATE;

typedef struct {
    PBYTE pData;
    UINT32 length;
    UINT32 crc;
} DtlsInStunPacket, *PDtlsInStunPacket;

typedef struct {
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    BOOL embedded;
    UINT32 crc;
} DtlsInStunSentRequest, *PDtlsInStunSentRequest;

typedef struct {
    MUTEX lock;
    DTLS_IN_STUN_STATE state;

    // Only packets the DTLS session can take are acknowledged
    BOOL dtlsStarted;

    // The flight to embed, round-robin
    DtlsInStunPacket stash[DTLS_IN_STUN_MAX_STASHED_PACKETS];
    UINT32 stashCount;
    UINT32 stashNext;

    // The flight the DTLS session is writing right now; replaces the stash once complete
    DtlsInStunPacket flight[DTLS_IN_STUN_MAX_STASHED_PACKETS];
    UINT32 flightCount;

    // The stash went out plain: messages only acknowledge from now on, a later flight is not captured to embed
    BOOL embedEnded;

    // CRC-32s of DTLS packets received, oldest first
    UINT32 acks[STUN_DTLS_IN_STUN_ACK_MAX_SEND];
    UINT32 ackCount;

    // Requests that carried the attributes
    DtlsInStunSentRequest sentRequests[DTLS_IN_STUN_MAX_SENT_REQUESTS];
    UINT32 sentRequestNext;

    // DTLS packets the peer embedded that went to the DTLS session
    UINT32 receivedPacketCount;
} DtlsInStun, *PDtlsInStun;

/**
 * Allocates a DTLS-in-STUN controller, OFF
 *
 * @param - PDtlsInStun* - OUT - the created controller
 *
 * @return - STATUS - status of execution
 */
STATUS createDtlsInStun(PDtlsInStun*);

/**
 * Frees a controller and the packets it holds
 *
 * @param - PDtlsInStun* - IN/OUT - the controller, set to NULL
 *
 * @return - STATUS - status of execution
 */
STATUS freeDtlsInStun(PDtlsInStun*);

/**
 * Starts embedding (TENTATIVE) from a clean state, or stops it (OFF) and drops everything held
 *
 * @param - PDtlsInStun - IN - the controller
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunStart(PDtlsInStun);
STATUS dtlsInStunStop(PDtlsInStun);

/**
 * @param - PDtlsInStun - IN - the controller, OFF if NULL
 *
 * @return - DTLS_IN_STUN_STATE - the current state
 */
DTLS_IN_STUN_STATE dtlsInStunGetState(PDtlsInStun);

/**
 * @param - PDtlsInStun - IN - the controller, inactive if NULL
 *
 * @return - BOOL - whether outgoing checks carry the attributes: TENTATIVE, CONFIRMED or PENDING
 */
BOOL dtlsInStunIsActive(PDtlsInStun);

/**
 * The DTLS session can take packets from now on
 *
 * @param - PDtlsInStun - IN - the controller
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunSetDtlsStarted(PDtlsInStun);

/**
 * Collects one outgoing DTLS packet of the flight being written
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - PBYTE - IN - the DTLS packet
 * @param - UINT32 - IN - its length
 *
 * @return - BOOL - whether it was taken; if not, the caller sends it plain
 */
BOOL dtlsInStunCapturePacket(PDtlsInStun, PBYTE, UINT32);

/**
 * The DTLS session finished writing a flight. A new flight replaces the stash; a retransmission of the current one is
 * dropped, its packets would carry CRCs the peer never acknowledges.
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - BOOL - IN - whether the flight repeats the last one
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunEndFlight(PDtlsInStun, BOOL);

/**
 * Appends DTLS-IN-STUN-ACK and DTLS-IN-STUN to an outgoing Binding Request or Response. Before the DTLS session can take
 * packets, an empty DTLS-IN-STUN only: an ACK attribute would acknowledge what the peer embedded in the request.
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - PStunPacket - IN/OUT - the message, MESSAGE-INTEGRITY and FINGERPRINT not yet added
 * @param - UINT32 - IN - largest serialized size, MESSAGE-INTEGRITY and FINGERPRINT included, the message may grow to
 * @param - BOOL - IN - whether to embed a DTLS packet; otherwise only acknowledge
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunAppendAttributes(PDtlsInStun, PStunPacket, UINT32, BOOL);

/**
 * Processes the attributes of an authenticated Binding Request or Success Response. A DTLS packet the peer embedded for
 * the DTLS session is copied out.
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - PStunPacket - IN - the message
 * @param - PBYTE - OUT - buffer of DTLS_IN_STUN_MAX_PACKET_SIZE for the embedded DTLS packet
 * @param - PUINT32 - OUT - its length, 0 if there is none to inject
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunOnStunMessage(PDtlsInStun, PStunPacket, PBYTE, PUINT32);

/**
 * A plain DTLS packet arrived; acknowledged like an embedded one
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - PBYTE - IN - the DTLS packet
 * @param - UINT32 - IN - its length
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunOnDtlsPacket(PDtlsInStun, PBYTE, UINT32);

/**
 * The local DTLS handshake completed. In DTLS 1.2 the client has nothing left the server may need, the server keeps its
 * last flight until acknowledged.
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - BOOL - IN - whether this side is the DTLS client
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunOnHandshakeComplete(PDtlsInStun, BOOL);

/**
 * Application data (SRTP, SCTP) arrived from the peer, which is therefore done with the handshake too
 *
 * @param - PDtlsInStun - IN - the controller
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunOnApplicationData(PDtlsInStun);

/**
 * The DTLS session failed; give up on embedding
 *
 * @param - PDtlsInStun - IN - the controller
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunOnDtlsFailed(PDtlsInStun);

/**
 * Takes the stashed flight out, to be sent plain once ICE has a pair, and ends embedding: later messages only
 * acknowledge, and later flights go plain only. Also after the peer turned out not to support SPED, when the flight
 * never reached it.
 *
 * @param - PDtlsInStun - IN - the controller
 * @param - PDtlsInStunPacket - OUT - DTLS_IN_STUN_MAX_STASHED_PACKETS entries whose pData the caller frees
 * @param - PUINT32 - OUT - number of packets
 *
 * @return - STATUS - status of execution
 */
STATUS dtlsInStunTakeStash(PDtlsInStun, PDtlsInStunPacket, PUINT32);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_DTLS_IN_STUN__ */
