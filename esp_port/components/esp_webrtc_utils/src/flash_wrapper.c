/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_vfs.h>
#include "flash_wrapper.h"

static const char* TAG = "flash_wrapper";

// Queue for flash operations
static QueueHandle_t flash_op_queue = NULL;

// Task handle for flash operation task
static TaskHandle_t flash_task_handle = NULL;

// NVS path prefix
#define NVS_PATH_PREFIX "/nvs/"

// Flash operation types
typedef enum {
    FLASH_OP_READ,
    FLASH_OP_WRITE,
    FLASH_OP_EXISTS,
    FLASH_OP_STAT,
    FLASH_OP_GET_SIZE,
    // NVS operations
    FLASH_OP_NVS_READ,
    FLASH_OP_NVS_WRITE,
    FLASH_OP_NVS_ERASE,
    FLASH_OP_NVS_EXISTS
} flash_op_type_t;

// Flash operation request structure
typedef struct {
    flash_op_type_t op_type;
    esp_err_t result;
    SemaphoreHandle_t done_sem;
    union {
        struct {
            char path[256];
            void* buf;
            size_t size;
            size_t offset;
            size_t* size_result;  // Pointer to store size result for GET_SIZE operation
            struct stat stat_info;
        } file;
        struct {
            char nvs_partition[32];
            char nvs_namespace[32];
            char nvs_key[32];
            nvs_type_t nvs_type;
            void* nvs_value;
            size_t nvs_value_size;
            bool* nvs_exists_result;
        } nvs;
    } op;
} flash_op_t;

// Parse NVS path in format /nvs/<partition>/<namespace>/<key>
static bool parse_nvs_path(const char* path, char* partition, char* namespace, char* key)
{
    if (strncmp(path, NVS_PATH_PREFIX, strlen(NVS_PATH_PREFIX)) != 0) {
        return false;
    }

    // Skip the prefix
    const char* p = path + strlen(NVS_PATH_PREFIX);

    // Parse partition
    const char* partition_end = strchr(p, '/');
    if (!partition_end) {
        return false;
    }
    size_t partition_len = partition_end - p;
    if (partition_len >= 32) {
        return false;
    }
    memcpy(partition, p, partition_len);
    partition[partition_len] = '\0';

    // Parse namespace
    p = partition_end + 1;
    const char* namespace_end = strchr(p, '/');
    if (!namespace_end) {
        return false;
    }
    size_t namespace_len = namespace_end - p;
    if (namespace_len >= 32) {
        return false;
    }
    memcpy(namespace, p, namespace_len);
    namespace[namespace_len] = '\0';

    // Parse key
    p = namespace_end + 1;
    size_t key_len = strlen(p);
    if (key_len == 0 || key_len >= 32) {
        return false;
    }
    strcpy(key, p);

    return true;
}


// --- NVS operation API ---

