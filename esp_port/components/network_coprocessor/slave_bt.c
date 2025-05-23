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

#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "slave_bt.h"
#include "interface.h"
#include "adapter.h"

#if CONFIG_NW_COPROC_BT_ENABLED
#ifdef CONFIG_BT_ENABLED
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "soc/lldesc.h"
#include "esp_mac.h"


#if BT_OVER_C3_S3

  #include "esp_private/gdma.h"

  #if BLUETOOTH_UART
    #include "hal/uhci_ll.h"
  #endif

#endif

static const char *TAG = "h_bt";

#if BLUETOOTH_HCI
/* ***** HCI specific part ***** */

#define VHCI_MAX_TIMEOUT_MS 	2000
static SemaphoreHandle_t vhci_send_sem;

static void controller_rcv_pkt_ready(void)
{
	if (vhci_send_sem)
		xSemaphoreGive(vhci_send_sem);
}

static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
	interface_buffer_handle_t buf_handle;
	uint8_t *buf = NULL;

	buf = (uint8_t *) malloc(len);

	if (!buf) {
		ESP_LOGE(TAG, "HCI Send packet: memory allocation failed");
		return ESP_FAIL;
	}

	memcpy(buf, data, len);

	memset(&buf_handle, 0, sizeof(buf_handle));

	buf_handle.if_type = ESP_HCI_IF;
	buf_handle.if_num = 0;
	buf_handle.payload_len = len;
	buf_handle.payload = buf;
	buf_handle.wlan_buf_handle = buf;
	buf_handle.free_buf_handle = free;

	ESP_HEXLOGV("bt_tx new", data, len, 32);

	if (send_to_host_queue(&buf_handle, PRIO_Q_BT)) {
		free(buf);
		return ESP_FAIL;
	}

	return 0;
}

static esp_vhci_host_callback_t vhci_host_cb = {
	controller_rcv_pkt_ready,
	host_rcv_pkt
};

void process_hci_rx_pkt(uint8_t *payload, uint16_t payload_len) {
	/* VHCI needs one extra byte at the start of payload */
	/* that is accomodated in esp_payload_header */
	ESP_HEXLOGV("bt_rx", payload, payload_len, 32);

	payload--;
	payload_len++;

	if (!esp_vhci_host_check_send_available()) {
		ESP_LOGD(TAG, "VHCI not available");
	}

#if SOC_ESP_NIMBLE_CONTROLLER
	esp_vhci_host_send_packet(payload, payload_len);
#else
	if (vhci_send_sem) {
		if (xSemaphoreTake(vhci_send_sem, VHCI_MAX_TIMEOUT_MS) == pdTRUE) {
			esp_vhci_host_send_packet(payload, payload_len);
		} else {
			ESP_LOGI(TAG, "VHCI sem timeout");
		}
	}
#endif
}

#elif BLUETOOTH_UART
/* ***** UART specific part ***** */

#if BT_OVER_C3_S3
// Operation functions for HCI UART Transport Layer
static bool hci_uart_tl_init(void);
static void hci_uart_tl_deinit(void);
static void hci_uart_tl_recv_async(uint8_t *buf, uint32_t size, esp_bt_hci_tl_callback_t callback, void *arg);
static void hci_uart_tl_send_async(uint8_t *buf, uint32_t size, esp_bt_hci_tl_callback_t callback, void *arg);
static void hci_uart_tl_flow_on(void);
static bool hci_uart_tl_flow_off(void);
static void hci_uart_tl_finish_transfers(void);

struct uart_txrxchannel {
    esp_bt_hci_tl_callback_t callback;
    void *arg;
    lldesc_t link;
};

struct uart_env_tag {
    struct uart_txrxchannel tx;
    struct uart_txrxchannel rx;
};

struct uart_env_tag uart_env;

static volatile uhci_dev_t *s_uhci_hw = &UHCI0;
static gdma_channel_handle_t s_rx_channel;
static gdma_channel_handle_t s_tx_channel;

