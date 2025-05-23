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
//

#ifndef __TRANSPORT_LAYER_INTERFACE_H
#define __TRANSPORT_LAYER_INTERFACE_H
#include "esp_err.h"
#include "esp_hosted_log.h"

#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE

#if defined(CONFIG_IDF_TARGET_ESP32)||defined(CONFIG_IDF_TARGET_ESP32C6)
	#include "driver/sdio_slave.h"
#else
	#error "SDIO is not supported for this chipset"
#endif

#endif

typedef enum {
	LENGTH_1_BYTE  = 1,
	LENGTH_2_BYTE  = 2,
	LENGTH_3_BYTE  = 3,
	LENGTH_4_BYTE  = 4,
} byte_length;

typedef void *wlan_buf_handle_t;

typedef enum {
	SDIO = 0,
	SPI = 1,
} transport_layer;

typedef enum {
	DEINIT,
	INIT,
	ACTIVE,
	DEACTIVE,
} INTERFACE_STATE;

typedef struct {
	union {
#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE
		sdio_slave_buf_handle_t sdio_buf_handle;
#endif
		wlan_buf_handle_t	wlan_buf_handle;
		void *priv_buffer_handle;
	};
	uint8_t if_type;
	uint8_t if_num;
	uint8_t *payload;
	uint8_t flag;
	uint16_t payload_len;
	uint16_t seq_num;
	uint8_t flow_ctl_en;

	void (*free_buf_handle)(void *buf_handle);
} interface_buffer_handle_t;

typedef struct {
	/*
	union {
	} phy_context;
	*/
	INTERFACE_STATE state;
}interface_handle_t;

#if CONFIG_ESP_SPI_HOST_INTERFACE
#define MAX_TRANSPORT_BUF_SIZE 1600
#elif CONFIG_ESP_SDIO_HOST_INTERFACE
#define MAX_TRANSPORT_BUF_SIZE 1536
#endif

#define BSSID_BYTES_SIZE       6

typedef struct {
	interface_handle_t * (*init)(void);
	int32_t (*write)(interface_handle_t *handle, interface_buffer_handle_t *buf_handle);
	int (*read)(interface_handle_t *handle, interface_buffer_handle_t *buf_handle);
	esp_err_t (*reset)(interface_handle_t *handle);
	void (*deinit)(interface_handle_t *handle);
} if_ops_t;

typedef struct {
	transport_layer type;
	void *priv;
	if_ops_t *if_ops;
	int (*event_handler)(uint8_t bitmap);
} interface_context_t;

typedef struct {
	uint8_t flow_ctl_start_thres;
	uint8_t flow_ctl_clear_thres;
} slave_config_t;

typedef struct {
	uint8_t flow_ctl_wifi;
	uint8_t flow_ctl_wifi_to_send;
} slave_state_t;

interface_context_t * interface_insert_driver(int (*callback)(uint8_t val));
int interface_remove_driver();
void generate_startup_event(uint8_t cap);
int send_to_host_queue(interface_buffer_handle_t *buf_handle, uint8_t queue_type);
uint32_t get_cur_back_pressure(void);
void send_dhcp_dns_info_to_host(uint8_t send_wifi_connected);

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

extern slave_config_t slv_cfg_g;
extern slave_state_t  slv_state_g;
#endif
