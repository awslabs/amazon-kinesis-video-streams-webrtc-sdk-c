// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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
#ifndef __TRANSPORT_DRV_H
#define __TRANSPORT_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/** Includes **/

#include "common.h"
#if 0
#include "os_wrapper.h"
#include "trace.h"
#endif
//#include "netdev_if.h"
#include "adapter.h"

/* ESP in sdkconfig has CONFIG_IDF_FIRMWARE_CHIP_ID entry.
 * supported values of CONFIG_IDF_FIRMWARE_CHIP_ID are - */
#define ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED (0xff)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32        (0x0)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32S2      (0x2)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C3      (0x5)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32S3      (0x9)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C2      (0xC)
#define ESP_PRIV_FIRMWARE_CHIP_ESP32C6      (0xD)


#if CONFIG_ESP_SPI_HOST_INTERFACE
#include "spi_wrapper.h"
#define SPI_MODE0                           (0)
#define SPI_MODE1                           (1)
#define SPI_MODE2                           (2)
#define SPI_MODE3                           (3)
#else
#include "sdio_wrapper.h"
#endif

struct esp_private {
	uint8_t     if_type;
	uint8_t     if_num;
	void        *netdev;
};

struct hosted_transport_context_t {
    uint8_t  *tx_buf;
    uint32_t  tx_buf_size;
    uint8_t  *rx_buf;
};

extern EventGroupHandle_t eg_h2s_wifi_flow_ctrl;

typedef int (*hosted_rxcb_t)(void *buffer, uint16_t len, void *free_buff_hdl);

typedef void (transport_free_cb_t)(void* buffer);
typedef esp_err_t (*transport_channel_tx_fn_t)(void *h, void *buffer, size_t len);
typedef esp_err_t (*transport_channel_rx_fn_t)(void *h, void *buffer, void * buff_to_free, size_t len);

typedef struct {
	void * api_chan;
	esp_hosted_if_type_t if_type;
    uint8_t secure;
	transport_channel_tx_fn_t tx;
	transport_channel_rx_fn_t rx;
	void *memp;
} transport_channel_t;

#if 0
/* netdev APIs*/
int esp_netdev_open(netdev_handle_t netdev);
int esp_netdev_close(netdev_handle_t netdev);
int esp_netdev_xmit(netdev_handle_t netdev, struct pbuf *net_buf);
#endif


esp_err_t setup_transport(void(*esp_hosted_up_cb)(void));
esp_err_t teardown_transport(void);
esp_err_t transport_drv_reconfigure(void);
transport_channel_t *transport_drv_add_channel(void *api_chan,
		esp_hosted_if_type_t if_type, uint8_t secure,
		transport_channel_tx_fn_t *tx, const transport_channel_rx_fn_t rx);
esp_err_t transport_drv_remove_channel(transport_channel_t *channel);

/* TODO To move to private header */
void process_capabilities(uint8_t cap);
void bus_init_internal(void);
void bus_deinit_internal(void);

void process_priv_communication(interface_buffer_handle_t *buf_handle);
void print_capabilities(uint32_t cap);
int process_init_event(uint8_t *evt_buf, uint16_t len);
esp_err_t send_slave_config(uint8_t host_cap, uint8_t firmware_chip_id,
		uint8_t raw_tp_direction, uint8_t low_thr_thesh, uint8_t high_thr_thesh);

uint8_t is_transport_rx_ready(void);
uint8_t is_transport_tx_ready(void);

#define H_BUFF_NO_ZEROCOPY 0
#define H_BUFF_ZEROCOPY 1

#define H_DEFLT_FREE_FUNC g_h.funcs->_h_free

#define MAX_RETRY_TRANSPORT_ACTIVE 1000


int esp_hosted_tx(uint8_t iface_type, uint8_t iface_num,
		uint8_t * buffer, uint16_t len, uint8_t buff_zerocopy, void (*free_buf_fun)(void* ptr));

int esp_hosted_register_wifi_rxcb(int ifx, hosted_rxcb_t fn);
int esp_hosted_register_wifi_txcb(int ifx, hosted_rxcb_t fn);

int serial_rx_handler(interface_buffer_handle_t * buf_handle);
void set_transport_state(uint8_t state);
#ifdef __cplusplus
}
#endif

#endif
