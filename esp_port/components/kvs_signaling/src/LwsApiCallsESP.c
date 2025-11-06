/**
 * Implementation of a API calls based on LibWebSocket
 */
#define LOG_CLASS "LwsApiCallsESP"
// #include "../Include_i.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
// #include "Signaling/Signaling.h"
#include "SignalingESP.h"
#include "Signaling/LwsApiCalls.h"
#include "Signaling/ChannelInfo.h"
#include "../../kvs_utils/src/request_info.h"

// ESP-specific includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_work_queue.h"

#include "DataBuffer.h"

#define TAG "LWS_API_ESP"

// Define constants needed for ESP implementation
#define WS_TASK_STACK_SIZE  (6 * 1024)
#define WS_BUFFER_SIZE      (4 * 1024)

// HTTP-specific constants
#define HTTP_RESPONSE_MAX_BUFFER_SIZE (4 * 1024) // Buffer to hold complete response
#define HTTP_RESPONSE_BUFFER_SIZE (1 * 1024) // Internal response buffer for http client
#define HTTP_REQUEST_BUFFER_SIZE (4 * 1024) // Request buffer for http client
#define HTTP_REQUEST_TIMEOUT_MS   5000 // 5 seconds

// Default connection timeout increased for ESP (since connections can take longer)
#define ESP_SIGNALING_DEFAULT_CONNECT_TIMEOUT (10 * HUNDREDS_OF_NANOS_IN_A_SECOND) // 10 seconds

// Additional status codes
// Create our own status code definitions since these don't exist in the original code
#define STATUS_SIGNALING_CLIENT_NOT_CONNECTED STATUS_SIGNALING_BASE + 0x00000040
#define STATUS_SIGNALING_SEND_MESSAGE_FAILED STATUS_SIGNALING_LWS_CALL_FAILED

// Templates needed for message formatting - copied from LwsApiCalls.h
#define SIGNALING_ICE_SERVER_LIST_TEMPLATE_START \
    ",\n" \
    "\t\"IceServerList\": ["

#define SIGNALING_ICE_SERVER_LIST_TEMPLATE_END "\n\t]"

#define SIGNALING_ICE_SERVER_TEMPLATE \
    "\n" \
    "\t\t{\n" \
    "\t\t\t\"Password\": \"%s\",\n" \
    "\t\t\t\"Ttl\": %" PRIu64 ",\n" \
    "\t\t\t\"Uris\": [%s],\n" \
    "\t\t\t\"Username\": \"%s\"\n" \
    "\t\t},"

// Send message JSON template
#define SIGNALING_SEND_MESSAGE_TEMPLATE \
    "{\n" \
    "\t\"action\": \"%s\",\n" \
    "\t\"RecipientClientId\": \"%.*s\",\n" \
    "\t\"MessagePayload\": \"%s\"%s\n" \
    "}"

// Send message JSON template with correlation id
#define SIGNALING_SEND_MESSAGE_TEMPLATE_WITH_CORRELATION_ID \
    "{\n" \
    "\t\"action\": \"%s\",\n" \
    "\t\"RecipientClientId\": \"%.*s\",\n" \
    "\t\"MessagePayload\": \"%s\",\n" \
    "\t\"CorrelationId\": \"%.*s\"%s\n" \
    "}"

// Structure to hold ESP WebSocket client context
typedef struct __EspSignalingClientWrapper {
    PSignalingClient signalingClient;
    esp_websocket_client_handle_t wsClient;
    MUTEX wsClientLock;
    BOOL isConnected;
    BOOL connectionAwaitingConfirmation; // Track pending connections
} EspSignalingClientWrapper, *PEspSignalingClientWrapper;

// Global ESP signaling client wrapper
static PEspSignalingClientWrapper gEspSignalingClientWrapper = NULL;

// Data buffer for reassembling WebSocket chunks
static PDataBuffer gWebSocketDataBuffer = NULL;

// Global flag to track if we've initialized the CA store
static BOOL gCaStoreInitialized = FALSE;

// Global time variable
static UINT64 gServerTime = 0;

// Initialize the global CA store for secure connections
STATUS initializeGlobalCaStore()
{
    STATUS retStatus = STATUS_SUCCESS;

    // Only initialize once
    if (!gCaStoreInitialized) {
        ESP_LOGI(TAG, "Initializing CA certificate store");

        // We will supply the Amazon Root CA directly in the config
        // instead of attaching the global CA store
        gCaStoreInitialized = TRUE;
    }

    return retStatus;
}

// Function declarations for ESP-specific implementations
STATUS describeChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS createChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS getChannelEndpointEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS getIceConfigEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS joinStorageSessionEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS describeMediaStorageConfEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS deleteChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS connectSignalingChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS terminateLwsListenerLoop(PSignalingClient pSignalingClient);
STATUS initializeGlobalCaStore();

// WebSocket buffer handling function declarations
STATUS initWebSocketBuffer(UINT32 suggestedSize);
STATUS freeWebSocketBuffer();
STATUS resetWebSocketBuffer();
STATUS expandWebSocketBuffer(UINT32 additionalSize);
STATUS appendToWebSocketBuffer(PCHAR pData, UINT32 dataLen, BOOL isFinal);

// Forward function declaration for HTTP API calls
STATUS performEspHttpRequest(PSignalingClient pSignalingClient, PCHAR url,
                           esp_http_client_method_t method, PCHAR body,
                           PCHAR* pResponseData, PUINT32 pResponseLen);

// Forward function declaration for received message handling
STATUS handleReceivedSignalingMessage(PSignalingClient pSignalingClient, PCHAR message, UINT32 messageLength);

// Structure to hold WebSocket message data for processing
typedef struct {
    PCHAR message;
    UINT32 messageLen;
    PSignalingClient pSignalingClient;
} WebSocketMessageData, *PWebSocketMessageData;

// Function to process a WebSocket message
void processWebSocketMessage(void *priv_data)
{
    STATUS status = STATUS_SUCCESS;
    PWebSocketMessageData pMsgData = (PWebSocketMessageData) priv_data;

    if (pMsgData == NULL) {
        ESP_LOGE(TAG, "NULL message data in work queue task");
        return;
    }

    // Process the message
    status = handleReceivedSignalingMessage(pMsgData->pSignalingClient,
                                            pMsgData->message,
                                            pMsgData->messageLen);

    if (STATUS_FAILED(status)) {
        ESP_LOGE(TAG, "Failed to handle WebSocket message with status 0x%08" PRIx32, status);
    }

    // Free message data
    SAFE_MEMFREE(pMsgData->message);
    SAFE_MEMFREE(pMsgData);
}

typedef struct {
    PCHAR responseData;
    PUINT32 pResponseLen;
    UINT32 currentLen;
    UINT32 maxLen;
    PSignalingClient pSignalingClient;
} HttpResponseContext;

/* UTC tm-to-epoch converter (adapted from ESP-RainMaker rmaker_timegm) */
static inline int is_leap_year(int y) {
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

static time_t utc_tm_to_epoch(struct tm *tm)
{
    int year  = tm->tm_year + 1900;
    int month = tm->tm_mon;

    /* Normalize month */
    if (month < 0) {
        year += (month - 11) / 12;
        month = 12 + (month % 12);
    } else if (month > 11) {
        year += month / 12;
        month %= 12;
    }

    static const int mdays_cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};

    int64_t days = 0;
    if (year >= 1970) {
        for (int y = 1970; y < year; ++y) {
            days += 365 + is_leap_year(y);
        }
    } else {
        for (int y = year; y < 1970; ++y) {
            days -= 365 + is_leap_year(y);
        }
    }

    days += mdays_cum[month];
    if (month > 1 && is_leap_year(year)) {
        days += 1;
    }
    days += (tm->tm_mday - 1);

    int64_t seconds = days * 86400LL
                    + (int64_t)tm->tm_hour * 3600
                    + (int64_t)tm->tm_min  * 60
                    + (int64_t)tm->tm_sec;

    return (time_t)seconds;
}

// Function to check for clock skew from the given time, updates the clock skew in the hash table if detected
STATUS checkAndStoreClockSkew(PSignalingClient pSignalingClient, PCHAR dateHeaderValue)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 serverTime = 0;
    UINT64 clockSkew = 0;
    PHashTable pClockSkewMap = NULL;
    PStateMachineState pStateMachineState = NULL;

    CHK(dateHeaderValue != NULL && pSignalingClient != NULL, STATUS_NULL_ARG);

    // Get the current time
    UINT64 nowTime = GETTIME();

    // Parse the date header to get server time using strptime
    // Format: "Fri, 27 Jun 2025 12:27:54 GMT"
    struct tm tm = {0};
    if (strptime(dateHeaderValue, "%a, %d %b %Y %H:%M:%S GMT", &tm) != NULL) {
        /* CRITICAL: Use utc_tm_to_epoch() NOT mktime()!
         * mktime() interprets tm as LOCAL time and applies timezone offset.
         * Since server sends GMT/UTC time, we must interpret it as UTC.
         */
        time_t serverEpochTime = utc_tm_to_epoch(&tm);
        if (serverEpochTime != -1) {
            // Convert to 100ns units (UINT64)
            serverTime = ((UINT64)serverEpochTime) * HUNDREDS_OF_NANOS_IN_A_SECOND;

            // Define time skew threshold (3 minutes in 100ns units)
            #define TIME_SKEW_THRESHOLD (3 * 60 * HUNDREDS_OF_NANOS_IN_A_SECOND)

            // Calculate absolute difference
            UINT64 timeDiff;
            if (serverTime > nowTime) {
                timeDiff = serverTime - nowTime;

                // Check if beyond threshold
                if (timeDiff > TIME_SKEW_THRESHOLD) {
                    // Server time is ahead
                    clockSkew = timeDiff;
                    ESP_LOGW(TAG, "Detected Clock Skew! Server time is AHEAD of Device time by %" PRIu64 " seconds",
                            timeDiff / HUNDREDS_OF_NANOS_IN_A_SECOND);
                }
            } else {
                timeDiff = nowTime - serverTime;

                // Check if beyond threshold
                if (timeDiff > TIME_SKEW_THRESHOLD) {
                    // Device time is ahead - set high bit to indicate this
                    clockSkew = timeDiff;
                    clockSkew |= ((UINT64) (1ULL << 63));
                    ESP_LOGW(TAG, "Detected Clock Skew! Device time is AHEAD of Server time by %" PRIu64 " seconds",
                            timeDiff / HUNDREDS_OF_NANOS_IN_A_SECOND);
                }
            }

            // Store the clockSkew in the hash table if we detected skew
            if (clockSkew != 0 && pSignalingClient->pStateMachine != NULL &&
                pSignalingClient->diagnostics.pEndpointToClockSkewHashMap != NULL) {

                CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));
                pClockSkewMap = pSignalingClient->diagnostics.pEndpointToClockSkewHashMap;

                // Update the global time
                gServerTime = serverTime;

                // Store the skew in the hash table
                CHK_STATUS(hashTablePut(pClockSkewMap, pStateMachineState->state, clockSkew));
                ESP_LOGI(TAG, "Stored clock skew in hash table for state 0x%" PRIx64, pStateMachineState->state);
            } else {
                ESP_LOGD(TAG, "Time skew is within threshold, not fixing");
            }
        }
    } else {
        ESP_LOGW(TAG, "Failed to parse date header: %s", dateHeaderValue ? dateHeaderValue : "NULL");
    }

CleanUp:
    return retStatus;
}

