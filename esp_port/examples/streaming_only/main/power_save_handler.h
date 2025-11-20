/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POWER_SAVE_HANDLER_H
#define POWER_SAVE_HANDLER_H

#include "app_webrtc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable automatic power save management
 *
 * Enables automatic deep sleep timer management based on WebRTC events.
 * The system will:
 * - Stop timer when streaming starts or connection is established
 * - Start timer when all streaming stops
 * - Handle multiple concurrent streaming sessions
 *
 * @return 0 on success, non-zero on failure
 */
int32_t power_save_enable(void);

/**
 * @brief Register CLI command for manual deep sleep
 *
 * Registers the "deep-sleep" CLI command that allows manually triggering
 * deep sleep on ESP32-P4.
 *
 * @return 0 on success, non-zero on failure
 */
int power_save_cli_register(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_SAVE_HANDLER_H */
