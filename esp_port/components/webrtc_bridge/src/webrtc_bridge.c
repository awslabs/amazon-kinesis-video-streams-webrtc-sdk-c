/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_work_queue.h"
#include "message_utils.h"
#include "webrtc_bridge.h"

#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
#if defined(ENABLE_SIGNALLING_ONLY)
#include "network_coprocessor.h"
#endif
#endif

#define RECEIVED_MSG_BUF_SIZE (10 * 1024)

#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
static SemaphoreHandle_t mutex;

#define RPC_USER_SPECIFIC_EVENT_DATA_SIZE (1024) // Not > 4K
#if ENABLE_SIGNALLING_ONLY
typedef struct custom_rpc_data_slave_to_host {
    int32_t resp; /* unused */
    int32_t uuid;
    int32_t int_2; /* is_fin */
    uint32_t seq_num;
    uint32_t total_len;
    uint16_t data_len;
    uint8_t data[RPC_USER_SPECIFIC_EVENT_DATA_SIZE];
} custom_rpc_data_t;
static custom_rpc_data_t custom_send_data;
#elif ENABLE_STREAMING_ONLY && CONFIG_ESP_HOSTED_ENABLED
#include "rpc_wrap.h"
// typedef struct {
//   int32_t int_1; /* uuid */
//   int32_t int_2; /* unused */
//   uint32_t uint_1; /* seq_num */
//   uint32_t uint_2; /* total_len */
//   uint16_t data_len;
//   uint8_t data[RPC_USER_SPECIFIC_EVENT_DATA_SIZE];
// } rpc_usr_t;
static rpc_usr_t req = {0};
static rpc_usr_t resp = {0};
#endif
// static custom_rpc_data_t custom_recv_data;
#else
#include "mqtt_client.h"
#define BROKER_URI "mqtt://mqtt.eclipseprojects.io"

#define SIGNALING_TOPIC "signal"
#define STREAMING_TOPIC "stream"

/* Signalling and streaming TO/FROM topics are flipped */
#if ENABLE_SIGNALLING_ONLY
#define TO_TOPIC STREAMING_TOPIC
#define FROM_TOPIC SIGNALING_TOPIC
#else
#define TO_TOPIC SIGNALING_TOPIC
#define FROM_TOPIC STREAMING_TOPIC
#endif

#define MAX_MQTT_MSG_SIZE (RECEIVED_MSG_BUF_SIZE)
// static char mqtt_message[MAX_MQTT_MSG_SIZE];

static esp_mqtt_client_handle_t g_mqtt_client;
#endif

static const char *TAG = "webrtc_bridge";

/* Callback for when we gather a received message from other side */
void on_webrtc_bridge_msg_received(const void *data, int len);

/* Function pointer for the message handler */
static webrtc_bridge_msg_cb_t message_handler = NULL;

/* Register message handler function */
void webrtc_bridge_register_handler(webrtc_bridge_msg_cb_t handler)
{
    message_handler = handler;
}

typedef struct send_msg {
    char *buf;
    int size;
} send_msg_t;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void handle_on_message_received(void *priv_data)
{
    received_msg_t *received_msg = (received_msg_t *) priv_data;

    // Use the registered handler if available, otherwise use the default
    if (message_handler) {
        message_handler((const void *)received_msg->buf, received_msg->data_size);
    } else {
        on_webrtc_bridge_msg_received((void *) received_msg->buf, received_msg->data_size);
    }

    /* Done! Free the buffer now */
    free(received_msg->buf);
    free(received_msg);
}

static void webrtc_bridge_send_via_hosted(const char *data, int len);

static void send_message_via_hosted_handler(void *priv_data)
{
    send_msg_t *send_msg = (send_msg_t *) priv_data;
    webrtc_bridge_send_via_hosted(send_msg->buf, send_msg->size);

    /* Done! Free the buffer now */
    free(send_msg->buf);
    free(send_msg);
}

#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
#if ENABLE_SIGNALLING_ONLY
extern void send_event_data_to_host(int event_id, void *data, int size);
#elif ENABLE_STREAMING_ONLY && CONFIG_ESP_HOSTED_ENABLED
static void usr_evt_cb(uint8_t usr_evt_num, rpc_usr_t *usr_evt)
{
    if (!usr_evt)
        return;

    /* This function is thread safe. No need for locks */
    esp_err_t append_ret = ESP_FAIL;
    static received_msg_t *received_msg = NULL;

    switch(usr_evt_num) {
        case 1: /* Intended fall-through */
        case 2: /* Intended fall-through */
        case 3: /* Intended fall-through */
        case 4: /* Intended fall-through */
        case 5: /* Intended fall-through */
            // ESP_LOGI(TAG, "==> Recvd custom RPC Event[%u]: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
            //  usr_evt_num, usr_evt->int_1,
            //  usr_evt->int_2, usr_evt->uint_1,
            //  usr_evt->uint_2, usr_evt->data_len, usr_evt->data);

            if (!received_msg) {
                if (usr_evt->uint_1 == 0) {
                    received_msg = esp_webrtc_create_buffer_for_msg(usr_evt->uint_2);
                }
                if (!received_msg) {
                    ESP_LOGE(TAG, "Memory issue or wrong seq number");
                    return;
                }
            }

            bool is_fin = usr_evt->int_2;
            append_ret = esp_webrtc_append_msg_to_existing(received_msg, usr_evt->data, usr_evt->data_len, is_fin);
            if (append_ret == ESP_OK) {
                /* Process the message after switch case. */
            } else if (append_ret == ESP_FAIL) {
                ESP_LOGW(TAG, "Failed to put the message into buffer...");
                free(received_msg->buf);
                free(received_msg);
                received_msg = NULL;
            } else {
                ESP_LOGW(TAG, "Waiting for the next part...");
            }

        break;

        default:
            ESP_LOGI(TAG, "Unhandled usr evt[%u]", usr_evt_num);
            return;
    }

    if (append_ret == ESP_OK) {
        /* Process the message now */
        esp_work_queue_add_task(&handle_on_message_received, (void *) received_msg);
        received_msg = NULL;
    }
    /* Do not free usr_evt, as it is already handled internally */
}
#endif

