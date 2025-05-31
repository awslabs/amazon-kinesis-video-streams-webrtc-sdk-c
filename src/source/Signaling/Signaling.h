/*******************************************
Signaling internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__
#define __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Request id header name
#define SIGNALING_REQUEST_ID_HEADER_NAME KVS_REQUEST_ID_HEADER_NAME ":"

// Signaling client from custom data conversion
#define SIGNALING_CLIENT_FROM_CUSTOM_DATA(h) ((PSignalingClient) (h))
#define CUSTOM_DATA_FROM_SIGNALING_CLIENT(p) ((UINT64) (p))

// Grace period for refreshing the ICE configuration
#define ICE_CONFIGURATION_REFRESH_GRACE_PERIOD (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Termination timeout
#define SIGNALING_CLIENT_SHUTDOWN_TIMEOUT ((2 + SIGNALING_SERVICE_API_CALL_TIMEOUT_IN_SECONDS) * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Signaling client state literal definitions
#define SIGNALING_CLIENT_STATE_UNKNOWN_STR                "Unknown"
#define SIGNALING_CLIENT_STATE_NEW_STR                    "New"
#define SIGNALING_CLIENT_STATE_GET_CREDENTIALS_STR        "Get Security Credentials"
#define SIGNALING_CLIENT_STATE_DESCRIBE_STR               "Describe Channel"
#define SIGNALING_CLIENT_STATE_CREATE_STR                 "Create Channel"
#define SIGNALING_CLIENT_STATE_GET_ENDPOINT_STR           "Get Channel Endpoint"
#define SIGNALING_CLIENT_STATE_GET_ICE_CONFIG_STR         "Get ICE Server Configuration"
#define SIGNALING_CLIENT_STATE_READY_STR                  "Ready"
#define SIGNALING_CLIENT_STATE_CONNECTING_STR             "Connecting"
#define SIGNALING_CLIENT_STATE_CONNECTED_STR              "Connected"
#define SIGNALING_CLIENT_STATE_DISCONNECTED_STR           "Disconnected"
#define SIGNALING_CLIENT_STATE_DELETE_STR                 "Delete"
#define SIGNALING_CLIENT_STATE_DELETED_STR                "Deleted"
#define SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA_STR         "Describe Media Storage"
#define SIGNALING_CLIENT_STATE_JOIN_SESSION_STR           "Join Session"
#define SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING_STR   "Join Session Waiting"
#define SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED_STR "Join Session Connected"

// Error refreshing ICE server configuration string
#define SIGNALING_ICE_CONFIG_REFRESH_ERROR_MSG "Failed refreshing ICE server configuration with status code 0x%08x."

// Error reconnecting to the signaling service
#define SIGNALING_RECONNECT_ERROR_MSG "Failed to reconnect with status code 0x%08x."

// Max error string length
#define SIGNALING_MAX_ERROR_MESSAGE_LEN 512

// Async ICE config refresh delay in case if the signaling is not yet in READY state
#define SIGNALING_ASYNC_ICE_CONFIG_REFRESH_DELAY (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// Max libWebSockets protocol count. IMPORTANT: Ensure it's 1 + PROTOCOL_INDEX_WSS
#define LWS_PROTOCOL_COUNT 2

/**
 * Default signaling clockskew (endpoint --> clockskew) hash table bucket count/length
 */
#define SIGNALING_CLOCKSKEW_HASH_TABLE_BUCKET_LENGTH 2
#define SIGNALING_CLOCKSKEW_HASH_TABLE_BUCKET_COUNT  MIN_HASH_BUCKET_COUNT // 16

// API call latency calculation
#define SIGNALING_API_LATENCY_CALCULATION(pClient, time, isCpApi)                                                                                    \
    MUTEX_LOCK((pClient)->diagnosticsLock);                                                                                                          \
    if (isCpApi) {                                                                                                                                   \
        (pClient)->diagnostics.cpApiLatency =                                                                                                        \
            EMA_ACCUMULATOR_GET_NEXT((pClient)->diagnostics.cpApiLatency, SIGNALING_GET_CURRENT_TIME((pClient)) - (time));                           \
    } else {                                                                                                                                         \
        (pClient)->diagnostics.dpApiLatency =                                                                                                        \
            EMA_ACCUMULATOR_GET_NEXT((pClient)->diagnostics.dpApiLatency, SIGNALING_GET_CURRENT_TIME((pClient)) - (time));                           \
    }                                                                                                                                                \
    MUTEX_UNLOCK((pClient)->diagnosticsLock);

