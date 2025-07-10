/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <esp_event.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ESP_WORKQ_SIZE           48
#define ESP_WORKQ_TASK_STACK     (16 * 1024)
#define ESP_WORKQ_TASK_PRIO      (8)

#define ESP_WORK_QUEUE_CONFIG_DEFAULT() { \
    .size = ESP_WORKQ_SIZE, \
    .stack_size = ESP_WORKQ_TASK_STACK, \
    .priority = ESP_WORKQ_TASK_PRIO, \
    .prefer_ext_ram = true, \
}

typedef struct {
    uint32_t size;              // Max number of tasks to queue
    uint32_t stack_size;        // Stack size for the task
    uint32_t priority;          // Priority of the task
    bool prefer_ext_ram;        // Whether to prefer external RAM for the task
} esp_work_queue_config_t;

/** Prototype for ESP RainMaker Work Queue Function
 *
 * @param[in] priv_data The private data associated with the work function.
 */
typedef void (*esp_work_fn_t)(void *priv_data);

/** Initializes the Work Queue
 *
 * This initializes the work queue, which is basically a mechanism to run
 * tasks in the context of a dedicated thread. You can start queueing tasks
 * after this, but they will get executed only after calling
 * esp_work_queue_start().
 *
 * Same as esp_work_queue_init_with_config(ESP_WORK_QUEUE_CONFIG_DEFAULT())
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_init(void);

/** Initializes the Work Queue with a custom configuration
 *
 * This initializes the work queue, which is basically a mechanism to run
 * tasks in the context of a dedicated thread. You can start queueing tasks
 * after this, but they will get executed only after calling
 * esp_work_queue_start().
 *
 * @param[in] config The configuration for the work queue.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_init_with_config(esp_work_queue_config_t *config);

/** De-initialize the Work Queue
 *
 * This de-initializes the work queue. Note that the work queue needs to
 * be stopped using esp_work_queue_stop() before calling this.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_deinit(void);

/** Start the Work Queue
 *
 * This starts the Work Queue thread which then starts executing the tasks queued.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_start(void);

/** Stop the Work Queue
 *
 * This stops a running Work Queue.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_stop(void);

/** Queue execution of a function in the Work Queue's context
 *
 * This API queues a work function for execution in the Work Queue Task's context.
 *
 * @param[in] work_fn The Work function to be queued.
 * @param[in] priv_data Private data to be passed to the work function.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_work_queue_add_task(esp_work_fn_t work_fn, void *priv_data);

#ifdef __cplusplus
}
#endif