static void webrtc_bridge_send_via_hosted(const char *data, int len)
{
    int32_t uuid = rand();
    int len_remain = len;
    int seq_num = 0;
#if ENABLE_SIGNALLING_ONLY
#define MAX_CHUNK_LEN  (1024)
#else
#define MAX_CHUNK_LEN  (1000)
#endif

    xSemaphoreTake(mutex, 5000);
    int32_t data_len = MAX_CHUNK_LEN;
    int32_t data_idx = 0;
#if ENABLE_SIGNALLING_ONLY  // Slave (C6) --> Host (P4)
    // RPC_ID__Event_USR1 = 778... BAD hacks?
    custom_send_data.uuid = uuid;
    custom_send_data.total_len = len;
    custom_send_data.int_2 = 0;
    while (len_remain > 0) {
        if (len_remain <= MAX_CHUNK_LEN) {
            data_len = len_remain;
            custom_send_data.int_2 = 1;
        }
        custom_send_data.seq_num = seq_num;
        custom_send_data.data_len = data_len;
        memcpy(custom_send_data.data, data + data_idx, data_len);
        // Send the event now...
        send_event_data_to_host(778, (void *) &custom_send_data, sizeof(custom_rpc_data_t));
        len_remain -= data_len;
        data_idx += data_len;
        seq_num++;
    }
#elif ENABLE_STREAMING_ONLY && CONFIG_ESP_HOSTED_ENABLED // Host (P4) --> Slave (C6)
    req.int_1 = uuid;
    req.uint_2 = len;
    req.int_2 = 0; /* is_fin */
    while (len_remain > 0) {
        if (len_remain <= MAX_CHUNK_LEN) {
            data_len = len_remain;
            req.int_2 = 1; /* is_fin */
        }
        req.uint_1 = seq_num;
        req.data_len = data_len;
        memcpy(req.data, data + data_idx, data_len);
        // rpc request encopassing our data
        // send_event_data_to_host(778, custom_send_data, sizeof(custom_rpc_data_t));
        rpc_send_usr_request(1, &req, &resp);
        len_remain -= data_len;
        data_idx += data_len;
        seq_num++;
    }
#else
    (void) len_remain;
    (void) seq_num;
    (void) data_len;
    (void) data_idx;
#endif
    xSemaphoreGive(mutex);
}
#endif

void webrtc_bridge_send_message(const char *data, int len)
{
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
    send_msg_t *send_msg = calloc(1, sizeof(send_msg_t));
    if (!send_msg) {
        ESP_LOGE(TAG, "Failed to allocate memory for send_msg");
        free(data);
        return;
    }
    send_msg->buf = data;
    send_msg->size = len;

    /* free delegated to send_message_via_hosted_handler */
    if (esp_work_queue_add_task(&send_message_via_hosted_handler, (void *) send_msg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add task to work queue");
        free(data);
        free(send_msg);
    }
#else
    int msg_id = esp_mqtt_client_publish(g_mqtt_client, TO_TOPIC, data, len, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    free(data);
#endif
}

#ifndef CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, FROM_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (message_handler) {
            message_handler(event->data, event->data_len);
        } else {
            on_webrtc_bridge_msg_received((void *) event->data, event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
#endif

// Default implementation for when no handler is registered
void on_webrtc_bridge_msg_received(const void *data, int len)
{
    /* Forward the received WebRTC message to the registered handler */
    if (message_handler) {
        ESP_LOGD(TAG, "Forwarding WebRTC message to registered handler (len: %d)", len);
        message_handler((const char *) data, len);
    } else {
        ESP_LOGW(TAG, "Message received but no handler registered (len: %d)", len);
    }
}

void webrtc_bridge_start(void)
{
    static bool init_done = false;
    if (init_done) {
        ESP_LOGI(TAG, "webrtc_bridge already started");
        return;
    }
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
    mutex = xSemaphoreCreateMutex();
#if ENABLE_SIGNALLING_ONLY
    /* Register our message handling function with the network coprocessor */
    network_coprocessor_register_webrtc_callback(&on_webrtc_bridge_msg_received);
    ESP_LOGI(TAG, "WebRTC bridge registered with network coprocessor");
#elif ENABLE_STREAMING_ONLY && CONFIG_ESP_HOSTED_ENABLED
    rpc_register_usr_event_callback(usr_evt_cb);
#endif
#else
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URI,
        .buffer.size = MAX_MQTT_MSG_SIZE,
        .buffer.out_size = MAX_MQTT_MSG_SIZE,
        .outbox.limit = 2 * MAX_MQTT_MSG_SIZE,
        .task.stack_size = (10 * 1024), // 6K is insufficient
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    g_mqtt_client = client;
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
#endif
    init_done = true;
}
