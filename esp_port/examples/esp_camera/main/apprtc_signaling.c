/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include "esp_work_queue.h"
#include "esp_heap_caps.h"

#include "apprtc_signaling.h"
#include "signaling_serializer.h"
#include "app_webrtc.h"
#include "signaling_conversion.h"

// Simple structure for peer signaling message
typedef struct {
    uint8_t *data;
    size_t size;
} esp_peer_signaling_msg_t;

// Placeholder for signaling config and ice info
typedef struct {
    // Add necessary fields here
    int dummy;
} esp_peer_signaling_cfg_t, esp_peer_signaling_ice_info_t;

// Forward declarations for WebRTC app functions
int apprtc_signaling_send_callback(signaling_msg_t *pSignalingMsg);

// Forward declarations
static void update_state(apprtc_signaling_state_t new_state);
static esp_err_t register_with_room(const char *room_id);
static int send_register(esp_websocket_client_handle_t ws, const char *room_id, const char *client_id);
static void reconnect_timer_callback(void* arg);
static char* generate_random_id(size_t length);
static void ws_event_work_handler(void *priv_data);
static esp_err_t send_custom_command(esp_websocket_client_handle_t ws, const char *cmd);
static void send_initial_messages(void);
static esp_err_t process_initial_messages(void);

#ifndef CONFIG_APPRTC_SERVER_URL
#define APPRTC_SERVER_URL "https://webrtc.espressif.com"
#else
#define APPRTC_SERVER_URL CONFIG_APPRTC_SERVER_URL
#endif

#ifndef CONFIG_APPRTC_ICE_SERVER_URL
#define APPRTC_ICE_SERVER_REQUEST_URL "https://webrtc.espressif.com/iceconfig"
#else
#define APPRTC_ICE_SERVER_REQUEST_URL CONFIG_APPRTC_ICE_SERVER_URL
#endif

#define WS_TASK_STACK_SIZE  (8 * 1024)
#define APPRTC_MAX_RECONNECT_ATTEMPTS 5
#define APPRTC_RECONNECT_INTERVAL_MS 3000
#define MAX_ROOM_ID_LENGTH 32
#define MAX_RESPONSE_SIZE 4096
#define ROOM_RETRY_INTERVAL_MS 5000
#define MAX_ROOM_RETRY_ATTEMPTS 3
#define MAX_QUEUED_MESSAGES 10

static const char *TAG = "apprtc_signaling";

// Client information structure
typedef struct {
    char  *client_id;
    bool   is_initiator;
    char  *wss_url;
    char  *room_link;
    char  *wss_post_url;
    char  *room_id;
    char  *ice_server;
    cJSON *msg_json;
    cJSON *root;
    char  *base_url;
} client_info_t;

// WebSocket client structure
typedef struct {
    esp_websocket_client_handle_t ws;
    int                          connected;
} wss_client_t;

// Main signaling structure
typedef struct {
    client_info_t                client_info;
    esp_peer_signaling_ice_info_t ice_info;
    wss_client_t                *wss_client;
    esp_peer_signaling_cfg_t     cfg;
    uint32_t                     ice_expire_time;
    bool                         ice_reloading;
    bool                         ice_reload_stop;
} wss_sig_t;

// Function pointer for the custom message sender
static STATUS (*custom_message_sender)(UINT64, signaling_msg_t*) = NULL;
static UINT64 custom_message_sender_data = 0;

// Client state
static struct {
    apprtc_signaling_state_t state;
    esp_websocket_client_handle_t client;
    char room_id[MAX_ROOM_ID_LENGTH + 1];
    char client_id[64];
    bool is_initiator;
    char *wss_url;  // WebSocket URL obtained from HTTPS request

    // Callbacks
    apprtc_signaling_message_handler_t message_handler;
    apprtc_signaling_state_change_handler_t state_handler;
    void *user_data;

    // Synchronization
    SemaphoreHandle_t mutex;

    // Reconnection
    int reconnect_attempts;
    esp_timer_handle_t reconnect_timer;

    // Room retry
    int room_retry_attempts;
    esp_timer_handle_t room_retry_timer;

    client_info_t client_info;

    // Message queue for when not connected
    message_queue_t message_queue;
} apprtc_client = {0};

// HTTP response buffer
typedef struct {
    char *data;
    size_t size;
} http_response_buffer_t;

// Structure to hold WebSocket event data for work queue
typedef struct {
    esp_websocket_event_id_t event_id;
    void *data;
    size_t data_len;
} ws_event_work_item_t;

// HTTP event handler for collecting response data
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buffer_t *response = (http_response_buffer_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Reallocate buffer if needed
            if (response->data == NULL) {
                response->data = heap_caps_calloc(1, evt->data_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            } else {
                response->data = heap_caps_realloc(response->data, response->size + evt->data_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            }
            if (response->data == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for HTTP response");
                return ESP_FAIL;
            }
            memcpy(response->data + response->size, evt->data, evt->data_len);
            response->size += evt->data_len;
            response->data[response->size] = '\0';
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Function to make HTTPS request and get WebSocket URL
static esp_err_t get_websocket_url(const char *room_id, char **wss_url)
{
    esp_err_t ret = ESP_FAIL;
    esp_http_client_handle_t client = NULL;
    http_response_buffer_t response = {0};
    char url[256];

    // Make HTTPS request to get client info
    snprintf(url, sizeof(url), "%s/join/%s", APPRTC_SERVER_URL, room_id);
    ESP_LOGI(TAG, "Making HTTPS request to: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_event_handler,
        .user_data = &response,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
        .skip_cert_common_name_check = true,
        .timeout_ms = 10000,
    };

    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        // free(body_str);
        goto cleanup;
    }

    // Set POST data
    // esp_http_client_set_post_field(client, body_str, strlen(body_str));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Make the request
    if (esp_http_client_perform(client) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed");
        // free(body_str);
        goto cleanup;
    }

    // free(body_str);

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
        goto cleanup;
    }

    // Parse JSON response
    cJSON *root = cJSON_Parse(response.data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        goto cleanup;
    }

    // Log the entire response
    char *response_str = cJSON_Print(root);
    if (response_str) {
        ESP_LOGI(TAG, "Full server response: %s", response_str);
        free(response_str);
    }

    // Get params object
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!params) {
        ESP_LOGE(TAG, "No params object in response");
        cJSON_Delete(root);
        goto cleanup;
    }

    // Clean up any existing client info
    if (apprtc_client.client_info.root) {
        cJSON_Delete(apprtc_client.client_info.root);
    }
    memset(&apprtc_client.client_info, 0, sizeof(client_info_t));

    // Store the root JSON for later use
    apprtc_client.client_info.root = root;

    // Parse all client info fields
    cJSON *item;

    // client_id
    item = cJSON_GetObjectItem(params, "client_id");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.client_id = item->valuestring;
        ESP_LOGI(TAG, "Client ID: %s", item->valuestring);
    }

    // is_initiator
    item = cJSON_GetObjectItem(params, "is_initiator");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.is_initiator = (strcmp(item->valuestring, "true") == 0);
        ESP_LOGI(TAG, "Is Initiator: %s (value: %d)", item->valuestring, apprtc_client.client_info.is_initiator);

        // If the server says we're the initiator, update our local flag
        if (apprtc_client.client_info.is_initiator) {
            apprtc_client.is_initiator = true;
            ESP_LOGI(TAG, "Server designated this client as the initiator");
        }
    }

    // wss_post_url
    item = cJSON_GetObjectItem(params, "wss_post_url");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.wss_post_url = item->valuestring;
        ESP_LOGI(TAG, "WSS Post URL: %s", item->valuestring);
    }

    // room_id
    item = cJSON_GetObjectItem(params, "room_id");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.room_id = item->valuestring;
        ESP_LOGI(TAG, "Room ID: %s", item->valuestring);
    }

    // wss_url
    item = cJSON_GetObjectItem(params, "wss_url");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.wss_url = item->valuestring;
        // *wss_url = strdup(item->valuestring);  // Copy the URL for the caller
        *wss_url = heap_caps_calloc(1, strlen(item->valuestring) + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (*wss_url) {
            strcpy(*wss_url, item->valuestring);
        }
        ESP_LOGI(TAG, "WSS URL: %s", item->valuestring);
    }

    // ice_server_url
    item = cJSON_GetObjectItem(params, "ice_server_url");
    if (item && cJSON_IsString(item)) {
        apprtc_client.client_info.ice_server = item->valuestring;
        ESP_LOGI(TAG, "ICE Server URL: %s", item->valuestring);
    }

    // messages
    item = cJSON_GetObjectItem(params, "messages");
    if (item) {
        apprtc_client.client_info.msg_json = item;
        char *messages_str = cJSON_Print(item);
        if (messages_str) {
            ESP_LOGI(TAG, "Messages: %s", messages_str);
            free(messages_str);
        }
    }

    ret = ESP_OK;