static esp_bt_hci_tl_t s_hci_uart_tl_funcs = {
    ._magic = ESP_BT_HCI_TL_MAGIC_VALUE,
    ._version = ESP_BT_HCI_TL_VERSION,
    ._reserved = 0,
    ._open = (void *)hci_uart_tl_init,
    ._close = (void *)hci_uart_tl_deinit,
    ._finish_transfers = (void *)hci_uart_tl_finish_transfers,
    ._recv = (void *)hci_uart_tl_recv_async,
    ._send = (void *)hci_uart_tl_send_async,
    ._flow_on = (void *)hci_uart_tl_flow_on,
    ._flow_off = (void *)hci_uart_tl_flow_off,
};

static bool hci_uart_tl_init(void)
{
    return true;
}

static void hci_uart_tl_deinit(void)
{
}

static IRAM_ATTR void hci_uart_tl_recv_async(uint8_t *buf, uint32_t size,
                        esp_bt_hci_tl_callback_t callback, void *arg)
{
    assert(buf != NULL);
    assert(size != 0);
    assert(callback != NULL);
    uart_env.rx.callback = callback;
    uart_env.rx.arg = arg;

    memset(&uart_env.rx.link, 0, sizeof(lldesc_t));
    uart_env.rx.link.buf = buf;
    uart_env.rx.link.size = size;

    s_uhci_hw->pkt_thres.thrs = size;

    gdma_start(s_rx_channel, (intptr_t)(&uart_env.rx.link));
}

static IRAM_ATTR void hci_uart_tl_send_async(uint8_t *buf, uint32_t size,
                        esp_bt_hci_tl_callback_t callback, void *arg)
{
    assert(buf != NULL);
    assert(size != 0);
    assert(callback != NULL);

    uart_env.tx.callback = callback;
    uart_env.tx.arg = arg;

    memset(&uart_env.tx.link, 0, sizeof(lldesc_t));
    uart_env.tx.link.length = size;
    uart_env.tx.link.buf = buf;
    uart_env.tx.link.eof = 1;

    gdma_start(s_tx_channel, (intptr_t)(&uart_env.tx.link));
}

static void hci_uart_tl_flow_on(void)
{
}

static bool hci_uart_tl_flow_off(void)
{
    return true;
}

static void hci_uart_tl_finish_transfers(void)
{
}

static IRAM_ATTR bool hci_uart_tl_rx_eof_callback(gdma_channel_handle_t dma_chan,
                                gdma_event_data_t *event_data, void *user_data)
{
    assert(dma_chan == s_rx_channel);
    assert(uart_env.rx.callback != NULL);
    esp_bt_hci_tl_callback_t callback = uart_env.rx.callback;
    void *arg = uart_env.rx.arg;

    // clear callback pointer
    uart_env.rx.callback = NULL;
    uart_env.rx.arg = NULL;

    // call handler
    callback(arg, ESP_BT_HCI_TL_STATUS_OK);

    // send notification to Bluetooth Controller task
    esp_bt_h4tl_eif_io_event_notify(1);

    return true;
}

static IRAM_ATTR bool hci_uart_tl_tx_eof_callback(gdma_channel_handle_t dma_chan,
                                gdma_event_data_t *event_data, void *user_data)
{
    assert(dma_chan == s_tx_channel);
    assert(uart_env.tx.callback != NULL);
    esp_bt_hci_tl_callback_t callback = uart_env.tx.callback;
    void *arg = uart_env.tx.arg;

    // clear callback pointer
    uart_env.tx.callback = NULL;
    uart_env.tx.arg = NULL;

    // call handler
    callback(arg, ESP_BT_HCI_TL_STATUS_OK);

    // send notification to Bluetooth Controller task
    esp_bt_h4tl_eif_io_event_notify(1);

    return true;
}
#endif

