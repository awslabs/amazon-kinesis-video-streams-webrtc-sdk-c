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

// API postfix definitions
#define CREATE_SIGNALING_CHANNEL_API_POSTFIX       "/createSignalingChannel"
#define DESCRIBE_SIGNALING_CHANNEL_API_POSTFIX     "/describeSignalingChannel"
#define GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX "/getSignalingChannelEndpoint"
#define DELETE_SIGNALING_CHANNEL_API_POSTFIX       "/deleteSignalingChannel"
#define GET_ICE_CONFIG_API_POSTFIX                 "/v1/get-ice-server-config"
#define JOIN_STORAGE_SESSION_API_POSTFIX           "/joinStorageSession"
#define DESCRIBE_MEDIA_STORAGE_CONF_API_POSTFIX    "/describeMediaStorageConfiguration"
#define UPDATE_MEDIA_STORAGE_CONF_API_POSTFIX      "/updateMediaStorageConfiguration"

// Signaling protocol name
#define SIGNALING_CHANNEL_PROTOCOL                 "\"WSS\", \"HTTPS\""
#define SIGNALING_CHANNEL_PROTOCOL_W_MEDIA_STORAGE "\"WSS\", \"HTTPS\", \"WEBRTC\""

// Parameterized string for Describe Channel API
#define DESCRIBE_CHANNEL_PARAM_JSON_TEMPLATE            "{\n\t\"ChannelName\": \"%s\"\n}"
#define DESCRIBE_MEDIA_STORAGE_CONF_PARAM_JSON_TEMPLATE "{\n\t\"ChannelARN\": \"%s\"\n}"

// Parameterized string for Delete Channel API
#define DELETE_CHANNEL_PARAM_JSON_TEMPLATE                                                                                                           \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"CurrentVersion\": \"%s\"\n}"

// Parameterized string for Create Channel API
#define CREATE_CHANNEL_PARAM_JSON_TEMPLATE                                                                                                           \
    "{\n\t\"ChannelName\": \"%s\","                                                                                                                  \
    "\n\t\"ChannelType\": \"%s\","                                                                                                                   \
    "\n\t\"SingleMasterConfiguration\": {"                                                                                                           \
    "\n\t\t\"MessageTtlSeconds\": %" PRIu64 "\n\t}"                                                                                                  \
    "%s\n}"

// Parameterized string for each tag pair as a JSON object
#define TAG_PARAM_JSON_OBJ_TEMPLATE "\n\t\t{\"Key\": \"%s\", \"Value\": \"%s\"},"

// Parameterized string for TagStream API - we should have at least one tag
#define TAGS_PARAM_JSON_TEMPLATE ",\n\t\"Tags\": [%s\n\t]"

// Parameterized string for Get Channel Endpoint API
#define GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE                                                                                                     \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"SingleMasterChannelEndpointConfiguration\": {"                                                                                            \
    "\n\t\t\"Protocols\": [%s],"                                                                                                                     \
    "\n\t\t\"Role\": \"%s\""                                                                                                                         \
    "\n\t}\n}"

// Parameterized string for Get Ice Server Config API
#define GET_ICE_CONFIG_PARAM_JSON_TEMPLATE                                                                                                           \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"ClientId\": \"%s\","                                                                                                                      \
    "\n\t\"Service\": \"TURN\""                                                                                                                      \
    "\n}"

#define SIGNALING_JOIN_STORAGE_SESSION_MASTER_PARAM_JSON_TEMPLATE "{\n\t\"channelArn\": \"%s\"\n}"
#define SIGNALING_JOIN_STORAGE_SESSION_VIEWER_PARAM_JSON_TEMPLATE                                                                                    \
    "{\n\t\"channelArn\": \"%s\","                                                                                                                   \
    "\n\t\"clientId\": \"%s\"\n}"
#define SIGNALING_UPDATE_STORAGE_CONFIG_PARAM_JSON_TEMPLATE                                                                                          \
    "{\n\t\"StreamARN\": \"%s\","                                                                                                                    \
    "\n\t\"ChannelARN\": \"%s\","                                                                                                                    \
    "\n\t\"StorageStatus\": \"%s\""                                                                                                                  \
    "\n}"

// Parameter names for Signaling connect URL
#define SIGNALING_ROLE_PARAM_NAME         "X-Amz-Role"
#define SIGNALING_CHANNEL_NAME_PARAM_NAME "X-Amz-ChannelName"
#define SIGNALING_CHANNEL_ARN_PARAM_NAME  "X-Amz-ChannelARN"
#define SIGNALING_CLIENT_ID_PARAM_NAME    "X-Amz-ClientId"

// Parameterized string for WSS connect
#define SIGNALING_ENDPOINT_MASTER_URL_WSS_TEMPLATE "%s?%s=%s"
#define SIGNALING_ENDPOINT_VIEWER_URL_WSS_TEMPLATE "%s?%s=%s&%s=%s"

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

// Send message JSON template
#define SIGNALING_SEND_MESSAGE_TEMPLATE                                                                                                              \
    "{\n"                                                                                                                                            \
    "\t\"action\": \"%s\",\n"                                                                                                                        \
    "\t\"RecipientClientId\": \"%.*s\",\n"                                                                                                           \
    "\t\"MessagePayload\": \"%s\"%s\n"                                                                                                               \
    "}"

// Send message JSON template with correlation id
#define SIGNALING_SEND_MESSAGE_TEMPLATE_WITH_CORRELATION_ID                                                                                          \
    "{\n"                                                                                                                                            \
    "\t\"action\": \"%s\",\n"                                                                                                                        \
    "\t\"RecipientClientId\": \"%.*s\",\n"                                                                                                           \
    "\t\"MessagePayload\": \"%s\",\n"                                                                                                                \
    "\t\"CorrelationId\": \"%.*s\"%s\n"                                                                                                              \
    "}"

#define SIGNALING_ICE_SERVER_LIST_TEMPLATE_START                                                                                                     \
    ",\n"                                                                                                                                            \
    "\t\"IceServerList\": ["

#define SIGNALING_ICE_SERVER_LIST_TEMPLATE_END "\n\t]"

#define SIGNALING_ICE_SERVER_TEMPLATE                                                                                                                \
    "\n"                                                                                                                                             \
    "\t\t{\n"                                                                                                                                        \
    "\t\t\t\"Password\": \"%s\",\n"                                                                                                                  \
    "\t\t\t\"Ttl\": %" PRIu64 ",\n"                                                                                                                  \
    "\t\t\t\"Uris\": [%s],\n"                                                                                                                        \
    "\t\t\t\"Username\": \"%s\"\n"                                                                                                                   \
    "\t\t},"

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
STATUS getMessageTypeFromString(PCHAR, UINT32, SIGNALING_MESSAGE_TYPE*);
PCHAR getMessageTypeInString(SIGNALING_MESSAGE_TYPE);
STATUS wakeLwsServiceEventLoop(PSignalingClient, UINT32);
STATUS terminateConnectionWithStatus(PSignalingClient, SERVICE_CALL_RESULT);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_LWS_API_CALLS__ */
