/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/param.h>
#include <assert.h>
#include "esp_log.h"
#include "esp_loader_io.h"
#include "esp_loader.h"
#include "loader_utils.h"

static const char *TAG = "loader_utils";

// Only bootloader addresses vary by chip
// DO NOT specify array size - let compiler determine it from initializers
static const uint32_t bootloader_addresses[] = {
    [ESP8266_CHIP] = 0x0,
    [ESP32_CHIP]   = 0x1000,
    [ESP32S2_CHIP] = 0x1000,
    [ESP32C3_CHIP] = 0x0,
    [ESP32S3_CHIP] = 0x0,
    [ESP32C2_CHIP] = 0x0,
    [ESP32C5_CHIP] = 0x2000,
    [ESP32H2_CHIP] = 0x0,
    [ESP32C6_CHIP] = 0x0,
    [ESP32P4_CHIP] = 0x2000
};

// If someone adds a new chip but forgets to update the array, compilation FAILS
_Static_assert(sizeof(bootloader_addresses) / sizeof(bootloader_addresses[0]) == ESP_MAX_CHIP,
               "bootloader_addresses array size mismatch! "
               "If you added a new chip to target_chip_t, you MUST add its address to bootloader_addresses[]");

static const char *get_error_string(const esp_loader_error_t error)
{
    const char *mapping[ESP_LOADER_ERROR_INVALID_RESPONSE + 1] = {
        "NONE", "UNKNOWN", "TIMEOUT", "IMAGE SIZE",
        "INVALID MD5", "INVALID PARAMETER", "INVALID TARGET",
        "UNSUPPORTED CHIP", "UNSUPPORTED FUNCTION", "INVALID RESPONSE"
    };

    assert(error <= ESP_LOADER_ERROR_INVALID_RESPONSE);

    return mapping[error];
}

esp_loader_error_t connect_to_target(uint32_t higher_transmission_rate)
{
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    esp_loader_error_t err = esp_loader_connect(&connect_config);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Cannot connect to target. Error: %s", get_error_string(err));

        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            ESP_LOGW(TAG, "Check if the host and the target are properly connected.");
        } else if (err == ESP_LOADER_ERROR_INVALID_TARGET) {
            ESP_LOGW(TAG, "You could be using an unsupported chip, or chip revision.");
        } else if (err == ESP_LOADER_ERROR_INVALID_RESPONSE) {
            ESP_LOGW(TAG, "Try lowering the transmission rate or using shorter wires to connect the host and the target.");
        }

        return err;
    }
    ESP_LOGI(TAG, "Connected to target");

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
    if (higher_transmission_rate && esp_loader_get_target() != ESP8266_CHIP) {
        err = esp_loader_change_transmission_rate(higher_transmission_rate);
        if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
            ESP_LOGE(TAG, "ESP8266 does not support change transmission rate command.");
            return err;
        } else if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Unable to change transmission rate on target.");
            return err;
        } else {
            err = loader_port_change_transmission_rate(higher_transmission_rate);
            if (err != ESP_LOADER_SUCCESS) {
                ESP_LOGE(TAG, "Unable to change transmission rate.");
                return err;
            }
            ESP_LOGI(TAG, "Transmission rate changed.");
        }
    }
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

    return ESP_LOADER_SUCCESS;
}

uint32_t get_bootloader_address(target_chip_t chip)
{
    return bootloader_addresses[chip];
}