// ESP WebSocket event handler
static void esp_websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    PEspSignalingClientWrapper pEspWrapper = (PEspSignalingClientWrapper)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    PSignalingClient pSignalingClient = NULL;

    // Safety check for NULL wrapper
    if (pEspWrapper == NULL) {
        ESP_LOGE(TAG, "NULL ESP wrapper in event handler!");
        return;
    }

    // Get the signaling client reference directly from the wrapper
    pSignalingClient = pEspWrapper->signalingClient;

    // Extra safety check for signaling client
    if (pSignalingClient == NULL) {
        ESP_LOGE(TAG, "NULL signaling client in event handler!");
        return;
    }

    ESP_LOGD(TAG, "WebSocket event: %d, signalingClient: %p", (int)event_id, pSignalingClient);

    /* Handle special opcodes up-front to prevent crashes when non-JSON frames
     * (like close frames) are processed as signaling messages */
    if (event_id == WEBSOCKET_EVENT_DATA && data != NULL) {
        if (data->data_ptr == NULL || data->data_len == 0) {
            ESP_LOGI(TAG, "WebSocket empty frame received, op_code: 0x%x, fin: %d",
                    data->op_code, data->fin);
        } else {
            // For frames with data, log the content
            DLOGD("WebSocket data preview: %.*s, op_code: 0x%x, fin: %d",
                   data->data_len > 100 ? 100 : data->data_len, data->data_ptr, data->op_code, data->fin);
        }
        switch (data->op_code) {
            case WS_TRANSPORT_OPCODES_CONT:
                /* Continuation frame */
                break;
            case WS_TRANSPORT_OPCODES_CLOSE: // Close frame
                ESP_LOGI(TAG, "Received WebSocket Close frame! Data: %.*s", data->data_len, data->data_ptr);

                // Check for AWS KVS "Going away" message in close frame
                if (data->data_len >= 5 && data->data_ptr != NULL &&
                    (strstr(data->data_ptr, "Going away") != NULL ||
                     strstr(data->data_ptr, "going away") != NULL ||
                     strstr(data->data_ptr, "GO_AWAY") != NULL ||
                     strstr(data->data_ptr, "Go away") != NULL ||
                     strstr(data->data_ptr, "go_away") != NULL)) {
                    ESP_LOGI(TAG, "Detected AWS KVS 'Going away' message in close frame");
                    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_SIGNALING_GO_AWAY);
                }

                /* No need to process this as a message - the connection will close */
                return;
            case WS_TRANSPORT_OPCODES_FIN: // FIN frame
                ESP_LOGI(TAG, "Received WebSocket FIN frame! Data: %.*s", data->data_len, data->data_ptr);
                /* No need to process this as a message - the connection will close */
                return;
            case WS_TRANSPORT_OPCODES_PING: // Ping frame
                ESP_LOGD(TAG, "Received WebSocket Ping frame! Data: %.*s", data->data_len, data->data_ptr);
                /* The ESP WebSocket client should automatically respond with a pong */
                return;
            case WS_TRANSPORT_OPCODES_PONG: // Pong frame
                ESP_LOGD(TAG, "Received WebSocket Pong frame! Data: %.*s", data->data_len, data->data_ptr);
                return;
            default:
                /* Only process text (0x1) or binary (0x2) frames */
                if (data->op_code != WS_TRANSPORT_OPCODES_TEXT && data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
                    ESP_LOGW(TAG, "Ignoring WebSocket frame with op_code: %d", data->op_code);
                    return;
                }
                break;
        }
    }

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");

            // Update connection state in both wrapper and signaling client
            MUTEX_LOCK(pEspWrapper->wsClientLock);
            pEspWrapper->isConnected = TRUE;
            pEspWrapper->connectionAwaitingConfirmation = FALSE;
            MUTEX_UNLOCK(pEspWrapper->wsClientLock);

            // Update signaling client state
            ATOMIC_STORE_BOOL(&pSignalingClient->connected, TRUE);
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

            // Signal the condition variable to wake up the waiting thread
            MUTEX_LOCK(pSignalingClient->connectedLock);
            CVAR_SIGNAL(pSignalingClient->connectedCvar);
            MUTEX_UNLOCK(pSignalingClient->connectedLock);

            ESP_LOGD(TAG, "WebSocket connected successfully");
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");

            BOOL connected = FALSE;

            MUTEX_LOCK(pEspWrapper->wsClientLock);
            connected = pEspWrapper->isConnected;
            pEspWrapper->isConnected = FALSE;
            MUTEX_UNLOCK(pEspWrapper->wsClientLock);

            // Now DISCONNECT is the primary event for handling connection cleanup and reconnection
            BOOL wasConnected = connected;
            ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);

            // Signal condition variables to wake up waiting threads
            MUTEX_LOCK(pSignalingClient->connectedLock);
            CVAR_SIGNAL(pSignalingClient->connectedCvar);
            MUTEX_UNLOCK(pSignalingClient->connectedLock);

            CVAR_BROADCAST(pSignalingClient->receiveCvar);
            CVAR_BROADCAST(pSignalingClient->sendCvar);
            ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_RESULT_OK);

            // Handle reconnection logic
            if (wasConnected && !ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
                // Check if this is a GOAWAY or ICE reconnect scenario
                SERVICE_CALL_RESULT currentResult = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result);

                if (currentResult == SERVICE_CALL_RESULT_SIGNALING_GO_AWAY) {
                    ESP_LOGI(TAG, "WebSocket closed due to GO_AWAY message, handling reconnection");

                    // Use the existing error callback mechanism to trigger reconnection
                    if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                        CHAR errorMsg[] = "WebSocket connection closed due to GO_AWAY message, reconnecting";
                        pSignalingClient->signalingClientCallbacks.errorReportFn(
                            pSignalingClient->signalingClientCallbacks.customData,
                            STATUS_SIGNALING_RECONNECT_FAILED,
                            errorMsg,
                            (UINT32) STRLEN(errorMsg));
                        ESP_LOGI(TAG, "Signaled for reconnection after GO_AWAY via error callback");
                    } else {
                        ESP_LOGW(TAG, "No error callback registered, cannot signal for reconnection after GO_AWAY");
                    }
                } else if (currentResult == SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE) {
                    ESP_LOGI(TAG, "WebSocket disconnected during ICE reconnect, not triggering full reconnection");
                } else {
                    ESP_LOGI(TAG, "WebSocket disconnected from active connection, triggering reconnection via error callback");

                    // Set the result failed to indicate disconnection
                    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);

                    // Use the existing error callback mechanism to trigger reconnection
                    if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                        CHAR errorMsg[] = "WebSocket connection lost, triggering reconnection";
                        ESP_LOGI(TAG, "DEBUG: Calling errorReportFn with STATUS_SIGNALING_RECONNECT_FAILED=0x%08x", STATUS_SIGNALING_RECONNECT_FAILED);
                        pSignalingClient->signalingClientCallbacks.errorReportFn(
                            pSignalingClient->signalingClientCallbacks.customData,
                            STATUS_SIGNALING_RECONNECT_FAILED,
                            errorMsg,
                            (UINT32) STRLEN(errorMsg));
                        ESP_LOGI(TAG, "Signaled for reconnection via error callback");
                    } else {
                        ESP_LOGW(TAG, "No error callback registered, cannot signal for reconnection");
                    }
                }
            } else if (!wasConnected) {
                ESP_LOGI(TAG, "WebSocket disconnected but was not previously connected, not triggering reconnection");
            } else if (ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
                ESP_LOGI(TAG, "WebSocket disconnected during shutdown, not triggering reconnection");
            }

            // Free the WebSocket buffer on disconnect
            freeWebSocketBuffer();
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data != NULL) {
                if (data->data_ptr == NULL || data->data_len == 0) {
                    // Empty PONG frames might come with op_code=0x1
                    // So we'll identify them based on being empty rather than by opcode
                    ESP_LOGI(TAG, "Received empty WebSocket frame, likely a control frame (PING/PONG), op_code: 0x%x, fin: %d",
                            data->op_code, data->fin);
                    break;
                }

                ESP_LOGD(TAG, "Received WebSocket data: %.*s", data->data_len, data->data_ptr);

                // Process the message now using our buffer append function
                STATUS bufferStatus = appendToWebSocketBuffer(data->data_ptr, data->data_len, data->fin);

                // If buffer is complete, process it using work queue
                if (bufferStatus == STATUS_DATA_BUFFER_COMPLETE) {
                    // Allocate message data structure
                    PWebSocketMessageData pMsgData = (PWebSocketMessageData) MEMALLOC(SIZEOF(WebSocketMessageData));
                    if (pMsgData == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for message data");
                        freeWebSocketBuffer();
                        break;
                    }

                    // Create a copy of the message
                    pMsgData->message = gWebSocketDataBuffer->buffer;
                    pMsgData->messageLen = gWebSocketDataBuffer->currentSize;
                    pMsgData->pSignalingClient = pSignalingClient;

                    // Give up ownership of buffer
                    gWebSocketDataBuffer->buffer = NULL;
                    freeWebSocketBuffer();

                    // Schedule message processing in the work queue
                    esp_err_t err = esp_work_queue_add_task(processWebSocketMessage, pMsgData);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to add task to work queue: %s", esp_err_to_name(err));
                        SAFE_MEMFREE(pMsgData->message);
                        SAFE_MEMFREE(pMsgData);
                    }
                } else if (STATUS_FAILED(bufferStatus)) {
                    ESP_LOGE(TAG, "Error processing WebSocket data with status 0x%08" PRIx32, bufferStatus);
                    resetWebSocketBuffer();
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");

            // Extract error details if available
            if (data != NULL) {
                ESP_LOGE(TAG, "WebSocket error details: op_code=%d, data_len=%d, fin=%d",
                         data->op_code, data->data_len, data->fin);

                if (data->data_ptr != NULL && data->data_len > 0) {
                    ESP_LOGE(TAG, "WebSocket error message: %.*s",
                             data->data_len > 100 ? 100 : data->data_len,  // Limit to 100 chars
                             data->data_ptr);
                }
            }

            if (pSignalingClient != NULL) {
                /*
                 * Set connection failure immediately to unblock waiting threads.
                 * This prevents the 10-second timeout delay when WebSocket connections fail.
                 * By immediately setting the result and signaling
                 * the condition variable, we enable immediate failure detection.
                 */

                // Mark as not connected
                ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);

                // Set error result to indicate connection failure
                ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT);

                // Signal waiting threads immediately
                MUTEX_LOCK(pSignalingClient->connectedLock);
                CVAR_BROADCAST(pSignalingClient->connectedCvar);
                MUTEX_UNLOCK(pSignalingClient->connectedLock);

                // Update wrapper state (lockless to avoid hanging in error handler)
                if (pEspWrapper != NULL) {
                    pEspWrapper->isConnected = FALSE;
                    pEspWrapper->connectionAwaitingConfirmation = FALSE;
                }

                CVAR_BROADCAST(pSignalingClient->receiveCvar);
                CVAR_BROADCAST(pSignalingClient->sendCvar);

                // Use the error callback to trigger reconnection
                if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                    CHAR errorMsg[] = "WebSocket connection error, triggering reconnection";
                    pSignalingClient->signalingClientCallbacks.errorReportFn(
                        pSignalingClient->signalingClientCallbacks.customData,
                        STATUS_SIGNALING_LWS_CALL_FAILED,
                        errorMsg,
                        (UINT32) STRLEN(errorMsg));
                    ESP_LOGI(TAG, "Signaled for reconnection via error callback");
                } else {
                    ESP_LOGW(TAG, "No error callback registered, cannot signal for reconnection");
                }
            } else {
                ESP_LOGW(TAG, "pSignalingClient is NULL in WEBSOCKET_EVENT_ERROR!");
            }
            break;

        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED - WebSocket connection closed");

            if (pSignalingClient != NULL) {
                ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);

                // Trigger reconnection via error callback
                if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                    CHAR errorMsg[] = "WebSocket connection closed, triggering reconnection";
                    pSignalingClient->signalingClientCallbacks.errorReportFn(
                        pSignalingClient->signalingClientCallbacks.customData,
                        STATUS_SIGNALING_RECONNECT_FAILED,
                        errorMsg,
                        (UINT32) STRLEN(errorMsg));
                    ESP_LOGI(TAG, "Signaled for reconnection via error callback");
                } else {
                    ESP_LOGW(TAG, "No error callback registered for close event");
                }
            }
            break;

        case WEBSOCKET_EVENT_FINISH:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH - WebSocket operation finished");
            // Usually comes after WEBSOCKET_EVENT_CLOSED, no additional action needed
            break;

        default:
            break;
    }
}

