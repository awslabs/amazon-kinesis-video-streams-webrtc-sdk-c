/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Maximum number of queued messages when not connected
#define MAX_QUEUED_MESSAGES 10

// Structure to hold a queued message
typedef struct {
    void *data;
    int len;
} queued_message_t;

// Message queue structure
typedef struct {
    queued_message_t messages[MAX_QUEUED_MESSAGES];
    int count;
    SemaphoreHandle_t mutex;
} message_queue_t;

/**
 * @brief Initialize a message queue
 *
 * @param queue Pointer to the message queue to initialize
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t message_queue_init(message_queue_t *queue);

/**
 * @brief Clean up a message queue and free all resources
 *
 * @param queue Pointer to the message queue to clean up
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t message_queue_cleanup(message_queue_t *queue);

/**
 * @brief Add a message to the queue
 *
 * @param queue Pointer to the message queue
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t message_queue_add(message_queue_t *queue, const void *data, int len);

/**
 * @brief Get the next message from the queue
 *
 * @param queue Pointer to the message queue
 * @param data Pointer to store the message data (caller must free)
 * @param len Pointer to store the message length
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t message_queue_get_next(message_queue_t *queue, void **data, int *len);

/**
 * @brief Check if the queue is empty
 *
 * @param queue Pointer to the message queue
 * @return true if the queue is empty, false otherwise
 */
bool message_queue_is_empty(message_queue_t *queue);

/**
 * @brief Get the number of messages in the queue
 *
 * @param queue Pointer to the message queue
 * @return int Number of messages in the queue
 */
int message_queue_get_count(message_queue_t *queue);

#endif /* MESSAGE_QUEUE_H */