#if BT_OVER_C3_S3
static void init_uart_c3_s3(void)
{
	ESP_LOGD(TAG, "Set-up BLE for ESP32-C3/ESP32-S3");
#if BLUETOOTH_UART == 1
	periph_module_enable(PERIPH_UART1_MODULE);
    periph_module_reset(PERIPH_UART1_MODULE);
#elif BLUETOOTH_UART == 2
	periph_module_enable(PERIPH_UART2_MODULE);
    periph_module_reset(PERIPH_UART2_MODULE);
#endif

	periph_module_enable(PERIPH_UHCI0_MODULE);
    periph_module_reset(PERIPH_UHCI0_MODULE);

    gpio_config_t io_output_conf = {
        .intr_type = DISABLE_INTR_ON_GPIO,    /* Disable interrupt */
        .mode = GPIO_MODE_OUTPUT,             /* Output mode */
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,  /* Bit mask of the output pins */
        .pull_down_en = 0,                    /* Disable pull-down mode */
        .pull_up_en = 0,                      /* Disable pull-up mode */
    };
    gpio_config(&io_output_conf);

    gpio_config_t io_input_conf = {
        .intr_type = DISABLE_INTR_ON_GPIO,    /* Disable interrupt */
        .mode = GPIO_MODE_INPUT,              /* Input mode */
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,   /* Bit mask of the input pins */
        .pull_down_en = 0,                    /* Disable pull-down mode */
        .pull_up_en = 0,                      /* Disable pull-down mode */
    };
    gpio_config(&io_input_conf);

	ESP_ERROR_CHECK( uart_set_pin(BLUETOOTH_UART, BT_TX_PIN,
				BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN) );
	ESP_LOGI(TAG, "UART Pins: Tx:%u Rx:%u RTS:%u CTS:%u",
			BT_TX_PIN, BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN);

    // configure UART1
    ESP_LOGI(TAG, "baud rate for HCI uart :: %d \n",
			CONFIG_EXAMPLE_HCI_UART_BAUDRATE);

    uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_HCI_UART_BAUDRATE,

        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = UART_RX_THRS,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(BLUETOOTH_UART, &uart_config));

    // install DMA driver
    gdma_channel_alloc_config_t tx_channel_config = {
        .flags.reserve_sibling = 1,
        .direction = GDMA_CHANNEL_DIRECTION_TX,
    };
    ESP_ERROR_CHECK(gdma_new_channel(&tx_channel_config, &s_tx_channel));
    gdma_channel_alloc_config_t rx_channel_config = {
        .direction = GDMA_CHANNEL_DIRECTION_RX,
        .sibling_chan = s_tx_channel,
    };
    ESP_ERROR_CHECK(gdma_new_channel(&rx_channel_config, &s_rx_channel));

    gdma_connect(s_tx_channel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_UHCI, 0));
    gdma_connect(s_rx_channel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_UHCI, 0));

    gdma_strategy_config_t strategy_config = {
        .auto_update_desc = false,
        .owner_check = false
    };
    gdma_apply_strategy(s_tx_channel, &strategy_config);
    gdma_apply_strategy(s_rx_channel, &strategy_config);

    gdma_rx_event_callbacks_t rx_cbs = {
        .on_recv_eof = hci_uart_tl_rx_eof_callback
    };
    gdma_register_rx_event_callbacks(s_rx_channel, &rx_cbs, NULL);

    gdma_tx_event_callbacks_t tx_cbs = {
        .on_trans_eof = hci_uart_tl_tx_eof_callback
    };
    gdma_register_tx_event_callbacks(s_tx_channel, &tx_cbs, NULL);

    // configure UHCI
    uhci_ll_init(s_uhci_hw);
    uhci_ll_set_eof_mode(s_uhci_hw, UHCI_RX_LEN_EOF);
    // disable software flow control
    s_uhci_hw->escape_conf.val = 0;
    uhci_ll_attach_uart_port(s_uhci_hw, 1);
}
#endif

