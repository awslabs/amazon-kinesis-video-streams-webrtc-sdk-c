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

/** Includes **/
#include "esp_wifi.h"
#include "transport_drv.h"
#include "adapter.h"
#include "stats.h"
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "serial_drv.h"
#include "serial_ll_if.h"
#include "esp_hosted_config.h"
#include "mempool.h"
#include "stats.h"
#include "errno.h"
#include "hci_drv.h"
#include "power_save_drv.h"
#include "hosted_os_adapter.h"
#include "os_wrapper.h"

/**
 * @brief  Slave capabilities are parsed
 *         Currently no added functionality to that
 * @param  None
 * @retval None
 */

DEFINE_LOG_TAG(transport);
static char chip_type = ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED;
void(*transport_esp_hosted_up_cb)(void) = NULL;
transport_channel_t *chan_arr[ESP_MAX_IF];

#if defined(H_H2S_WIFI_FLOW_CTRL)
event_group_handle_t eg_h2s_wifi_flow_ctrl;
#endif


static uint8_t transport_state = TRANSPORT_INACTIVE;

static void process_event(uint8_t *evt_buf, uint16_t len);

static void *init_timeout_timer = NULL;

static void init_timeout_cb(void *arg)
{
	ESP_LOGE(TAG, "Init event not received within timeout, Reseting myself");
	esp_restart();
}

uint8_t is_transport_rx_ready(void)
{
	return (transport_state >= TRANSPORT_RX_ACTIVE);
}

uint8_t is_transport_tx_ready(void)
{
	return (transport_state >= TRANSPORT_TX_ACTIVE);
}

#if !H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET
static void reset_slave(void)
{
	ESP_LOGW(TAG, "Reset slave using GPIO[%u]", H_GPIO_PIN_RESET_Pin);
	g_h.funcs->_h_config_gpio(H_GPIO_PIN_RESET_Port, H_GPIO_PIN_RESET_Pin, H_GPIO_MODE_DEF_OUTPUT);

	g_h.funcs->_h_write_gpio(H_GPIO_PIN_RESET_Port, H_GPIO_PIN_RESET_Pin, H_RESET_VAL_ACTIVE);
	g_h.funcs->_h_msleep(50);
	g_h.funcs->_h_write_gpio(H_GPIO_PIN_RESET_Port, H_GPIO_PIN_RESET_Pin, H_RESET_VAL_INACTIVE);
	g_h.funcs->_h_msleep(50);
	g_h.funcs->_h_write_gpio(H_GPIO_PIN_RESET_Port, H_GPIO_PIN_RESET_Pin, H_RESET_VAL_ACTIVE);

	/* stop transport transactions short time to avoid slave sync issues */
	g_h.funcs->_h_sleep(1);
}
#endif
static void transport_driver_event_handler(uint8_t event)
{
	switch(event)
	{
		case TRANSPORT_TX_ACTIVE:
		{
			/* Initiate control path now */
			ESP_LOGI(TAG, "Base transport is set-up\n\r");
			if (transport_esp_hosted_up_cb)
				transport_esp_hosted_up_cb();
			transport_state = TRANSPORT_TX_ACTIVE;
			break;
		}

		default:
		break;
	}
}

void set_transport_state(uint8_t state)
{
	transport_driver_event_handler(state);
}

static void transport_drv_init(void)
{
	bus_init_internal();
	hci_drv_init();

#if defined(H_H2S_WIFI_FLOW_CTRL)
	eg_h2s_wifi_flow_ctrl = g_h.funcs->_h_create_event_group();
	assert(eg_h2s_wifi_flow_ctrl);

	/* By default, clear flow control */
	g_h.funcs->_h_set_event_bit(eg_h2s_wifi_flow_ctrl,
			H_EVTGRP_BIT_FC_ALLOW_WIFI);

	#if defined(H_H2S_WIFI_FLOW_CTRL_DROP)
		ESP_LOGI(TAG, "H2S wifi_tx flow_ctrl policy: 'drop'");
	#elif defined(H_H2S_WIFI_FLOW_CTRL_BLOCK)
		ESP_LOGI(TAG, "H2S wifi_tx flow_ctrl policy: 'block'");
	#elif defined(H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT)
		ESP_LOGI(TAG, "H2S wifi_tx flow_ctrl policy: 'block_with_timeout[%lu]'",
				H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT_TICKS);
	#endif
#endif
}

