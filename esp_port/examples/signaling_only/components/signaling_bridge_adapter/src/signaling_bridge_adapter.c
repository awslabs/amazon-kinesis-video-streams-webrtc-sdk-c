/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "string.h"
#include "stdlib.h"
#include "webrtc_bridge.h"
#include "app_webrtc.h"
#include "signaling_bridge_adapter.h"
#include "signaling_serializer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED && !defined(CONFIG_IDF_TARGET_ESP32P4)
#include "network_coprocessor.h"
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#include "host_power_save.h"
#endif

static const char *TAG = "signaling_bridge_adapter";

// Static configuration storage
static signaling_bridge_adapter_config_t g_config = {0};
static bool g_initialized = false;

// Global reference for bridge peer connection session
static uint64_t g_custom_data = 0;
static WEBRTC_STATUS (*g_on_message_received)(uint64_t, webrtc_message_t*) = NULL;

// Message queue for buffering messages during P4 wake-up
#define MAX_QUEUE_SIZE 24

typedef enum {
    QUEUE_STATE_IDLE,              // Normal operation, P4 is awake (unused - we start in WAITING)
    QUEUE_STATE_WAITING_FOR_WAKEUP, // Queue all messages until P4 sends READY signal
    QUEUE_STATE_READY              // P4 is ready, messages flow directly
} queue_state_t;

// Design Note: Queue starts in WAITING_FOR_WAKEUP state to handle race conditions.
//
// Simple and Robust Approach:
//   When OFFER arrives, ALWAYS:
//   1. Transition to WAITING_FOR_WAKEUP state
//   2. Call wakeup_host() - physically wake P4 if sleeping (harmless if already awake)
//   3. Send READY_QUERY - ask P4 for readiness (P4 responds with READY if initialized)
//   4. Enqueue OFFER and subsequent messages
//   5. Wait for P4's READY signal
//
// State Machine (BIDIRECTIONAL):
//   WAITING_FOR_WAKEUP ←→ READY
//
// Transitions:
//   WAITING → READY:  When P4 sends READY signal
//   READY → WAITING:  When OFFER arrives (always, regardless of P4 power state)
//
// Benefits:
//   - No race conditions (don't trust power state detection)
//   - P4 handler always ready when messages arrive
//   - Works if P4 is sleeping, waking, or already fully awake
//   - wakeup_host() + READY_QUERY ensures P4 always responds
//
// Deadlock Prevention (2 mechanisms):
//   1. C6 startup: Send READY_QUERY to detect if P4 already running
//   2. On OFFER: Send READY_QUERY to detect if P4 already initialized
//   Both ensure we get READY signal regardless of P4 state.

typedef struct {
    char *serialized_data;
    size_t data_len;
} queued_message_t;

static struct {
    queued_message_t messages[MAX_QUEUE_SIZE];
    int count;
    queue_state_t state;
    SemaphoreHandle_t mutex;
} g_message_queue = {0};

/**
 * @brief Send READY_QUERY to P4 to ask if it's ready
 *
 * This is sent on C6 startup to check if P4 is already running.
 * If P4 responds with READY, we can transition directly to READY state.
 * If no response, we stay in WAITING_FOR_WAKEUP state.
 */
static void send_ready_query_to_p4(void)
{
    signaling_msg_t query_msg = {
        .version = 0,
        .messageType = SIGNALING_MSG_TYPE_READY_QUERY,
        .correlationId = "",
        .peerClientId = "",
        .payloadLen = 0,
        .payload = NULL
    };

    size_t serialized_len = 0;
    char *serialized_msg = serialize_signaling_message(&query_msg, &serialized_len);

    if (serialized_msg == NULL) {
        ESP_LOGE(TAG, "Failed to serialize READY_QUERY message");
        return;
    }

    ESP_LOGI(TAG, "Sending READY_QUERY to P4 to check if already running");
    webrtc_bridge_send_message(serialized_msg, serialized_len);
}

/**
 * @brief Initialize the message queue
 */
static void init_message_queue(void)
{
    if (g_message_queue.mutex == NULL) {
        g_message_queue.mutex = xSemaphoreCreateMutex();
        g_message_queue.count = 0;
        // Start in WAITING_FOR_WAKEUP state to queue all messages until P4 sends READY
        // This handles the race condition where P4 appears "awake" but handler isn't ready yet
        g_message_queue.state = QUEUE_STATE_WAITING_FOR_WAKEUP;
        ESP_LOGI(TAG, "Message queue initialized in WAITING_FOR_WAKEUP state");
        ESP_LOGI(TAG, "All messages will be queued until P4 sends READY signal");

        // Note: READY_QUERY will be sent after bridge initialization is complete
    }
}

