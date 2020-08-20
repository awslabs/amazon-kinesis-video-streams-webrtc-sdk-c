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
#define SIGNALING_CLIENT_FROM_CUSTOM_DATA(h) ((PSignalingClient)(h))
#define CUSTOM_DATA_FROM_SIGNALING_CLIENT(p) ((UINT64)(p))

// Grace period for refreshing the ICE configuration
#define ICE_CONFIGURATION_REFRESH_GRACE_PERIOD (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Termination timeout
#define SIGNALING_CLIENT_SHUTDOWN_TIMEOUT (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Signaling client state literal definitions
#define SIGNALING_CLIENT_STATE_UNKNOWN_STR         "Unknown"
#define SIGNALING_CLIENT_STATE_NEW_STR             "New"
#define SIGNALING_CLIENT_STATE_GET_CREDENTIALS_STR "Get Security Credentials"
#define SIGNALING_CLIENT_STATE_DESCRIBE_STR        "Describe Channel"
#define SIGNALING_CLIENT_STATE_CREATE_STR          "Create Channel"
#define SIGNALING_CLIENT_STATE_GET_ENDPOINT_STR    "Get Channel Endpoint"
#define SIGNALING_CLIENT_STATE_GET_ICE_CONFIG_STR  "Get ICE Server Configuration"
#define SIGNALING_CLIENT_STATE_READY_STR           "Ready"
#define SIGNALING_CLIENT_STATE_CONNECTING_STR      "Connecting"
#define SIGNALING_CLIENT_STATE_CONNECTED_STR       "Connected"
#define SIGNALING_CLIENT_STATE_DISCONNECTED_STR    "Disconnected"
#define SIGNALING_CLIENT_STATE_DELETE_STR          "Delete"
#define SIGNALING_CLIENT_STATE_DELETED_STR         "Deleted"

// Error refreshing ICE server configuration string
#define SIGNALING_ICE_CONFIG_REFRESH_ERROR_MSG "Failed refreshing ICE server configuration with status code 0x%08x."

// Error reconnecting to the signaling service
#define SIGNALING_RECONNECT_ERROR_MSG "Failed to reconnect with status code 0x%08x."

// Max error string length
#define SIGNALING_MAX_ERROR_MESSAGE_LEN 512

// Async ICE config refresh delay in case if the signaling is not yet in READY state
#define SIGNALING_ASYNC_ICE_CONFIG_REFRESH_DELAY (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// API call latency calculation
#define SIGNALING_API_LATENCY_CALCULATION(pClient, time, isCpApi)                                                                                    \
    MUTEX_LOCK((pClient)->diagnosticsLock);                                                                                                          \
    if (isCpApi) {                                                                                                                                   \
        (pClient)->diagnostics.cpApiLatency = EMA_ACCUMULATOR_GET_NEXT((pClient)->diagnostics.cpApiLatency, GETTIME() - (time));                     \
    } else {                                                                                                                                         \
        (pClient)->diagnostics.dpApiLatency = EMA_ACCUMULATOR_GET_NEXT((pClient)->diagnostics.dpApiLatency, GETTIME() - (time));                     \
    }                                                                                                                                                \
    MUTEX_UNLOCK((pClient)->diagnosticsLock);

#define SIGNALING_UPDATE_ERROR_COUNT(pClient, status)                                                                                                \
    if ((pClient) != NULL && STATUS_FAILED(status)) {                                                                                                \
        ATOMIC_INCREMENT(&(pClient)->diagnostics.numberOfErrors);                                                                                    \
    }

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

    //
    // Below members will be used for direct injection for tests hooks
    //

    // Injected ICE server refresh period
    UINT64 iceRefreshPeriod;

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
    SignalingApiCallHookFunc deletePreHookFn;
    SignalingApiCallHookFunc deletePostHookFn;
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
    UINT64 createTime;
    UINT64 connectTime;
    UINT64 cpApiLatency;
    UINT64 dpApiLatency;
} SignalingDiagnostics, PSignalingDiagnostics;

/**
 * Internal representation of the Signaling client.
 */