esp_err_t teardown_transport(void)
{
	/* Stop and cleanup init timeout timer if still active */
	if (init_timeout_timer) {
		g_h.funcs->_h_timer_stop(init_timeout_timer);
		init_timeout_timer = NULL;
	}

	bus_deinit_internal();
	transport_state = TRANSPORT_INACTIVE;
	return ESP_OK;
}

esp_err_t setup_transport(void(*esp_hosted_up_cb)(void))
{
	g_h.funcs->_h_hosted_init_hook();
	transport_drv_init();
	transport_esp_hosted_up_cb = esp_hosted_up_cb;
	return ESP_OK;
}

esp_err_t transport_drv_reconfigure(void)
{
	static int retry_slave_connection = 0;

	ESP_LOGI(TAG, "Attempt connection with slave: retry[%u]", retry_slave_connection);

	set_host_wakeup_reason(esp_sleep_get_wakeup_cause());

	/* Start init timeout timer if not already started */
	if (!init_timeout_timer) {
		init_timeout_timer = g_h.funcs->_h_timer_start(10, RPC__TIMER_ONESHOT, init_timeout_cb, NULL);
		if (!init_timeout_timer) {
			ESP_LOGE(TAG, "Failed to create init timeout timer");
			return ESP_FAIL;
		}
		ESP_LOGI(TAG, "Started init timeout timer");
	}

	if (1) {
		if (is_host_reboot_due_to_deep_sleep()) {
			ESP_LOGI(TAG, "Host awake from sleep");
			transport_state = TRANSPORT_TX_ACTIVE;
		} else {
			ESP_LOGI(TAG, "Host bootup");
#if H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET
			transport_state = TRANSPORT_RX_ACTIVE;
#endif
		}
		/* No need to repeat the stuff, what slave has already ready with */
	}

#if !H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET
	/* This would come into picture, only if the host has
	 * reset pin connected to slave's 'EN' or 'RST' GPIO */
	if (!is_transport_tx_ready()) {
		reset_slave();
		transport_state = TRANSPORT_RX_ACTIVE;

		while (!is_transport_tx_ready()) {
			if (retry_slave_connection < MAX_RETRY_TRANSPORT_ACTIVE) {
				retry_slave_connection++;
				if (retry_slave_connection%10==0) {
					ESP_LOGE(TAG, "Not able to connect with ESP-Hosted slave device");
					reset_slave();
				}
			} else {
				ESP_LOGE(TAG, "Failed to get ESP_Hosted slave transport up");
				return ESP_FAIL;
			}
			g_h.funcs->_h_sleep(1);
		}
	} else {
		ESP_LOGI(TAG, "Transport is already up");
	}
#endif
	retry_slave_connection = 0;

	return ESP_OK;
}

esp_err_t transport_drv_remove_channel(transport_channel_t *channel)
{
	if (!channel)
		return ESP_FAIL;

	switch (channel->if_type) {
	case ESP_AP_IF:
	case ESP_STA_IF:
		//Should we additionally do:
		//esp_wifi_internal_reg_rxcb(channel->if_type, NULL);
		break;
	case ESP_SERIAL_IF:
		/* TODO */
		break;
	default:
		break;
	}

	assert(chan_arr[channel->if_type] == channel);

    mempool_destroy(channel->memp);
	chan_arr[channel->if_type] = NULL;
	HOSTED_FREE(channel);

    return ESP_OK;
}

static void transport_sta_free_cb(void *buf)
{
	mempool_free(chan_arr[ESP_STA_IF]->memp, buf);
}

static void transport_ap_free_cb(void *buf)
{
	mempool_free(chan_arr[ESP_AP_IF]->memp, buf);
}

static void transport_serial_free_cb(void *buf)
{
	mempool_free(chan_arr[ESP_SERIAL_IF]->memp, buf);
}