/**
 * @brief Enqueue a message for later delivery
 */
static int enqueue_message(const char *serialized_data, size_t data_len)
{
    if (g_message_queue.mutex == NULL) {
        ESP_LOGE(TAG, "Queue not initialized");
        return -1;
    }

    xSemaphoreTake(g_message_queue.mutex, portMAX_DELAY);

    if (g_message_queue.count >= MAX_QUEUE_SIZE) {
        ESP_LOGW(TAG, "Queue full, dropping newest message");
        xSemaphoreGive(g_message_queue.mutex);
        free((void*)serialized_data);
        return -1;
    }

    // Add to queue
    g_message_queue.messages[g_message_queue.count].serialized_data = (char*)serialized_data;
    g_message_queue.messages[g_message_queue.count].data_len = data_len;
    g_message_queue.count++;

    ESP_LOGI(TAG, "Enqueued message (%d bytes), queue size: %d/%d",
             data_len, g_message_queue.count, MAX_QUEUE_SIZE);

    xSemaphoreGive(g_message_queue.mutex);
    return 0;
}

/**
 * @brief Flush all queued messages to P4
 */
static void flush_queue(void)
{
    if (g_message_queue.mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_message_queue.mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Flushing %d queued messages to P4", g_message_queue.count);

    for (int i = 0; i < g_message_queue.count; i++) {
        ESP_LOGI(TAG, "Sending queued message %d/%d (%d bytes)",
                 i + 1, g_message_queue.count, g_message_queue.messages[i].data_len);

        // Send via bridge - webrtc_bridge_send_message takes ownership
        webrtc_bridge_send_message(g_message_queue.messages[i].serialized_data,
                                   g_message_queue.messages[i].data_len);

        // Clear the slot (ownership transferred)
        g_message_queue.messages[i].serialized_data = NULL;
        g_message_queue.messages[i].data_len = 0;
    }

    g_message_queue.count = 0;
    ESP_LOGI(TAG, "Queue flushed successfully");

    xSemaphoreGive(g_message_queue.mutex);
}

/**
 * @brief Clear all queued messages without sending
 */
static void clear_queue(void)
{
    if (g_message_queue.mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_message_queue.mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Clearing %d queued messages", g_message_queue.count);

    for (int i = 0; i < g_message_queue.count; i++) {
        if (g_message_queue.messages[i].serialized_data != NULL) {
            free(g_message_queue.messages[i].serialized_data);
            g_message_queue.messages[i].serialized_data = NULL;
            g_message_queue.messages[i].data_len = 0;
        }
    }

    g_message_queue.count = 0;
    xSemaphoreGive(g_message_queue.mutex);
}

/**
 * @brief Set the queue state
 */
static void set_queue_state(queue_state_t new_state)
{
    if (g_message_queue.mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_message_queue.mutex, portMAX_DELAY);

    const char *state_names[] = {"IDLE", "WAITING_FOR_WAKEUP", "READY"};
    ESP_LOGI(TAG, "Queue state: %s -> %s",
             state_names[g_message_queue.state], state_names[new_state]);

    g_message_queue.state = new_state;
    xSemaphoreGive(g_message_queue.mutex);
}

/**
 * @brief Get the current queue state
 */
static queue_state_t get_queue_state(void)
{
    if (g_message_queue.mutex == NULL) {
        return QUEUE_STATE_IDLE;
    }

    xSemaphoreTake(g_message_queue.mutex, portMAX_DELAY);
    queue_state_t state = g_message_queue.state;
    xSemaphoreGive(g_message_queue.mutex);
    return state;
}

/**
 * @brief Handle signaling messages received from the streaming device via webrtc_bridge
 */
static void handle_bridged_message(const void* data, int len)
{
    ESP_LOGD(TAG, "Received bridged message from streaming device (%d bytes)", len);

    signaling_msg_t signalingMsg = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message");
        return;
    }

    ESP_LOGD(TAG, "Deserialized message type %d from peer %s",
             (int) signalingMsg.messageType, signalingMsg.peerClientId);

    // Handle READY signal from P4
    if (signalingMsg.messageType == SIGNALING_MSG_TYPE_READY) {
        ESP_LOGI(TAG, "Received READY signal from P4, flushing queued messages");
        set_queue_state(QUEUE_STATE_READY);
        flush_queue();

        // Clean up
        if (signalingMsg.payload != NULL) {
            free(signalingMsg.payload);
        }
        return;
    }

    // Note: ICE requests are now handled via synchronous RPC, not bridge messages
    // This provides much better performance (89ms vs 1.4s) by bypassing the async queue

    // Convert to webrtc_message_t format for potential direct handling by bridge_peer_connection
    webrtc_message_t webrtcMsg = {0};
    webrtcMsg.version = signalingMsg.version;

    // Map message types
    switch (signalingMsg.messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown signaling message type: %d", signalingMsg.messageType);
            if (signalingMsg.payload != NULL) {
                free(signalingMsg.payload);
            }
            return;
    }

    // Copy message fields
    strncpy(webrtcMsg.correlation_id, signalingMsg.correlationId, sizeof(webrtcMsg.correlation_id) - 1);
    webrtcMsg.correlation_id[sizeof(webrtcMsg.correlation_id) - 1] = '\0';

    strncpy(webrtcMsg.peer_client_id, signalingMsg.peerClientId, sizeof(webrtcMsg.peer_client_id) - 1);
    webrtcMsg.peer_client_id[sizeof(webrtcMsg.peer_client_id) - 1] = '\0';

    webrtcMsg.payload = signalingMsg.payload;
    webrtcMsg.payload_len = signalingMsg.payloadLen;

    // If we have a registered message callback, use it directly
    // if (g_on_message_received != NULL) {
    ESP_LOGD(TAG, "Using registered callback for message type %d", (int) webrtcMsg.message_type);
    WEBRTC_STATUS status = g_on_message_received(g_custom_data, &webrtcMsg);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to process message via callback: 0x%08" PRIx32, (uint32_t) status);
    } else {
        ESP_LOGD(TAG, "Successfully processed message via callback");
    }

    // Clean up
    if (signalingMsg.payload != NULL) {
        free(signalingMsg.payload);
    }
}