STATUS getMessageTypeFromString(PCHAR typeStr, UINT32 typeLen, SIGNALING_MESSAGE_TYPE* pMessageType)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 len;

    CHK(typeStr != NULL && pMessageType != NULL, STATUS_NULL_ARG);

    if (typeLen == 0) {
        len = (UINT32) STRLEN(typeStr);
    } else {
        len = typeLen;
    }

    if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_OFFER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_OFFER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_ANSWER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_ICE_CANDIDATE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    } else if (0 == STRNCMP(typeStr, SIGNALING_GO_AWAY, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_GO_AWAY;
    } else if (0 == STRNCMP(typeStr, SIGNALING_RECONNECT_ICE_SERVER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_STATUS_RESPONSE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE;
    } else {
        *pMessageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;
        CHK_WARN(FALSE, retStatus, "Unrecognized message type received");
    }

CleanUp:

    LEAVES();
    return retStatus;
}

PCHAR getMessageTypeInString(SIGNALING_MESSAGE_TYPE messageType)
{
    switch (messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            return SIGNALING_SDP_TYPE_OFFER;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            return SIGNALING_SDP_TYPE_ANSWER;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            return SIGNALING_ICE_CANDIDATE;
        case SIGNALING_MESSAGE_TYPE_GO_AWAY:
            return SIGNALING_GO_AWAY;
        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            return SIGNALING_RECONNECT_ICE_SERVER;
        case SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE:
            return SIGNALING_STATUS_RESPONSE;
        case SIGNALING_MESSAGE_TYPE_UNKNOWN:
            return SIGNALING_MESSAGE_UNKNOWN;
    }
    return SIGNALING_MESSAGE_UNKNOWN;
}

STATUS handleReceivedSignalingMessage(PSignalingClient pSignalingClient, PCHAR message, UINT32 messageLength)
{
    STATUS retStatus = STATUS_SUCCESS;
    ReceivedSignalingMessage receivedSignalingMessage;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;
    UINT32 i, strLen;
    BOOL parsedStatusResponse = FALSE, jsonInIceServerList = FALSE;
    CHAR messageTypeStr[MAX_SIGNALING_MESSAGE_TYPE_LEN + 1] = {0};
    UINT32 outLen;
    UINT32 decodedLen = 0;
    PBYTE pDecodedData = NULL;
    jsmntok_t* pToken;
    INT32 j;
    UINT64 ttl;

    CHK(pSignalingClient != NULL && message != NULL, STATUS_NULL_ARG);

    // Initialize the received signaling message
    MEMSET(&receivedSignalingMessage, 0, SIZEOF(ReceivedSignalingMessage));
    receivedSignalingMessage.signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;

    // ESP_LOGI(TAG, "Parsing received signaling message");

    // Parse the incoming JSON message
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, message, messageLength, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));

    // Verify we have at least the outer JSON object
    CHK(tokenCount >= 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    // Extract message fields
    for (i = 1; i < tokenCount; i++) {
        // Look for either "messageType" or "action" field as the message type
        if (compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "messageType") ||
            compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "action")) {

            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_MESSAGE_TYPE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

            // First copy the string so we can log it
            STRNCPY(messageTypeStr, message + tokens[i + 1].start, strLen);
            messageTypeStr[strLen] = '\0';

            // Now use getMessageTypeFromString to parse it
            CHK_STATUS(getMessageTypeFromString(messageTypeStr, strLen,
                                               &receivedSignalingMessage.signalingMessage.messageType));
            i++;
        } else if (compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "messagePayload")) {
            // Extract message payload
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            if (strLen > 0) {
                // First find out the size of the decoded data
                CHK_STATUS(base64Decode(message + tokens[i + 1].start, strLen, NULL, &decodedLen));
                // ESP_LOGI(TAG, "Base64 decoded length will be %u bytes", decodedLen);

#ifdef DYNAMIC_SIGNALING_PAYLOAD
                // Allocate a buffer to store the decoded data
                pDecodedData = (PBYTE) MEMALLOC(decodedLen + 1);
                CHK(pDecodedData != NULL, STATUS_NOT_ENOUGH_MEMORY);
                receivedSignalingMessage.signalingMessage.payload = pDecodedData;
#else
                pDecodedData = receivedSignalingMessage.signalingMessage.payload;
#endif
                // Now decode the payload
                outLen = decodedLen;
                CHK_STATUS(base64Decode(message + tokens[i + 1].start, strLen, pDecodedData, &outLen));

                // Null terminate
                pDecodedData[outLen] = '\0';

                // Store the actual payload length
                receivedSignalingMessage.signalingMessage.payloadLen = outLen;

                // ESP_LOGI(TAG, "Message payload decoded successfully, length: %d", outLen);
            }
            i++;
        } else if (compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "senderClientId")) {
            // Extract sender client ID
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            if (strLen <= MAX_SIGNALING_CLIENT_ID_LEN) {
                STRNCPY(receivedSignalingMessage.signalingMessage.peerClientId, message + tokens[i + 1].start, strLen);
                receivedSignalingMessage.signalingMessage.peerClientId[strLen] = '\0';
            }
            i++;
        } else if (!parsedStatusResponse && compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "statusResponse")) {
            parsedStatusResponse = TRUE;
            i++;
        } else if (parsedStatusResponse && compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "correlationId")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_CORRELATION_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(receivedSignalingMessage.signalingMessage.correlationId, message + tokens[i + 1].start, strLen);
            receivedSignalingMessage.signalingMessage.correlationId[MAX_CORRELATION_ID_LEN] = '\0';
            i++;
        } else if (parsedStatusResponse && compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "errorType")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_ERROR_TYPE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(receivedSignalingMessage.errorType, message + tokens[i + 1].start, strLen);
            receivedSignalingMessage.errorType[MAX_ERROR_TYPE_STRING_LEN] = '\0';
            i++;
        } else if (parsedStatusResponse && compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "statusCode")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_STATUS_CODE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

            // Parse the status code
            CHK_STATUS(STRTOUI32(message + tokens[i + 1].start, message + tokens[i + 1].end, 10,
                               &receivedSignalingMessage.statusCode));
            i++;
        } else if (parsedStatusResponse && compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "description")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_MESSAGE_DESCRIPTION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(receivedSignalingMessage.description, message + tokens[i + 1].start, strLen);
            receivedSignalingMessage.description[MAX_MESSAGE_DESCRIPTION_LEN] = '\0';
            i++;
        } else if (!jsonInIceServerList &&
                 receivedSignalingMessage.signalingMessage.messageType == SIGNALING_MESSAGE_TYPE_OFFER &&
                 compareJsonString(message, &tokens[i], JSMN_STRING, (PCHAR) "IceServerList")) {
            jsonInIceServerList = TRUE;

            CHK(tokens[i + 1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
            CHK(tokens[i + 1].size <= MAX_ICE_CONFIG_COUNT, STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT);

            // Zero the ice configs
            MEMSET(&pSignalingClient->iceConfigs, 0x00, MAX_ICE_CONFIG_COUNT * SIZEOF(IceConfigInfo));
            pSignalingClient->iceConfigCount = 0;
            i++;
        } else if (jsonInIceServerList) {
            // Handle ICE server configuration parsing similar to LwsApiCalls.c
            pToken = &tokens[i];
            if (pToken->type == JSMN_OBJECT) {
                pSignalingClient->iceConfigCount++;
            } else if (compareJsonString(message, pToken, JSMN_STRING, (PCHAR) "Username")) {
                strLen = (UINT32)(pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_USER_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].userName, message + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].userName[MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(message, pToken, JSMN_STRING, (PCHAR) "Password")) {
                strLen = (UINT32)(pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].password, message + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].userName[MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';
                i++;
            } else if (compareJsonString(message, pToken, JSMN_STRING, (PCHAR) "Ttl")) {
                CHK_STATUS(STRTOUI64(message + pToken[1].start, message + pToken[1].end, 10, &ttl));

                // NOTE: Ttl value is in seconds
                pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].ttl = ttl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                i++;
            } else if (compareJsonString(message, pToken, JSMN_STRING, (PCHAR) "Uris")) {
                // Expect an array of elements
                CHK(pToken[1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                CHK(pToken[1].size <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);
                for (j = 0; j < pToken[1].size; j++) {
                    strLen = (UINT32)(pToken[j + 2].end - pToken[j + 2].start);
                    CHK(strLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_SIGNALING_MAX_ICE_URI_LEN);
                    STRNCPY(pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].uris[j], message + pToken[j + 2].start, strLen);
                    pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].uris[j][MAX_ICE_CONFIG_URI_LEN] = '\0';
                    pSignalingClient->iceConfigs[pSignalingClient->iceConfigCount - 1].uriCount++;
                }

                i += pToken[1].size + 1;
            }
        }
    }

    // If message type is UNKNOWN after parsing, try to determine it from payload
    if (receivedSignalingMessage.signalingMessage.messageType == SIGNALING_MESSAGE_TYPE_UNKNOWN) {
        // Check for RECONNECT_ICE_SERVER message - this doesn't need payload
        if (STRSTR(message, "RECONNECT_ICE_SERVER") != NULL) {
            ESP_LOGI(TAG, "Detected RECONNECT_ICE_SERVER message, setting message type accordingly");
            receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER;
        }
        // For other message types, check payload if available
        else if (receivedSignalingMessage.signalingMessage.payloadLen > 0) {
            // Look for ICE candidate in the payload
            if (STRSTR(receivedSignalingMessage.signalingMessage.payload, "candidate") != NULL) {
                ESP_LOGI(TAG, "Detected ICE candidate in payload, setting message type accordingly");
                receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            }
            // Check for SDP offer in the payload
            else if (STRSTR(receivedSignalingMessage.signalingMessage.payload, "offer") != NULL) {
                ESP_LOGI(TAG, "Detected SDP offer in payload, setting message type accordingly");
                receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
            }
            // Check for SDP answer in the payload
            else if (STRSTR(receivedSignalingMessage.signalingMessage.payload, "answer") != NULL) {
                ESP_LOGI(TAG, "Detected SDP answer in payload, setting message type accordingly");
                receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
            }
        }
    }

    // Handle special message types
    switch (receivedSignalingMessage.signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            ESP_LOGI(TAG, "Received RECONNECT_ICE_SERVER message, setting status for ICE reconnection");
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE);
            break;

        case SIGNALING_MESSAGE_TYPE_OFFER:
            // Update offer received timestamp for timing diagnostics
            MUTEX_LOCK(pSignalingClient->offerSendReceiveTimeLock);
            pSignalingClient->offerReceivedTime = GETTIME();
            MUTEX_UNLOCK(pSignalingClient->offerSendReceiveTimeLock);
            break;

        default:
            // No special handling for other message types
            break;
    }

    // Update diagnostics
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfMessagesReceived);

    // Call the message received callback if registered
    // IMPORTANT: We call this with no locks held to avoid deadlocks
    // Note: Even for GOAWAY and RECONNECT_ICE_SERVER, we still call the callback
    // so the application can be aware of these events
    if (pSignalingClient->signalingClientCallbacks.messageReceivedFn != NULL) {
        pSignalingClient->signalingClientCallbacks.messageReceivedFn(
            pSignalingClient->signalingClientCallbacks.customData,
            &receivedSignalingMessage);
    } else {
        ESP_LOGW(TAG, "No message received callback registered!");
    }

