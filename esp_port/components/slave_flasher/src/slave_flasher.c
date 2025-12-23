#include "esp_loader.h"
#include "esp32_port.h"
#include "slave_flasher.h"
#include "loader_utils.h"

#include "esp_spiffs.h"
#include "mbedtls/md5.h"

static const char *TAG = "slave_flasher";

#define HIGHER_BAUDRATE 2000000
#define PARTITION_TABLE_ADDRESS  CONFIG_SLAVE_PARTITION_TABLE_ADDRESS
#define APPLICATION_ADDRESS      CONFIG_SLAVE_APPLICATION_ADDRESS

// Max line size
#define BUF_LEN 128
#define SLAVE_BUFFER_SIZE 512
static uint8_t buf[BUF_LEN] = {0};
static char slave_line_buffer[SLAVE_BUFFER_SIZE] = {0};
static size_t slave_line_pos = 0;

static void slave_print_line(const char* line)
{
    if (!line) return;

    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') line++;

    if (*line != '\0') {
        printf("\033[1;33m[SLAVE]\033[0m %s\n", line);
        fflush(stdout);
    }
}

static void slave_monitor(void *arg)
{
#if (HIGHER_BAUDRATE != 115200)
    uart_flush_input(UART_NUM_1);
    uart_flush(UART_NUM_1);
    uart_set_baudrate(UART_NUM_1, 115200);
#endif

    slave_line_pos = 0;

    while (1) {
        int rxBytes = uart_read_bytes(UART_NUM_1, buf, BUF_LEN - 1, 100 / portTICK_PERIOD_MS);
        if (rxBytes <= 0) continue;

        buf[rxBytes] = '\0';

        // Process received data line by line
        for (char *p = (char *)buf; *p; ) {
            char *line_end = strpbrk(p, "\n\r");

            if (line_end) {
                // Complete line found - append to buffer and print
                size_t chunk_len = line_end - p;
                if (slave_line_pos + chunk_len < SLAVE_BUFFER_SIZE) {
                    memcpy(&slave_line_buffer[slave_line_pos], p, chunk_len);
                    slave_line_buffer[slave_line_pos + chunk_len] = '\0';
                    slave_print_line(slave_line_buffer);  // slave_print_line now handles newline
                    slave_line_pos = 0;
                }
                p = line_end + 1;  // Skip line break
            } else {
                // Partial line - append remainder to buffer
                size_t remaining = strlen(p);
                if (slave_line_pos + remaining >= SLAVE_BUFFER_SIZE - 1) {
                    // Buffer overflow - print and reset
                    slave_line_buffer[SLAVE_BUFFER_SIZE - 1] = '\0';
                    slave_print_line(slave_line_buffer);
                    slave_line_pos = 0;
                }
                memcpy(&slave_line_buffer[slave_line_pos], p, remaining);
                slave_line_pos += remaining;
                slave_line_buffer[slave_line_pos] = '\0';
                break;
            }
        }
    }
}