/**
 * @brief Register callbacks for bridge peer connection
 *
 * This function registers callbacks for the bridge peer connection interface
 * to use when receiving messages from the bridge.
 */
WEBRTC_STATUS signaling_bridge_adapter_register_callbacks(
    uint64_t custom_data,
    WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*))
{
    ESP_LOGI(TAG, "Registering bridge peer connection callbacks");

    g_custom_data = custom_data;
    g_on_message_received = on_message_received; // Callback to be called when meesages from bridge are received

    return WEBRTC_STATUS_SUCCESS;
}

WEBRTC_STATUS signaling_bridge_adapter_init(const signaling_bridge_adapter_config_t *config)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Signaling bridge adapter already initialized");
        return WEBRTC_STATUS_SUCCESS;
    }

    // Initialize signaling serializer
    signaling_serializer_init();

    // Initialize message queue
    init_message_queue();

    if (config) {
        // Store configuration
        memcpy(&g_config, config, sizeof(signaling_bridge_adapter_config_t));
    }

    // Register the bridge message handler to receive messages from streaming device
    webrtc_bridge_register_handler(handle_bridged_message);
    ESP_LOGI(TAG, "Registered bridge message handler");

    // Register RPC handler with network coprocessor for ICE server queries
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED && !defined(CONFIG_IDF_TARGET_ESP32P4)
    network_coprocessor_register_ice_server_query_callback(signaling_bridge_adapter_rpc_handler);
    ESP_LOGI(TAG, "Registered ICE server RPC handler");
#endif

    g_initialized = true;
    ESP_LOGI(TAG, "Signaling bridge adapter initialized successfully");
    return WEBRTC_STATUS_SUCCESS;
}

WEBRTC_STATUS signaling_bridge_adapter_start(void)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Signaling bridge adapter not initialized");
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Start the WebRTC bridge
    webrtc_bridge_start();
    ESP_LOGI(TAG, "WebRTC bridge started");

    // Wait for bridge transport to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Now that bridge is fully started, query P4 to see if it's already ready
    // This prevents deadlock if P4 is already running when C6 boots
    send_ready_query_to_p4();

    return WEBRTC_STATUS_SUCCESS;
}