CleanUp:

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    SAFE_MEMFREE(pDecodedData);
#endif

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to parse signaling message with status 0x%08" PRIx32, retStatus);
    }

    return retStatus;
}

STATUS terminateEspSignalingClient(PSignalingClient pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    esp_websocket_client_handle_t wsClientToDestroy = NULL;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    if (gEspSignalingClientWrapper != NULL) {
        MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
        locked = TRUE;

        if (gEspSignalingClientWrapper->wsClient != NULL) {
            ESP_LOGI(TAG, "Terminating WebSocket client - stopping first");

            /* Store reference and clear it immediately to prevent other operations */
            wsClientToDestroy = gEspSignalingClientWrapper->wsClient;
            gEspSignalingClientWrapper->wsClient = NULL;

            /* Update connection state immediately */
            ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
            gEspSignalingClientWrapper->isConnected = FALSE;

            /* Release the lock before potentially blocking operations */
            MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
            locked = FALSE;

            /* Now perform the potentially blocking operations without holding the lock */
            ESP_LOGI(TAG, "Attempting graceful WebSocket client stop");
            esp_websocket_client_stop(wsClientToDestroy);

            /* Wait a bit for the stop operation to take effect */
            vTaskDelay(pdMS_TO_TICKS(200));

            esp_websocket_client_destroy(wsClientToDestroy);
            ESP_LOGI(TAG, "WebSocket client termination completed");
        } else {
            /* No client to destroy, just update state */
            ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
            gEspSignalingClientWrapper->isConnected = FALSE;
        }
    }

CleanUp:
    if (locked && gEspSignalingClientWrapper != NULL) {
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
    }

    return retStatus;
}

// Add checkAndCorrectForClockSkew function implementation (similar to the one in LwsApiCalls.c)
STATUS checkAndCorrectForClockSkew(PSignalingClient pSignalingClient, PRequestInfo pRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    PStateMachineState pStateMachineState;
    PHashTable pClockSkewMap;
    UINT64 clockSkewOffset;

    CHK(pSignalingClient != NULL && pRequestInfo != NULL, STATUS_NULL_ARG);

    // Skip if we don't have a state machine or hash table
    if (pSignalingClient->pStateMachine == NULL ||
        pSignalingClient->diagnostics.pEndpointToClockSkewHashMap == NULL) {
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));

    pClockSkewMap = pSignalingClient->diagnostics.pEndpointToClockSkewHashMap;

    // Check if we have clockSkew for this endpoint
    if (STATUS_SUCCEEDED(hashTableGet(pClockSkewMap, pStateMachineState->state, &clockSkewOffset))) {
        // If we made it here that means there is clock skew
        if (clockSkewOffset & ((UINT64) (1ULL << 63))) {
            clockSkewOffset ^= ((UINT64) (1ULL << 63));
            ESP_LOGD(TAG, "Detected device time is AHEAD of server time!");
            pRequestInfo->currentTime -= clockSkewOffset;
        } else {
            ESP_LOGD(TAG, "Detected server time is AHEAD of device time!");
            pRequestInfo->currentTime += clockSkewOffset;
        }

        ESP_LOGW(TAG, "Clockskew corrected!");
    }

CleanUp:
    LEAVES();
    return retStatus;
}

// Modify connectEspSignalingClient to call the function
STATUS connectEspSignalingClient(PSignalingClient pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    esp_websocket_client_config_t ws_cfg = {0};
    PRequestInfo pRequestInfo = NULL;
    PCHAR url = NULL;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Log ESP-IDF version for debugging
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());

    // Initialize the global CA store
    CHK_STATUS(initializeGlobalCaStore());

    // Free any existing client
    terminateEspSignalingClient(pSignalingClient);

    // Initialize the wrapper if not already done
    if (gEspSignalingClientWrapper == NULL) {
        gEspSignalingClientWrapper = (PEspSignalingClientWrapper) MEMCALLOC(1, SIZEOF(EspSignalingClientWrapper));
        CHK(gEspSignalingClientWrapper != NULL, STATUS_NOT_ENOUGH_MEMORY);

        // Create mutex properly
        gEspSignalingClientWrapper->wsClientLock = MUTEX_CREATE(TRUE);
        CHK(IS_VALID_MUTEX_VALUE(gEspSignalingClientWrapper->wsClientLock), STATUS_INVALID_OPERATION);

        gEspSignalingClientWrapper->signalingClient = pSignalingClient;
        gEspSignalingClientWrapper->isConnected = FALSE;
        gEspSignalingClientWrapper->connectionAwaitingConfirmation = FALSE;
    } else {
        // Always update the signaling client reference to ensure we're using the current one
        ESP_LOGI(TAG, "Updating signaling client reference in wrapper");

        MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
        gEspSignalingClientWrapper->signalingClient = pSignalingClient;
        gEspSignalingClientWrapper->isConnected = FALSE;
        gEspSignalingClientWrapper->connectionAwaitingConfirmation = FALSE;
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
    }

    // Set the initial connection state to ensure proper synchronization across all threads
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
    locked = TRUE;

    // Construct the proper WebSocket URL with the necessary parameters
    // This is crucial - the URL construction must match what's in LwsApiCalls.c
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        UINT32 urlLen = SNPRINTF(NULL, 0, SIGNALING_ENDPOINT_VIEWER_URL_WSS_TEMPLATE,
                pSignalingClient->channelEndpointWss,
                SIGNALING_CHANNEL_ARN_PARAM_NAME,
                pSignalingClient->channelDescription.channelArn,
                SIGNALING_CLIENT_ID_PARAM_NAME,
                pSignalingClient->clientInfo.signalingClientInfo.clientId);
        url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
        CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(url, urlLen + 1, SIGNALING_ENDPOINT_VIEWER_URL_WSS_TEMPLATE,
                pSignalingClient->channelEndpointWss,
                SIGNALING_CHANNEL_ARN_PARAM_NAME,
                pSignalingClient->channelDescription.channelArn,
                SIGNALING_CLIENT_ID_PARAM_NAME,
                pSignalingClient->clientInfo.signalingClientInfo.clientId);
    } else {
        UINT32 urlLen = SNPRINTF(NULL, 0, SIGNALING_ENDPOINT_MASTER_URL_WSS_TEMPLATE,
                pSignalingClient->channelEndpointWss,
                SIGNALING_CHANNEL_ARN_PARAM_NAME,
                pSignalingClient->channelDescription.channelArn);
        url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
        CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(url, urlLen + 1, SIGNALING_ENDPOINT_MASTER_URL_WSS_TEMPLATE,
                pSignalingClient->channelEndpointWss,
                SIGNALING_CHANNEL_ARN_PARAM_NAME,
                pSignalingClient->channelDescription.channelArn);
    }

    ESP_LOGD(TAG, "WebSocket URL (before signing): %s", url);

    // Create RequestInfo for SigV4 signing with the properly formatted URL
    CHK_STATUS(createRequestInfo(
        url,
        NULL, // No body for WebSocket connections
        pSignalingClient->pChannelInfo->pRegion,
        NULL, // certPath
        NULL, // sslCertPath
        NULL, // sslPrivateKeyPath
        SSL_CERTIFICATE_TYPE_NOT_SPECIFIED,
        "KVS-ESP32", // userAgent
        HTTP_REQUEST_TIMEOUT_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, // connectionTimeout
        HTTP_REQUEST_TIMEOUT_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, // completionTimeout
        0, // lowSpeedLimit
        0, // lowSpeedTimeLimit
        pSignalingClient->pAwsCredentials,
        &pRequestInfo));

    // If getCurrentTime callback is provided, use it to set the current time
    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    // Check and correct for clock skew before signing
    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    if (gServerTime != 0) {
        // Force the timestamp to the server time
        pRequestInfo->currentTime = gServerTime;

        // Reset the global time
        gServerTime = 0;
    }

    // Set the HTTP method for WebSocket - GET
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    // Remove headers that will be added back by LWS/ESP WebSocket client
    CHK_STATUS(removeRequestHeader(pRequestInfo, (PCHAR) "user-agent"));

    // IMPORTANT: Use signAwsRequestInfoQueryParam for WebSocket instead of signAwsRequestInfo
    STATUS signStatus = signAwsRequestInfoQueryParam(pRequestInfo);
    if (STATUS_FAILED(signStatus)) {
        ESP_LOGE(TAG, "signAwsRequestInfoQueryParam failed with status: 0x%08" PRIx32, signStatus);
        CHK(FALSE, signStatus);
    }
    CHK_STATUS(removeRequestHeaders(pRequestInfo));

    ESP_LOGD(TAG, "Using signed WebSocket URL");
    ESP_LOGD(TAG, "url after signing: %s", pRequestInfo->url);

    // Add more verbose logging for troubleshooting
    ESP_LOGD(TAG, "Full request details - Verb: %d, Host: %s, Path: %s",
             pRequestInfo->verb,
             pRequestInfo->url,
             pRequestInfo->body ? pRequestInfo->body : "(none)");

    // Use the signed URL for WebSocket connection
    ws_cfg.uri = pRequestInfo->url;
    ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    // Configure other WebSocket parameters
    ws_cfg.task_stack = WS_TASK_STACK_SIZE;
    ws_cfg.task_prio = 5;
    ws_cfg.buffer_size = WS_BUFFER_SIZE;

    // Set ping interval to match SIGNALING_SERVICE_WSS_PING_PONG_INTERVAL_IN_SECONDS (10 seconds)
    ws_cfg.ping_interval_sec = 10;

    // Disable auto reconnect to let the state machine handle reconnection with fresh credentials
    // Auto-reconnect would fail after 5 minutes when the signed URL expires
    ws_cfg.disable_auto_reconnect = TRUE;

    // Set reconnect timeout to match ESP-IDF recommendations
    ws_cfg.reconnect_timeout_ms = 10000; // 10 seconds

    // Configure TCP keepalive to match SIGNALING_SERVICE_TCP_KEEPALIVE settings
    ws_cfg.network_timeout_ms = SIGNALING_SERVICE_TCP_KEEPALIVE_IN_SECONDS * 1000;

    // Create WebSocket client
    gEspSignalingClientWrapper->wsClient = esp_websocket_client_init(&ws_cfg);
    CHK(gEspSignalingClientWrapper->wsClient != NULL, STATUS_INTERNAL_ERROR);

    // Register event handler
    esp_websocket_register_events(gEspSignalingClientWrapper->wsClient,
                                  WEBSOCKET_EVENT_ANY,
                                  esp_websocket_event_handler,
                                  gEspSignalingClientWrapper);

    // Set initial state before starting the client
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Mark that we're waiting for a connection
    MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
    gEspSignalingClientWrapper->connectionAwaitingConfirmation = TRUE;
    MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);

    // Start WebSocket client
    CHK(esp_websocket_client_start(gEspSignalingClientWrapper->wsClient) == ESP_OK, STATUS_INTERNAL_ERROR);
    ESP_LOGD(TAG, "WebSocket client started");

    // Wait for WebSocket connection with timeout
    UINT64 timeout = (pSignalingClient->clientInfo.connectTimeout != 0) ?
                     pSignalingClient->clientInfo.connectTimeout :
                     ESP_SIGNALING_DEFAULT_CONNECT_TIMEOUT;

    UINT64 startTime = GETTIME();
    BOOL connected = FALSE;
    SERVICE_CALL_RESULT callResult = SERVICE_CALL_RESULT_NOT_SET;

    ESP_LOGD(TAG, "Waiting for WebSocket connection with timeout %" PRIu64 " ns", timeout);

    // Wait for the connection event or timeout
    while (TRUE) {
        // Direct check of ESP WebSocket connection state - this is the most reliable indicator
        BOOL espClientConnected = FALSE;

        // Safely check WebSocket client state
        MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
        if (gEspSignalingClientWrapper->wsClient != NULL) {
            espClientConnected = esp_websocket_client_is_connected(gEspSignalingClientWrapper->wsClient);
        }
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);

        // Check the signaling client connected flag from atomic store
        connected = ATOMIC_LOAD_BOOL(&pSignalingClient->connected);
        callResult = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result);

        ESP_LOGD(TAG, "Connection wait loop - connected: %s, esp_client.connected: %s, callResult: %d",
                 connected ? "TRUE" : "FALSE",
                 espClientConnected ? "TRUE" : "FALSE",
                 callResult);

        // If any indicator shows we're connected, proceed
        if (connected || espClientConnected) {
            // Synchronize connection state if needed
            if (!connected && espClientConnected) {
                ESP_LOGD(TAG, "Syncing connection status from ESP client to signaling client");
                ATOMIC_STORE_BOOL(&pSignalingClient->connected, TRUE);
                connected = TRUE;

                if (callResult == SERVICE_CALL_RESULT_NOT_SET) {
                    ESP_LOGD(TAG, "Setting result to SERVICE_CALL_RESULT_OK");
                    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
                }
            }

            ESP_LOGD(TAG, "WebSocket connected successfully");
            break;
        }

        // Check for error conditions that should cause immediate exit
        if (callResult != SERVICE_CALL_RESULT_NOT_SET && callResult != SERVICE_CALL_RESULT_OK) {
            ESP_LOGW(TAG, "WebSocket connection failed with call result: %d, exiting wait loop", callResult);
            retStatus = STATUS_SIGNALING_LWS_CALL_FAILED;
            break;
        }

        // Additional check: If we've been trying to connect for a while and the client is NULL,
        // that usually means the connection failed and was cleaned up
        MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
        BOOL clientExists = (gEspSignalingClientWrapper->wsClient != NULL);
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);

        if (!clientExists && (GETTIME() - startTime > 5 * HUNDREDS_OF_NANOS_IN_A_SECOND)) {
            ESP_LOGW(TAG, "WebSocket client is NULL after connection attempt - treating as failure");
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT);
            retStatus = STATUS_SIGNALING_LWS_CALL_FAILED;
            break;
        }

        // Check for timeout
        if (GETTIME() - startTime > timeout) {
            ESP_LOGW(TAG, "WebSocket connection timeout after %" PRIu64 " ns", GETTIME() - startTime);
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT);
            retStatus = STATUS_OPERATION_TIMED_OUT;
            break;
        }

        MUTEX_LOCK(pSignalingClient->connectedLock);
        // Wait for a short time using the condition variable
        CVAR_WAIT(pSignalingClient->connectedCvar,
                 pSignalingClient->connectedLock,
                 500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // 500ms polling interval
        MUTEX_UNLOCK(pSignalingClient->connectedLock);
    }

    // Update connection status in the signaling client
    if (connected) {
        ESP_LOGD(TAG, "WebSocket connection confirmed as successful");
        // Store the time when we connect for diagnostics
        MUTEX_LOCK(pSignalingClient->diagnosticsLock);
        pSignalingClient->diagnostics.connectTime = GETTIME();
        MUTEX_UNLOCK(pSignalingClient->diagnosticsLock);
    } else {
        ESP_LOGE(TAG, "WebSocket connection failed with status: 0x%08" PRIx32, retStatus);
        // CHK(FALSE, retStatus == STATUS_SUCCESS ? STATUS_SIGNALING_CONNECT_FAILED : retStatus);
    }


