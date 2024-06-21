/*******************************************
Signaling LibWebSocket based API calls include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_LWS_API_CALLS__
#define __KINESIS_VIDEO_WEBRTC_LWS_API_CALLS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Timeout values
#define SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SIGNALING_SERVICE_API_CALL_TIMEOUT_IN_SECONDS                                                                                                \
    ((SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT + SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT) / HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SIGNALING_SERVICE_TCP_KEEPALIVE_IN_SECONDS                3
#define SIGNALING_SERVICE_TCP_KEEPALIVE_PROBE_COUNT               3
#define SIGNALING_SERVICE_TCP_KEEPALIVE_PROBE_INTERVAL_IN_SECONDS 1
#define SIGNALING_SERVICE_WSS_PING_PONG_INTERVAL_IN_SECONDS       10
#define SIGNALING_SERVICE_WSS_HANGUP_IN_SECONDS                   7200

// Protocol indexes
#define PROTOCOL_INDEX_HTTPS 0
#define PROTOCOL_INDEX_WSS   1

#define SIGNALING_SDP_TYPE_OFFER       "SDP_OFFER"
#define SIGNALING_SDP_TYPE_ANSWER      "SDP_ANSWER"
#define SIGNALING_ICE_CANDIDATE        "ICE_CANDIDATE"
#define SIGNALING_GO_AWAY              "GO_AWAY"
#define SIGNALING_RECONNECT_ICE_SERVER "RECONNECT_ICE_SERVER"
#define SIGNALING_STATUS_RESPONSE      "STATUS_RESPONSE"
#define SIGNALING_MESSAGE_UNKNOWN      "UNKNOWN"

// Max length of the signaling message type string length
#define MAX_SIGNALING_MESSAGE_TYPE_LEN ARRAY_SIZE(SIGNALING_RECONNECT_ICE_SERVER)

// Max value length for the status code
#define MAX_SIGNALING_STATUS_MESSAGE_LEN 16

// max length for http date header, must follow RFC 7231, should be less than 32 characters
#define MAX_DATE_HEADER_BUFFER_LENGTH 64

#define MIN_CLOCK_SKEW_TIME_TO_CORRECT (5 * HUNDREDS_OF_NANOS_IN_A_MINUTE)

// Defining max bloat size per item in the JSON template
#define ICE_SERVER_INFO_TEMPLATE_BLOAT_SIZE 128

// Max bloat size for representing a single ICE URI in the JSON
#define ICE_SERVER_URI_BLOAT_SIZE 10

// Max string length for representing the URIs
#define MAX_ICE_SERVER_URI_STR_LEN (MAX_ICE_CONFIG_URI_COUNT * (MAX_ICE_CONFIG_URI_LEN + ICE_SERVER_URI_BLOAT_SIZE))

// Max string length for representing an ICE config
#define MAX_ICE_SERVER_INFO_STR_LEN                                                                                                                  \
    (MAX_ICE_SERVER_URI_STR_LEN + MAX_ICE_CONFIG_USER_NAME_LEN + MAX_ICE_CONFIG_CREDENTIAL_LEN + ICE_SERVER_INFO_TEMPLATE_BLOAT_SIZE)

// Max string length for the ice server info which includes the template length * max struct count * content
#define MAX_ICE_SERVER_INFOS_STR_LEN (MAX_ICE_CONFIG_COUNT * MAX_ICE_SERVER_INFO_STR_LEN)

// Encoded max ice server infos string len
#define MAX_ENCODED_ICE_SERVER_INFOS_STR_LEN (MAX_ICE_SERVER_INFOS_STR_LEN + ICE_SERVER_INFO_TEMPLATE_BLOAT_SIZE)

// Scratch buffer size
#define LWS_SCRATCH_BUFFER_SIZE (MAX_JSON_PARAMETER_STRING_LEN + LWS_PRE)

// Send and receive buffer size
#define LWS_MESSAGE_BUFFER_SIZE (SIZEOF(CHAR) * (MAX_SIGNALING_MESSAGE_LEN + LWS_PRE))

#define AWS_SIG_V4_HEADER_HOST (PCHAR) "host"

// Specifies whether to block on the correlation id
#define BLOCK_ON_CORRELATION_ID FALSE

// Service loop iteration wait time when there is an already servicing thread
#define LWS_SERVICE_LOOP_ITERATION_WAIT (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// Check for the stale credentials
#define CHECK_SIGNALING_CREDENTIALS_EXPIRATION(p)                                                                                                    \
    do {                                                                                                                                             \
        if (SIGNALING_GET_CURRENT_TIME((p)) >= (p)->pAwsCredentials->expiration) {                                                                   \
            ATOMIC_STORE(&(p)->result, (SIZE_T) SERVICE_CALL_NOT_AUTHORIZED);                                                                        \
            CHK(FALSE, retStatus);                                                                                                                   \
        }                                                                                                                                            \
    } while (FALSE)

/**
 * Index of the signaling protocol handling WSS
 * IMPORTANT!!! This should match the correct index in the
 * signaling client protocol array
 */