#if CONFIG_IDF_TARGET_ESP32
static void init_uart_esp32(void)
{
	ESP_LOGD(TAG, "Set-up BLE for ESP32");
#if BLUETOOTH_UART == 1
	periph_module_enable(PERIPH_UART1_MODULE);
	periph_module_reset(PERIPH_UART1_MODULE);
#elif BLUETOOTH_UART == 2
	periph_module_enable(PERIPH_UART2_MODULE);
	periph_module_reset(PERIPH_UART2_MODULE);
#endif

	periph_module_enable(PERIPH_UHCI0_MODULE);
	periph_module_reset(PERIPH_UHCI0_MODULE);


	ESP_ERROR_CHECK( uart_set_pin(BLUETOOTH_UART, BT_TX_PIN,
		BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN) );
	ESP_LOGI(TAG, "UART Pins: Tx:%u Rx:%u RTS:%u CTS:%u",
			BT_TX_PIN, BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN);

}
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C6))
static void init_uart_c2_c6(void)
{
    ESP_LOGD(TAG, "Set-up BLE for ESP32-C2/C6");

#if defined(CONFIG_IDF_TARGET_ESP32C2)
    ESP_LOGI(TAG, "UART Pins: Tx:%u Rx:%u", BT_TX_PIN, BT_RX_PIN);
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    //ESP_ERROR_CHECK( uart_set_pin(BLUETOOTH_UART, BT_TX_PIN,
    //  BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN) );
    ESP_LOGI(TAG, "UART Pins: Tx:%u Rx:%u", BT_TX_PIN, BT_RX_PIN);
    //ESP_LOGI(TAG, "UART Pins: Tx:%u Rx:%u RTS:%u CTS:%u",
            //BT_TX_PIN, BT_RX_PIN, BT_RTS_PIN, BT_CTS_PIN);
#endif
}
#endif

void init_uart(void)
{
#if CONFIG_IDF_TARGET_ESP32
	init_uart_esp32();
#elif (defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C6))
	init_uart_c2_c6();
#elif BT_OVER_C3_S3
	init_uart_c3_s3();
#endif
}
#endif

#if BLUETOOTH_HCI
#if SOC_ESP_NIMBLE_CONTROLLER
#include "nimble/ble_hci_trans.h"

typedef enum {
    DATA_TYPE_COMMAND = 1,
    DATA_TYPE_ACL     = 2,
    DATA_TYPE_SCO     = 3,
    DATA_TYPE_EVENT   = 4
} serial_data_type_t;

/* Host-to-controller command. */
#define BLE_HCI_TRANS_BUF_CMD       3

/* ACL_DATA_MBUF_LEADINGSPCAE: The leadingspace in user info header for ACL data */
#define ACL_DATA_MBUF_LEADINGSPCAE    4

void esp_vhci_host_send_packet(uint8_t *data, uint16_t len)
{
    if (*(data) == DATA_TYPE_COMMAND) {
        struct ble_hci_cmd *cmd = NULL;
        cmd = (struct ble_hci_cmd *) ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_CMD);
	if (!cmd) {
		ESP_LOGE(TAG, "Failed to allocate memory for HCI transport buffer");
		return;
	}

        memcpy((uint8_t *)cmd, data + 1, len - 1);
        ble_hci_trans_hs_cmd_tx((uint8_t *)cmd);
    }

    if (*(data) == DATA_TYPE_ACL) {
        struct os_mbuf *om = os_msys_get_pkthdr(len, ACL_DATA_MBUF_LEADINGSPCAE);
        assert(om);
        os_mbuf_append(om, &data[1], len - 1);
        ble_hci_trans_hs_acl_tx(om);
    }

}

bool esp_vhci_host_check_send_available() {
    // not need to check in esp new controller
    return true;
}

int
ble_hs_hci_rx_evt(uint8_t *hci_ev, void *arg)
{
    uint16_t len = hci_ev[1] + 3;
    uint8_t *data = (uint8_t *)malloc(len);
    data[0] = 0x04;
    memcpy(&data[1], hci_ev, len - 1);
    ble_hci_trans_buf_free(hci_ev);
    vhci_host_cb.notify_host_recv(data, len);
    free(data);
    return 0;
}