CleanUp:
    if (locked && gEspSignalingClientWrapper != NULL) {
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
    }

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    if (STATUS_FAILED(retStatus) && gEspSignalingClientWrapper != NULL) {
        terminateEspSignalingClient(pSignalingClient);
    }

    SAFE_MEMFREE(url);

    return retStatus;
}

STATUS connectSignalingChannelEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Initialize WebSocket buffer before connecting
    CHK_STATUS(initWebSocketBuffer(0));

    // Call the ESP-specific connection function with SigV4 signing
    retStatus = connectEspSignalingClient(pSignalingClient);

    // The result will be set inside connectEspSignalingClient

CleanUp:
    return retStatus;
}

STATUS sendEspWebSocketMessage(PSignalingClient pSignalingClient, PCHAR pMessage, UINT32 msgLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    int result;

    CHK(pSignalingClient != NULL && pMessage != NULL, STATUS_NULL_ARG);
    CHK(gEspSignalingClientWrapper != NULL && gEspSignalingClientWrapper->wsClient != NULL, STATUS_INTERNAL_ERROR);

    // Short critical section to check connection state
    MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
    locked = TRUE;
    BOOL isConnected = gEspSignalingClientWrapper->isConnected;
    MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
    locked = FALSE;

    // Check connection
    if (!isConnected) {
        // Double-check with ESP client's own state
        if (!esp_websocket_client_is_connected(gEspSignalingClientWrapper->wsClient)) {
            ESP_LOGE(TAG, "WebSocket is not connected, cannot send message");
            CHK_STATUS(STATUS_SIGNALING_CLIENT_NOT_CONNECTED);
        } else {
            // ESP client says connected but wrapper doesn't - update wrapper state
            ESP_LOGW(TAG, "Connection state mismatch, updating wrapper state");
            MUTEX_LOCK(gEspSignalingClientWrapper->wsClientLock);
            gEspSignalingClientWrapper->isConnected = TRUE;
            MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
        }
    }

    // Log message details for debugging
    // ESP_LOGI(TAG, "Sending WebSocket message, length: %d", msgLen);
    // ESP_LOGI(TAG, "Message content (first 50 bytes): %.50s%s",
    //          pMessage, msgLen > 50 ? "..." : "");

    // Send the message using ESP WebSocket client (no locks needed during actual send)
    result = esp_websocket_client_send_text(gEspSignalingClientWrapper->wsClient, pMessage, msgLen, portMAX_DELAY);
    if (result < 0) {
        ESP_LOGE(TAG, "Failed to send WebSocket message, error: %d", result);
        CHK_STATUS(STATUS_SIGNALING_SEND_MESSAGE_FAILED);
    } else {
        ESP_LOGD(TAG, "Successfully sent %d bytes", result);
    }

CleanUp:
    if (locked && gEspSignalingClientWrapper != NULL) {
        MUTEX_UNLOCK(gEspSignalingClientWrapper->wsClientLock);
    }

    return retStatus;
}

// Implement the API calls that replace the LWS ones in Signaling.c
STATUS describeChannelEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PCHAR url = NULL;
    PCHAR paramsJson = NULL;
    PCHAR pResponseStr = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 i, strLen, resultLen;
    UINT32 tokenCount;
    UINT64 messageTtl;
    BOOL jsonInChannelDescription = FALSE, jsonInMvConfiguration = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    UINT32 urlLen = STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(DESCRIBE_SIGNALING_CHANNEL_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);

    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, DESCRIBE_SIGNALING_CHANNEL_API_POSTFIX);

    // Prepare the json params for the call
    UINT32 paramsJsonLen = SNPRINTF(NULL, 0, DESCRIBE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName);
    CHK(paramsJsonLen > 0, STATUS_INTERNAL_ERROR);
    paramsJson = (PCHAR) MEMALLOC(paramsJsonLen + 1); // +1 for the NULL terminator
    CHK(paramsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
    SNPRINTF(paramsJson, paramsJsonLen + 1, DESCRIBE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     paramsJson, &pResponseStr, &resultLen);

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) retStatus);

    // Early return if we have a non-success result
    CHK(STATUS_SUCCEEDED(retStatus) && resultLen != 0 && pResponseStr != NULL, STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(&pSignalingClient->channelDescription, 0x00, SIZEOF(SignalingChannelDescription));

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (!jsonInChannelDescription) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelInfo")) {
                pSignalingClient->channelDescription.version = SIGNALING_CHANNEL_DESCRIPTION_CURRENT_VERSION;
                jsonInChannelDescription = TRUE;
                i++;
            }
        } else {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelARN")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelName")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_CHANNEL_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelName, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelName[MAX_CHANNEL_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Version")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_UPDATE_VERSION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.updateVersion, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.updateVersion[MAX_UPDATE_VERSION_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelStatus")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_DESCRIBE_CHANNEL_STATUS_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                pSignalingClient->channelDescription.channelStatus = getChannelStatusFromString(pResponseStr + tokens[i + 1].start, strLen);
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelType")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_DESCRIBE_CHANNEL_TYPE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                pSignalingClient->channelDescription.channelType = getChannelTypeFromString(pResponseStr + tokens[i + 1].start, strLen);
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "CreationTime")) {
                // Skip creation time as we don't need it currently
                i++;
            } else {
                if (!jsonInMvConfiguration) {
                    if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "SingleMasterConfiguration")) {
                        jsonInMvConfiguration = TRUE;
                        i++;
                    }
                } else {
                    if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "MessageTtlSeconds")) {
                        CHK_STATUS(STRTOUI64(pResponseStr + tokens[i + 1].start, pResponseStr + tokens[i + 1].end, 10, &messageTtl));

                        // NOTE: Ttl value is in seconds
                        pSignalingClient->channelDescription.messageTtl = messageTtl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                        i++;
                    }
                }
            }
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelStatus != SIGNALING_CHANNEL_STATUS_DELETING, STATUS_SIGNALING_CHANNEL_BEING_DELETED);

    // Store successful result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status: 0x%08x", retStatus);
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    SAFE_MEMFREE(pResponseStr);
    SAFE_MEMFREE(paramsJson);
    SAFE_MEMFREE(url);

    LEAVES();
    return retStatus;
}