int signaling_bridge_adapter_send_message(webrtc_message_t *signalingMessage)
{
    if (!signalingMessage) {
        ESP_LOGE(TAG, "Invalid signaling message");
        return -1;
    }

    // Convert to signaling_msg_t format for bridge
    signaling_msg_t signalingMsg = {0};
    signalingMsg.version = signalingMessage->version;

    // Map message types between webrtc_message_type_t and signaling_msg_type
    switch (signalingMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_OFFER;
            break;
        case WEBRTC_MESSAGE_TYPE_ANSWER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ANSWER;
            break;
        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
            break;
        case WEBRTC_MESSAGE_TYPE_TRIGGER_OFFER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_TRIGGER_OFFER;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported message type: %d", (int) signalingMessage->message_type);
            return -1;
    }

    strncpy(signalingMsg.peerClientId, signalingMessage->peer_client_id, sizeof(signalingMsg.peerClientId) - 1);
    strncpy(signalingMsg.correlationId, signalingMessage->correlation_id, sizeof(signalingMsg.correlationId) - 1);
    signalingMsg.payload = signalingMessage->payload;
    signalingMsg.payloadLen = signalingMessage->payload_len;

    ESP_LOGD(TAG, "Sending signaling message type %d to streaming device", (int) signalingMessage->message_type);

    size_t serializedMsgLen = 0;
    char *serializedMsg = serialize_signaling_message(&signalingMsg, &serializedMsgLen);
    if (serializedMsg == NULL) {
        ESP_LOGE(TAG, "Failed to serialize signaling message");
        return -1;
    }

    // Check if this is an OFFER - always wake P4 and queue messages
    // This is the simplest and most robust approach: assume P4 needs waking
    // even if it appears awake (handler might not be ready yet)
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    if (signalingMessage->message_type == WEBRTC_MESSAGE_TYPE_OFFER) {
        ESP_LOGI(TAG, "Received OFFER - triggering wake-up sequence (harmless if P4 already awake)");
        set_queue_state(QUEUE_STATE_WAITING_FOR_WAKEUP);
        wakeup_host(2 * 1000);  // Physical wake-up (harmless if P4 already awake)
        send_ready_query_to_p4();  // Query readiness (P4 will respond with READY if already initialized)
        ESP_LOGI(TAG, "Enqueueing OFFER message, waiting for P4 READY signal");
        return enqueue_message(serializedMsg, serializedMsgLen);
    }
#endif

    // Check queue state - if waiting for P4 to be ready, enqueue the message
    queue_state_t state = get_queue_state();
    if (state == QUEUE_STATE_WAITING_FOR_WAKEUP) {
        ESP_LOGI(TAG, "Enqueueing message type %d (waiting for P4 READY signal)", (int) signalingMessage->message_type);
        return enqueue_message(serializedMsg, serializedMsgLen);
    }

    // Normal operation - send directly
    // ownership of serializedMsg is transferred to webrtc_bridge_send_message
    webrtc_bridge_send_message(serializedMsg, serializedMsgLen);

    ESP_LOGD(TAG, "Successfully sent message type %d via bridge", signalingMessage->message_type);
    return 0;
}

int signaling_bridge_adapter_rpc_handler(int index, uint8_t **data, int *len, bool *have_more)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Signaling bridge adapter not initialized");
        return -1;
    }

    ESP_LOGI(TAG, "RPC: ICE server query for index %d", index);

    // Delegate to WebRTC abstraction layer
    bool use_turn = true; // Always request TURN servers via RPC
    uint8_t* raw_data = NULL;
    int raw_len = 0;
    WEBRTC_STATUS status = app_webrtc_get_server_by_idx(index, use_turn, &raw_data, &raw_len, have_more);

    if (status == WEBRTC_STATUS_SUCCESS && raw_data != NULL && raw_len > 0) {
        // Transfer ownership to caller
        *data = raw_data;
        *len = raw_len;

        ESP_LOGI(TAG, "RPC: Successfully retrieved ICE server for index %d (len: %d, have_more: %s)",
                 index, raw_len, *have_more ? "true" : "false");
        return 0; // Success
    } else if (*have_more == true) {
        ESP_LOGI(TAG, "RPC: ICE server query for index %d is still in progress", index);
        *data = NULL;
        *len = 0;
        return 0; // Success
    } else {
        ESP_LOGE(TAG, "RPC: Failed to retrieve ICE server for index %d: 0x%08" PRIx32, index, (uint32_t) status);
        if (raw_data) {
            free(raw_data); // Clean up on failure
        }
        *data = NULL;
        *len = 0;
        *have_more = false;
        return -1; // Failure
    }
}

void signaling_bridge_adapter_trigger_wakeup(void)
{
    ESP_LOGI(TAG, "Triggering P4 wake-up and setting queue to waiting state");
    set_queue_state(QUEUE_STATE_WAITING_FOR_WAKEUP);
}

void signaling_bridge_adapter_deinit(void)
{
    if (g_initialized) {
        // Clear message queue
        clear_queue();

        // Delete mutex
        if (g_message_queue.mutex != NULL) {
            vSemaphoreDelete(g_message_queue.mutex);
            g_message_queue.mutex = NULL;
        }

        // Unregister RPC handler
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED && !defined(CONFIG_IDF_TARGET_ESP32P4)
        network_coprocessor_register_ice_server_query_callback(NULL);
#endif

        // Clear configuration
        memset(&g_config, 0, sizeof(signaling_bridge_adapter_config_t));
        g_initialized = false;

        ESP_LOGI(TAG, "Signaling bridge adapter deinitialized");
    }
}