cleanup:
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (response.data) {
        free(response.data);
    }
    return ret;
}

static void apprtc_websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    ws_event_work_item_t *work_item = NULL;

    // Log all WebSocket events
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            if (!data || !data->data_ptr || data->data_len <= 0) {
                ESP_LOGW(TAG, "Invalid WebSocket data event");
                return;
            }

            // Check if data is valid UTF-8 text before logging
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                // Log the raw message
                ESP_LOGI(TAG, "Received raw WebSocket message (%d bytes): %.*s",
                        data->data_len, data->data_len, (char*)data->data_ptr);

                // Check for duplicate registration error in the raw message
                if (data->data_len > 20 && strstr((char*)data->data_ptr, "Duplicated register request")) {
                    ESP_LOGI(TAG, "Detected duplicate registration error in raw message - this is normal after reconnection");
                }
            } else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                // Binary data - log it but don't try to print as text
                ESP_LOGI(TAG, "Received raw WebSocket binary data (%d bytes)", data->data_len);
            } else {
                // Other format - skip processing to avoid disconnection
                ESP_LOGI(TAG, "Received raw WebSocket data with op_code %d (%d bytes) - skipping processing",
                        data->op_code, data->data_len);
                return;
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error event");
            break;
        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGI(TAG, "WebSocket connection closed");
            update_state(APPRTC_SIGNALING_STATE_DISCONNECTED);

            // Check if we were already registered successfully
            if (apprtc_client.client_id[0] != '\0' ||
                (apprtc_client.client_info.client_id != NULL && strlen(apprtc_client.client_info.client_id) > 0)) {
                ESP_LOGI(TAG, "WebSocket closed after successful registration, attempting to reconnect");

                // Save client ID if we have it in client_info but not in the main struct
                if (apprtc_client.client_id[0] == '\0' && apprtc_client.client_info.client_id != NULL) {
                    strncpy(apprtc_client.client_id, apprtc_client.client_info.client_id,
                            sizeof(apprtc_client.client_id) - 1);
                    apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
                    ESP_LOGI(TAG, "Saved client ID before reconnection: %s", apprtc_client.client_id);
                }

                // Try to reconnect after a short delay with the same room ID
                esp_timer_start_once(apprtc_client.reconnect_timer, 3000 * 1000); // 3 seconds
            } else {
                ESP_LOGI(TAG, "WebSocket closed before registration, waiting before reconnecting");
                esp_timer_start_once(apprtc_client.reconnect_timer, APPRTC_RECONNECT_INTERVAL_MS * 1000);
            }
            break;
        default:
            ESP_LOGI(TAG, "WebSocket event: %d", (int) event_id);
            break;
    }

    // Allocate work item
    work_item = calloc(1, sizeof(ws_event_work_item_t));
    if (!work_item) {
        ESP_LOGE(TAG, "Failed to allocate work item");
        return;
    }

    work_item->event_id = event_id;
    work_item->data = NULL;
    work_item->data_len = 0;

    // For data events, safely copy the data
    if (event_id == WEBSOCKET_EVENT_DATA && data->data_len > 0 && data->data_ptr) {
        // Only process text data, skip binary data
        if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
            // work_item->data = malloc(data->data_len + 1);
            work_item->data = heap_caps_calloc(1, data->data_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!work_item->data) {
                ESP_LOGE(TAG, "Failed to allocate memory for WebSocket data");
                free(work_item);
                return;
            }
            memcpy(work_item->data, data->data_ptr, data->data_len);
            ((char *)work_item->data)[data->data_len] = '\0';  // Null terminate
            work_item->data_len = data->data_len;
        } else {
            // For binary data, just log it and don't process further
            ESP_LOGI(TAG, "Skipping binary WebSocket data (%d bytes)", data->data_len);
            work_item->data = NULL;
            work_item->data_len = 0;
        }
    }

    // Queue the work item
    if (esp_work_queue_add_task(ws_event_work_handler, work_item) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue WebSocket event");
        if (work_item->data) {
            free(work_item->data);
        }
        free(work_item);
        return;
    }
}

static esp_err_t ensure_apprtc_client_initialized(void);

