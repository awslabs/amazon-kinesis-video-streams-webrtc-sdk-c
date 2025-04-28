/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Sync time with SNTP server and wait for the time to be set.
 *
 * This function will initialize the SNTP client and start the time synchronization process.
 * It will wait for the time to be set before returning.
 */
void esp_webrtc_time_sntp_time_sync_and_wait();

/**
 * @brief Sync time with SNTP server without waiting for the time to be set.
 *
 * This function will initialize the SNTP client and start the time synchronization process.
 * However, it will not wait for the time to be set.
 */
void esp_webrtc_time_sntp_time_sync_no_wait();

#ifdef __cplusplus
}
#endif
