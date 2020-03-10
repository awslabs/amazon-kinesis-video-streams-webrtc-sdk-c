/*******************************************
Signaling internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__
#define __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// Request id header name
#define SIGNALING_REQUEST_ID_HEADER_NAME                                    KVS_REQUEST_ID_HEADER_NAME ":"

// Signaling client from custom data conversion
#define SIGNALING_CLIENT_FROM_CUSTOM_DATA(h) ((PSignalingClient) (h))
#define CUSTOM_DATA_FROM_SIGNALING_CLIENT(p) ((UINT64) (p))

// Grace period for refreshing the ICE configuration
#define ICE_CONFIGURATION_REFRESH_GRACE_PERIOD                              (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Termination timeout
#define SIGNALING_CLIENT_SHUTDOWN_TIMEOUT                                   (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Signaling client state literal definitions
#define SIGNALING_CLIENT_STATE_UNKNOWN_STR                                  "Unknown"
#define SIGNALING_CLIENT_STATE_NEW_STR                                      "New"
#define SIGNALING_CLIENT_STATE_GET_CREDENTIALS_STR                          "Get Security Credentials"
#define SIGNALING_CLIENT_STATE_DESCRIBE_STR                                 "Describe Channel"
#define SIGNALING_CLIENT_STATE_CREATE_STR                                   "Create Channel"
#define SIGNALING_CLIENT_STATE_GET_ENDPOINT_STR                             "Get Channel Endpoint"
#define SIGNALING_CLIENT_STATE_GET_ICE_CONFIG_STR                           "Get ICE Server Configuration"
#define SIGNALING_CLIENT_STATE_READY_STR                                    "Ready"
#define SIGNALING_CLIENT_STATE_CONNECTING_STR                               "Connecting"
#define SIGNALING_CLIENT_STATE_CONNECTED_STR                                "Connected"
#define SIGNALING_CLIENT_STATE_DISCONNECTED_STR                             "Disconnected"

// Error refreshing ICE server configuration string
#define SIGNALING_ICE_CONFIG_REFRESH_ERROR_MSG                              "Failed refreshing ICE server configuration with status code 0x%08x."

// Error reconnecting to the signaling service
#define SIGNALING_RECONNECT_ERROR_MSG                                       "Failed to reconnect with status code 0x%08x."

// Max error string length
#define SIGNALING_MAX_ERROR_MESSAGE_LEN                                     512

// Forward declaration
typedef struct __LwsCallInfo *PLwsCallInfo;

// Testability hooks functions
typedef STATUS (*SignalingApiCallHookFunc)(UINT64, UINT64);

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

    // Timer queue to handle stale ICE configuration
    TIMER_QUEUE_HANDLE timerQueueHandle;
} SignalingClient, *PSignalingClient;

// Public handle to and from object converters
#define TO_SIGNALING_CLIENT_HANDLE(p) ((SIGNALING_CLIENT_HANDLE) (p))
#define FROM_SIGNALING_CLIENT_HANDLE(h) (IS_VALID_SIGNALING_CLIENT_HANDLE(h) ? (PSignalingClient) (h) : NULL)

STATUS createSignalingSync(PSignalingClientInfoInternal, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSignalingClient*);
STATUS freeSignaling(PSignalingClient*);

STATUS signalingSendMessageSync(PSignalingClient, PSignalingMessage);
STATUS signalingGetIceConfigInfoCout(PSignalingClient, PUINT32);
STATUS signalingGetIceConfigInfo(PSignalingClient, UINT32, PIceConfigInfo*);
STATUS signalingConnectSync(PSignalingClient);

STATUS validateSignalingCallbacks(PSignalingClient, PSignalingClientCallbacks);
STATUS validateSignalingClientInfo(PSignalingClient, PSignalingClientInfoInternal);
STATUS validateIceConfiguration(PSignalingClient);

STATUS signalingStoreOngoingMessage(PSignalingClient, PSignalingMessage);
STATUS signalingRemoveOngoingMessage(PSignalingClient, PCHAR);
STATUS signalingGetOngoingMessage(PSignalingClient, PCHAR, PCHAR, PSignalingMessage*);

STATUS refreshIceConfigurationCallback(UINT32, UINT64, UINT64);

STATUS awaitForThreadTermination(PThreadTracker, UINT64);
STATUS initializeThreadTracker(PThreadTracker);
STATUS uninitializeThreadTracker(PThreadTracker);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__ */