int
ble_hs_rx_data(struct os_mbuf *om, void *arg)
{
    uint16_t len = om->om_len + 1;
    uint8_t *data = (uint8_t *)malloc(len);
    data[0] = 0x02;
    os_mbuf_copydata(om, 0, len - 1, &data[1]);
    vhci_host_cb.notify_host_recv(data, len);
    free(data);
    os_mbuf_free_chain(om);
    return 0;
}

#endif
#endif
#endif

esp_err_t initialise_bluetooth(void)
{
	uint8_t mac[BSSID_BYTES_SIZE] = {0};
#ifdef CONFIG_BT_ENABLED
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_BT));
	ESP_LOGI(TAG, "ESP Bluetooth MAC addr: %02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#ifdef BLUETOOTH_UART
  #if BT_OVER_C3_S3
	bt_cfg.hci_tl_funcs = &s_hci_uart_tl_funcs;
  #endif

	init_uart();
#endif
	ESP_ERROR_CHECK( esp_bt_controller_init(&bt_cfg) );
#if BLUETOOTH_BLE
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_BLE) );
#elif BLUETOOTH_BT
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) );
#elif BLUETOOTH_BT_BLE
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_BTDM) );
#endif

#if BLUETOOTH_HCI
	esp_err_t ret = ESP_OK;

#if SOC_ESP_NIMBLE_CONTROLLER
    ble_hci_trans_cfg_hs((ble_hci_trans_rx_cmd_fn *)ble_hs_hci_rx_evt,NULL,
                         (ble_hci_trans_rx_acl_fn *)ble_hs_rx_data,NULL);
#else
	ret = esp_vhci_host_register_callback(&vhci_host_cb);
#endif

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to register VHCI callback");
		return ret;
	}

	vhci_send_sem = xSemaphoreCreateBinary();
	if (vhci_send_sem == NULL) {
		ESP_LOGE(TAG, "Failed to create VHCI send sem");
		return ESP_ERR_NO_MEM;
	}

	xSemaphoreGive(vhci_send_sem);
#endif
#endif

	return ESP_OK;
}

void deinitialize_bluetooth(void)
{
#ifdef CONFIG_BT_ENABLED
#if BLUETOOTH_HCI
	if (vhci_send_sem) {
		/* Dummy take and give sema before deleting it */
		xSemaphoreTake(vhci_send_sem, portMAX_DELAY);
		xSemaphoreGive(vhci_send_sem);
		vSemaphoreDelete(vhci_send_sem);
		vhci_send_sem = NULL;
	}
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
#endif
#endif
}

uint8_t get_bluetooth_capabilities(void)
{
	uint8_t cap = 0;
#ifdef CONFIG_BT_ENABLED
	ESP_LOGI(TAG, "- BT/BLE");
#if BLUETOOTH_HCI
#if CONFIG_ESP_SPI_HOST_INTERFACE
	ESP_LOGI(TAG, "   - HCI Over SPI");
	cap |= ESP_BT_SPI_SUPPORT;
#else
	ESP_LOGI(TAG, "   - HCI Over SDIO");
	cap |= ESP_BT_SDIO_SUPPORT;
#endif
#elif BLUETOOTH_UART
	ESP_LOGI(TAG, "   - HCI Over UART");
	cap |= ESP_BT_UART_SUPPORT;
#endif

#if BLUETOOTH_BLE
	ESP_LOGI(TAG, "   - BLE only");
	cap |= ESP_BLE_ONLY_SUPPORT;
#elif BLUETOOTH_BT
	ESP_LOGI(TAG, "   - BR_EDR only");
	cap |= ESP_BR_EDR_ONLY_SUPPORT;
#elif BLUETOOTH_BT_BLE
	ESP_LOGI(TAG, "   - BT/BLE dual mode");
	cap |= ESP_BLE_ONLY_SUPPORT | ESP_BR_EDR_ONLY_SUPPORT;
#endif
#endif
	return cap;
}
#endif /* CONFIG_NW_COPROC_BT_ENABLED */
