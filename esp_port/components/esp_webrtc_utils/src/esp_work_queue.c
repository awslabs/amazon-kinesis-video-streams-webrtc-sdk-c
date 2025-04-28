/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_err.h>
#include <esp_log.h>

#include <esp_work_queue.h>

#define ESP_WORKQ_SIZE           32
#define ESP_WORKQ_TASK_STACK     (24 * 1024)
#define ESP_WORKQ_TASK_PRIO      (8)

static const char *TAG = "esp_work_queue";

typedef enum {
    WORK_QUEUE_STATE_DEINIT = 0,
    WORK_QUEUE_STATE_INIT_DONE,
    WORK_QUEUE_STATE_RUNNING,
    WORK_QUEUE_STATE_STOP_REQUESTED,
} esp_work_queue_state_t;

typedef struct {
    esp_work_fn_t work_fn;
    void *priv_data;
} esp_work_queue_entry_t;

static QueueHandle_t work_queue;
static esp_work_queue_state_t queue_state;

static void esp_webrtc_handle_work_queue(void)
{
    esp_work_queue_entry_t work_queue_entry;
    /* 2 sec delay to prevent spinning */
    BaseType_t ret = xQueueReceive(work_queue, &work_queue_entry, 2000 / portTICK_PERIOD_MS);
    while (ret == pdTRUE) {
        work_queue_entry.work_fn(work_queue_entry.priv_data);
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to avoid starvation to other tasks
        ret = xQueueReceive(work_queue, &work_queue_entry, 0);
    }
}

static void esp_work_queue_task(void *param)
{
    ESP_LOGI(TAG, "ESP Work Queue task started.");
    while (queue_state != WORK_QUEUE_STATE_STOP_REQUESTED) {
        esp_webrtc_handle_work_queue();
    }
    ESP_LOGI(TAG, "Stopping Work Queue task");
    queue_state = WORK_QUEUE_STATE_INIT_DONE;
    vTaskDelete(NULL);
}

esp_err_t esp_work_queue_add_task(esp_work_fn_t work_fn, void *priv_data)
{
    if (!work_queue) {
        ESP_LOGE(TAG, "Cannot enqueue function as Work Queue hasn't been created.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_work_queue_entry_t work_queue_entry = {
        .work_fn = work_fn,
        .priv_data = priv_data,
    };

    // The wait time is 0, so the function should return immediately
    if (xQueueSend(work_queue, &work_queue_entry, pdMS_TO_TICKS(0)) == pdTRUE) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Failed to add work task");
    return ESP_FAIL;
}

esp_err_t esp_work_queue_init(void)
{
    if (queue_state != WORK_QUEUE_STATE_DEINIT) {
        ESP_LOGW(TAG, "Work Queue already initialiased/started.");
        return ESP_OK;
    }
    work_queue = xQueueCreate(ESP_WORKQ_SIZE, sizeof(esp_work_queue_entry_t));
    if (!work_queue) {
        ESP_LOGE(TAG, "Failed to create Work Queue.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Work Queue created.");
    queue_state = WORK_QUEUE_STATE_INIT_DONE;
    return ESP_OK;
}

esp_err_t esp_work_queue_deinit(void)
{
    if (queue_state != WORK_QUEUE_STATE_STOP_REQUESTED) {
        esp_work_queue_stop();
    }

    while (queue_state == WORK_QUEUE_STATE_STOP_REQUESTED) {
        ESP_LOGI(TAG, "Waiting for esp_work_queue being stopped...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if (queue_state == WORK_QUEUE_STATE_DEINIT) {
        return ESP_OK;
    } else if (queue_state != WORK_QUEUE_STATE_INIT_DONE) {
        ESP_LOGE(TAG, "Cannot deinitialize Work Queue as the task is still running.");
        return ESP_ERR_INVALID_STATE;
    } else {
        vQueueDelete(work_queue);
        work_queue = NULL;
        queue_state = WORK_QUEUE_STATE_DEINIT;
    }
    ESP_LOGI(TAG, "esp_work_queue was successfully deinitialized");
    return ESP_OK;
}

esp_err_t esp_work_queue_start(void)
{
    if (queue_state == WORK_QUEUE_STATE_RUNNING) {
        ESP_LOGW(TAG, "Work Queue already started.");
        return ESP_OK;
    }
    if (queue_state != WORK_QUEUE_STATE_INIT_DONE) {
        ESP_LOGE(TAG, "Failed to start Work Queue as it wasn't initialized.");
        return ESP_ERR_INVALID_STATE;
    }

    StaticTask_t *task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    // void *task_stack = heap_caps_calloc_prefer(1, ESP_WORKQ_TASK_STACK, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL);
    void *task_stack = heap_caps_malloc(ESP_WORKQ_TASK_STACK, MALLOC_CAP_INTERNAL);
    assert(task_buffer && task_stack);

    /* the task never exits, so do not bother to free the buffers */
    xTaskCreateStatic(&esp_work_queue_task, "esp_workq_task", ESP_WORKQ_TASK_STACK,
                      NULL, ESP_WORKQ_TASK_PRIO, task_stack, task_buffer);

    // if (xTaskCreate(&esp_work_queue_task, "rmaker_queue_task", ESP_WORKQ_TASK_STACK,
    //             NULL, ESP_WORKQ_TASK_PRIO, NULL) != pdPASS) {
    //     ESP_LOGE(TAG, "Couldn't create RainMaker work queue task");
    //     return ESP_FAIL;
    // }
    queue_state = WORK_QUEUE_STATE_RUNNING;
    return ESP_OK;
}

esp_err_t esp_work_queue_stop(void)
{
    if (queue_state == WORK_QUEUE_STATE_RUNNING) {
        queue_state = WORK_QUEUE_STATE_STOP_REQUESTED;
    }
    return ESP_OK;
}