#define SIGNALING_UPDATE_ERROR_COUNT(pClient, status)                                                                                                \
    if ((pClient) != NULL && STATUS_FAILED(status)) {                                                                                                \
        ATOMIC_INCREMENT(&(pClient)->diagnostics.numberOfErrors);                                                                                    \
    }

#define IS_CURRENT_TIME_CALLBACK_SET(pClient) ((pClient) != NULL && ((pClient)->signalingClientCallbacks.getCurrentTimeFn != NULL))

#define SIGNALING_GET_CURRENT_TIME(pClient)                                                                                                          \
    (IS_CURRENT_TIME_CALLBACK_SET((pClient))                                                                                                         \
         ? ((pClient)->signalingClientCallbacks.getCurrentTimeFn((pClient)->signalingClientCallbacks.customData))                                    \
         : GETTIME())

#define DEFAULT_CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS 7

#define SIGNALING_STATE_MACHINE_NAME (PCHAR) "SIGNALING"

static const ExponentialBackoffRetryStrategyConfig DEFAULT_SIGNALING_STATE_MACHINE_EXPONENTIAL_BACKOFF_RETRY_CONFIGURATION = {
    /* Exponential wait times with this config will look like following -
        ************************************
        * Retry Count *      Wait time     *
        * **********************************
        *     1       *    100ms + jitter  *
        *     2       *    200ms + jitter  *
        *     3       *    400ms + jitter  *
        *     4       *    800ms + jitter  *
        *     5       *   1600ms + jitter  *
        *     6       *   3200ms + jitter  *
        *     7       *   6400ms + jitter  *
        *     8       *  10000ms + jitter  *
        *     9       *  10000ms + jitter  *
        *    10       *  10000ms + jitter  *
        ************************************
        jitter = random number between [0, wait time)
    */
    KVS_INFINITE_EXPONENTIAL_RETRIES,                       /* max retry count */
    10000,                                                  /* max retry wait time in milliseconds */
    100,                                                    /* factor determining exponential curve in milliseconds */
    DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, /* minimum time in milliseconds to reset retry state */
    FULL_JITTER,                                            /* use full jitter variant */
    0                                                       /* jitter value unused for full jitter variant */
};

// Forward declaration
typedef struct __LwsCallInfo* PLwsCallInfo;

// Testability hooks functions
typedef STATUS (*SignalingApiCallHookFunc)(UINT64);

/**
 * Internal client info object
 */
typedef struct {
    // Public client info structure
    SignalingClientInfo signalingClientInfo;

    // V1 features
    CHAR cacheFilePath[MAX_PATH_LEN + 1];

    //
    // Below members will be used for direct injection for tests hooks
    //

    // Injected connect timeout
    UINT64 connectTimeout;

    // Custom data to be passed to the hooks
    UINT64 hookCustomData;

    // API pre and post ingestion points
    SignalingApiCallHookFunc describePreHookFn;
    SignalingApiCallHookFunc describePostHookFn;
    SignalingApiCallHookFunc createPreHookFn;
    SignalingApiCallHookFunc createPostHookFn;
    SignalingApiCallHookFunc getEndpointPreHookFn;
    SignalingApiCallHookFunc getEndpointPostHookFn;
    SignalingApiCallHookFunc getIceConfigPreHookFn;
    SignalingApiCallHookFunc getIceConfigPostHookFn;
    SignalingApiCallHookFunc connectPreHookFn;
    SignalingApiCallHookFunc connectPostHookFn;
    SignalingApiCallHookFunc joinSessionPreHookFn;
    SignalingApiCallHookFunc joinSessionPostHookFn;
    SignalingApiCallHookFunc describeMediaStorageConfPreHookFn;
    SignalingApiCallHookFunc describeMediaStorageConfPostHookFn;
    SignalingApiCallHookFunc deletePreHookFn;
    SignalingApiCallHookFunc deletePostHookFn;

    // Retry strategy used for signaling state machine
    KvsRetryStrategy signalingStateMachineRetryStrategy;
    KvsRetryStrategyCallbacks signalingStateMachineRetryStrategyCallbacks;
} SignalingClientInfoInternal, *PSignalingClientInfoInternal;

/**
 * Thread execution tracker
 */
typedef struct {
    volatile ATOMIC_BOOL terminated;
    TID threadId;
    MUTEX lock;
    CVAR await;
} ThreadTracker, *PThreadTracker;

/**
 * Internal structure tracking various parameters for diagnostics and metrics/stats
 */