static esp_err_t do_nvs_op(flash_op_t *op)
{
    nvs_handle_t nvs_handle;
    esp_err_t nvs_ret = ESP_OK;

    switch (op->op_type) {
        case FLASH_OP_NVS_READ:
            nvs_ret = nvs_open(op->op.nvs.nvs_namespace, NVS_READONLY, &nvs_handle);
            if (nvs_ret == ESP_OK) {
                switch (op->op.nvs.nvs_type) {
                    case NVS_TYPE_I32:
                        nvs_ret = nvs_get_i32(nvs_handle, op->op.nvs.nvs_key, (int32_t*)op->op.nvs.nvs_value);
                        break;
                    case NVS_TYPE_U32:
                        nvs_ret = nvs_get_u32(nvs_handle, op->op.nvs.nvs_key, (uint32_t*)op->op.nvs.nvs_value);
                        break;
                    case NVS_TYPE_STR:
                        nvs_ret = nvs_get_str(nvs_handle, op->op.nvs.nvs_key, (char*)op->op.nvs.nvs_value, &op->op.nvs.nvs_value_size);
                        break;
                    case NVS_TYPE_BLOB:
                        nvs_ret = nvs_get_blob(nvs_handle, op->op.nvs.nvs_key, op->op.nvs.nvs_value, &op->op.nvs.nvs_value_size);
                        break;
                    default:
                        nvs_ret = ESP_ERR_INVALID_ARG;
                }
                nvs_close(nvs_handle);
            }
            break;

        case FLASH_OP_NVS_WRITE:
            nvs_ret = nvs_open(op->op.nvs.nvs_namespace, NVS_READWRITE, &nvs_handle);
            if (nvs_ret == ESP_OK) {
                switch (op->op.nvs.nvs_type) {
                    case NVS_TYPE_I32:
                        nvs_ret = nvs_set_i32(nvs_handle, op->op.nvs.nvs_key, *(int32_t*)op->op.nvs.nvs_value);
                        break;
                    case NVS_TYPE_U32:
                        nvs_ret = nvs_set_u32(nvs_handle, op->op.nvs.nvs_key, *(uint32_t*)op->op.nvs.nvs_value);
                        break;
                    case NVS_TYPE_STR:
                        nvs_ret = nvs_set_str(nvs_handle, op->op.nvs.nvs_key, (const char*)op->op.nvs.nvs_value);
                        break;
                    case NVS_TYPE_BLOB:
                        nvs_ret = nvs_set_blob(nvs_handle, op->op.nvs.nvs_key, op->op.nvs.nvs_value, op->op.nvs.nvs_value_size);
                        break;
                    default:
                        nvs_ret = ESP_ERR_INVALID_ARG;
                }
                if (nvs_ret == ESP_OK) {
                    nvs_ret = nvs_commit(nvs_handle);
                }
                nvs_close(nvs_handle);
            }
            break;

        case FLASH_OP_NVS_ERASE:
            nvs_ret = nvs_open(op->op.nvs.nvs_namespace, NVS_READWRITE, &nvs_handle);
            if (nvs_ret == ESP_OK) {
                nvs_ret = nvs_erase_key(nvs_handle, op->op.nvs.nvs_key);
                if (nvs_ret == ESP_OK) {
                    nvs_ret = nvs_commit(nvs_handle);
                }
                nvs_close(nvs_handle);
            }
            break;

        case FLASH_OP_NVS_EXISTS:
            nvs_ret = nvs_open(op->op.nvs.nvs_namespace, NVS_READONLY, &nvs_handle);
            if (nvs_ret == ESP_OK) {
                size_t required_size = 0;
                nvs_ret = nvs_get_blob(nvs_handle, op->op.nvs.nvs_key, NULL, &required_size);
                if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NOT_ENOUGH_SPACE || nvs_ret == ESP_ERR_NVS_INVALID_LENGTH) {
                    if (op->op.nvs.nvs_exists_result) *op->op.nvs.nvs_exists_result = true;
                    nvs_ret = ESP_OK;
                } else {
                    if (op->op.nvs.nvs_exists_result) *op->op.nvs.nvs_exists_result = false;
                }
                nvs_close(nvs_handle);
            }
            break;

        default:
            nvs_ret = ESP_ERR_INVALID_ARG;
            break;
    }

    return nvs_ret;
}

// --- NVS file-path operation API ---