static esp_err_t transport_drv_sta_tx(void *h, void *buffer, size_t len)
{
	void * copy_buff = NULL;

	if (!buffer || !len)
		return ESP_OK;

#if defined(H_H2S_WIFI_FLOW_CTRL)
	uint32_t timeout = 0;

  #if defined(H_H2S_WIFI_FLOW_CTRL_DROP)
	timeout = 0;
  #elif defined(H_H2S_WIFI_FLOW_CTRL_BLOCK)
	timeout = HOSTED_BLOCK_MAX;
  #elif defined(H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT)
	timeout = H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT_TICKS;
  #endif

	uint32_t start_time_us = esp_timer_get_time() & 0xFFFFFFFF;
	uint8_t timed_out = 0;

	do {
		hosted_event_bits_t bits = g_h.funcs->_h_wait_for_event_bits(eg_h2s_wifi_flow_ctrl,
				H_EVTGRP_BIT_FC_ALLOW_WIFI, 0 /* ticks to wait */);
		if ((bits & H_EVTGRP_BIT_FC_ALLOW_WIFI) != H_EVTGRP_BIT_FC_ALLOW_WIFI) {
			/* by default allow wifi bit would be set, to allow Wi-Fi traffic.
			 * If the flow control is on, the bit allow_wifi would be cleared.
			 * Wait for timeout duration to let allow_wifi to set.
			 * After timeout, if allow_wifi still not set, drop packet
			 */
			uint32_t cur_time_us = esp_timer_get_time() & 0xFFFFFFFF;
			uint32_t elapsed_time;

			if (cur_time_us >= start_time_us) {
				/* No rollover */
				elapsed_time = cur_time_us - start_time_us;
			} else {
				/* Rollover */
				elapsed_time = (UINT32_MAX - start_time_us + 1) + cur_time_us;
			}
			if ( elapsed_time >= timeout) {
				timed_out = 1;
			} else {
				vPortYield();
			}
		} else {
			break;
		}
	} while (!timed_out);


	if (unlikely(timed_out)) {
#if ESP_PKT_STATS
		pkt_stats.sta_tx_flowctrl_drop++;
#endif

#if defined(ESP_ERR_ESP_NETIF_TX_FAILED)
		errno = -ENOBUFS;
		return ESP_ERR_ESP_NETIF_TX_FAILED;
#else
		errno = -ENOMEM;
		return ESP_ERR_NO_MEM;
#endif
	}
#endif

	assert(h && h==chan_arr[ESP_STA_IF]->api_chan);

	/*  Prepare transport buffer directly consumable */
	copy_buff = mempool_alloc(((struct mempool*)chan_arr[ESP_STA_IF]->memp), MAX_TRANSPORT_BUFFER_SIZE, true);
	assert(copy_buff);
	g_h.funcs->_h_memcpy((uint8_t *) copy_buff + H_ESP_PAYLOAD_HEADER_OFFSET, buffer, len);

	return esp_hosted_tx(ESP_STA_IF, 0, copy_buff, len, H_BUFF_ZEROCOPY, transport_sta_free_cb);
}

static esp_err_t transport_drv_ap_tx(void *h, void *buffer, size_t len)
{
	void * copy_buff = NULL;

	if (!buffer || !len)
    return ESP_OK;

	assert(h && h==chan_arr[ESP_AP_IF]->api_chan);

	/*  Prepare transport buffer directly consumable */
	copy_buff = mempool_alloc(((struct mempool*)chan_arr[ESP_AP_IF]->memp), MAX_TRANSPORT_BUFFER_SIZE, true);
	assert(copy_buff);
	g_h.funcs->_h_memcpy((uint8_t *) copy_buff + H_ESP_PAYLOAD_HEADER_OFFSET, buffer, len);

	return esp_hosted_tx(ESP_AP_IF, 0, copy_buff, len, H_BUFF_ZEROCOPY, transport_ap_free_cb);
}

esp_err_t transport_drv_serial_tx(void *h, void *buffer, size_t len)
{
	/* TODO */
	assert(h && h==chan_arr[ESP_SERIAL_IF]->api_chan);
	return esp_hosted_tx(ESP_SERIAL_IF, 0, buffer, len, H_BUFF_NO_ZEROCOPY, transport_serial_free_cb);
}


