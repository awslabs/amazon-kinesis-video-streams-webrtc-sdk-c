// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#ifndef __HOSTED_HCI_DRV_H
#define __HOSTED_HCI_DRV_H

#include "common.h"

#include "esp_hosted_bt_config.h"

#if H_BT_ENABLED

#if H_BT_HOST_ESP_NIMBLE
#include "host/ble_hs_mbuf.h"
#endif

#endif

#define BLE_HCI_EVENT_HDR_LEN               (2)
#define BLE_HCI_CMD_HDR_LEN                 (3)

void hci_drv_init(void);

void hci_drv_show_configuration(void);

// Handles BT Rx
int hci_rx_handler(interface_buffer_handle_t *buf_handle);

#endif
