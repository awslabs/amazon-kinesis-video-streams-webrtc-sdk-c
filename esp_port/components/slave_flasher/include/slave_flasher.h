/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

/**
 * @brief Flash the slave device with firmware images from SPIFFS and start monitoring.
 *
 * Mounts the "slave" SPIFFS partition (at /spiffs) and uses esp-serial-flasher
 * over UART_NUM_1 to program the slave chip. Three images are expected on
 * SPIFFS and are flashed to the addresses configured via Kconfig:
 *   - /spiffs/bootloader.bin       -> bootloader address for the target chip
 *   - /spiffs/partition-table.bin  -> CONFIG_SLAVE_PARTITION_TABLE_ADDRESS
 *   - /spiffs/app.bin              -> CONFIG_SLAVE_APPLICATION_ADDRESS
 *
 * For each image the MD5 of the local file is compared against the MD5 of the
 * corresponding region already on the slave's flash. Matching regions are
 * skipped; mismatching regions are (re)written and verified. After flashing,
 * the slave is reset and a background task ("slave_monitor") is spawned to
 * stream the slave's UART output to stdout, and a "write_slave" console
 * command is registered for sending data to the slave console.
 *
 * UART pins, GPIO0/RESET trigger pins, and flash addresses are taken from
 * Kconfig (CONFIG_SLAVE_UART_RX_PIN, CONFIG_SLAVE_UART_TX_PIN,
 * CONFIG_ESP_GPIO_SLAVE_RESET_SLAVE, CONFIG_SLAVE_GPIO0_TRIGGER_PIN,
 * CONFIG_SLAVE_PARTITION_TABLE_ADDRESS, CONFIG_SLAVE_APPLICATION_ADDRESS).
 *
 * @note This function is blocking and may take several seconds to complete
 *       depending on image sizes and whether (re)flashing is required.
 * @note If the slave cannot be entered into bootloader mode, flashing is
 *       skipped but the monitor task and CLI command are still started, and
 *       ESP_OK is returned.
 *
 * @return
 *   - ESP_OK   on success (monitor task started; images flashed or already up to date)
 *   - ESP_FAIL on SPIFFS mount failure, serial init failure, or a flash/verify error
 *   - ESP_ERR_NO_MEM        if intermediate buffers cannot be allocated
 *   - ESP_ERR_INVALID_SIZE  if one of the firmware images on SPIFFS is empty
 */
esp_err_t flash_slave(void);