transport_channel_t *transport_drv_add_channel(void *api_chan,
		esp_hosted_if_type_t if_type, uint8_t secure,
		transport_channel_tx_fn_t *tx, const transport_channel_rx_fn_t rx)
{
	transport_channel_t *channel = NULL;

	ESP_ERROR_CHECK(if_type >= ESP_MAX_IF);

	if (!tx || !rx) {
		ESP_LOGE(TAG, "%s fail for IF[%u]: tx or rx is NULL", __func__, if_type );
		return NULL;
	}

	if (chan_arr[if_type]) {
		/* Channel config already existed */
		ESP_LOGW(TAG, "Channel [%u] already created, replace with new callbacks", if_type);
		HOSTED_FREE(chan_arr[if_type]);
	}


	chan_arr[if_type] = g_h.funcs->_h_calloc(sizeof(transport_channel_t), 1);
	assert(chan_arr[if_type]);
	channel = chan_arr[if_type];

	switch (if_type) {

	case ESP_STA_IF:
		*tx = transport_drv_sta_tx;
		break;

	case ESP_AP_IF:
		*tx = transport_drv_ap_tx;
		break;

	case ESP_SERIAL_IF:
		*tx = transport_drv_serial_tx;
		break;

	default:
		//*tx = transport_drv_tx;
		ESP_LOGW(TAG, "Not yet suppported ESP_Hosted interface for if_type[%u]", if_type);
		return NULL;
	}

	channel->api_chan = api_chan;
	channel->if_type = if_type;
	channel->secure = secure;
	channel->tx = *tx;
	channel->rx = rx;

	/* Need to change size wrt transport */
	channel->memp = mempool_create(MAX_TRANSPORT_BUFFER_SIZE);
#ifdef CONFIG_ESP_USE_MEMPOOL
	assert(channel->memp);
#endif

	ESP_LOGI(TAG, "Add ESP-Hosted channel IF[%u]: S[%u] Tx[%p] Rx[%p]",
			if_type, secure, *tx, rx);

	return channel;
}

void process_capabilities(uint8_t cap)
{
	ESP_LOGI(TAG, "capabilities: 0x%x",cap);
}

void process_priv_communication(interface_buffer_handle_t *buf_handle)
{
	if (!buf_handle || !buf_handle->payload || !buf_handle->payload_len)
		return;

	process_event(buf_handle->payload, buf_handle->payload_len);
}

void print_capabilities(uint32_t cap)
{
	ESP_LOGI(TAG, "Features supported are:");
	if (cap & ESP_WLAN_SDIO_SUPPORT)
		ESP_LOGI(TAG, "\t * WLAN");
	if ((cap & ESP_BT_UART_SUPPORT) || (cap & ESP_BT_SDIO_SUPPORT)) {
		ESP_LOGI(TAG, "\t * BT/BLE");
		if (cap & ESP_BT_UART_SUPPORT)
			ESP_LOGI(TAG, "\t   - HCI over UART");
		if (cap & ESP_BT_SDIO_SUPPORT)
			ESP_LOGI(TAG, "\t   - HCI over SDIO");
		if ((cap & ESP_BLE_ONLY_SUPPORT) && (cap & ESP_BR_EDR_ONLY_SUPPORT))
			ESP_LOGI(TAG, "\t   - BT/BLE dual mode");
		else if (cap & ESP_BLE_ONLY_SUPPORT)
			ESP_LOGI(TAG, "\t   - BLE only");
		else if (cap & ESP_BR_EDR_ONLY_SUPPORT)
			ESP_LOGI(TAG, "\t   - BR EDR only");
	}
}

static void process_event(uint8_t *evt_buf, uint16_t len)
{
	int ret = 0;
	struct esp_priv_event *event;

	if (!evt_buf || !len)
		return;

	event = (struct esp_priv_event *) evt_buf;

	if (event->event_type == ESP_PRIV_EVENT_INIT) {

		ESP_LOGI(TAG, "Received INIT event from ESP32 peripheral");
		ESP_HEXLOGD("Slave_init_evt", event->event_data, event->event_len, 32);

		ret = process_init_event(event->event_data, event->event_len);
		if (ret) {
			ESP_LOGE(TAG, "failed to init event\n\r");
		} else {

#if H_HOST_PS_ALLOWED && H_HOST_WAKEUP_GPIO
			power_save_drv_init(H_HOST_WAKEUP_GPIO);
#elif H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET
			#error "Slave reset mandatory if Host Deep-sleep Wakeup GPIO is not configured"
#endif
		}
	} else {
		ESP_LOGW(TAG, "Drop unknown event\n\r");
	}
}

