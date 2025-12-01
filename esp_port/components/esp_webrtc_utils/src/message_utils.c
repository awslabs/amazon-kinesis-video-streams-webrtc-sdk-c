/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "message_utils.h"

static const char *TAG = "message_utils";

received_msg_t *esp_webrtc_create_buffer_for_msg(int capacity)
{
    ESP_LOGI(TAG, "Creating buffer of capacity %d", capacity);
    received_msg_t *msg = (received_msg_t *) calloc(1, sizeof(received_msg_t));
    if (!msg) {
        ESP_LOGE(TAG, "Failed allocation of message structure");
        return NULL;
    }

    msg->buf = heap_caps_malloc_prefer(capacity, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL);
    if (!msg->buf) {
        ESP_LOGE(TAG, "Failed allocation of buffer size %d", capacity);
        free(msg);
        return NULL;
    }
    msg->capacity = capacity;
    msg->data_size = 0;
    return msg;
}

esp_err_t esp_webrtc_append_msg_to_existing(received_msg_t *dst_msg, void *data_ptr, int data_len, bool is_fin)
{
    ESP_LOGI(TAG, "Appending message of size %d at %d location, is_fin: %d", data_len, dst_msg->data_size, is_fin);

    if (!is_fin) {
        if (dst_msg->data_size + data_len > dst_msg->capacity) {
            ESP_LOGE(TAG, "Cannot fit all the data in %d size buffer. current_filled: %d, incoming_data: %d",
                    dst_msg->capacity, dst_msg->data_size, data_len);
            dst_msg->data_size = dst_msg->capacity + 1; /* Overflow the limit forcefully */
            return ESP_FAIL;
        } else { /* Gather fragmented message */
            memcpy(dst_msg->buf + dst_msg->data_size, data_ptr, data_len);
            dst_msg->data_size += data_len;
            return ESP_ERR_NOT_FINISHED;
        }
    } else if (dst_msg->data_size == 0) { /* non-fragmented message */
        memcpy(dst_msg->buf + dst_msg->data_size, data_ptr, data_len);
        dst_msg->data_size += data_len;
        return ESP_OK;
    } else { /* final part of the fragment */
        if (dst_msg->data_size + data_len <= dst_msg->capacity) { /* valid fragmented gathered message */
            /* Copy last piece */
            memcpy(dst_msg->buf + dst_msg->data_size, data_ptr, data_len);
            dst_msg->data_size += data_len;
            /* Process now... */
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Discarding the message which could not be fit");
            dst_msg->data_size = 0;
            return ESP_FAIL;
        }
    }
    return ESP_FAIL;
}