static esp_err_t check_and_flash_partition(const char *file_path, uint32_t addr)
{
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", file_path);
        return ESP_FAIL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    if (size == 0) {
        ESP_LOGE(TAG, "File %s is empty", file_path);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    // Compute MD5 by reading file in chunks (memory efficient)
    // Use heap allocation to avoid stack overflow
    #define MD5_MAX_LEN 16
    #define MD5_CHUNK_SIZE 4096  // 4KB chunks for MD5 computation

    mbedtls_md5_context ctx;
    unsigned char digest[MD5_MAX_LEN];
    uint8_t *chunk = malloc(MD5_CHUNK_SIZE);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate MD5 buffer for %s", file_path);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);

    size_t remaining = size;
    while (remaining > 0) {
        size_t to_read = MIN(remaining, MD5_CHUNK_SIZE);
        size_t read = fread(chunk, 1, to_read, f);
        if (read != to_read) {
            ESP_LOGE(TAG, "Failed to read file %s (read %zu of %zu)", file_path, read, to_read);
            free(chunk);
            fclose(f);
            return ESP_FAIL;
        }
        mbedtls_md5_update(&ctx, chunk, read);
        remaining -= read;
    }

    mbedtls_md5_finish(&ctx, digest);
    free(chunk);  // Free MD5 buffer before allocating flash buffer

    // Create a string of the digest (32 hex chars + null terminator)
    char digest_str[MD5_MAX_LEN * 2 + 1];

    for (int i = 0; i < MD5_MAX_LEN; i++) {
        sprintf(&digest_str[i * 2], "%02x", (unsigned int)digest[i]);
    }
    digest_str[MD5_MAX_LEN * 2] = '\0';  // Ensure null termination

    ESP_LOGI(TAG, "Computed MD5 hash of %s: %s", file_path, digest_str);

    // Verify MD5 against flash
    if (esp_loader_flash_verify_known_md5(addr, size, (uint8_t *)digest_str) != ESP_LOADER_SUCCESS) {
        ESP_LOGI(TAG, "%s MD5 mismatch, flashing...", file_path);

        // Stream file for flashing (read chunks and write to flash)
        esp_loader_error_t err = esp_loader_flash_start(addr, size, 1024);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to start flash operation: %d", err);
            fclose(f);
            return ESP_FAIL;
        }

        rewind(f);
        remaining = size;
        size_t written = 0;
        uint8_t *flash_chunk = malloc(1024);  // Match flash_binary payload size
        if (!flash_chunk) {
            ESP_LOGE(TAG, "Failed to allocate flash buffer for %s", file_path);
            fclose(f);
            return ESP_ERR_NO_MEM;
        }

        while (remaining > 0) {
            size_t to_read = MIN(remaining, 1024);
            size_t read = fread(flash_chunk, 1, to_read, f);
            if (read != to_read) {
                ESP_LOGE(TAG, "Failed to read file for flashing (read %zu of %zu)", read, to_read);
                free(flash_chunk);
                fclose(f);
                return ESP_FAIL;
            }

            err = esp_loader_flash_write(flash_chunk, to_read);
            if (err != ESP_LOADER_SUCCESS) {
                ESP_LOGE(TAG, "Failed to write flash chunk: %d", err);
                free(flash_chunk);
                fclose(f);
                return ESP_FAIL;
            }

            remaining -= to_read;
            written += to_read;

            int progress = (int)(((float)written / size) * 100);
            printf("\rFlashing progress: %d%%", progress);
            // fflush(stdout);
        }
        printf("\n");  // Newline after progress completes

        free(flash_chunk);
        err = esp_loader_flash_verify();
        fclose(f);

        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to finish flash operation: %d", err);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Successfully flashed %s", file_path);
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "%s MD5 match, skipping...", file_path);
        fclose(f);
        return ESP_OK;
    }
}

esp_err_t flash_slave()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "slave",
      .max_files = 5,
      .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    const loader_esp32_config_t config = {
        .baud_rate = 115200,
        .uart_port = UART_NUM_1,
        .uart_rx_pin = CONFIG_SLAVE_UART_RX_PIN,
        .uart_tx_pin = CONFIG_SLAVE_UART_TX_PIN,
        .reset_trigger_pin = CONFIG_ESP_GPIO_SLAVE_RESET_SLAVE,
        .gpio0_trigger_pin = CONFIG_SLAVE_GPIO0_TRIGGER_PIN,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return ESP_FAIL;
    }

    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {

        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        uint32_t partition_addr = PARTITION_TABLE_ADDRESS;
        uint32_t app_addr = APPLICATION_ADDRESS;

        ret = check_and_flash_partition("/spiffs/bootloader.bin", bootloader_addr);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = check_and_flash_partition("/spiffs/partition-table.bin", partition_addr);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = check_and_flash_partition("/spiffs/app.bin", app_addr);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_LOGI(TAG, "Done!");
        esp_loader_reset_target();

        // Delay for skipping the boot message of the targets
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }

    xTaskCreate(slave_monitor, "slave_monitor", 2048, NULL, configMAX_PRIORITIES - 1, NULL);

    return ESP_OK;
}