static esp_err_t get_chip_str_from_id(int chip_id, char* chip_str)
{
	int ret = ESP_OK;
	assert(chip_str);

	switch(chip_id) {
	case ESP_PRIV_FIRMWARE_CHIP_ESP32:
		strcpy(chip_str, "esp32");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C2:
		strcpy(chip_str, "esp32c2");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C3:
		strcpy(chip_str, "esp32c3");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32C6:
		strcpy(chip_str, "esp32c6");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32S2:
		strcpy(chip_str, "esp32s2");
		break;
	case ESP_PRIV_FIRMWARE_CHIP_ESP32S3:
		strcpy(chip_str, "esp32s3");
		break;
	default:
		ESP_LOGW(TAG, "Unsupported chip id: %u", chip_id);
		strcpy(chip_str, "unsupported");
		ret = ESP_FAIL;
		break;
	}
	return ret;
}

static void verify_host_config_for_slave(uint8_t chip_type)
{
	uint8_t exp_chip_id = 0xff;


#if CONFIG_SLAVE_CHIPSET_ESP32
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32;
#elif CONFIG_SLAVE_CHIPSET_ESP32C2
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C2;
#elif CONFIG_SLAVE_CHIPSET_ESP32C3
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C3;
#elif CONFIG_SLAVE_CHIPSET_ESP32C6
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32C6;
#elif CONFIG_SLAVE_CHIPSET_ESP32S2
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32S2;
#elif CONFIG_SLAVE_CHIPSET_ESP32S3
	exp_chip_id = ESP_PRIV_FIRMWARE_CHIP_ESP32S3;
#else
	ESP_LOGW(TAG, "Incorrect host config for ESP slave chipset[%x]", chip_type);
	assert(0!=0);
#endif
	if (chip_type!=exp_chip_id) {
		char slave_str[20], exp_str[20];

		memset(slave_str, '\0', 20);
		memset(exp_str, '\0', 20);

		get_chip_str_from_id(chip_type, slave_str);
		get_chip_str_from_id(exp_chip_id, exp_str);
		ESP_LOGE(TAG, "Identified slave [%s] != Expected [%s]\n\t\trun 'idf.py menuconfig' at host to reselect the slave?\n\t\tAborting.. ", slave_str, exp_str);
		sleep(10);
		assert(0!=0);
	}
}

esp_err_t send_slave_config(uint8_t host_cap, uint8_t firmware_chip_id,
		uint8_t raw_tp_direction, uint8_t low_thr_thesh, uint8_t high_thr_thesh)
{
#define LENGTH_1_BYTE 1
	struct esp_priv_event *event = NULL;
	uint8_t *pos = NULL;
	uint16_t len = 0;
	uint8_t *sendbuf = NULL;

	sendbuf = malloc(512); /*Arbitrary number*/
	assert(sendbuf);

	/* Populate event data */
	//event = (struct esp_priv_event *) (sendbuf + sizeof(struct esp_payload_header)); //ZeroCopy
	event = (struct esp_priv_event *) (sendbuf);

	event->event_type = ESP_PRIV_EVENT_INIT;

	/* Populate TLVs for event */
	pos = event->event_data;

	/* TLVs start */

	/* TLV - Board type */
	ESP_LOGI(TAG, "Slave chip Id[%x]", ESP_PRIV_FIRMWARE_CHIP_ID);
	*pos = HOST_CAPABILITIES;                          pos++;len++;
	*pos = LENGTH_1_BYTE;                              pos++;len++;
	*pos = host_cap;                                   pos++;len++;

	/* TLV - Capability */
	*pos = RCVD_ESP_FIRMWARE_CHIP_ID;                  pos++;len++;
	*pos = LENGTH_1_BYTE;                              pos++;len++;
	*pos = firmware_chip_id;                           pos++;len++;

	*pos = SLV_CONFIG_TEST_RAW_TP;                     pos++;len++;
	*pos = LENGTH_1_BYTE;                              pos++;len++;
	*pos = raw_tp_direction;                           pos++;len++;

	*pos = SLV_CFG_FLOW_CTL_START_THRESHOLD;           pos++;len++;
	*pos = LENGTH_1_BYTE;                              pos++;len++;
	*pos = high_thr_thesh;                             pos++;len++;

	*pos = SLV_CFG_FLOW_CTL_CLEAR_THRESHOLD;           pos++;len++;
	*pos = LENGTH_1_BYTE;                              pos++;len++;
	*pos = low_thr_thesh;                              pos++;len++;

	ESP_LOGI(TAG, "raw_tp_dir[%s], flow_ctrl: low[%u] high[%u]",
			raw_tp_direction == ESP_TEST_RAW_TP__HOST_TO_ESP? "h2s":
			raw_tp_direction == ESP_TEST_RAW_TP__ESP_TO_HOST? "s2h":
			raw_tp_direction == ESP_TEST_RAW_TP__BIDIRECTIONAL? "bi-dir":
			"-", low_thr_thesh, high_thr_thesh);

	/* TLVs end */

	event->event_len = len;

	/* payload len = Event len + sizeof(event type) + sizeof(event len) */
	len += 2;

	return esp_hosted_tx(ESP_PRIV_IF, 0, sendbuf, len, H_BUFF_NO_ZEROCOPY, free);
}

