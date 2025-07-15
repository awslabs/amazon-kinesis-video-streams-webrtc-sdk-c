/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <esp_console.h>
#include <esp_log.h>
#include <sdkconfig.h>

#include "apprtc_signaling.h"
#include "app_webrtc.h"
#include "esp_work_queue.h"

static const char *TAG = "webrtc_cli";

// Function to get the current role type as a string
static const char* get_role_type_str(void)
{
#if CONFIG_APPRTC_ROLE_TYPE == 0
    return "MASTER";
#else
    return "VIEWER";
#endif
}

// Structure to pass room joining data to the work queue task
typedef struct {
    char room_id[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    bool is_new_room;
} join_room_task_data_t;

// Work queue task function for joining a room
static void join_room_task(void *priv_data)
{
    join_room_task_data_t *task_data = (join_room_task_data_t *)priv_data;
    if (!task_data) {
        ESP_LOGE(TAG, "Invalid task data");
        return;
    }

    const char *room_id = NULL;
    if (!task_data->is_new_room) {
        room_id = task_data->room_id;
        ESP_LOGI(TAG, "[Work Queue] Joining room: %s (Role: %s)", room_id, get_role_type_str());
    } else {
        ESP_LOGI(TAG, "[Work Queue] Creating a new room (Role: %s)", get_role_type_str());
    }

    esp_err_t err = apprtc_signaling_connect(room_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[Work Queue] Failed to join/create room: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[Work Queue] Room join/create request sent successfully");
    }

    // Free the task data
    free(task_data);
}

static int webrtc_join_room_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    if (argc != 2) {
        printf("Usage: join-room <room_id>\n");
        printf("       join-room new     (to create a new room)\n");
        return 0;
    }

    // Allocate task data for the work queue
    join_room_task_data_t *task_data = calloc(1, sizeof(join_room_task_data_t));
    if (!task_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for task data");
        printf("Failed to allocate memory for task data\n");
        return 0;
    }

    if (strcmp(argv[1], "new") != 0) {
        // Copy room ID to task data
        strncpy(task_data->room_id, argv[1], MAX_SIGNALING_CLIENT_ID_LEN);
        task_data->room_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        task_data->is_new_room = false;

        ESP_LOGI(TAG, "Joining room: %s (Role: %s)", task_data->room_id, get_role_type_str());
        printf("Joining room %s as %s...\n", task_data->room_id, get_role_type_str());
        printf("This will connect to the room and process any initial offer/ICE candidates.\n");
    } else {
        task_data->is_new_room = true;
        ESP_LOGI(TAG, "Creating a new room (Role: %s)", get_role_type_str());
        printf("Creating a new room as %s...\n", get_role_type_str());
    }

    // Add the task to the work queue
    esp_err_t err = esp_work_queue_add_task(join_room_task, task_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add join room task to work queue: %s", esp_err_to_name(err));
        printf("Failed to add join room task to work queue: %s\n", esp_err_to_name(err));
        free(task_data);
        return 0;
    }

    printf("Room join/create request queued successfully.\n");
    printf("Check the logs for connection progress and WebRTC events.\n");

    // If we're joining an existing room, remind the user that we'll process the offer
    if (!task_data->is_new_room) {
        printf("When joining an existing room, the initial offer and ICE candidates will be processed automatically.\n");
    }

    return 0;
}

static int webrtc_get_room_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    const char* room_id = apprtc_signaling_get_room_id();
    if (room_id != NULL && room_id[0] != '\0') {
        printf("Current room ID: %s\n", room_id);
    } else {
        printf("Not connected to any room\n");
    }

    return 0;
}

// Work queue task function for disconnecting from a room
static void disconnect_room_task(void *priv_data)
{
    ESP_LOGI(TAG, "[Work Queue] Disconnecting from room");

    esp_err_t err = apprtc_signaling_disconnect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[Work Queue] Failed to disconnect from room: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[Work Queue] Disconnected from room successfully");
    }
}

static int webrtc_disconnect_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    // Check if we're connected to a room
    const char* room_id = apprtc_signaling_get_room_id();
    if (room_id == NULL || room_id[0] == '\0') {
        printf("Not connected to any room\n");
        return 0;
    }

    // Add the disconnect task to the work queue
    esp_err_t err = esp_work_queue_add_task(disconnect_room_task, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add disconnect task to work queue: %s", esp_err_to_name(err));
        printf("Failed to add disconnect task to work queue: %s\n", esp_err_to_name(err));
        return 0;
    }

    printf("Room disconnect request queued successfully.\n");
    printf("Check the logs for disconnection progress.\n");

    return 0;
}

static int webrtc_get_role_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    printf("Current role type: %s\n", get_role_type_str());
    printf("Role explanation:\n");
    printf("  - MASTER: Creates a room and waits for viewers to connect\n");
    printf("  - VIEWER: Can create a room or join an existing one\n");

    return 0;
}

