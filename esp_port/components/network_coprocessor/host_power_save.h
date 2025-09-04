/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HOST_POWER_SAVE_H__
#define __HOST_POWER_SAVE_H__

#include "sdkconfig.h"
#include "interface.h"

#if defined(CONFIG_SLAVE_CONTROLS_HOST) && defined(CONFIG_HOST_DEEP_SLEEP_ALLOWED)
  #define HOST_PS_ALLOWED 1
#endif

#ifdef CONFIG_HOST_DEEP_SLEEP_ALLOWED
  #define H_HOST_PS_ALLOWED 1
#else
  #define H_HOST_PS_ALLOWED 0
#endif

void host_power_save_init(void (*host_wakeup_callback)(void));
void host_power_save_deinit(void);
int is_host_wakeup_needed(interface_buffer_handle_t *buf_handle);
int wakeup_host_mandate(uint32_t timeout_ms);
int wakeup_host(uint32_t timeout_ms);
void host_power_save_alert(uint32_t ps_evt);
int is_host_power_saving(void);


#endif