STATUS createChannelEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PCHAR url = NULL;
    PCHAR paramsJson = NULL;
    PCHAR tagsJson = NULL;
    PCHAR tagsContent = NULL;
    PCHAR pCurPtr = NULL, pResponseStr = NULL;
    UINT32 i, strLen, resultLen, tagsJsonLen = 0, tagsContentLen = 0;
    INT32 charsCopied;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Create the API url
    UINT32 urlLen = STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(CREATE_SIGNALING_CHANNEL_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, CREATE_SIGNALING_CHANNEL_API_POSTFIX);

    // Process tags if any
    if (pSignalingClient->pChannelInfo->tagCount != 0) {
        // First calculate required size for tags content
        for (i = 0; i < pSignalingClient->pChannelInfo->tagCount; i++) {
            tagsContentLen += SNPRINTF(NULL, 0, TAG_PARAM_JSON_OBJ_TEMPLATE,
                            pSignalingClient->pChannelInfo->pTags[i].name,
                            pSignalingClient->pChannelInfo->pTags[i].value);
        }

        // Allocate memory for tags content
        tagsContent = (PCHAR) MEMALLOC(tagsContentLen + 1); // +1 for null terminator
        CHK(tagsContent != NULL, STATUS_NOT_ENOUGH_MEMORY);

        // Fill tags content
        pCurPtr = tagsContent;
        for (i = 0; i < pSignalingClient->pChannelInfo->tagCount; i++) {
            charsCopied = SNPRINTF(pCurPtr, tagsContentLen + 1 - (pCurPtr - tagsContent),
                                  TAG_PARAM_JSON_OBJ_TEMPLATE,
                                  pSignalingClient->pChannelInfo->pTags[i].name,
                                  pSignalingClient->pChannelInfo->pTags[i].value);
            CHK(charsCopied > 0 && charsCopied <= tagsContentLen + 1 - (pCurPtr - tagsContent), STATUS_INTERNAL_ERROR);
            pCurPtr += charsCopied;
        }

        // Remove the trailing comma
        if (pCurPtr > tagsContent) {
            *(pCurPtr - 1) = '\0';
        }

        // Calculate size for full tags JSON
        tagsJsonLen = SNPRINTF(NULL, 0, TAGS_PARAM_JSON_TEMPLATE, tagsContent);

        // Allocate memory for full tags JSON
        tagsJson = (PCHAR) MEMALLOC(tagsJsonLen + 1); // +1 for null terminator
        CHK(tagsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);

        // Format the full tags JSON
        SNPRINTF(tagsJson, tagsJsonLen + 1, TAGS_PARAM_JSON_TEMPLATE, tagsContent);
    } else {
        // No tags, use empty string
        tagsJson = (PCHAR) MEMALLOC(1);
        CHK(tagsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
        tagsJson[0] = '\0';
    }

    // Prepare the json params for the call
    UINT32 paramsJsonLen = SNPRINTF(NULL, 0, CREATE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName,
             getStringFromChannelType(pSignalingClient->pChannelInfo->channelType),
             pSignalingClient->pChannelInfo->messageTtl / HUNDREDS_OF_NANOS_IN_A_SECOND, tagsJson);

    paramsJson = (PCHAR) MEMALLOC(paramsJsonLen + 1); // +1 for the NULL terminator
    CHK(paramsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
    SNPRINTF(paramsJson, paramsJsonLen + 1, CREATE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName,
             getStringFromChannelType(pSignalingClient->pChannelInfo->channelType),
             pSignalingClient->pChannelInfo->messageTtl / HUNDREDS_OF_NANOS_IN_A_SECOND, tagsJson);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     paramsJson, &pResponseStr, &resultLen);

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) retStatus);

    // Early return if we have a non-success result
    CHK(STATUS_SUCCEEDED(retStatus) && resultLen != 0 && pResponseStr != NULL, STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse out the ARN
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelARN")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
            pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
            i++;
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_SIGNALING_NO_ARN_RETURNED_ON_CREATE);

    // Store successful result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status: 0x%08x", retStatus);
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    SAFE_MEMFREE(pResponseStr);
    SAFE_MEMFREE(paramsJson);
    SAFE_MEMFREE(url);
    SAFE_MEMFREE(tagsJson);
    SAFE_MEMFREE(tagsContent);

    LEAVES();
    return retStatus;
}

STATUS getChannelEndpointEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PCHAR url = NULL;
    PCHAR paramsJson = NULL;
    UINT32 i, resultLen, strLen, protocolLen = 0, endpointLen = 0;
    PCHAR pResponseStr = NULL, pProtocol = NULL, pEndpoint = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;
    BOOL jsonInResourceEndpointList = FALSE, protocol = FALSE, endpoint = FALSE, inEndpointArray = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Create the API url
    UINT32 urlLen = STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX);

    // Prepare the json params for the call
    if (pSignalingClient->mediaStorageConfig.storageStatus == FALSE) {
        UINT32 paramsJsonLen = SNPRINTF(NULL, 0, GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
        paramsJson = (PCHAR) MEMALLOC(paramsJsonLen + 1); // +1 for the NULL terminator
        CHK(paramsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(paramsJson, paramsJsonLen + 1, GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
    } else {
        UINT32 paramsJsonLen = SNPRINTF(NULL, 0, GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL_W_MEDIA_STORAGE, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
        paramsJson = (PCHAR) MEMALLOC(paramsJsonLen + 1); // +1 for the NULL terminator
        CHK(paramsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(paramsJson, paramsJsonLen + 1, GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL_W_MEDIA_STORAGE, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
    }

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     paramsJson, &pResponseStr, &resultLen);

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) retStatus);

    // Early return if we have a non-success result
    CHK(STATUS_SUCCEEDED(retStatus) && resultLen != 0 && pResponseStr != NULL, STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse and extract the endpoints
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    pSignalingClient->channelEndpointWss[0] = '\0';
    pSignalingClient->channelEndpointHttps[0] = '\0';
    pSignalingClient->channelEndpointWebrtc[0] = '\0';

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (!jsonInResourceEndpointList) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ResourceEndpointList")) {
                jsonInResourceEndpointList = TRUE;
                i++;
            }
        } else {
            if (!inEndpointArray && tokens[i].type == JSMN_ARRAY) {
                inEndpointArray = TRUE;
            } else {
                if (tokens[i].type == JSMN_OBJECT) {
                    // Process if both are set
                    if (protocol && endpoint) {
                        if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointWss, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        } else if (0 == STRNCMPI(pProtocol, HTTPS_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointHttps, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        } else if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointWebrtc, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointWebrtc[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        }
                    }

                    protocol = FALSE;
                    endpoint = FALSE;
                    protocolLen = 0;
                    endpointLen = 0;
                    pProtocol = NULL;
                    pEndpoint = NULL;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Protocol")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    pProtocol = pResponseStr + tokens[i + 1].start;
                    protocolLen = strLen;
                    protocol = TRUE;
                    i++;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ResourceEndpoint")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_SIGNALING_ENDPOINT_URI_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    pEndpoint = pResponseStr + tokens[i + 1].start;
                    endpointLen = strLen;
                    endpoint = TRUE;
                    i++;
                }
            }
        }
    }

    // Check if we have unprocessed protocol
    if (protocol && endpoint) {
        if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointWss, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        } else if (0 == STRNCMPI(pProtocol, HTTPS_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointHttps, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        } else if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointWebrtc, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointWebrtc[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0' && pSignalingClient->channelEndpointWss[0] != '\0',
        STATUS_SIGNALING_MISSING_ENDPOINTS_IN_GET_ENDPOINT);

    // Store successful result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status: 0x%08x", retStatus);
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    SAFE_MEMFREE(pResponseStr);
    SAFE_MEMFREE(paramsJson);
    SAFE_MEMFREE(url);

    LEAVES();
    return retStatus;
}

STATUS getIceConfigEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PCHAR url = NULL;
    PCHAR paramsJson = NULL;
    PCHAR pResponseStr = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmntok_t* pToken;
    UINT32 i, strLen, resultLen, configCount = 0, tokenCount;
    INT32 j;
    UINT64 ttl;
    BOOL jsonInIceServerList = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0', STATUS_INTERNAL_ERROR);

    // Update the diagnostics info on the number of ICE refresh calls
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.iceRefreshCount);

    // Create the API url
    UINT32 urlLen = STRLEN(pSignalingClient->channelEndpointHttps) + STRLEN(GET_ICE_CONFIG_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    STRCPY(url, pSignalingClient->channelEndpointHttps);
    STRCAT(url, GET_ICE_CONFIG_API_POSTFIX);

    // Prepare the json params for the call
    UINT32 paramsJsonLen = SNPRINTF(NULL, 0, GET_ICE_CONFIG_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->clientInfo.signalingClientInfo.clientId);
    paramsJson = (PCHAR) MEMALLOC(paramsJsonLen + 1); // +1 for the NULL terminator
    CHK(paramsJson != NULL, STATUS_NOT_ENOUGH_MEMORY);
    SNPRINTF(paramsJson, paramsJsonLen + 1, GET_ICE_CONFIG_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->clientInfo.signalingClientInfo.clientId);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     paramsJson, &pResponseStr, &resultLen);


    // It should be okay if this failed
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGW(TAG, "Failed to get ice config, proceeding anyway...");
        // Store successful result
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        retStatus = STATUS_SUCCESS;
        goto CleanUp;
    }

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) retStatus);

    // Early return if we have a non-success result
    CHK(STATUS_SUCCEEDED(retStatus) && resultLen != 0 && pResponseStr != NULL, STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(&pSignalingClient->iceConfigs, 0x00, MAX_ICE_CONFIG_COUNT * SIZEOF(IceConfigInfo));
    pSignalingClient->iceConfigCount = 0;

    // Loop through the tokens and extract the ice configuration
    for (i = 0; i < tokenCount; i++) {
        if (!jsonInIceServerList) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "IceServerList")) {
                jsonInIceServerList = TRUE;

                CHK(tokens[i + 1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                // We may not fit all the configs, but that't fine
                // CHK(tokens[i + 1].size <= MAX_ICE_CONFIG_COUNT, STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT);
            }
        } else {
            pToken = &tokens[i];
            if (pToken->type == JSMN_OBJECT) {
                if (configCount + 1 > MAX_ICE_CONFIG_COUNT) {
                    ESP_LOGW(TAG, "Max ice config count reached");
                    break;
                }
                configCount++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Username")) {
                strLen = (UINT32) (pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_USER_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[configCount - 1].userName, pResponseStr + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[configCount - 1].userName[MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Password")) {
                strLen = (UINT32) (pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[configCount - 1].password, pResponseStr + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[configCount - 1].userName[MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Ttl")) {
                CHK_STATUS(STRTOUI64(pResponseStr + pToken[1].start, pResponseStr + pToken[1].end, 10, &ttl));

                // NOTE: Ttl value is in seconds
                pSignalingClient->iceConfigs[configCount - 1].ttl = ttl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Uris")) {
                // Expect an array of elements
                CHK(pToken[1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                CHK(pToken[1].size <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);
                for (j = 0; j < pToken[1].size; j++) {
                    strLen = (UINT32) (pToken[j + 2].end - pToken[j + 2].start);
                    CHK(strLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_SIGNALING_MAX_ICE_URI_LEN);
                    STRNCPY(pSignalingClient->iceConfigs[configCount - 1].uris[j], pResponseStr + pToken[j + 2].start, strLen);
                    pSignalingClient->iceConfigs[configCount - 1].uris[j][MAX_ICE_CONFIG_URI_LEN] = '\0';
                    pSignalingClient->iceConfigs[configCount - 1].uriCount++;
                }

                i += pToken[1].size + 1;
            }
        }
    }

    // Perform some validation on the ice configuration
    pSignalingClient->iceConfigCount = configCount;
    CHK_STATUS(validateIceConfiguration(pSignalingClient));

    // Store successful result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status: 0x%08x", retStatus);
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    SAFE_MEMFREE(pResponseStr);
    SAFE_MEMFREE(paramsJson);
    SAFE_MEMFREE(url);

    LEAVES();
    return retStatus;
}

STATUS deleteChannelEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR url = NULL;
    CHAR body[1024] = {0};
    PCHAR responseData = NULL;
    UINT32 responseLen = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Create the URL
    UINT32 urlLen = STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(DELETE_SIGNALING_CHANNEL_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, DELETE_SIGNALING_CHANNEL_API_POSTFIX);

    // Create the request body
    SNPRINTF(body, ARRAY_SIZE(body),
            "{\n"
            "   \"ChannelARN\": \"%s\"\n"
            "}",
            pSignalingClient->pChannelInfo->pChannelArn);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     body, &responseData, &responseLen);

    if (STATUS_SUCCEEDED(retStatus)) {
        ESP_LOGI(TAG, "Delete channel successful");

        // For this example, we'll simulate a successful response
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
    } else {
        ESP_LOGE(TAG, "Failed to delete channel");
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    }

CleanUp:
    SAFE_MEMFREE(responseData);
    SAFE_MEMFREE(url);
    return retStatus;
}

STATUS joinStorageSessionEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR url = NULL;
    CHAR body[1024] = {0};
    PCHAR responseData = NULL;
    UINT32 responseLen = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Create the URL
    UINT32 urlLen = SNPRINTF(NULL, 0, "%s%s",
             pSignalingClient->pChannelInfo->pControlPlaneUrl,
             JOIN_STORAGE_SESSION_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    SNPRINTF(url, urlLen + 1, "%s%s",
             pSignalingClient->pChannelInfo->pControlPlaneUrl,
             JOIN_STORAGE_SESSION_API_POSTFIX);

    // Create the request body
    SNPRINTF(body, ARRAY_SIZE(body),
            "{\n"
            "   \"ChannelARN\": \"%s\"\n"
            "}",
            pSignalingClient->pChannelInfo->pChannelArn);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     body, &responseData, &responseLen);

    if (STATUS_SUCCEEDED(retStatus) && responseData != NULL) {
        ESP_LOGI(TAG, "Join storage session response: %s", responseData);

        // TODO: Parse JSON response and extract storage session information
        // For this example, we'll simulate a successful response
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
    } else {
        ESP_LOGE(TAG, "Failed to join storage session");
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    }

CleanUp:
    SAFE_MEMFREE(responseData);
    SAFE_MEMFREE(url);
    return retStatus;
}

STATUS describeMediaStorageConfEsp(PSignalingClient pSignalingClient, UINT64 time)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR url = NULL;
    CHAR body[1024] = {0};
    PCHAR responseData = NULL;
    UINT32 responseLen = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Create the URL
    UINT32 urlLen = SNPRINTF(NULL, 0, "%s%s",
             pSignalingClient->pChannelInfo->pControlPlaneUrl,
             DESCRIBE_MEDIA_STORAGE_CONF_API_POSTFIX);
    url = (PCHAR) MEMALLOC(urlLen + 1); // +1 for the NULL terminator
    CHK(url != NULL, STATUS_NOT_ENOUGH_MEMORY);
    SNPRINTF(url, urlLen + 1, "%s%s",
             pSignalingClient->pChannelInfo->pControlPlaneUrl,
             DESCRIBE_MEDIA_STORAGE_CONF_API_POSTFIX);

    // Create the request body
    SNPRINTF(body, ARRAY_SIZE(body),
            "{\n"
            "   \"ChannelARN\": \"%s\"\n"
            "}",
            pSignalingClient->pChannelInfo->pChannelArn);

    // Make the HTTP request
    retStatus = performEspHttpRequest(pSignalingClient, url, HTTP_METHOD_POST,
                                     body, &responseData, &responseLen);

    if (STATUS_SUCCEEDED(retStatus) && responseData != NULL) {
        ESP_LOGI(TAG, "Describe media storage config response: %s", responseData);

        // TODO: Parse JSON response and extract media storage configuration
        // For this example, we'll simulate a successful response
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
    } else {
        ESP_LOGE(TAG, "Failed to describe media storage configuration");
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    }

CleanUp:
    SAFE_MEMFREE(responseData);
    SAFE_MEMFREE(url);

    return retStatus;
}

// Simple JSON string extraction helper - does basic parsing without a full JSON parser
STATUS extractJsonString(PCHAR jsonData, PCHAR key, PCHAR value, UINT32 valueLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR keyStart, valueStart, valueEnd;
    CHAR searchKey[256];

    CHK(jsonData != NULL && key != NULL && value != NULL && valueLen > 0, STATUS_NULL_ARG);

    // Create search key with quotes and colon
    SNPRINTF(searchKey, ARRAY_SIZE(searchKey), "\"%s\"", key);

    keyStart = STRSTR(jsonData, searchKey);
    CHK(keyStart != NULL, STATUS_INVALID_ARG);

    // Find the value start after the key
    valueStart = STRCHR(keyStart + STRLEN(searchKey), '\"');
    CHK(valueStart != NULL, STATUS_INVALID_ARG);
    valueStart++; // Skip the opening quote

    // Find the value end
    valueEnd = STRCHR(valueStart, '\"');
    CHK(valueEnd != NULL, STATUS_INVALID_ARG);

    // Extract the value
    UINT32 extractLen = (UINT32) (valueEnd - valueStart);
    extractLen = MIN(extractLen, valueLen - 1);

    MEMCPY(value, valueStart, extractLen);
    value[extractLen] = '\0';

CleanUp:
    return retStatus;
}

// Extracts a number from a JSON string
STATUS extractJsonNumber(PCHAR jsonData, PCHAR key, PUINT64 value)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR keyStart, valueStart, valueEnd;
    CHAR searchKey[256];
    CHAR valueStr[256];

    CHK(jsonData != NULL && key != NULL && value != NULL, STATUS_NULL_ARG);

    // Create search key with quotes and colon
    SNPRINTF(searchKey, ARRAY_SIZE(searchKey), "\"%s\"", key);

    keyStart = STRSTR(jsonData, searchKey);
    CHK(keyStart != NULL, STATUS_INVALID_ARG);

    // Find the value start after the key and colon
    valueStart = STRCHR(keyStart + STRLEN(searchKey), ':');
    CHK(valueStart != NULL, STATUS_INVALID_ARG);
    valueStart++; // Skip the colon

    // Skip any whitespace
    while (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\n' || *valueStart == '\r') {
        valueStart++;
    }

    // Find the value end (delimiter can be comma, curly brace, or end of string)
    valueEnd = valueStart;
    while (*valueEnd != '\0' && *valueEnd != ',' && *valueEnd != '}' && *valueEnd != ']') {
        valueEnd++;
    }

    // Extract the value as a string
    UINT32 extractLen = (UINT32) (valueEnd - valueStart);
    extractLen = MIN(extractLen, ARRAY_SIZE(valueStr) - 1);

    MEMCPY(valueStr, valueStart, extractLen);
    valueStr[extractLen] = '\0';

    // Convert to number using STRTOUL instead of STRTOULL
    *value = (UINT64) STRTOUL(valueStr, NULL, 10);

CleanUp:
    return retStatus;
}