static esp_err_t do_nvs_file_path_op(flash_op_t *op, const char *partition, const char *namespace, const char *key)
{
    nvs_handle_t nvs_handle;
    esp_err_t nvs_ret = ESP_OK;

    // Open the NVS partition
    if (strcmp(partition, "nvs") == 0) {
        nvs_ret = nvs_open(namespace,
                           (op->op_type == FLASH_OP_WRITE) ? NVS_READWRITE : NVS_READONLY,
                           &nvs_handle);
    } else {
        nvs_ret = nvs_flash_init_partition(partition);
        if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES) {
            nvs_ret = nvs_open_from_partition(partition, namespace,
                                              (op->op_type == FLASH_OP_WRITE) ? NVS_READWRITE : NVS_READONLY,
                                              &nvs_handle);
        }
    }

    if (nvs_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS partition: %s", partition);
        return nvs_ret;
    }

    switch (op->op_type) {
        case FLASH_OP_READ: {
            size_t required_size = op->op.file.size;
            nvs_ret = nvs_get_blob(nvs_handle, key, op->op.file.buf, &required_size);
            if (nvs_ret == ESP_OK && required_size <= op->op.file.size) {
                op->result = ESP_OK;
            } else {
                op->result = ESP_FAIL;
            }
            break;
        }
        case FLASH_OP_WRITE:
            nvs_ret = nvs_set_blob(nvs_handle, key, op->op.file.buf, op->op.file.size);
            if (nvs_ret == ESP_OK) {
                nvs_ret = nvs_commit(nvs_handle);
            }
            op->result = nvs_ret;
            break;
        case FLASH_OP_EXISTS: {
            size_t required_size = 0;
            nvs_ret = nvs_get_blob(nvs_handle, key, NULL, &required_size);
            if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                if (op->op.file.exists_result != NULL) {
                    *op->op.file.exists_result = true;
                }
                op->result = ESP_OK;
            } else {
                if (op->op.file.exists_result != NULL) {
                    *op->op.file.exists_result = false;
                }
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;
        }
        case FLASH_OP_STAT: {
            size_t required_size = 0;
            nvs_ret = nvs_get_blob(nvs_handle, key, NULL, &required_size);
            if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                memset(&op->op.file.stat_info, 0, sizeof(struct stat));
                op->op.file.stat_info.st_size = required_size;
                op->op.file.stat_info.st_mode = S_IFREG | 0644;
                op->result = ESP_OK;
            } else {
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;
        }
        case FLASH_OP_GET_SIZE: {
            size_t required_size = 0;
            nvs_ret = nvs_get_blob(nvs_handle, key, NULL, &required_size);
            if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                if (op->op.file.size_result != NULL) {
                    *(op->op.file.size_result) = required_size;
                }
                op->result = ESP_OK;
            } else {
                if (op->op.file.size_result != NULL) {
                    *(op->op.file.size_result) = 0;
                }
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;
        }
        default:
            op->result = ESP_ERR_INVALID_ARG;
            break;
    }

    nvs_close(nvs_handle);
    return op->result;
}


