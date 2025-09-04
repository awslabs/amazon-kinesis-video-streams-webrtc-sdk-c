#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_cli.h"
#include "app_storage.h"

#include "media_stream.h"
#include "signaling_serializer.h"
#include "webrtc_bridge.h"
#include "webrtc_bridge_signaling.h"
#include "esp_work_queue.h"
#include "app_webrtc.h"
#include "kvs_peer_connection.h"

extern esp_err_t esp_hosted_wait_for_slave(void);
extern int sleep_command_register_cli();

static const char *TAG = "streaming_only";

// Data channel callback context
typedef struct {
    char peer_id[64];
} data_channel_context_t;

// Global data channel context for active peer
static data_channel_context_t g_data_channel_ctx;

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
static char wifi_ip[72];

// Data channel callbacks
static void on_data_channel_open(uint64_t custom_data, void *p_data_channel, const char *peer_id)
{
    data_channel_context_t *ctx = (data_channel_context_t *)(uintptr_t)custom_data;

    ESP_LOGI(TAG, "on_data_channel_open called with custom_data: 0x%llx", (unsigned long long)custom_data);
    ESP_LOGI(TAG, "p_data_channel: %p, peer_id: %s", p_data_channel, peer_id);

    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid data channel context (NULL) in open callback");
        return;
    }

    // Store the peer_id in our context
    if (peer_id != NULL) {
        strncpy(ctx->peer_id, peer_id, sizeof(ctx->peer_id) - 1);
        ctx->peer_id[sizeof(ctx->peer_id) - 1] = '\0';
    }

    ESP_LOGI(TAG, "Data channel opened for peer: %s", ctx->peer_id);

    // Send a welcome message when the channel opens
    const char *welcome_msg = "Hello from ESP32! Data channel is now open. Send a message and I'll echo it back.";
    ESP_LOGI(TAG, "Sending welcome message to peer: %s", ctx->peer_id);

    WEBRTC_STATUS status = app_webrtc_send_data_channel_message(
        ctx->peer_id,
        p_data_channel,
        false,
        (const uint8_t *)welcome_msg,
        strlen(welcome_msg)
    );

    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to send welcome message: 0x%08x", status);
    } else {
        ESP_LOGI(TAG, "Successfully sent welcome message");
    }
}

static void on_data_channel_message(uint64_t custom_data, void *p_data_channel, const char *peer_id,
                                    bool is_binary, uint8_t *p_message, uint32_t message_len)
{
    data_channel_context_t *ctx = (data_channel_context_t *)(uintptr_t)custom_data;

    ESP_LOGI(TAG, "on_data_channel_message called with custom_data: 0x%llx", (unsigned long long)custom_data);
    ESP_LOGI(TAG, "p_data_channel: %p, peer_id: %s, is_binary: %d, p_message: %p, message_len: %" PRIu32,
             p_data_channel, peer_id, (int) is_binary, p_message, message_len);

    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid data channel context (NULL) in message callback");
        return;
    }

    if (p_message == NULL) {
        ESP_LOGE(TAG, "Invalid message pointer (NULL) in message callback");
        return;
    }

    // Store or update the peer_id in our context
    if (peer_id != NULL) {
        strncpy(ctx->peer_id, peer_id, sizeof(ctx->peer_id) - 1);
        ctx->peer_id[sizeof(ctx->peer_id) - 1] = '\0';
    }

    ESP_LOGI(TAG, "Data channel message received from peer %s: %.*s (binary: %s)",
             peer_id, (int) message_len, p_message, is_binary ? "yes" : "no");

    // Echo the message back
    ESP_LOGI(TAG, "Echoing message back to peer: %s", peer_id);
    WEBRTC_STATUS status = app_webrtc_send_data_channel_message(peer_id, p_data_channel, is_binary, p_message, message_len);

    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to echo message back: 0x%08x", status);
    } else {
        ESP_LOGI(TAG, "Successfully echoed message back");
    }
}

#define GOT_IP_BIT BIT0

// WiFi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
#if 0
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else
#endif

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        memset(wifi_ip, 0, sizeof(wifi_ip)/sizeof(wifi_ip[0]));
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        memcpy(wifi_ip, &event->ip_info.ip, 4);
        xEventGroupSetBits(s_wifi_event_group, GOT_IP_BIT);
    }
}

char* esp_get_ip(void)
{
    return wifi_ip;
}

#ifdef CONFIG_HOST_USES_STATIC_NETIF
esp_netif_t *create_slave_sta_netif_with_static_ip(void)
{
    ESP_LOGI(TAG, "Create netif with static IP");
     /* Create "almost" default station, but with un-flagged DHCP client */
    esp_netif_inherent_config_t netif_cfg;
    memcpy(&netif_cfg, ESP_NETIF_BASE_DEFAULT_WIFI_STA, sizeof(netif_cfg));
    netif_cfg.flags &= ~ESP_NETIF_DHCP_CLIENT;
    esp_netif_config_t cfg_sta = {
        .base = &netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    esp_netif_t *sta_netif = esp_netif_new(&cfg_sta);
    assert(sta_netif);

    ESP_LOGI(TAG, "Creating slave sta netif with static IP");

    /* stop dhcpc */
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));

    return sta_netif;
}
#endif

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#ifdef CONFIG_HOST_USES_STATIC_NETIF
    create_slave_sta_netif_with_static_ip();
