/*******************************************
Signaling internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__
#define __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

// Request id header name
#define SIGNALING_REQUEST_ID_HEADER_NAME                                    KVS_REQUEST_ID_HEADER_NAME ":"

// Signaling client from custom data conversion
#define SIGNALING_CLIENT_FROM_CUSTOM_DATA(h) ((PSignalingClient) (h))
#define CUSTOM_DATA_FROM_SIGNALING_CLIENT(p) ((UINT64) (p))

// Grace period for refreshing the ICE configuration
#define ICE_CONFIGURATION_REFRESH_GRACE_PERIOD                              (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Termination timeout
#define SIGNALING_CLIENT_SHUTDOWN_TIMEOUT                                   (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Forward declaration
typedef struct __LwsCallInfo *PLwsCallInfo;

/**
 * Thread execution tracker
 */
typedef struct {
    ATOMIC_BOOL terminated;
    TID threadId;
    MUTEX lock;
    CVAR await;
} ThreadTracker, *PThreadTracker;

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

    // Stored Client info
    SignalingClientInfo clientInfo;

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

    // LWS context to use
    struct lws_context* pLwsContext;

    // Signaling protocols
    struct lws_protocols signalingProtocols[3];

    // List of the ongoing messages
    PStackQueue pMessageQueue;

    // Message queue lock
    MUTEX messageQueueLock;

    // LWS needs to be locked
    MUTEX lwsServiceLock;

    // Timer queue to handle stale ICE configuration
    TIMER_QUEUE_HANDLE timerQueueHandle;
} SignalingClient, *PSignalingClient;

// Public handle to and from object converters
#define TO_SIGNALING_CLIENT_HANDLE(p) ((SIGNALING_CLIENT_HANDLE) (p))
#define FROM_SIGNALING_CLIENT_HANDLE(h) (IS_VALID_SIGNALING_CLIENT_HANDLE(h) ? (PSignalingClient) (h) : NULL)

STATUS createSignalingSync(PSignalingClientInfo, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSignalingClient*);
STATUS freeSignaling(PSignalingClient*);

STATUS signalingSendMessageSync(PSignalingClient, PSignalingMessage);
STATUS signalingGetIceConfigInfoCout(PSignalingClient, PUINT32);
STATUS signalingGetIceConfigInfo(PSignalingClient, UINT32, PIceConfigInfo*);
STATUS signalingConnectSync(PSignalingClient);

STATUS validateSignalingCallbacks(PSignalingClient, PSignalingClientCallbacks);
STATUS validateSignalingClientInfo(PSignalingClient, PSignalingClientInfo);
STATUS validateIceConfiguration(PSignalingClient);

STATUS signalingStoreOngoingMessage(PSignalingClient, PSignalingMessage);
STATUS signalingRemoveOngoingMessage(PSignalingClient, PCHAR);
STATUS signalingGetOngoingMessage(PSignalingClient, PCHAR, PSignalingMessage*);

STATUS refreshIceConfigurationCallback(UINT32, UINT64, UINT64);

STATUS awaitForThreadTermination(PThreadTracker, UINT64);
STATUS initializeThreadTracker(PThreadTracker);
STATUS uninitializeThreadTracker(PThreadTracker);

#pragma pack(pop, include_i)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_SIGNALING_CLIENT__ */