int process_init_event(uint8_t *evt_buf, uint16_t len)
{
	uint8_t len_left = len, tag_len;
	uint8_t *pos;
	uint8_t raw_tp_config = H_TEST_RAW_TP_DIR;

	if (!evt_buf)
		return ESP_FAIL;

	/* Stop and delete the init timeout timer since we received the init event */
	if (init_timeout_timer) {
		g_h.funcs->_h_timer_stop(init_timeout_timer);
		init_timeout_timer = NULL;
		ESP_LOGI(TAG, "Init event received within timeout, cleared timer");
	}

	pos = evt_buf;
	ESP_LOGD(TAG, "Init event length: %u", len);
	if (len > 64) {
		ESP_LOGE(TAG, "Init event length: %u", len);
#if CONFIG_ESP_SPI_HOST_INTERFACE
		ESP_LOGE(TAG, "Seems incompatible SPI mode try changing SPI mode. Asserting for now.");
#endif
		assert(len < 64);
	}

	while (len_left) {
		tag_len = *(pos + 1);

		if (*pos == ESP_PRIV_CAPABILITY) {
			ESP_LOGI(TAG, "EVENT: %2x", *pos);
			process_capabilities(*(pos + 2));
			print_capabilities(*(pos + 2));
		} else if (*pos == ESP_PRIV_FIRMWARE_CHIP_ID) {
			ESP_LOGI(TAG, "EVENT: %2x", *pos);
			chip_type = *(pos+2);
			verify_host_config_for_slave(chip_type);
		} else if (*pos == ESP_PRIV_TEST_RAW_TP) {
			ESP_LOGI(TAG, "EVENT: %2x", *pos);
#if TEST_RAW_TP
			process_test_capabilities(*(pos + 2));
#else
			if (*(pos + 2))
				ESP_LOGW(TAG, "Slave enabled Raw Throughput Testing, but not enabled on Host");
#endif
		} else if (*pos == ESP_PRIV_RX_Q_SIZE) {
			ESP_LOGD(TAG, "slave rx queue size: %u", *(pos + 2));
		} else if (*pos == ESP_PRIV_TX_Q_SIZE) {
			ESP_LOGD(TAG, "slave tx queue size: %u", *(pos + 2));
		} else {
			ESP_LOGD(TAG, "Unsupported EVENT: %2x", *pos);
		}
		pos += (tag_len+2);
		len_left -= (tag_len+2);
	}

	if ((chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32) &&
		(chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S2) &&
		(chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C2) &&
		(chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C3) &&
		(chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C6) &&
		(chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S3)) {
		ESP_LOGI(TAG, "ESP board type is not mentioned, ignoring [%d]\n\r", chip_type);
		chip_type = ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED;
		return -1;
	} else {
		ESP_LOGI(TAG, "ESP board type is : %d \n\r", chip_type);
	}

	transport_driver_event_handler(TRANSPORT_TX_ACTIVE);
	return send_slave_config(0, chip_type, raw_tp_config,
		H_H2S_WIFI_FLOW_CTRL_LOW_TH,
		H_H2S_WIFI_FLOW_CTRL_HIGH_TH);
}

int serial_rx_handler(interface_buffer_handle_t * buf_handle)
{
	return serial_ll_rx_handler(buf_handle);
}