#define WSS_SIGNALING_PROTOCOL_INDEX 1

typedef struct __LwsCallInfo LwsCallInfo;
struct __LwsCallInfo {
    // Size of the data in the buffer
    volatile SIZE_T sendBufferSize;

    // Offset from which to send data
    volatile SIZE_T sendOffset;

    // Service exit indicator;
    volatile ATOMIC_BOOL cancelService;

    // Protocol index
    UINT32 protocolIndex;

    // Call info object
    CallInfo callInfo;

    // Back reference to the signaling client
    PSignalingClient pSignalingClient;

    // Scratch buffer for http processing
    CHAR buffer[LWS_SCRATCH_BUFFER_SIZE];

    // Scratch buffer for sending
    BYTE sendBuffer[LWS_MESSAGE_BUFFER_SIZE];

    // Scratch buffer for receiving
    BYTE receiveBuffer[LWS_MESSAGE_BUFFER_SIZE];

    // Size of the data in the receive buffer
    UINT32 receiveBufferSize;
};

typedef struct {
    // The first member is the public signaling message structure
    ReceivedSignalingMessage receivedSignalingMessage;

    // The messaging client object
    PSignalingClient pSignalingClient;
} SignalingMessageWrapper, *PSignalingMessageWrapper;

// Signal handler routine
VOID lwsSignalHandler(INT32);

// Performs a blocking call
STATUS lwsCompleteSync(PLwsCallInfo);

// LWS listener handler
PVOID lwsListenerHandler(PVOID);

// Retry thread
PVOID reconnectHandler(PVOID);

// LWS callback routine
INT32 lwsHttpCallbackRoutine(struct lws*, enum lws_callback_reasons, PVOID, PVOID, size_t);
INT32 lwsWssCallbackRoutine(struct lws*, enum lws_callback_reasons, PVOID, PVOID, size_t);

BOOL isCallResultSignatureExpired(PCallInfo);
BOOL isCallResultSignatureNotYetCurrent(PCallInfo);

STATUS describeChannelLws(PSignalingClient, UINT64);
STATUS createChannelLws(PSignalingClient, UINT64);
STATUS getChannelEndpointLws(PSignalingClient, UINT64);
STATUS getIceConfigLws(PSignalingClient, UINT64);
STATUS connectSignalingChannelLws(PSignalingClient, UINT64);
STATUS joinStorageSessionLws(PSignalingClient, UINT64);
STATUS describeMediaStorageConfLws(PSignalingClient, UINT64);
STATUS deleteChannelLws(PSignalingClient, UINT64);

STATUS createLwsCallInfo(PSignalingClient, PRequestInfo, UINT32, PLwsCallInfo*);
STATUS freeLwsCallInfo(PLwsCallInfo*);

PVOID receiveLwsMessageWrapper(PVOID);

STATUS sendLwsMessage(PSignalingClient, SIGNALING_MESSAGE_TYPE, PCHAR, PCHAR, UINT32, PCHAR, UINT32);
STATUS writeLwsData(PSignalingClient, BOOL);
STATUS terminateLwsListenerLoop(PSignalingClient);
STATUS receiveLwsMessage(PSignalingClient, PCHAR, UINT32);
PCHAR getMessageTypeInString(SIGNALING_MESSAGE_TYPE);
STATUS wakeLwsServiceEventLoop(PSignalingClient, UINT32);
STATUS terminateConnectionWithStatus(PSignalingClient, SERVICE_CALL_RESULT);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_LWS_API_CALLS__ */