typedef struct {
    volatile SIZE_T numberOfMessagesSent;
    volatile SIZE_T numberOfMessagesReceived;
    volatile SIZE_T iceRefreshCount;
    volatile SIZE_T numberOfErrors;
    volatile SIZE_T numberOfRuntimeErrors;
    volatile SIZE_T numberOfReconnects;
    UINT64 describeChannelStartTime;
    UINT64 describeChannelEndTime;
    UINT64 getSignalingChannelEndpointStartTime;
    UINT64 getSignalingChannelEndpointEndTime;
    UINT64 getIceServerConfigStartTime;
    UINT64 getIceServerConfigEndTime;
    UINT64 getTokenStartTime;
    UINT64 getTokenEndTime;
    UINT64 createChannelStartTime;
    UINT64 createChannelEndTime;
    UINT64 connectStartTime;
    UINT64 connectEndTime;
    UINT64 createTime;
    UINT64 connectTime;
    UINT64 cpApiLatency;
    UINT64 dpApiLatency;
    UINT64 getTokenCallTime;
    UINT64 describeCallTime;
    UINT64 describeMediaCallTime;
    UINT64 createCallTime;
    UINT64 getEndpointCallTime;
    UINT64 getIceConfigCallTime;
    UINT64 connectCallTime;
    UINT64 createClientTime;
    UINT64 fetchClientTime;
    UINT64 connectClientTime;
    UINT64 offerToAnswerTime;
    UINT64 joinSessionCallTime;
    UINT64 joinSessionToOfferRecvTime;
    PHashTable pEndpointToClockSkewHashMap;
    UINT32 stateMachineRetryCount;
} SignalingDiagnostics, PSignalingDiagnostics;

/**
 * Internal representation of the Signaling client.
 */
typedef struct {
    // Current version of the structure
    UINT32 version;

    // Current service call result
    volatile SIZE_T result;

    // Sent message result
    volatile SIZE_T messageResult;

    // Client is ready to connect to signaling channel
    volatile ATOMIC_BOOL clientReady;

    // Shutting down the entire client
    volatile ATOMIC_BOOL shutdown;

    // Wss is connected
    volatile ATOMIC_BOOL connected;

    // The channel is being deleted
    volatile ATOMIC_BOOL deleting;

    // The channel is deleted
    volatile ATOMIC_BOOL deleted;

    // Having state machine logic rely on call result of SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE
    // to transition to ICE config state is not enough in Async update mode when
    // connect is in progress as the result of connect will override the result
    // of SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE indicating state transition
    // if it comes first forcing the state machine to loop back to connected state.
    volatile ATOMIC_BOOL refreshIceConfig;

    // Indicates that there is another thread attempting to grab the service lock
    volatile ATOMIC_BOOL serviceLockContention;

    volatile ATOMIC_BOOL offerReceived;

    // Stored Client info
    SignalingClientInfoInternal clientInfo;

    // Stored callbacks
    SignalingClientCallbacks signalingClientCallbacks;

    // AWS credentials provider
    PAwsCredentialProvider pCredentialProvider;

    // Channel info
    PChannelInfo pChannelInfo;

    // Returned signaling channel description
    SignalingChannelDescription channelDescription;

    // Returned media storage session
    MediaStorageConfig mediaStorageConfig;

    // Signaling endpoint
    CHAR channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN + 1];

    // Signaling endpoint
    CHAR channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN + 1];

    // Media storage endpoint
    CHAR channelEndpointWebrtc[MAX_SIGNALING_ENDPOINT_URI_LEN + 1];

    // Number of Ice Server objects
    UINT32 iceConfigCount;

    // Returned Ice configurations
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];

    // The state machine
    PStateMachine pStateMachine;

    // Current AWS credentials
    PAwsCredentials pAwsCredentials;

    // Service call context
    ServiceCallContext serviceCallContext;

    // Interlocking the state transitions
    MUTEX stateLock;

    // Sync mutex for connected condition variable
    MUTEX connectedLock;

    // Conditional variable for Connected state
    CVAR connectedCvar;

    // Sync mutex for sending condition variable
    MUTEX sendLock;

    // Conditional variable for sending interlock
    CVAR sendCvar;

    // Sync mutex for receiving response to the message condition variable
    MUTEX receiveLock;

    // Conditional variable for receiving response to the sent message
    CVAR receiveCvar;

    // Indicates when the ICE configuration has been retrieved
    UINT64 iceConfigTime;

    // Indicates when the ICE configuration is considered expired
    UINT64 iceConfigExpiration;

    // Ongoing listener call info
    PLwsCallInfo pOngoingCallInfo;

    // Listener thread for the socket
    ThreadTracker listenerTracker;

    // Restarted thread handler
    ThreadTracker reconnecterTracker;

    // LWS context to use for Restful API
    struct lws_context* pLwsContext;

    // Signaling protocols - one more for the NULL terminator protocol
    struct lws_protocols signalingProtocols[LWS_PROTOCOL_COUNT + 1];

    // Stored wsi objects
    struct lws* currentWsi[LWS_PROTOCOL_COUNT];

    // List of the ongoing messages
    PStackQueue pMessageQueue;

    // Message queue lock
    MUTEX messageQueueLock;

    // LWS needs to be locked
    MUTEX lwsServiceLock;

    // Serialized access to LWS service call
    MUTEX lwsSerializerLock;

    // Re-entrant lock for diagnostics/stats
    MUTEX diagnosticsLock;

    // Internal diagnostics object
    SignalingDiagnostics diagnostics;

    // Tracking when was the Last time the APIs were called
    UINT64 describeTime;
    UINT64 createTime;
    UINT64 getEndpointTime;
    UINT64 getIceConfigTime;
    UINT64 deleteTime;
    UINT64 connectTime;
    UINT64 describeMediaTime;
    UINT64 answerTime;
    UINT64 offerReceivedTime;
    UINT64 offerSentTime;

    MUTEX offerSendReceiveTimeLock;
    UINT64 joinSessionTime;

    // mutex for join session wait condition variable
    MUTEX jssWaitLock;

    // Conditional variable for join storage session wait state
    CVAR jssWaitCvar;
} SignalingClient, *PSignalingClient;