static int webrtc_status_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    // Get the current room ID
    const char* room_id = apprtc_signaling_get_room_id();

    // Get the current signaling state
    apprtc_signaling_state_t state = apprtc_signaling_get_state();
    const char* state_str = "Unknown";

    switch (state) {
        case APPRTC_SIGNALING_STATE_DISCONNECTED:
            state_str = "Disconnected";
            break;
        case APPRTC_SIGNALING_STATE_CONNECTING:
            state_str = "Connecting";
            break;
        case APPRTC_SIGNALING_STATE_CONNECTED:
            state_str = "Connected";
            break;
        case APPRTC_SIGNALING_STATE_ERROR:
            state_str = "Error";
            break;
        default:
            state_str = "Unknown";
            break;
    }

    printf("WebRTC Status:\n");
    printf("  Role: %s\n", get_role_type_str());
    printf("  Signaling State: %s\n", state_str);

    if (room_id != NULL && room_id[0] != '\0') {
        printf("  Room ID: %s\n", room_id);
        printf("  Room URL: https://webrtc.espressif.com/r/%s\n", room_id);
    } else {
        printf("  Room ID: Not connected to any room\n");
    }

    return 0;
}

// Work queue task function for retrying room connection
static void retry_room_task(void *priv_data)
{
    join_room_task_data_t *task_data = (join_room_task_data_t *)priv_data;
    if (!task_data) {
        ESP_LOGE(TAG, "Invalid task data");
        return;
    }

    // First disconnect from any existing connection
    ESP_LOGI(TAG, "[Work Queue] Disconnecting from any existing room before retry");
    apprtc_signaling_disconnect();

    // Small delay to ensure disconnect completes
    vTaskDelay(pdMS_TO_TICKS(1000));

    const char *room_id = NULL;
    if (!task_data->is_new_room) {
        room_id = task_data->room_id;
        ESP_LOGI(TAG, "[Work Queue] Retrying connection to room: %s (Role: %s)", room_id, get_role_type_str());
    } else {
        ESP_LOGI(TAG, "[Work Queue] Retrying to create a new room (Role: %s)", get_role_type_str());
    }

    esp_err_t err = apprtc_signaling_connect(room_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[Work Queue] Failed to retry room connection: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[Work Queue] Room connection retry sent successfully");
    }

    // Free the task data
    free(task_data);
}

static int webrtc_retry_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");

    if (argc != 2 && argc != 1) {
        printf("Usage: retry-room [room_id]\n");
        printf("       retry-room        (to retry with the current room ID)\n");
        printf("       retry-room new    (to retry creating a new room)\n");
        return 0;
    }

    // Allocate task data for the work queue
    join_room_task_data_t *task_data = calloc(1, sizeof(join_room_task_data_t));
    if (!task_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for task data");
        printf("Failed to allocate memory for task data\n");
        return 0;
    }

    // If no argument is provided, use the current room ID
    if (argc == 1) {
        const char* current_room_id = apprtc_signaling_get_room_id();
        if (current_room_id != NULL && current_room_id[0] != '\0') {
            strncpy(task_data->room_id, current_room_id, MAX_SIGNALING_CLIENT_ID_LEN);
            task_data->room_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
            task_data->is_new_room = false;

            ESP_LOGI(TAG, "Retrying connection to current room: %s (Role: %s)", task_data->room_id, get_role_type_str());
            printf("Retrying connection to current room %s as %s...\n", task_data->room_id, get_role_type_str());
        } else {
            printf("Not currently connected to any room. Please specify a room ID or 'new'.\n");
            free(task_data);
            return 0;
        }
    } else if (strcmp(argv[1], "new") != 0) {
        // Copy room ID to task data
        strncpy(task_data->room_id, argv[1], MAX_SIGNALING_CLIENT_ID_LEN);
        task_data->room_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        task_data->is_new_room = false;

        ESP_LOGI(TAG, "Retrying connection to room: %s (Role: %s)", task_data->room_id, get_role_type_str());
        printf("Retrying connection to room %s as %s...\n", task_data->room_id, get_role_type_str());
    } else {
        task_data->is_new_room = true;
        ESP_LOGI(TAG, "Retrying to create a new room (Role: %s)", get_role_type_str());
        printf("Retrying to create a new room as %s...\n", get_role_type_str());
    }

    // Add the task to the work queue
    esp_err_t err = esp_work_queue_add_task(retry_room_task, task_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add retry room task to work queue: %s", esp_err_to_name(err));
        printf("Failed to add retry room task to work queue: %s\n", esp_err_to_name(err));
        free(task_data);
        return 0;
    }

    printf("Room connection retry request queued successfully.\n");
    printf("Check the logs for connection progress and WebRTC events.\n");

    return 0;
}

static esp_console_cmd_t webrtc_cmds[] = {
    {
        .command = "join-room",
        .help = "join-room <room_id> | join-room new",
        .func = webrtc_join_room_cli_handler,
    },
    {
        .command = "get-room",
        .help = "Get the current room ID",
        .func = webrtc_get_room_cli_handler,
    },
    {
        .command = "disconnect",
        .help = "Disconnect from the current room",
        .func = webrtc_disconnect_cli_handler,
    },
    {
        .command = "get-role",
        .help = "Show the current WebRTC role type",
        .func = webrtc_get_role_cli_handler,
    },
    {
        .command = "status",
        .help = "Show WebRTC connection status",
        .func = webrtc_status_cli_handler,
    },
    {
        .command = "retry-room",
        .help = "Retry connecting to a room",
        .func = webrtc_retry_cli_handler,
    }
};

int webrtc_register_cli()
{
    int cmds_num = sizeof(webrtc_cmds) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", webrtc_cmds[i].command);
        esp_console_cmd_register(&webrtc_cmds[i]);
    }
    return 0;
}