// Separated file operation handler
static esp_err_t do_file_op(flash_op_t *op)
{
    FILE* f;
    switch (op->op_type) {
        case FLASH_OP_GET_SIZE:
            f = fopen(op->op.file.path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                size_t size = ftell(f);
                if (op->op.file.size_result != NULL) {
                    *(op->op.file.size_result) = size;
                }
                op->result = ESP_OK;
                fclose(f);
            } else {
                op->result = ESP_ERR_NOT_FOUND;
                if (op->op.file.size_result != NULL) {
                    *(op->op.file.size_result) = 0;
                }
            }
            break;

        case FLASH_OP_READ:
            f = fopen(op->op.file.path, "rb");
            if (f) {
                fseek(f, op->op.file.offset, SEEK_SET);
                op->result = (fread(op->op.file.buf, 1, op->op.file.size, f) == op->op.file.size) ? ESP_OK : ESP_FAIL;
                fclose(f);
            } else {
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;

        case FLASH_OP_WRITE:
            f = fopen(op->op.file.path, "wb");
            if (f) {
                op->result = (fwrite(op->op.file.buf, 1, op->op.file.size, f) == op->op.file.size) ? ESP_OK : ESP_FAIL;
                fclose(f);
            } else {
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;

        case FLASH_OP_EXISTS:
            f = fopen(op->op.file.path, "rb");
            if (f) {
                fclose(f);
                op->result = ESP_OK;
            } else {
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;

        case FLASH_OP_STAT:
            if (stat(op->op.file.path, &op->op.file.stat_info) == 0) {
                op->result = ESP_OK;
            } else {
                op->result = ESP_ERR_NOT_FOUND;
            }
            break;

        default:
            op->result = ESP_ERR_INVALID_ARG;
            break;
    }
    return op->result;
}

// Flash operation task
static void IRAM_ATTR flash_op_task(void* arg)
{
    flash_op_t op;
    char partition[32], namespace[32], key[32];

    while (1) {
        if (xQueueReceive(flash_op_queue, &op, portMAX_DELAY) == pdTRUE) {
            // Handle NVS file-path operations
            if ((op.op_type == FLASH_OP_READ || op.op_type == FLASH_OP_WRITE ||
                 op.op_type == FLASH_OP_EXISTS || op.op_type == FLASH_OP_STAT ||
                 op.op_type == FLASH_OP_GET_SIZE) &&
                 parse_nvs_path(op.op.file.path, partition, namespace, key)) {

                ESP_LOGI(TAG, "Converting file operation to NVS operation: partition=%s, namespace=%s, key=%s",
                         partition, namespace, key);

                do_nvs_file_path_op(&op, partition, namespace, key);

            } else {
                // Regular file operations and direct NVS ops
                switch (op.op_type) {
                    case FLASH_OP_GET_SIZE:
                    case FLASH_OP_READ:
                    case FLASH_OP_WRITE:
                    case FLASH_OP_EXISTS:
                    case FLASH_OP_STAT:
                        do_file_op(&op);
                        break;

                    case FLASH_OP_NVS_READ:
                    case FLASH_OP_NVS_WRITE:
                    case FLASH_OP_NVS_ERASE:
                    case FLASH_OP_NVS_EXISTS:
                        op.result = do_nvs_op(&op);
                        break;

                    default:
                        op.result = ESP_ERR_INVALID_ARG;
                        break;
                }
            }
            xSemaphoreGive(op.done_sem);
        }
    }
}

esp_err_t flash_wrapper_init(void)
{
    if (flash_op_queue != NULL) {
        return ESP_OK; // Already initialized
    }

    flash_op_queue = xQueueCreate(10, sizeof(flash_op_t));
    if (flash_op_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create flash operation queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t res = xTaskCreate(
        flash_op_task,
        "flash_op_task",
        4096,
        NULL,
        5,
        &flash_task_handle
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create flash operation task");
        vQueueDelete(flash_op_queue);
        flash_op_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t flash_wrapper_deinit(void)
{
    if (flash_task_handle != NULL) {
        vTaskDelete(flash_task_handle);
        flash_task_handle = NULL;
    }

    if (flash_op_queue != NULL) {
        vQueueDelete(flash_op_queue);
        flash_op_queue = NULL;
    }

    return ESP_OK;
}

static esp_err_t submit_flash_op(flash_op_t* op)
{
    esp_err_t ret;
    SemaphoreHandle_t sem;

    if (flash_op_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Create a new semaphore for this operation
    sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    op->done_sem = sem;

    // Submit operation to queue
    if (xQueueSend(flash_op_queue, op, portMAX_DELAY) != pdTRUE) {
        vSemaphoreDelete(sem);
        return ESP_FAIL;
    }

    // Wait for operation to complete with blocking semaphore
    if (xSemaphoreTake(sem, portMAX_DELAY) != pdTRUE) {
        ret = ESP_ERR_TIMEOUT;
    } else {
        ret = op->result;
    }

    vSemaphoreDelete(sem);
    return ret;
}

esp_err_t flash_wrapper_read(const char* path, void* buf, size_t size, size_t offset)
{
    flash_op_t op = {
        .op_type = FLASH_OP_READ,
        .op.file.buf = buf,
        .op.file.size = size,
        .op.file.offset = offset
    };
    strncpy(op.op.file.path, path, sizeof(op.op.file.path) - 1);
    return submit_flash_op(&op);
}

esp_err_t flash_wrapper_write(const char* path, const void* buf, size_t size)
{
    flash_op_t op = {
        .op_type = FLASH_OP_WRITE,
        .op.file.buf = (void*)buf,
        .op.file.size = size,
        .op.file.offset = 0
    };
    strncpy(op.op.file.path, path, sizeof(op.op.file.path) - 1);
    return submit_flash_op(&op);
}

esp_err_t flash_wrapper_exists(const char* path, bool* exists)
{
    flash_op_t op = {
        .op_type = FLASH_OP_EXISTS
    };
    strncpy(op.op.file.path, path, sizeof(op.op.file.path) - 1);
    esp_err_t ret = submit_flash_op(&op);
    *exists = (ret == ESP_OK);
    return ESP_OK;
}

esp_err_t flash_wrapper_stat(const char* path, struct stat* st)
{
    flash_op_t op = {
        .op_type = FLASH_OP_STAT
    };
    strncpy(op.op.file.path, path, sizeof(op.op.file.path) - 1);
    esp_err_t ret = submit_flash_op(&op);
    if (ret == ESP_OK) {
        *st = op.op.file.stat_info;
    }
    return ret;
}

esp_err_t flash_wrapper_get_size(const char* path, size_t* size)
{
    flash_op_t op = {
        .op_type = FLASH_OP_GET_SIZE,
        .op.file.size_result = size  // Pass the size pointer to the operation
    };
    strncpy(op.op.file.path, path, sizeof(op.op.file.path) - 1);
    ESP_LOGI(TAG, "Getting size of file %s", op.op.file.path);
    esp_err_t ret = submit_flash_op(&op);
    return ret;
}

esp_err_t flash_wrapper_read_cert(const char* cert_path, uint8_t* cert_buf, size_t cert_len, size_t* bytes_read)
{
    if (!cert_path || !cert_buf || !bytes_read || cert_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize bytes read to 0
    *bytes_read = 0;

    // Check if file exists
    bool exists = false;
    esp_err_t ret = flash_wrapper_exists(cert_path, &exists);
    if (ret != ESP_OK || !exists) {
        ESP_LOGE(TAG, "Certificate file %s not found", cert_path);
        return ESP_ERR_NOT_FOUND;
    }

    // Read the certificate data
    ret = flash_wrapper_read(cert_path, cert_buf, cert_len, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read certificate file %s", cert_path);
        return ret;
    }

    // Get actual file size using our flash task
    struct stat st;
    ret = flash_wrapper_stat(cert_path, &st);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get certificate file size");
        return ESP_FAIL;
    }

    *bytes_read = st.st_size;
    return ESP_OK;
}

// NVS wrapper functions
esp_err_t flash_wrapper_nvs_read(const char* ns, const char* key, nvs_type_t type, void* value, size_t* value_size)
{
    flash_op_t op = {
        .op_type = FLASH_OP_NVS_READ,
    };
    strncpy(op.op.nvs.nvs_namespace, ns, sizeof(op.op.nvs.nvs_namespace) - 1);
    strncpy(op.op.nvs.nvs_key, key, sizeof(op.op.nvs.nvs_key) - 1);
    op.op.nvs.nvs_type = type;
    op.op.nvs.nvs_value = value;
    if (value_size) op.op.nvs.nvs_value_size = *value_size;
    esp_err_t ret = submit_flash_op(&op);
    if (value_size) *value_size = op.op.nvs.nvs_value_size;
    return ret;
}

esp_err_t flash_wrapper_nvs_write(const char* ns, const char* key, nvs_type_t type, const void* value, size_t value_size)
{
    flash_op_t op = {
        .op_type = FLASH_OP_NVS_WRITE,
    };
    strncpy(op.op.nvs.nvs_namespace, ns, sizeof(op.op.nvs.nvs_namespace) - 1);
    strncpy(op.op.nvs.nvs_key, key, sizeof(op.op.nvs.nvs_key) - 1);
    op.op.nvs.nvs_type = type;
    op.op.nvs.nvs_value = (void*)value;
    op.op.nvs.nvs_value_size = value_size;
    return submit_flash_op(&op);
}

esp_err_t flash_wrapper_nvs_erase(const char* ns, const char* key)
{
    flash_op_t op = {
        .op_type = FLASH_OP_NVS_ERASE,
    };
    strncpy(op.op.nvs.nvs_namespace, ns, sizeof(op.op.nvs.nvs_namespace) - 1);
    strncpy(op.op.nvs.nvs_key, key, sizeof(op.op.nvs.nvs_key) - 1);
    return submit_flash_op(&op);
}

esp_err_t flash_wrapper_nvs_exists(const char* ns, const char* key, bool* exists)
{
    flash_op_t op = {
        .op_type = FLASH_OP_NVS_EXISTS,
        .op.nvs.nvs_exists_result = exists
    };
    strncpy(op.op.nvs.nvs_namespace, ns, sizeof(op.op.nvs.nvs_namespace) - 1);
    strncpy(op.op.nvs.nvs_key, key, sizeof(op.op.nvs.nvs_key) - 1);
    return submit_flash_op(&op);
}