static void ws_event_work_handler(void *priv_data)
{
    ws_event_work_item_t *work_item = (ws_event_work_item_t *)priv_data;
    if (!work_item) {
        ESP_LOGE(TAG, "Invalid work item");
        return;
    }

    switch (work_item->event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            update_state(APPRTC_SIGNALING_STATE_CONNECTED);
            // Register with room after connection
            if (register_with_room(apprtc_client.room_id) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register with room");
                update_state(APPRTC_SIGNALING_STATE_ERROR);
            } else {
                apprtc_client.reconnect_attempts = 0;
                send_initial_messages();

                // Process any queued messages
                // Ensure message queue is initialized before checking count
                ensure_apprtc_client_initialized();

                if (message_queue_get_count(&apprtc_client.message_queue) > 0) {
                    ESP_LOGI(TAG, "Processing %d queued messages", message_queue_get_count(&apprtc_client.message_queue));
                    // Wait a short time to ensure registration is complete
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    apprtc_signaling_process_queued_messages();
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            update_state(APPRTC_SIGNALING_STATE_DISCONNECTED);

            // Check if we were already registered successfully
            if (apprtc_client.client_id[0] != '\0' ||
                (apprtc_client.client_info.client_id != NULL && strlen(apprtc_client.client_info.client_id) > 0)) {
                ESP_LOGI(TAG, "WebSocket disconnected after successful registration, attempting to reconnect");

                // Save client ID if we have it in client_info but not in the main struct
                if (apprtc_client.client_id[0] == '\0' && apprtc_client.client_info.client_id != NULL) {
                    strncpy(apprtc_client.client_id, apprtc_client.client_info.client_id,
                            sizeof(apprtc_client.client_id) - 1);
                    apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
                    ESP_LOGI(TAG, "Saved client ID before reconnection: %s", apprtc_client.client_id);
                }

                // Try to reconnect immediately with the same room ID
                esp_timer_start_once(apprtc_client.reconnect_timer, 1000 * 1000); // 1 second
            } else if (apprtc_client.state != APPRTC_SIGNALING_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "WebSocket disconnected before registration, waiting before reconnecting");
                esp_timer_start_once(apprtc_client.reconnect_timer, APPRTC_RECONNECT_INTERVAL_MS * 1000);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (work_item->data_len > 0 && work_item->data) {
                // Check if the data appears to be valid text
                bool is_valid_text = true;
                for (size_t i = 0; i < work_item->data_len; i++) {
                    if (((char*)work_item->data)[i] < 0x20 &&
                        ((char*)work_item->data)[i] != '\t' &&
                        ((char*)work_item->data)[i] != '\n' &&
                        ((char*)work_item->data)[i] != '\r') {
                        is_valid_text = false;
                        break;
                    }
                }

                if (is_valid_text) {
                    ESP_LOGI(TAG, "Received WebSocket message (%d bytes): %.*s",
                            work_item->data_len, work_item->data_len, (char*)work_item->data);

                    // Check for empty messages or empty error fields
                    if (strstr((char*)work_item->data, "{\"msg\":\"{\\\"error\\\":\\\"\\\"}\"") != NULL ||
                        strstr((char*)work_item->data, "{\"error\":\"\"}") != NULL) {
                        ESP_LOGD(TAG, "Received empty message or empty error response, ignoring");
                        break;
                    }

                    // Parse the message to check for registration confirmation
                    cJSON *json = cJSON_Parse(work_item->data);
                    if (json) {
                        // Check for error field
                        cJSON *error = cJSON_GetObjectItem(json, "error");
                        if (error && cJSON_IsString(error) && strlen(cJSON_GetStringValue(error)) > 0) {
                            // Only consider it an error if the error field has content
                            ESP_LOGW(TAG, "Received error response: %s", cJSON_GetStringValue(error));
                            cJSON_Delete(json);
                            break;
                        }

                        // Check for message field
                        cJSON *msg = cJSON_GetObjectItem(json, "msg");
                        if (msg && cJSON_IsString(msg)) {
                            const char *msg_str = cJSON_GetStringValue(msg);

                            // Skip empty messages or those with empty error fields
                            if (msg_str && (strlen(msg_str) == 0 || strcmp(msg_str, "{\"error\":\"\"}") == 0)) {
                                ESP_LOGD(TAG, "Skipping empty inner message");
                                cJSON_Delete(json);
                                break;
                            }

                            ESP_LOGI(TAG, "Processing message: %s", msg_str);

                            // Process the inner message only if it's a WebRTC signaling message
                            if (msg_str && strlen(msg_str) > 0 && strcmp(msg_str, "{\"error\":\"\"}") != 0) {
                                ESP_LOGI(TAG, "Received inner message: %s", msg_str);

                                // Parse the inner message to check if it's a WebRTC signaling message
                                cJSON *inner_json = cJSON_Parse(msg_str);
                                if (inner_json) {
                                    cJSON *type = cJSON_GetObjectItem(inner_json, "type");
                                    if (type && cJSON_IsString(type)) {
                                        const char *type_str = cJSON_GetStringValue(type);

                                        // Only forward WebRTC signaling messages
                                        if (strcmp(type_str, "offer") == 0 ||
                                            strcmp(type_str, "answer") == 0 ||
                                            strcmp(type_str, "candidate") == 0) {
                                            ESP_LOGI(TAG, "Forwarding WebRTC signaling message: %s", type_str);

                                            if (apprtc_client.message_handler) {
                                                apprtc_client.message_handler(msg_str, strlen(msg_str), apprtc_client.user_data);
                                            }
                                        } else {
                                            ESP_LOGI(TAG, "Ignoring non-WebRTC message type: %s", type_str);
                                        }
                                    } else {
                                        ESP_LOGI(TAG, "Inner message has no type field, ignoring");
                                    }
                                    cJSON_Delete(inner_json);
                                } else {
                                    ESP_LOGI(TAG, "Failed to parse inner message as JSON, ignoring");
                                }
                            }

                            // Check for registration confirmation
                            cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
                            if (cmd && cJSON_IsString(cmd) && strcmp(cJSON_GetStringValue(cmd), "response") == 0) {
                                cJSON *resp = cJSON_GetObjectItem(json, "resp");
                                if (resp && cJSON_IsString(resp) && strcmp(cJSON_GetStringValue(resp), "register") == 0) {
                                    // Registration successful
                                    ESP_LOGI(TAG, "Registration confirmed by server");

                                    // Check for client ID in the response
                                    cJSON *clientid = cJSON_GetObjectItem(json, "clientid");
                                    if (clientid && cJSON_IsString(clientid)) {
                                        strncpy(apprtc_client.client_id, cJSON_GetStringValue(clientid),
                                                sizeof(apprtc_client.client_id) - 1);
                                        apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
                                        ESP_LOGI(TAG, "Server assigned client ID: %s", apprtc_client.client_id);
                                    }

                                    // Stop the room retry timer
                                    if (apprtc_client.room_retry_timer != NULL) {
                                        esp_timer_stop(apprtc_client.room_retry_timer);
                                    }

                                    // Reset retry attempts
                                    apprtc_client.room_retry_attempts = 0;
                                }
                            }

                            cJSON_Delete(json);
                        } else {
                            ESP_LOGW(TAG, "Failed to parse WebSocket message as JSON");
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "WebSocket message contains invalid UTF-8 characters");
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
            update_state(APPRTC_SIGNALING_STATE_ERROR);
            break;

        default:
            ESP_LOGI(TAG, "Unhandled WebSocket event: %d", work_item->event_id);
            break;
    }

    // Free resources
    if (work_item->data) {
        free(work_item->data);
    }
    free(work_item);
}

static void room_retry_timer_callback(void* arg)
{
    ESP_LOGI(TAG, "No response to registration, attempting to rejoin room (attempt %d/%d)",
             apprtc_client.room_retry_attempts + 1, MAX_ROOM_RETRY_ATTEMPTS);

    if (apprtc_client.room_retry_attempts >= MAX_ROOM_RETRY_ATTEMPTS) {
        ESP_LOGE(TAG, "Max room retry attempts reached");
        update_state(APPRTC_SIGNALING_STATE_ERROR);
        return;
    }

    apprtc_client.room_retry_attempts++;

    // Check if we're still connected before trying to register again
    if (apprtc_client.state == APPRTC_SIGNALING_STATE_CONNECTED &&
        esp_websocket_client_is_connected(apprtc_client.client)) {
        ESP_LOGI(TAG, "WebSocket still connected, retrying registration");
        register_with_room(apprtc_client.room_id);
    } else {
        ESP_LOGW(TAG, "WebSocket not connected, cannot retry registration");
        // Try to reconnect the WebSocket
        if (apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTING) {
            update_state(APPRTC_SIGNALING_STATE_DISCONNECTED);
            if (apprtc_client.reconnect_timer != NULL) {
                esp_timer_start_once(apprtc_client.reconnect_timer, APPRTC_RECONNECT_INTERVAL_MS * 1000);
            }
        }
    }
}

esp_err_t apprtc_signaling_init(apprtc_signaling_message_handler_t message_handler,
                               apprtc_signaling_state_change_handler_t state_handler,
                               void* user_data)
{
    ESP_LOGI(TAG, "Initializing AppRTC signaling client");

    if (!message_handler) {
        ESP_LOGE(TAG, "Message handler cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize mutex
    if (apprtc_client.mutex == NULL) {
        apprtc_client.mutex = xSemaphoreCreateMutex();
        if (apprtc_client.mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    // Initialize work queue
    esp_err_t err = esp_work_queue_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return err;
    }

    err = esp_work_queue_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        esp_work_queue_deinit();
        return err;
    }

    xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);

    // Reset client state
    apprtc_client.state = APPRTC_SIGNALING_STATE_DISCONNECTED;
    apprtc_client.message_handler = message_handler;
    apprtc_client.state_handler = state_handler;
    apprtc_client.user_data = user_data;
    apprtc_client.reconnect_attempts = 0;
    memset(apprtc_client.room_id, 0, sizeof(apprtc_client.room_id));

    // Initialize message queue - make sure to do this before it's used
    err = message_queue_init(&apprtc_client.message_queue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize message queue");
        xSemaphoreGive(apprtc_client.mutex);
        return err;
    }

#if AUTO_REJOIN_ENABLED
    // Create reconnect timer if it doesn't exist
    esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_callback,
        .name = "reconnect_timer"
    };

    if (apprtc_client.reconnect_timer == NULL) {
        if (esp_timer_create(&timer_args, &apprtc_client.reconnect_timer) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create reconnect timer");
            xSemaphoreGive(apprtc_client.mutex);
            return ESP_FAIL;
        }
    }

    // Create room retry timer
    esp_timer_create_args_t room_timer_args = {
        .callback = room_retry_timer_callback,
        .name = "room_retry_timer"
    };

    if (apprtc_client.room_retry_timer == NULL) {
        if (esp_timer_create(&room_timer_args, &apprtc_client.room_retry_timer) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create room retry timer");
            xSemaphoreGive(apprtc_client.mutex);
            return ESP_FAIL;
        }
    }
    // Reset room retry attempts
    apprtc_client.room_retry_attempts = 0;
#endif

    xSemaphoreGive(apprtc_client.mutex);
    return ESP_OK;
}

esp_err_t apprtc_signaling_connect(const char* room_id)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Connecting to AppRTC signaling server, room: %s", room_id ? room_id : "new");

    // CRITICAL: Initialize message queue FIRST before any operations that might trigger callbacks
    ret = ensure_apprtc_client_initialized();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppRTC client early in connect");
        return ret;
    }

    if (!apprtc_client.mutex) {
        ESP_LOGE(TAG, "AppRTC client mutex is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);

    // If we're already connected or connecting, return an error
    if (apprtc_client.state == APPRTC_SIGNALING_STATE_CONNECTED ||
        apprtc_client.state == APPRTC_SIGNALING_STATE_CONNECTING) {
        ESP_LOGW(TAG, "Already connected or connecting to signaling server");
        xSemaphoreGive(apprtc_client.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Update state
    update_state(APPRTC_SIGNALING_STATE_CONNECTING);

    // Clean up any existing WebSocket URL
    if (apprtc_client.wss_url) {
        free(apprtc_client.wss_url);
        apprtc_client.wss_url = NULL;
    }

    // If room ID is provided, copy it
    if (room_id) {
        strncpy(apprtc_client.room_id, room_id, MAX_ROOM_ID_LENGTH);
        apprtc_client.room_id[MAX_ROOM_ID_LENGTH] = '\0';
        apprtc_client.is_initiator = false;
    }
#ifdef CONFIG_APPRTC_USE_FIXED_ROOM
    else {
        // Use the fixed room ID from config
        strncpy(apprtc_client.room_id, CONFIG_APPRTC_ROOM_ID, MAX_ROOM_ID_LENGTH);
        apprtc_client.room_id[MAX_ROOM_ID_LENGTH] = '\0';
        apprtc_client.is_initiator = true;
        ESP_LOGI(TAG, "Using fixed room ID: %s", apprtc_client.room_id);
    }
#else
    else {
        // Generate a random room ID
        char *random_id = generate_random_id(10);
        if (random_id) {
            strncpy(apprtc_client.room_id, random_id, MAX_ROOM_ID_LENGTH);
            apprtc_client.room_id[MAX_ROOM_ID_LENGTH] = '\0';
            free(random_id);
            apprtc_client.is_initiator = true;
        } else {
            ESP_LOGE(TAG, "Failed to generate random room ID");
            xSemaphoreGive(apprtc_client.mutex);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    // Step 1: Get WebSocket URL via HTTPS request
    esp_err_t err = get_websocket_url(apprtc_client.room_id, &apprtc_client.wss_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WebSocket URL");
        update_state(APPRTC_SIGNALING_STATE_ERROR);
        xSemaphoreGive(apprtc_client.mutex);
        return err;
    }

    // Process initial messages received from the server (offer and ICE candidates)
    if (!apprtc_client.is_initiator) {
        // Only process initial messages if we're not the initiator (i.e., we're joining a room)
        err = process_initial_messages();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to process initial messages");
            // Continue anyway, as this is not a fatal error
        }
    }

    // Use the server-provided client_id
    if (apprtc_client.client_info.client_id) {
        strncpy(apprtc_client.client_id, apprtc_client.client_info.client_id, sizeof(apprtc_client.client_id) - 1);
        apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
        ESP_LOGI(TAG, "Using server-provided client ID: %s", apprtc_client.client_id);
    }

    // Step 2: Configure and connect WebSocket client
    if (apprtc_client.client == NULL) {
        // Create Origin header
        char origin[128];
        snprintf(origin, sizeof(origin), "Origin: %s/join\r\n", APPRTC_SERVER_URL);

        esp_websocket_client_config_t websocket_cfg = {
            .uri = apprtc_client.wss_url,
            .headers = origin,
            .disable_auto_reconnect = true,  // We'll handle reconnection ourselves
            .reconnect_timeout_ms = 60 * 1000,
            .network_timeout_ms = 30000,     // Increased timeout
            .buffer_size = 32 * 1024,        // Increased buffer size for large messages
            .task_stack = WS_TASK_STACK_SIZE,
            .ping_interval_sec = 5,          // Send ping every 5 seconds to keep connection alive
            .pingpong_timeout_sec = 10,      // Timeout if no pong received in 10 seconds
            .keep_alive_enable = true,       // Enable TCP keep-alive
            .keep_alive_idle = 5,            // Keep-alive idle time (5 seconds)
            .keep_alive_interval = 5,        // Keep-alive interval (5 seconds)
            .keep_alive_count = 3,           // Keep-alive packet retry count
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
            .crt_bundle_attach = esp_crt_bundle_attach,
#endif
#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
            .skip_cert_common_name_check = true,
#else
            .skip_cert_common_name_check = false,
#endif
            .transport = WEBSOCKET_TRANSPORT_OVER_SSL,
        };

        ESP_LOGI(TAG, "Connecting to WebSocket URL: %s", websocket_cfg.uri);
        apprtc_client.client = esp_websocket_client_init(&websocket_cfg);
        if (apprtc_client.client == NULL) {
            ESP_LOGE(TAG, "Failed to initialize WebSocket client");
            update_state(APPRTC_SIGNALING_STATE_ERROR);
            xSemaphoreGive(apprtc_client.mutex);
            return ESP_FAIL;
        }

        // Register WebSocket event handler
        esp_websocket_register_events(apprtc_client.client, WEBSOCKET_EVENT_ANY, apprtc_websocket_event_handler, NULL);
    }

    // Start WebSocket connection
    err = esp_websocket_client_start(apprtc_client.client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WebSocket server");
        update_state(APPRTC_SIGNALING_STATE_ERROR);
        xSemaphoreGive(apprtc_client.mutex);
        return err;
    }

    xSemaphoreGive(apprtc_client.mutex);

    return ESP_OK;
}

esp_err_t apprtc_signaling_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting from signaling server");

    xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);

    // Stop WebSocket client if it exists
    if (apprtc_client.client) {
        esp_websocket_client_stop(apprtc_client.client);
        esp_websocket_client_destroy(apprtc_client.client);
        apprtc_client.client = NULL;
    }

    // Free WebSocket URL if it exists
    if (apprtc_client.wss_url) {
        free(apprtc_client.wss_url);
        apprtc_client.wss_url = NULL;
    }

    // Clean up message queue
    message_queue_cleanup(&apprtc_client.message_queue);

    // Reset state and room ID
    update_state(APPRTC_SIGNALING_STATE_DISCONNECTED);
    memset(apprtc_client.room_id, 0, sizeof(apprtc_client.room_id));
    apprtc_client.is_initiator = false;
    apprtc_client.reconnect_attempts = 0;

    // Stop reconnect timer if it's running
    if (apprtc_client.reconnect_timer) {
        esp_timer_stop(apprtc_client.reconnect_timer);
    }

    // Stop and delete room retry timer
    if (apprtc_client.room_retry_timer) {
        esp_timer_stop(apprtc_client.room_retry_timer);
        esp_timer_delete(apprtc_client.room_retry_timer);
        apprtc_client.room_retry_timer = NULL;
    }

    xSemaphoreGive(apprtc_client.mutex);
    return ESP_OK;
}

esp_err_t apprtc_signaling_send_message(const char* message, size_t message_len)
{
    if (!message || message_len == 0) {
        ESP_LOGE(TAG, "Invalid message");
        return ESP_ERR_INVALID_ARG;
    }

    if (apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Not connected to signaling server");
        return ESP_ERR_INVALID_STATE;
    }

    // Try to parse the message as JSON to add our client ID
    cJSON *json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse message as JSON");
        // Just send the raw message
        return (esp_websocket_client_send_text(apprtc_client.client, message, message_len, portMAX_DELAY) > 0)
            ? ESP_OK : ESP_FAIL;
    }

    // Add client ID if not present or replace existing one
    if (cJSON_HasObjectItem(json, "clientId")) {
        cJSON_DeleteItemFromObject(json, "clientId");
    }

    // Use the client ID designated by the server
    if (apprtc_client.client_id[0] != '\0') {
        cJSON_AddStringToObject(json, "clientId", apprtc_client.client_id);
        ESP_LOGI(TAG, "Using server-designated client ID: %s for outgoing message", apprtc_client.client_id);
    } else {
        // Fallback to room ID if client ID is not available
        cJSON_AddStringToObject(json, "clientId", apprtc_client.room_id);
        ESP_LOGW(TAG, "Client ID not available, using room ID: %s instead", apprtc_client.room_id);
    }

    // Create the wrapper message with "cmd":"send"
    cJSON *wrapper = cJSON_CreateObject();
    if (!wrapper) {
        ESP_LOGE(TAG, "Failed to create wrapper JSON object");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Add the command
    cJSON_AddStringToObject(wrapper, "cmd", "send");

    // Convert the inner message to a string
    char *inner_message = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!inner_message) {
        ESP_LOGE(TAG, "Failed to serialize inner message");
        cJSON_Delete(wrapper);
        return ESP_FAIL;
    }

    // Add the inner message as "msg" field
    cJSON_AddStringToObject(wrapper, "msg", inner_message);
    free(inner_message);

    // Serialize the wrapper message
    char *new_message = cJSON_PrintUnformatted(wrapper);
    cJSON_Delete(wrapper);

    if (!new_message) {
        ESP_LOGE(TAG, "Failed to serialize wrapper message");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending message to room %s: %s", apprtc_client.room_id, new_message);

    // Double-check connection before sending
    esp_err_t ret = ESP_FAIL;

    if (apprtc_client.client != NULL && esp_websocket_client_is_connected(apprtc_client.client)) {
        int result = esp_websocket_client_send_text(apprtc_client.client, new_message, strlen(new_message), portMAX_DELAY);
        if (result > 0) {
            ESP_LOGD(TAG, "Message sent successfully");
            ret = ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to send message");

            // If we have a reconnect timer, start it to attempt reconnection
            if (apprtc_client.reconnect_timer != NULL) {
                ESP_LOGI(TAG, "Scheduling reconnection attempt");
                esp_timer_start_once(apprtc_client.reconnect_timer, 1000 * 1000); // 1 second
            }
        }
    } else {
        ESP_LOGE(TAG, "WebSocket client is not connected");
        update_state(APPRTC_SIGNALING_STATE_DISCONNECTED);

        // If we have a reconnect timer, start it to attempt reconnection
        if (apprtc_client.reconnect_timer != NULL) {
            ESP_LOGI(TAG, "Scheduling reconnection attempt");
            esp_timer_start_once(apprtc_client.reconnect_timer, 1000 * 1000); // 1 second
        }
    }

    free(new_message);
    return ret;
}

const char* apprtc_signaling_get_room_id(void)
{
    return apprtc_client.room_id[0] != '\0' ? apprtc_client.room_id : NULL;
}

apprtc_signaling_state_t apprtc_signaling_get_state(void)
{
    return apprtc_client.state;
}

/**
 * Register a custom message sender for WebRTC signaling
 */
esp_err_t apprtc_signaling_register_message_sender(
                STATUS (*message_sender)(UINT64, signaling_msg_t*), UINT64 custom_data)
{
    ESP_LOGI(TAG, "Registering custom signaling message sender");

    if (message_sender == NULL) {
        ESP_LOGE(TAG, "Message sender function cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);
    custom_message_sender = message_sender;
    custom_message_sender_data = custom_data;
    xSemaphoreGive(apprtc_client.mutex);

    ESP_LOGI(TAG, "Custom signaling message sender registered successfully");
    return ESP_OK;
}

/************************* Private functions *************************/

static void update_state(apprtc_signaling_state_t new_state)
{
    if (apprtc_client.state != new_state) {
        apprtc_client.state = new_state;

        // Notify state change if callback is registered
        if (apprtc_client.state_handler) {
            apprtc_client.state_handler(new_state, apprtc_client.user_data);
        }

        ESP_LOGI(TAG, "AppRTC signaling state changed to: %d", new_state);
    }
}

static void reconnect_timer_callback(void* arg)
{
    (void)arg;

    xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);

    if (apprtc_client.state != APPRTC_SIGNALING_STATE_DISCONNECTED &&
        apprtc_client.state != APPRTC_SIGNALING_STATE_ERROR) {
        xSemaphoreGive(apprtc_client.mutex);
        return;
    }

    apprtc_client.reconnect_attempts++;

    if (apprtc_client.reconnect_attempts > APPRTC_MAX_RECONNECT_ATTEMPTS) {
        ESP_LOGE(TAG, "Max reconnection attempts reached");
        update_state(APPRTC_SIGNALING_STATE_ERROR);
        xSemaphoreGive(apprtc_client.mutex);
        return;
    }

    ESP_LOGI(TAG, "Attempting to reconnect (%d/%d)",
             apprtc_client.reconnect_attempts, APPRTC_MAX_RECONNECT_ATTEMPTS);

    // Try to reconnect with the same room ID
    char room_id[MAX_ROOM_ID_LENGTH + 1];
    strncpy(room_id, apprtc_client.room_id, MAX_ROOM_ID_LENGTH);
    room_id[MAX_ROOM_ID_LENGTH] = '\0';

    // Save the client ID and initiator status
    char client_id[64];
    bool was_initiator = apprtc_client.is_initiator;
    if (apprtc_client.client_id[0] != '\0') {
        strncpy(client_id, apprtc_client.client_id, sizeof(client_id) - 1);
        client_id[sizeof(client_id) - 1] = '\0';
    } else {
        client_id[0] = '\0';
    }

    xSemaphoreGive(apprtc_client.mutex);

    // Destroy the current WebSocket connection if it exists
    if (apprtc_client.client) {
        esp_websocket_client_stop(apprtc_client.client);
        esp_websocket_client_destroy(apprtc_client.client);
        apprtc_client.client = NULL;
    }

    // Wait a short time before reconnecting
    vTaskDelay(pdMS_TO_TICKS(500));

    // Call connect again with the same room ID
    esp_err_t err = apprtc_signaling_connect(room_id);

    // Restore client ID and initiator status if reconnection was successful
    if (err == ESP_OK && client_id[0] != '\0') {
        xSemaphoreTake(apprtc_client.mutex, portMAX_DELAY);
        strncpy(apprtc_client.client_id, client_id, sizeof(apprtc_client.client_id) - 1);
        apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
        apprtc_client.is_initiator = was_initiator;
        xSemaphoreGive(apprtc_client.mutex);

        ESP_LOGI(TAG, "Restored client ID: %s and initiator status: %d after reconnection",
                 apprtc_client.client_id, apprtc_client.is_initiator);
    }
}

static esp_err_t register_with_room(const char *room_id)
{
    if (!room_id || !apprtc_client.client) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Registering with room: %s", room_id);
    return send_register(apprtc_client.client, room_id, apprtc_client.client_info.client_id);
}

/**
 * @brief Process a received message from AppRTC and forward it to WebRTC SDK
 *
 * This function extracts the inner message if needed, converts it to signaling_msg_t format,
 * and forwards it to the message handler.
 */
esp_err_t apprtc_signaling_process_message(const char *message, size_t message_len)
{
    if (!message || message_len == 0) {
        ESP_LOGE(TAG, "Invalid message");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Processing message: %.*s", (int)message_len, message);

    // Check if the message is a JSON object
    cJSON *json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse message as JSON");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if the message has a "msg" field, which is common in AppRTC protocol
    cJSON *msg_field = cJSON_GetObjectItem(json, "msg");
    if (msg_field && cJSON_IsString(msg_field)) {
        const char *inner_msg = cJSON_GetStringValue(msg_field);

        // Check if the inner message is empty or just contains an empty error
        if (!inner_msg || strlen(inner_msg) == 0 || strcmp(inner_msg, "{\"error\":\"\"}") == 0) {
            ESP_LOGI(TAG, "Received empty message or empty error, ignoring");
            cJSON_Delete(json);
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Found inner message: %s", inner_msg);

        // Parse the inner message to check the type
        cJSON *inner_json = cJSON_Parse(inner_msg);
        if (inner_json) {
            cJSON *type = cJSON_GetObjectItem(inner_json, "type");
            if (type && cJSON_IsString(type)) {
                const char *type_str = cJSON_GetStringValue(type);
                ESP_LOGI(TAG, "Message type: %s", type_str);

                // Only forward WebRTC signaling messages
                if (strcmp(type_str, "offer") == 0 ||
                    strcmp(type_str, "answer") == 0 ||
                    strcmp(type_str, "candidate") == 0) {
                    ESP_LOGI(TAG, "Forwarding WebRTC signaling message: %s", type_str);

                    if (apprtc_client.message_handler) {
                        apprtc_client.message_handler(inner_msg, strlen(inner_msg), apprtc_client.user_data);
                    } else {
                        ESP_LOGE(TAG, "No message handler registered");
                    }
                } else {
                    ESP_LOGI(TAG, "Ignoring non-WebRTC message type: %s", type_str);
                }
            } else {
                ESP_LOGI(TAG, "Inner message has no type field, ignoring");
            }
            cJSON_Delete(inner_json);
        } else {
            ESP_LOGI(TAG, "Failed to parse inner message as JSON, ignoring");
        }

        cJSON_Delete(json);
        return ESP_OK;
    }

    // For direct messages (not wrapped in "msg" field), check if they're WebRTC signaling messages
    cJSON *direct_type = cJSON_GetObjectItem(json, "type");
    if (direct_type && cJSON_IsString(direct_type)) {
        const char *type_str = cJSON_GetStringValue(direct_type);
        ESP_LOGI(TAG, "Direct message type: %s", type_str);

        // Only forward WebRTC signaling messages
        if (strcmp(type_str, "offer") == 0 ||
            strcmp(type_str, "answer") == 0 ||
            strcmp(type_str, "candidate") == 0) {
            ESP_LOGI(TAG, "Forwarding direct WebRTC signaling message: %s", type_str);

            if (apprtc_client.message_handler) {
                apprtc_client.message_handler(message, message_len, apprtc_client.user_data);
            } else {
                ESP_LOGE(TAG, "No message handler registered for fallback processing");
            }
        } else {
            ESP_LOGI(TAG, "Ignoring non-WebRTC direct message type: %s", type_str);
        }
    } else {
        ESP_LOGI(TAG, "Message has no type field, treating as non-WebRTC message");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

int apprtc_signaling_send_callback(signaling_msg_t *pSignalingMsg)
{
    esp_err_t ret;
    char *apprtc_json_msg = NULL;
    size_t apprtc_json_len = 0;

    if (!pSignalingMsg) {
        ESP_LOGE(TAG, "Invalid data for sending");
        return -1;
    }

    // Ensure early initialization before any operations
    esp_err_t init_err = ensure_apprtc_client_initialized();
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppRTC client early");
        return -1;
    }

    // Check if we're connected to the signaling server first
    if (apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTED ||
        !apprtc_client.client ||
        !esp_websocket_client_is_connected(apprtc_client.client)) {
        ESP_LOGW(TAG, "Not connected to signaling server, queueing message for later");

        // Queue the message for later sending
        if (apprtc_signaling_queue_message(pSignalingMsg, sizeof(signaling_msg_t)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to queue message");
            return -1;
        }

        // Return a special status code to indicate "message queued"
        return 1;  // Custom code for "message queued"
    }

    // Log the payload for debugging
    if (pSignalingMsg->payload != NULL && pSignalingMsg->payloadLen > 0) {
        ESP_LOGI(TAG, "Signaling message payload: %.*s", (int)pSignalingMsg->payloadLen, pSignalingMsg->payload);
    } else {
        ESP_LOGW(TAG, "Signaling message has no payload");
    }

    // Use the client ID designated by the server
    if (apprtc_client.client_id[0] != '\0') {
        STRNCPY(pSignalingMsg->peerClientId, apprtc_client.client_id, SS_MAX_SIGNALING_CLIENT_ID_LEN);
        pSignalingMsg->peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        ESP_LOGI(TAG, "Using server-designated client ID: %s for message", apprtc_client.client_id);
    } else if (apprtc_client.room_id[0] != '\0') {
        // Fallback to room ID if client ID is not available
        STRNCPY(pSignalingMsg->peerClientId, apprtc_client.room_id, SS_MAX_SIGNALING_CLIENT_ID_LEN);
        pSignalingMsg->peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        ESP_LOGW(TAG, "Client ID not available, using room ID: %s instead", apprtc_client.room_id);
    }

    ESP_LOGI(TAG, "Processing message for room: %s", apprtc_client.room_id);

    // Convert directly to AppRTC format using the conversion module
    int status = signaling_message_to_apprtc_json(pSignalingMsg, &apprtc_json_msg, &apprtc_json_len);
    if (status != 0 || apprtc_json_msg == NULL) {
        ESP_LOGE(TAG, "Failed to convert to AppRTC JSON format, status: %d", status);
        return -1;
    }

    // Log the message being sent
    ESP_LOGI(TAG, "Sending AppRTC message: %.*s", (int)apprtc_json_len, apprtc_json_msg);

    // Send the converted message via AppRTC signaling
    ret = apprtc_signaling_send_message(apprtc_json_msg, apprtc_json_len);

    // Free the converted message
    free(apprtc_json_msg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send message via AppRTC signaling");
        return -1;
    }

    ESP_LOGI(TAG, "Message sent successfully");
    return 0;
}

static int send_register(esp_websocket_client_handle_t ws, const char *room_id, const char *client_id)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "register");
    cJSON_AddStringToObject(json, "roomid", room_id);
    cJSON_AddStringToObject(json, "clientid", client_id);
    int ret = 0;
    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (payload) {
        ESP_LOGI(TAG, "Sending registration message: %s", payload);
        ret = esp_websocket_client_send_text(ws, payload, strlen(payload), portMAX_DELAY);
        free(payload);

        // Start the room retry timer in case we don't get a response
        if (apprtc_client.room_retry_timer != NULL) {
            esp_timer_start_once(apprtc_client.room_retry_timer, ROOM_RETRY_INTERVAL_MS * 1000);
        }
    }
    return ret > 0 ? ESP_OK : ESP_FAIL;
}

static int send_bye(esp_websocket_client_handle_t ws)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "send");
    cJSON_AddStringToObject(msg, "type", "bye");
    char *msg_body = cJSON_PrintUnformatted(msg);
    if (msg_body) {
        cJSON_AddStringToObject(json, "msg", msg_body);
    }
    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    cJSON_Delete(msg);
    int ret = 0;
    if (payload) {
        ESP_LOGI(TAG, "Sending bye message: %s", payload);
        ret = esp_websocket_client_send_text(ws, payload, strlen(payload), portMAX_DELAY);
        free(payload);
    }
    free(msg_body);
    return ret > 0 ? ESP_OK : ESP_FAIL;
}

static char* generate_random_id(size_t length)
{
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t charset_size = sizeof(charset) - 1;

    char* id = heap_caps_calloc(1, length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (id == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        uint32_t random_value = esp_random();
        id[i] = charset[random_value % charset_size];
    }
    id[length] = '\0';

    return id;
}

static void send_initial_messages(void)
{
    ESP_LOGI(TAG, "Sending initial messages");

    // Check if we're connected first with multiple checks
    if (apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Cannot send initial messages - signaling state not connected (state: %d)", apprtc_client.state);
        goto schedule_reconnect;
    }

    if (apprtc_client.client == NULL) {
        ESP_LOGE(TAG, "Cannot send initial messages - WebSocket client is NULL");
        goto schedule_reconnect;
    }

    if (!esp_websocket_client_is_connected(apprtc_client.client)) {
        ESP_LOGE(TAG, "Cannot send initial messages - WebSocket not connected");
        goto schedule_reconnect;
    }

    if (apprtc_client.is_initiator) {
        ESP_LOGI(TAG, "This client is the initiator - creating and sending offer");

        // Make sure we have a valid client ID
        if (apprtc_client.client_id[0] == '\0') {
            // Try to get client ID from client_info if available
            if (apprtc_client.client_info.client_id != NULL) {
                strncpy(apprtc_client.client_id, apprtc_client.client_info.client_id,
                        sizeof(apprtc_client.client_id) - 1);
                apprtc_client.client_id[sizeof(apprtc_client.client_id) - 1] = '\0';
                ESP_LOGI(TAG, "Using client ID from client_info: %s", apprtc_client.client_id);
            } else {
                ESP_LOGW(TAG, "No client ID available, using room ID instead");
                int status = webrtcAppCreateAndSendOffer(apprtc_client.room_id);
                if (status != 0) {
                    ESP_LOGE(TAG, "Failed to create and send offer: %d", status);
                } else {
                    ESP_LOGI(TAG, "Offer sent successfully");
                }
            }
        }

        if (apprtc_client.client_id[0] != '\0') {
            // Use the app_webrtc.c function to create and send an offer
            // Use the client ID designated by the server
            ESP_LOGI(TAG, "Using client ID: %s for offer", apprtc_client.client_id);
            int status = webrtcAppCreateAndSendOffer(apprtc_client.client_id);
            if (status != 0) {
                ESP_LOGE(TAG, "Failed to create and send offer: %d", status);
            } else {
                ESP_LOGI(TAG, "Offer sent successfully");
            }
        }
    } else {
        // For non-initiator, we should have already processed the initial offer and ICE candidates
        // during the apprtc_signaling_connect function. If we received and processed an offer,
        // the WebRTC stack should automatically generate and send an answer.
        ESP_LOGI(TAG, "This client is not the initiator - already processed offer");
    }

    return;

schedule_reconnect:
    // Schedule reconnection
    if (apprtc_client.reconnect_timer != NULL) {
        ESP_LOGI(TAG, "Scheduling reconnection attempt");
        esp_timer_start_once(apprtc_client.reconnect_timer, 2000 * 1000); // 2 seconds
    }
}

// Function to send customized data
static int send_customized_data(wss_client_t *wss, esp_peer_signaling_msg_t *msg)
{
    if (msg->data[msg->size] != 0) {
        ESP_LOGW(TAG, "Not a valid string");
        return -1;
    }
    cJSON *json = cJSON_CreateObject();
    cJSON *msg_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "send");
    cJSON_AddStringToObject(msg_obj, "type", "customized");
    cJSON_AddStringToObject(msg_obj, "data", (char *)msg->data);
    char *msg_body = cJSON_PrintUnformatted(msg_obj);
    if (msg_body) {
        cJSON_AddStringToObject(json, "msg", msg_body);
    }
    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    cJSON_Delete(msg_obj);
    int ret = 0;
    if (payload) {
        ESP_LOGI(TAG, "send to remote : %s", payload);
        ret = esp_websocket_client_send_text(wss->ws, payload, strlen(payload), portMAX_DELAY);
        free(payload);
    }
    free(msg_body);
    return ret > 0 ? 0 : -1;
}

// Update the send_custom_command function to use send_customized_data
static esp_err_t send_custom_command(esp_websocket_client_handle_t ws, const char *cmd)
{
    wss_client_t wss = {
        .ws = ws,
        .connected = 1
    };

    esp_peer_signaling_msg_t msg = {
        .data = (uint8_t *)cmd,
        .size = strlen(cmd)
    };

    return send_customized_data(&wss, &msg) == 0 ? ESP_OK : ESP_FAIL;
}

// Public API function to send custom commands
esp_err_t apprtc_signaling_send_custom_command(const char *cmd)
{
    if (!cmd || apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    return send_custom_command(apprtc_client.client, cmd);
}

// Function to process initial messages (offer and ICE candidates) from the server
static esp_err_t process_initial_messages(void)
{
    if (!apprtc_client.client_info.msg_json || !cJSON_IsArray(apprtc_client.client_info.msg_json)) {
        ESP_LOGW(TAG, "No initial messages to process");
        return ESP_OK;
    }

    // CRITICAL: Ensure message queue is initialized before processing messages that might trigger callbacks
    esp_err_t init_ret = ensure_apprtc_client_initialized();
    if (init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppRTC client in process_initial_messages");
        return init_ret;
    }

    int message_count = cJSON_GetArraySize(apprtc_client.client_info.msg_json);
    ESP_LOGI(TAG, "Processing %d initial messages", message_count);

    // Process each message in the array
    for (int i = 0; i < message_count; i++) {
        cJSON *message_item = cJSON_GetArrayItem(apprtc_client.client_info.msg_json, i);
        if (!message_item || !cJSON_IsString(message_item)) {
            ESP_LOGW(TAG, "Message %d is not a string", i);
            continue;
        }

        const char *message_str = message_item->valuestring;
        size_t message_len = strlen(message_str);

        ESP_LOGI(TAG, "Processing initial message %d: %.*s", i, (int)message_len, message_str);

        // Parse the message JSON
        cJSON *message_json = cJSON_Parse(message_str);
        if (!message_json) {
            ESP_LOGW(TAG, "Failed to parse message %d as JSON", i);
            continue;
        }

        // Check the message type
        cJSON *type = cJSON_GetObjectItem(message_json, "type");
        if (type && cJSON_IsString(type)) {
            const char *type_str = type->valuestring;
            ESP_LOGI(TAG, "Message %d type: %s", i, type_str);

            // Handle offer message
            if (strcmp(type_str, "offer") == 0) {
                ESP_LOGI(TAG, "Received initial offer");

                // Forward to the registered message handler
                if (apprtc_client.message_handler) {
                    apprtc_client.message_handler(message_str, message_len, apprtc_client.user_data);
                } else {
                    ESP_LOGE(TAG, "No message handler registered for initial offer");
                }
            }
            // Handle answer message
            else if (strcmp(type_str, "answer") == 0) {
                ESP_LOGI(TAG, "Received initial answer");

                // Forward to the registered message handler
                if (apprtc_client.message_handler) {
                    apprtc_client.message_handler(message_str, message_len, apprtc_client.user_data);
                } else {
                    ESP_LOGE(TAG, "No message handler registered for initial answer");
                }
            }
            // Handle ICE candidate message
            else if (strcmp(type_str, "candidate") == 0) {
                ESP_LOGI(TAG, "Received initial ICE candidate");

                // Forward to the registered message handler
                if (apprtc_client.message_handler) {
                    apprtc_client.message_handler(message_str, message_len, apprtc_client.user_data);
                } else {
                    ESP_LOGE(TAG, "No message handler registered for initial ICE candidate");
                }
            } else {
                ESP_LOGI(TAG, "Ignoring non-WebRTC initial message type: %s", type_str);
            }
        }

        cJSON_Delete(message_json);
    }

    return ESP_OK;
}

/**
 * @brief Queue a message to be sent when the signaling channel is connected
 *
 * @param data The message data to queue
 * @param len The length of the message data
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_queue_message(const void *data, int len)
{
    // Ensure early initialization
    esp_err_t init_err = ensure_apprtc_client_initialized();
    if (init_err != ESP_OK) {
        return init_err;
    }

    return message_queue_add(&apprtc_client.message_queue, data, len);
}

/**
 * @brief Process any queued messages that were stored while disconnected
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_process_queued_messages(void)
{
    esp_err_t ret = ESP_OK;
    int processed = 0;

    // Check if we're connected
    if (apprtc_client.state != APPRTC_SIGNALING_STATE_CONNECTED ||
        !apprtc_client.client ||
        !esp_websocket_client_is_connected(apprtc_client.client)) {
        ESP_LOGW(TAG, "Cannot process queued messages - not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Process all queued messages
    while (!message_queue_is_empty(&apprtc_client.message_queue)) {
        void *data = NULL;
        int len = 0;

        // Get the next message
        if (message_queue_get_next(&apprtc_client.message_queue, &data, &len) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get message from queue");
            ret = ESP_FAIL;
            break;
        }


        // Process the message
        ESP_LOGI(TAG, "Processing queued message %d", processed + 1);

        signaling_msg_t *signalingMessage = (signaling_msg_t *)data;
        // Call the send callback directly with the message data
        int result = apprtc_signaling_send_callback(signalingMessage);
        if (result != 0) {
            ESP_LOGW(TAG, "Failed to process queued message %d, result: %d", processed + 1, result);
            ret = ESP_FAIL;
        } else {
            processed++;
        }

        // Free the message data
        free(data);
    }

    ESP_LOGI(TAG, "Processed %d queued messages", processed);
    return ret;
}

// Early initialization to ensure critical components are ready
static esp_err_t ensure_apprtc_client_initialized(void)
{
    static bool initialized = false;

    if (initialized) {
        // Double-check that the message queue is still valid
        if (apprtc_client.message_queue.mutex == NULL) {
            ESP_LOGE(TAG, "Message queue mutex became NULL after initialization!");
            initialized = false; // Reset to try again
        } else {
            return ESP_OK;
        }
    }

    ESP_LOGI(TAG, "Ensuring AppRTC client initialization...");

    // Initialize message queue early to prevent crashes
    if (apprtc_client.message_queue.mutex == NULL) {
        ESP_LOGI(TAG, "Message queue not initialized, initializing now...");
        esp_err_t err = message_queue_init(&apprtc_client.message_queue);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize message queue early: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Message queue initialized early successfully");
    } else {
        ESP_LOGI(TAG, "Message queue already initialized (mutex: %p)", apprtc_client.message_queue.mutex);
    }

    initialized = true;
    ESP_LOGI(TAG, "AppRTC client initialization check completed");
    return ESP_OK;
}
