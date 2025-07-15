/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "message_queue.h"

static const char *TAG = "message_queue";

esp_err_t message_queue_init(message_queue_t *queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize mutex
    queue->mutex = xSemaphoreCreateMutex();
    if (queue->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize queue
    queue->count = 0;
    memset(queue->messages, 0, sizeof(queue->messages));

    ESP_LOGI(TAG, "Message queue initialized");
    return ESP_OK;
}

esp_err_t message_queue_cleanup(message_queue_t *queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(queue->mutex, portMAX_DELAY);

    // Free all message data
    for (int i = 0; i < queue->count; i++) {
        if (queue->messages[i].data != NULL) {
            free(queue->messages[i].data);
            queue->messages[i].data = NULL;
        }
    }
    queue->count = 0;

    xSemaphoreGive(queue->mutex);

    // Delete mutex
    vSemaphoreDelete(queue->mutex);
    queue->mutex = NULL;

    ESP_LOGI(TAG, "Message queue cleaned up");
    return ESP_OK;
}

esp_err_t message_queue_add(message_queue_t *queue, const void *data, int len)
{
    esp_err_t ret = ESP_OK;

    if (queue == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(queue->mutex, portMAX_DELAY);

    // Check if queue is full
    if (queue->count >= MAX_QUEUED_MESSAGES) {
        ESP_LOGE(TAG, "Queue is full, dropping oldest message");

        // Free the oldest message
        if (queue->messages[0].data != NULL) {
            free(queue->messages[0].data);
        }

        // Shift all messages down
        for (int i = 0; i < MAX_QUEUED_MESSAGES - 1; i++) {
            queue->messages[i] = queue->messages[i + 1];
        }

        // Decrement count to make room for new message
        queue->count--;
    }

    // Allocate memory for the message data
    void *message_data = malloc(len);
    if (!message_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for queued message");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    // Copy the message data
    memcpy(message_data, data, len);

    // Add to queue
    int idx = queue->count;
    queue->messages[idx].data = message_data;
    queue->messages[idx].len = len;
    queue->count++;

    ESP_LOGI(TAG, "Message added to queue (%d/%d)", queue->count, MAX_QUEUED_MESSAGES);

cleanup:
    xSemaphoreGive(queue->mutex);
    return ret;
}

esp_err_t message_queue_get_next(message_queue_t *queue, void **data, int *len)
{
    esp_err_t ret = ESP_OK;

    if (queue == NULL || data == NULL || len == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    *data = NULL;
    *len = 0;

    xSemaphoreTake(queue->mutex, portMAX_DELAY);

    // Check if queue is empty
    if (queue->count == 0) {
        ESP_LOGW(TAG, "Queue is empty");
        ret = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }

    // Get the oldest message
    *data = queue->messages[0].data;
    *len = queue->messages[0].len;

    // Shift all messages down
    for (int i = 0; i < queue->count - 1; i++) {
        queue->messages[i] = queue->messages[i + 1];
    }

    // Clear the last entry
    queue->messages[queue->count - 1].data = NULL;
    queue->messages[queue->count - 1].len = 0;

    // Decrement count
    queue->count--;

    ESP_LOGI(TAG, "Message retrieved from queue (%d/%d)", queue->count, MAX_QUEUED_MESSAGES);

cleanup:
    xSemaphoreGive(queue->mutex);
    return ret;
}

bool message_queue_is_empty(message_queue_t *queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue pointer is NULL");
        return true;
    }

    bool is_empty;
    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    is_empty = (queue->count == 0);
    xSemaphoreGive(queue->mutex);

    return is_empty;
}

int message_queue_get_count(message_queue_t *queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue pointer is NULL");
        return 0;
    }

    int count;
    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    count = queue->count;
    xSemaphoreGive(queue->mutex);

    return count;
}