#else
    esp_netif_create_default_wifi_sta();
#endif

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_hosted_wait_for_slave());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            GOT_IP_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(20000));

    if (bits & GOT_IP_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
    }
}

static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Initialized.");
            break;
        case APP_WEBRTC_EVENT_DEINITIALIZING:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Deinitialized.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connection Requested.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connected: %s", event_data->peer_id);

            // Update data channel context with the connected peer ID
            strncpy(g_data_channel_ctx.peer_id, event_data->peer_id, sizeof(g_data_channel_ctx.peer_id) - 1);
            g_data_channel_ctx.peer_id[sizeof(g_data_channel_ctx.peer_id) - 1] = '\0';
            ESP_LOGI(TAG, "Updated data channel context with peer ID: %s", g_data_channel_ctx.peer_id);

            // Note: Data channel callbacks are now registered early in app_main
            // No need to register them here again
            break;
        case APP_WEBRTC_EVENT_PEER_DISCONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Disconnected: %s", event_data->peer_id);

            // Clear data channel context if this is our active peer
            if (strcmp(g_data_channel_ctx.peer_id, event_data->peer_id) == 0) {
                memset(&g_data_channel_ctx, 0, sizeof(g_data_channel_ctx));
            }
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Started for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STOPPED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Stopped for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "[KVS Event] Received Offer.");
            break;
        case APP_WEBRTC_EVENT_SENT_ANSWER:
            ESP_LOGI(TAG, "[KVS Event] Sent Answer.");
            break;
        case APP_WEBRTC_EVENT_ERROR:
            /* fall-through */
        case APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED:
            ESP_LOGE(TAG, "[KVS Event] Error Event %d: Code %d, Message: %s",
                     (int) event_data->event_id, (int) event_data->status_code, event_data->message);
            break;
        default:
            ESP_LOGI(TAG, "[KVS Event] Unhandled Event ID: %d", (int) event_data->event_id);
            break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    WEBRTC_STATUS status;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32 WebRTC Streaming Example");

    esp_cli_start();

    /* Register deep sleep command */
    sleep_command_register_cli();

    // Initialize WiFi
    wifi_init_sta();

    app_storage_init();

    // Initialize signaling serializer
    signaling_serializer_init();

    // Register the event callback *before* init to catch all events
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback.");
    }

    // Get media interfaces for streaming (capture for sending, player for receiving)
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Configure WebRTC with our new simplified API - streaming-only mode
    app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();

    // Set up bridge signaling interface and config
    bridge_signaling_config_t bridge_config = {
        .client_id = "streaming_client",
        .log_level = 2
    };

    // Essential configuration - signaling interface
    app_webrtc_config.signaling_client_if = getBridgeSignalingClientInterface();
    app_webrtc_config.signaling_cfg = &bridge_config;

    // Enable pluggable peer connection using KVS implementation
    app_webrtc_config.peer_connection_if = kvs_peer_connection_if_get();

    // Media interfaces for bi-directional streaming
    app_webrtc_config.video_capture = video_capture;
    app_webrtc_config.audio_capture = audio_capture;
    app_webrtc_config.video_player = video_player;
    app_webrtc_config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC streaming-only device with simplified API:");
    ESP_LOGI(TAG, "  - Mode: streaming-only (no signaling, receives via bridge)");
    ESP_LOGI(TAG, "  - Media type: auto-detected (audio+video from interfaces)");
    ESP_LOGI(TAG, "  - Streaming: bi-directional (can send and receive media)");
    ESP_LOGI(TAG, "  - Bridge: communicates with signaling device");
    ESP_LOGI(TAG, "  - Role: MASTER (default - optimized for streaming)");

    // Initialize WebRTC application with simplified API
    status = app_webrtc_init(&app_webrtc_config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, (uint32_t) status);
        goto CleanUp;
    }

    // Enable media reception for bi-directional streaming
    app_webrtc_enable_media_reception(true);

    // Initialize data channel context
    memset(&g_data_channel_ctx, 0, sizeof(g_data_channel_ctx));
    strcpy(g_data_channel_ctx.peer_id, "default");  // Will be updated when peer connects

    // Register data channel callbacks early - they will be used for any peer that connects
    ESP_LOGI(TAG, "Registering data channel callbacks early");
    status = app_webrtc_set_data_channel_callbacks(
        "default",  // Will be updated when actual peer connects
        on_data_channel_open,
        on_data_channel_message,
        (uint64_t)&g_data_channel_ctx
    );

    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Early callback registration pending - callbacks will be applied when peers connect (status: 0x%08" PRIx32 ")", (uint32_t) status);
        // Non-fatal, continue initialization
    } else {
        ESP_LOGI(TAG, "Data channel callbacks registered early successfully");
    }

    // Start webrtc bridge
    webrtc_bridge_start();

    ESP_LOGI(TAG, "Streaming example initialized, waiting for signaling messages");

    // Run WebRTC application
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, (uint32_t) status);
        goto CleanUp;
    }

CleanUp:
    // Do not terminate the WebRTC application in streaming-only mode
    // Only streaming sessions are created and destroyed internally
    // app_webrtc_terminate();
}