typedef struct {
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

    // Based on the channel info we can async the ice config on create channel
    // call only and not async on repeat state transition when refreshing for example.
    volatile ATOMIC_BOOL asyncGetIceConfig;

    // Having state machine logic rely on call result of SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE
    // to transition to ICE config state is not enough in Async update mode when
    // connect is in progress as the result of connect will override the result
    // of SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE indicating state transition
    // if it comes first forcing the state machine to loop back to connected state.
    volatile ATOMIC_BOOL refreshIceConfig;

    // Indicate whether the ICE configuration has been retrieved at least once
    volatile ATOMIC_BOOL iceConfigRetrieved;

    // Current version of the structure
    UINT32 version;

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

    // Signaling endpoint
    CHAR channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN + 1];

    // Signaling endpoint
    CHAR channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN + 1];

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

    // Indicates whether to self-prime on Ready or not
    BOOL continueOnReady;

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

    // Execute the state machine until this time
    UINT64 stepUntil;

    // Ongoing listener call info
    PLwsCallInfo pOngoingCallInfo;

    // Listener thread for the socket
    ThreadTracker listenerTracker;

    // Restarted thread handler
    ThreadTracker reconnecterTracker;

    // LWS context to use for Restful API
    struct lws_context* pLwsContext;

    // Signaling protocols
    struct lws_protocols signalingProtocols[3];

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

    // Timer queue to handle stale ICE configuration
    TIMER_QUEUE_HANDLE timerQueueHandle;

    // Internal diagnostics object
    SignalingDiagnostics diagnostics;

    // Tracking when was the Last time the APIs were called
    UINT64 describeTime;
    UINT64 createTime;
    UINT64 getEndpointTime;
    UINT64 getIceConfigTime;
    UINT64 deleteTime;
    UINT64 connectTime;
} SignalingClient, *PSignalingClient;

// Public handle to and from object converters
#define TO_SIGNALING_CLIENT_HANDLE(p)   ((SIGNALING_CLIENT_HANDLE)(p))
#define FROM_SIGNALING_CLIENT_HANDLE(h) (IS_VALID_SIGNALING_CLIENT_HANDLE(h) ? (PSignalingClient)(h) : NULL)

STATUS createSignalingSync(PSignalingClientInfoInternal, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSignalingClient*);
STATUS freeSignaling(PSignalingClient*);

STATUS signalingSendMessageSync(PSignalingClient, PSignalingMessage);
STATUS signalingGetIceConfigInfoCout(PSignalingClient, PUINT32);
STATUS signalingGetIceConfigInfo(PSignalingClient, UINT32, PIceConfigInfo*);
STATUS signalingConnectSync(PSignalingClient);
STATUS signalingDisconnectSync(PSignalingClient);
STATUS signalingDeleteSync(PSignalingClient);

STATUS validateSignalingCallbacks(PSignalingClient, PSignalingClientCallbacks);
STATUS validateSignalingClientInfo(PSignalingClient, PSignalingClientInfoInternal);
STATUS validateIceConfiguration(PSignalingClient);

STATUS signalingStoreOngoingMessage(PSignalingClient, PSignalingMessage);
STATUS signalingRemoveOngoingMessage(PSignalingClient, PCHAR);
STATUS signalingGetOngoingMessage(PSignalingClient, PCHAR, PCHAR, PSignalingMessage*);

STATUS refreshIceConfigurationCallback(UINT32, UINT64, UINT64);

UINT64 signalingGetCurrentTime(UINT64);

STATUS awaitForThreadTermination(PThreadTracker, UINT64);
STATUS initializeThreadTracker(PThreadTracker);
STATUS uninitializeThreadTracker(PThreadTracker);

STATUS terminateOngoingOperations(PSignalingClient, BOOL);

STATUS describeChannel(PSignalingClient, UINT64);
STATUS createChannel(PSignalingClient, UINT64);
STATUS getChannelEndpoint(PSignalingClient, UINT64);
STATUS getIceConfig(PSignalingClient, UINT64);
STATUS connectSignalingChannel(PSignalingClient, UINT64);
STATUS deleteChannel(PSignalingClient, UINT64);
STATUS signalingGetMetrics(PSignalingClient, PSignalingClientMetrics);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__ */