// HTTP Event Handler
static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    HttpResponseContext* ctx = (HttpResponseContext*)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            // Check if the header is Date
            if (strcasecmp(evt->header_key, "Date") == 0) {
                ESP_LOGI(TAG, "Date Header from server: %s", evt->header_value);
                if (STATUS_FAILED(checkAndStoreClockSkew(ctx->pSignalingClient, evt->header_value))) {
                    ESP_LOGE(TAG, "Failed to check and store clock skew");
                }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            if (ctx != NULL && ctx->responseData != NULL && ctx->pResponseLen != NULL) {
                // Check if we have enough space
                if (ctx->currentLen + evt->data_len < ctx->maxLen) {
                    // Copy data to buffer
                    MEMCPY(ctx->responseData + ctx->currentLen, evt->data, evt->data_len);
                    ctx->currentLen += evt->data_len;
                    ctx->responseData[ctx->currentLen] = '\0'; // Null terminate
                    *ctx->pResponseLen = ctx->currentLen;
                } else {
                    ESP_LOGE(TAG, "HTTP response buffer too small");
                    return ESP_FAIL;
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Helper function to perform HTTP requests
STATUS performEspHttpRequest(PSignalingClient pSignalingClient, PCHAR url,
                          esp_http_client_method_t method, PCHAR body,
                          PCHAR* pResponseData, PUINT32 pResponseLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t config = {0};
    HttpResponseContext context = {0};
    PCHAR response = NULL;
    UINT32 responseLen = 0;
    PRequestInfo pRequestInfo = NULL;
    PAwsCredentials pAwsCredentials = NULL;

    CHK(pSignalingClient != NULL && url != NULL, STATUS_NULL_ARG);

    // Initialize the global CA store
    CHK_STATUS(initializeGlobalCaStore());

    // Allocate response buffer
    response = (PCHAR) MEMALLOC(HTTP_RESPONSE_MAX_BUFFER_SIZE);
    CHK(response != NULL, STATUS_NOT_ENOUGH_MEMORY);
    response[0] = '\0';

    ESP_LOGD(TAG, "Sending HTTP request to %s", url);

    // Set up context
    context.responseData = response;
    context.pResponseLen = &responseLen;
    context.currentLen = 0;
    context.maxLen = HTTP_RESPONSE_MAX_BUFFER_SIZE - 1; // Leave space for null terminator
    context.pSignalingClient = pSignalingClient; // Set the signaling client pointer

    // Create a RequestInfo structure for SigV4 signing
    if (pSignalingClient->pAwsCredentials != NULL) {
        pAwsCredentials = pSignalingClient->pAwsCredentials;

        // Create RequestInfo for SigV4 signing
        CHK_STATUS(createRequestInfo(
            url,
            body,
            pSignalingClient->pChannelInfo->pRegion,
            NULL, // certPath
            NULL, // sslCertPath
            NULL, // sslPrivateKeyPath
            SSL_CERTIFICATE_TYPE_NOT_SPECIFIED,
            "KVS-ESP32", // userAgent
            HTTP_REQUEST_TIMEOUT_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, // connectionTimeout
            HTTP_REQUEST_TIMEOUT_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, // completionTimeout
            0, // lowSpeedLimit
            0, // lowSpeedTimeLimit
            pAwsCredentials,
            &pRequestInfo));

        // Set the HTTP method
        switch (method) {
            case HTTP_METHOD_GET:
                pRequestInfo->verb = HTTP_REQUEST_VERB_GET;
                break;
            case HTTP_METHOD_POST:
                pRequestInfo->verb = HTTP_REQUEST_VERB_POST;
                break;
            case HTTP_METHOD_PUT:
                pRequestInfo->verb = HTTP_REQUEST_VERB_PUT;
                break;
            // case HTTP_METHOD_DELETE:
            //     pRequestInfo->verb = HTTP_REQUEST_VERB_DELETE;
            //     break;
            default:
                pRequestInfo->verb = HTTP_REQUEST_VERB_POST;
                break;
        }

        // Check and correct for clock skew before signing
        checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

        // Sign the request using AWS SigV4
        CHK_STATUS(signAwsRequestInfo(pRequestInfo));
    }

    // Configure HTTP client
    config.url = url;
    config.method = method;
    config.event_handler = httpEventHandler;
    config.user_data = &context;
    config.timeout_ms = HTTP_REQUEST_TIMEOUT_MS;
    config.buffer_size = HTTP_RESPONSE_BUFFER_SIZE;
    config.buffer_size_tx = HTTP_REQUEST_BUFFER_SIZE;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    // Initialize HTTP client
    client = esp_http_client_init(&config);
    CHK(client != NULL, STATUS_INTERNAL_ERROR);

    // Set common headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    // Add AWS SigV4 headers if request was signed
    if (pRequestInfo != NULL) {
        PSingleListNode pCurNode = NULL;
        PRequestHeader pRequestHeader = NULL;
        UINT64 item;

        // Get the list of headers from signed request
        CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));

        // Add each header to the ESP HTTP client request
        while (pCurNode != NULL) {
            CHK_STATUS(singleListGetNodeData(pCurNode, &item));
            // Fix the unsafe cast by using a proper macro or cast method
            pRequestHeader = (PRequestHeader)(ULONG_PTR)item;

            esp_http_client_set_header(client, pRequestHeader->pName, pRequestHeader->pValue);

            CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
        }

        // Early cleanup of request info
        if (pRequestInfo != NULL) {
            freeRequestInfo(&pRequestInfo);
        }
    }

    // Set body if provided
    if (body != NULL) {
        esp_http_client_set_post_field(client, body, STRLEN(body));
    }

    // Perform request
    esp_err_t err = ESP_OK;
    const int retryCount = 10;
    for (int i = 0; i < retryCount; i++) {
        err = esp_http_client_perform(client);
        if (err == ESP_ERR_HTTP_EAGAIN) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue; // retry
        }
        if (err == ESP_OK) {
            break;
        }
        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed with error 0x%x: %s", err, esp_err_to_name(err));
        CHK(FALSE, STATUS_INTERNAL_ERROR);
    }

    // Check status code
    int statusCode = esp_http_client_get_status_code(client);
    if (statusCode < 200 || statusCode >= 300) {
        ESP_LOGE(TAG, "HTTP request failed with status code %d", statusCode);
        if (response != NULL && responseLen > 0) {
            ESP_LOGE(TAG, "Response data: %.*s", (int) responseLen, response);
        }
        CHK(FALSE, STATUS_INTERNAL_ERROR);
    }

    // Set response if requested
    if (pResponseData != NULL && pResponseLen != NULL) {
        *pResponseData = response;
        *pResponseLen = responseLen;
        response = NULL; // Ownership transferred, don't free
    }

CleanUp:
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    SAFE_MEMFREE(response);

    return retStatus;
}