// Public handle to and from object converters
#define TO_SIGNALING_CLIENT_HANDLE(p)   ((SIGNALING_CLIENT_HANDLE) (p))
#define FROM_SIGNALING_CLIENT_HANDLE(h) (IS_VALID_SIGNALING_CLIENT_HANDLE(h) ? (PSignalingClient) (h) : NULL)

STATUS createSignalingSync(PSignalingClientInfoInternal, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSignalingClient*);
STATUS freeSignaling(PSignalingClient*);

STATUS signalingSendMessageSync(PSignalingClient, PSignalingMessage);
STATUS signalingGetIceConfigInfoCount(PSignalingClient, PUINT32);
STATUS signalingGetIceConfigInfo(PSignalingClient, UINT32, PIceConfigInfo*);
STATUS signalingFetchSync(PSignalingClient);
STATUS signalingConnectSync(PSignalingClient);
STATUS signalingDisconnectSync(PSignalingClient);
STATUS signalingDeleteSync(PSignalingClient);

STATUS validateSignalingCallbacks(PSignalingClient, PSignalingClientCallbacks);
STATUS validateSignalingClientInfo(PSignalingClient, PSignalingClientInfoInternal);
STATUS validateIceConfiguration(PSignalingClient);

STATUS signalingStoreOngoingMessage(PSignalingClient, PSignalingMessage);
STATUS signalingRemoveOngoingMessage(PSignalingClient, PCHAR);
STATUS signalingGetOngoingMessage(PSignalingClient, PCHAR, PCHAR, PSignalingMessage*);

STATUS refreshIceConfiguration(PSignalingClient);

UINT64 signalingGetCurrentTime(UINT64);

STATUS awaitForThreadTermination(PThreadTracker, UINT64);
STATUS initializeThreadTracker(PThreadTracker);
STATUS uninitializeThreadTracker(PThreadTracker);

STATUS terminateOngoingOperations(PSignalingClient);

STATUS describeChannel(PSignalingClient, UINT64);
STATUS createChannel(PSignalingClient, UINT64);
STATUS getChannelEndpoint(PSignalingClient, UINT64);
STATUS getIceConfig(PSignalingClient, UINT64);
STATUS connectSignalingChannel(PSignalingClient, UINT64);
STATUS joinStorageSession(PSignalingClient, UINT64);
STATUS describeMediaStorageConf(PSignalingClient, UINT64);
STATUS deleteChannel(PSignalingClient, UINT64);
STATUS signalingGetMetrics(PSignalingClient, PSignalingClientMetrics);

STATUS configureRetryStrategyForSignalingStateMachine(PSignalingClient);
STATUS setupDefaultRetryStrategyForSignalingStateMachine(PSignalingClient);
STATUS freeClientRetryStrategy(PSignalingClient);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__ */
