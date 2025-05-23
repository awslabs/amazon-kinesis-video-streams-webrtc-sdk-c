// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** prevent recursive inclusion **/
#ifndef __POWER_SAVE_H
#define __POWER_SAVE_H

/** Includes **/
#include "common.h"
#include "esp_sleep.h"

/** Constants/Macros **/

/** Exported Structures **/

/** Exported variables **/

/** Inline functions **/

/** Exported Functions **/
int is_feature_enabled_host_power_save(void);
int is_host_reboot_due_to_deep_sleep(void);
int is_host_power_saving(void);
int start_host_power_save(void);
int set_host_wakeup_reason(int reason);

/**
 * @brief Initialize power save driver and configure GPIO for slave reset detection
 *
 * @param gpio_num GPIO number to configure for slave reset detection
 */
void power_save_drv_init(uint32_t gpio_num);

/**
 * @brief Start timer to put p4 in deep sleep
 *
 * @param time_ms After time_ms are elapsed, the p4 will be put in deep sleep unless the timer is stopped
 * @return int 0 on success, failure otherwise
 */
int host_power_save_timer_start(uint32_t time_ms);

/**
 * @brief Stop host power save timer
 *
 * @return int 0 on success, failure otherwise
 */
int host_power_save_timer_stop(void);

/**
 * @brief Restart host power save timer with new timer value
 *
 * @param time_ms time in ms for which the restarted timer run before deep sleep
 * @return int 0 on success, failure otherwise
 */
int host_power_save_timer_restart(uint32_t time_ms);

#endif /* __POWER_SAVE_H */