// Initialize the WebSocket buffer with appropriate size
STATUS initWebSocketBuffer(UINT32 suggestedSize)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Free any existing buffer first to prevent memory leaks
    freeWebSocketBuffer();

    // Use our DataBuffer module to create a new buffer
    CHK_STATUS(initDataBuffer(&gWebSocketDataBuffer, suggestedSize));

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket buffer with status 0x%08" PRIx32, retStatus);
    }
    return retStatus;
}

// Free the WebSocket buffer
STATUS freeWebSocketBuffer()
{
    STATUS retStatus = STATUS_SUCCESS;

    // Free the data buffer if it exists
    if (gWebSocketDataBuffer != NULL) {
        retStatus = freeDataBuffer(&gWebSocketDataBuffer);
    }

    return retStatus;
}

// Reset the WebSocket buffer
STATUS resetWebSocketBuffer()
{
    STATUS retStatus = STATUS_SUCCESS;

    // If no buffer exists, initialize one
    if (gWebSocketDataBuffer == NULL) {
        ESP_LOGW(TAG, "WebSocket buffer was NULL during reset, initializing with default size");
        CHK_STATUS(initWebSocketBuffer(0));
    } else {
        // Reset the existing buffer
        CHK_STATUS(resetDataBuffer(gWebSocketDataBuffer));
        ESP_LOGD(TAG, "WebSocket buffer reset (max size: %" PRIu32 ")", gWebSocketDataBuffer->maxSize);
    }

CleanUp:
    return retStatus;
}

// Expand the WebSocket buffer if needed
STATUS expandWebSocketBuffer(UINT32 additionalSize)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Check if we have a valid buffer to expand
    CHK(gWebSocketDataBuffer != NULL, STATUS_NULL_ARG);

    // Use our DataBuffer module to expand the buffer
    CHK_STATUS(expandDataBuffer(gWebSocketDataBuffer, additionalSize));

    ESP_LOGI(TAG, "WebSocket buffer expanded to %" PRIu32 " bytes", gWebSocketDataBuffer->maxSize);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to expand WebSocket buffer with status 0x%08" PRIx32, retStatus);
    }
    return retStatus;
}

// Append data to the WebSocket buffer
STATUS appendToWebSocketBuffer(PCHAR pData, UINT32 dataLen, BOOL isFinal)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Check if we need to initialize the buffer
    if (gWebSocketDataBuffer == NULL) {
        // Initialize with a size that can hold at least this message
        UINT32 initialSize = MAX(DATA_BUFFER_DEFAULT_SIZE, dataLen * 2);
        if (isFinal) {
            // If this is the final message, we can use the exact size of the message
            initialSize = dataLen;
        }
        CHK_STATUS(initWebSocketBuffer(initialSize));
    }

    // Use our DataBuffer module to append data
    CHK_STATUS(retStatus = appendDataBuffer(gWebSocketDataBuffer, pData, dataLen, isFinal));

CleanUp:
    if (STATUS_FAILED(retStatus) && retStatus != STATUS_DATA_BUFFER_COMPLETE) {
        ESP_LOGE(TAG, "Failed to append to WebSocket buffer with status 0x%08" PRIx32, retStatus);
        resetWebSocketBuffer();
    }

    return retStatus;
}

// Implement the terminateLwsListenerLoop function
STATUS terminateLwsListenerLoop(PSignalingClient pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    // Nothing to do here

CleanUp:

    return retStatus;
}

STATUS sendEspSignalingMessage(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR encodedIceConfig[MAX_ENCODED_ICE_SERVER_INFOS_STR_LEN + 1];
    CHAR encodedUris[MAX_ICE_SERVER_URI_STR_LEN + 1];
    UINT32 writtenSize, urisLen, iceConfigLen = 0, messageLen, corrLen;
    UINT32 bufferOffset = 0, bufferSize = 0;
    PCHAR buffer = NULL;
    PCHAR pMessageType;
    UINT64 curTime;

    CHK(pSignalingClient != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK(pSignalingMessage->peerClientId != NULL, STATUS_NULL_ARG);

    // Validate the payload
#ifdef DYNAMIC_SIGNALING_PAYLOAD
    CHK(pSignalingMessage->payload != NULL, STATUS_SIGNALING_NO_PAYLOAD_IN_MESSAGE);
    messageLen = pSignalingMessage->payloadLen;
#else
    CHK(pSignalingMessage->payload[0] != '\0', STATUS_SIGNALING_NO_PAYLOAD_IN_MESSAGE);
    messageLen = (UINT32) STRLEN(pSignalingMessage->payload);
#endif

    // Get the correlation length
    corrLen = (UINT32) STRLEN(pSignalingMessage->correlationId);

    // Convert message type to string
    switch (pSignalingMessage->messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            pMessageType = (PCHAR) SIGNALING_SDP_TYPE_OFFER;
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            pMessageType = (PCHAR) SIGNALING_SDP_TYPE_ANSWER;
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            pMessageType = (PCHAR) SIGNALING_ICE_CANDIDATE;
            break;
        case SIGNALING_MESSAGE_TYPE_GO_AWAY:
            pMessageType = (PCHAR) SIGNALING_GO_AWAY;
            break;
        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            pMessageType = (PCHAR) SIGNALING_RECONNECT_ICE_SERVER;
            break;
        default:
            CHK(FALSE, STATUS_INVALID_ARG);
    }

    ESP_LOGD(TAG, "Sending signaling message: type=%s, recipient=%s", pMessageType, pSignalingMessage->peerClientId);

    // Start off with an empty string for ICE config
    encodedIceConfig[0] = '\0';

    // In case of an Offer, package the ICE candidates only if we have a set of non-expired ICE configs
    if (pSignalingMessage->messageType == SIGNALING_MESSAGE_TYPE_OFFER && pSignalingClient->iceConfigCount != 0 &&
        (curTime = SIGNALING_GET_CURRENT_TIME(pSignalingClient)) <= pSignalingClient->iceConfigExpiration &&
        STATUS_SUCCEEDED(validateIceConfiguration(pSignalingClient))) {
        // Start the ice infos by copying the preamble, then the main body and then the ending
        STRCPY(encodedIceConfig, SIGNALING_ICE_SERVER_LIST_TEMPLATE_START);
        iceConfigLen = ARRAY_SIZE(SIGNALING_ICE_SERVER_LIST_TEMPLATE_START) - 1; // remove the null terminator

        for (UINT32 iceCount = 0; iceCount < pSignalingClient->iceConfigCount; iceCount++) {
            encodedUris[0] = '\0';
            for (UINT32 uriCount = 0; uriCount < pSignalingClient->iceConfigs[iceCount].uriCount; uriCount++) {
                STRCAT(encodedUris, "\"");
                STRCAT(encodedUris, pSignalingClient->iceConfigs[iceCount].uris[uriCount]);
                STRCAT(encodedUris, "\",");
            }

            // remove the last comma
            urisLen = STRLEN(encodedUris);
            encodedUris[--urisLen] = '\0';

            // Construct the encoded ice config
            // NOTE: We need to subtract the passed time to get the TTL of the expiration correct
            writtenSize = (UINT32) SNPRINTF(encodedIceConfig + iceConfigLen, MAX_ICE_SERVER_INFO_STR_LEN, SIGNALING_ICE_SERVER_TEMPLATE,
                                        pSignalingClient->iceConfigs[iceCount].password,
                                        (pSignalingClient->iceConfigs[iceCount].ttl - (curTime - pSignalingClient->iceConfigTime)) /
                                            HUNDREDS_OF_NANOS_IN_A_SECOND,
                                        encodedUris, pSignalingClient->iceConfigs[iceCount].userName);
            CHK(writtenSize <= MAX_ICE_SERVER_INFO_STR_LEN, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);
            iceConfigLen += writtenSize;
        }

        // Get rid of the last comma
        iceConfigLen--;

        // Closing the JSON array
        STRCPY(encodedIceConfig + iceConfigLen, SIGNALING_ICE_SERVER_LIST_TEMPLATE_END);
    }

    // Calculate the size needed for the buffer
    UINT32 jsonStructureSize = STRLEN(SIGNALING_SEND_MESSAGE_TEMPLATE_WITH_CORRELATION_ID);

    // Add sizes for known parts
    jsonStructureSize += STRLEN(pMessageType); // action value
    jsonStructureSize += STRLEN(pSignalingMessage->peerClientId); // RecipientClientId value

    // Calculate base64 encoded payload size (4*ceil(n/3))
    UINT32 base64EncodedSize = ((messageLen + 2) / 3) * 4 + 1; // +1 for null terminator

    // Add ice config size
    UINT32 iceConfigSize = STRLEN(encodedIceConfig);

    // Calculate total buffer size needed with a safety margin
    bufferSize = jsonStructureSize + base64EncodedSize + corrLen + iceConfigSize;

    // Allocate the buffer
    buffer = (PCHAR) MEMCALLOC(1, bufferSize);
    CHK(buffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Start composing message - build the header part of the JSON
    bufferOffset = (UINT32) SNPRINTF(buffer, bufferSize,
                      "{\n"
                      "\t\"action\": \"%s\",\n"
                      "\t\"RecipientClientId\": \"%.*s\",\n"
                      "\t\"MessagePayload\": \"",
                      pMessageType,
                      MAX_SIGNALING_CLIENT_ID_LEN,
                      pSignalingMessage->peerClientId);

    CHK(bufferOffset < bufferSize - 1, STATUS_BUFFER_TOO_SMALL);

    // Base64 encode payload directly into the buffer at current offset
    writtenSize = bufferSize - bufferOffset - 1; // Leave space for null terminator
    CHK_STATUS(base64Encode(pSignalingMessage->payload, messageLen, buffer + bufferOffset, &writtenSize));
    bufferOffset += writtenSize;
    CHK(bufferOffset < bufferSize - 1, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);

    // Add closing quote for MessagePayload
    STRCAT(buffer, "\""); // Do not add 1 as base64Encode already adds the null terminator

    // Add correlation ID if present
    if (corrLen != 0) {
        INT32 correlationPart = SNPRINTF(buffer + bufferOffset, bufferSize - bufferOffset,
                               ",\n"
                               "\t\"CorrelationId\": \"%.*s\"",
                               (int) corrLen,
                               pSignalingMessage->correlationId);
        CHK(correlationPart > 0 && correlationPart < (INT32)(bufferSize - bufferOffset), STATUS_BUFFER_TOO_SMALL);
        bufferOffset += correlationPart;
    }

    // Add ICE config (if any) and closing brace
    INT32 finalPart = SNPRINTF(buffer + bufferOffset, bufferSize - bufferOffset,
                     "%s\n}",
                     encodedIceConfig);
    CHK(finalPart > 0 && finalPart < (INT32)(bufferSize - bufferOffset), STATUS_BUFFER_TOO_SMALL);
    bufferOffset += finalPart;

    // Ensure the buffer is null-terminated
    buffer[bufferOffset] = '\0';

    // Send using ESP implementation
    // ESP_LOGI(TAG, "Sending WebSocket message, length: %d", bufferOffset);
    // ESP_LOGI(TAG, "Message contents: %.*s", (int) bufferOffset, buffer);
    CHK_STATUS(sendEspWebSocketMessage(pSignalingClient, buffer, bufferOffset));

CleanUp:
    // Free dynamically allocated buffer
    SAFE_MEMFREE(buffer);

    LEAVES();
    return retStatus;
}
