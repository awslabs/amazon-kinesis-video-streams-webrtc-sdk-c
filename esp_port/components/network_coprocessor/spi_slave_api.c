// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "soc/gpio_reg.h"
#include "esp_log.h"
#include "interface.h"
#include "adapter.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "endian.h"
#include "freertos/task.h"
#include "mempool.h"
#include "stats.h"
#include "esp_timer.h"

static const char TAG[] = "SPI_DRIVER";
/* SPI settings */
#define SPI_BITS_PER_WORD          8

#define ESP_SPI_MODE               CONFIG_ESP_SPI_MODE
#define GPIO_MOSI                  CONFIG_ESP_SPI_GPIO_MOSI
#define GPIO_MISO                  CONFIG_ESP_SPI_GPIO_MISO
#define GPIO_SCLK                  CONFIG_ESP_SPI_GPIO_CLK
#define GPIO_CS                    CONFIG_ESP_SPI_GPIO_CS
#define GPIO_DATA_READY            CONFIG_ESP_SPI_GPIO_DATA_READY
#define GPIO_HANDSHAKE             CONFIG_ESP_SPI_GPIO_HANDSHAKE

#define ESP_SPI_CONTROLLER         CONFIG_ESP_SPI_CONTROLLER

#define SPI_RX_QUEUE_SIZE          CONFIG_ESP_SPI_RX_Q_SIZE
#define SPI_TX_QUEUE_SIZE          CONFIG_ESP_SPI_TX_Q_SIZE

/* By default both Handshake and Data Ready used Active High,
 * unless configured otherwise.
 * For Active low, set value as 0 */
#define H_HANDSHAKE_ACTIVE_HIGH    1
#define H_DATAREADY_ACTIVE_HIGH    1

/* SPI-DMA settings */
#define SPI_DMA_ALIGNMENT_BYTES    4
#define SPI_DMA_ALIGNMENT_MASK     (SPI_DMA_ALIGNMENT_BYTES-1)
#define IS_SPI_DMA_ALIGNED(VAL)    (!((VAL)& SPI_DMA_ALIGNMENT_MASK))
#define MAKE_SPI_DMA_ALIGNED(VAL)  (VAL += SPI_DMA_ALIGNMENT_BYTES - \
				((VAL)& SPI_DMA_ALIGNMENT_MASK))

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
    #define DMA_CHAN               ESP_SPI_CONTROLLER
#else
    #define DMA_CHAN               SPI_DMA_CH_AUTO
#endif


#if ESP_SPI_MODE==0
#  error "SPI mode 0 at SLAVE is NOT supported"
#endif
/* SPI internal configs */
#define SPI_BUFFER_SIZE            MAX_TRANSPORT_BUF_SIZE
#define SPI_QUEUE_SIZE             3

#define GPIO_MASK_DATA_READY (1ULL << GPIO_DATA_READY)
#define GPIO_MASK_HANDSHAKE (1ULL << GPIO_HANDSHAKE)

#define H_CS_INTR_TO_CLEAR_HS                        GPIO_INTR_NEGEDGE

#if H_HANDSHAKE_ACTIVE_HIGH
  #define H_HS_VAL_ACTIVE                            GPIO_OUT_W1TS_REG
  #define H_HS_VAL_INACTIVE                          GPIO_OUT_W1TC_REG
  #define H_HS_PULL_REGISTER                         GPIO_PULLDOWN_ONLY
#else
  #define H_HS_VAL_ACTIVE                            GPIO_OUT_W1TC_REG
  #define H_HS_VAL_INACTIVE                          GPIO_OUT_W1TS_REG
  #define H_HS_PULL_REGISTER                         GPIO_PULLUP_ONLY
#endif

#if H_DATAREADY_ACTIVE_HIGH
  #define H_DR_VAL_ACTIVE                            GPIO_OUT_W1TS_REG
  #define H_DR_VAL_INACTIVE                          GPIO_OUT_W1TC_REG
  #define H_DR_PULL_REGISTER                         GPIO_PULLDOWN_ONLY
#else
  #define H_DR_VAL_ACTIVE                            GPIO_OUT_W1TC_REG
  #define H_DR_VAL_INACTIVE                          GPIO_OUT_W1TS_REG
  #define H_DR_PULL_REGISTER                         GPIO_PULLUP_ONLY
#endif

static interface_context_t context;
static interface_handle_t if_handle_g;
static SemaphoreHandle_t spi_tx_sem;
static SemaphoreHandle_t spi_rx_sem;
static QueueHandle_t spi_rx_queue[MAX_PRIORITY_QUEUES];
static QueueHandle_t spi_tx_queue[MAX_PRIORITY_QUEUES];
#if DUMMY_TRANS_DESIGN
static SemaphoreHandle_t spi_sema;
#endif

static interface_handle_t * esp_spi_init(void);
static int32_t esp_spi_write(interface_handle_t *handle,
				interface_buffer_handle_t *buf_handle);
static int esp_spi_read(interface_handle_t *if_handle, interface_buffer_handle_t * buf_handle);
static esp_err_t esp_spi_reset(interface_handle_t *handle);
static void esp_spi_deinit(interface_handle_t *handle);
static void esp_spi_read_done(void *handle);
#if !DUMMY_TRANS_DESIGN
static void queue_next_transaction(void);
#endif

if_ops_t if_ops = {
	.init = esp_spi_init,
	.write = esp_spi_write,
	.read = esp_spi_read,
	.reset = esp_spi_reset,
	.deinit = esp_spi_deinit,
};

#define SPI_MEMPOOL_NUM_BLOCKS     ((SPI_TX_QUEUE_SIZE+SPI_RX_QUEUE_SIZE)+SPI_QUEUE_SIZE*2)
static struct hosted_mempool * buf_mp_tx_g;
static struct hosted_mempool * buf_mp_rx_g;
static struct hosted_mempool * trans_mp_g;
#if DUMMY_TRANS_DESIGN
static uint8_t dummy_queued = pdFALSE;
#endif

static inline void spi_mempool_create()
{
	buf_mp_tx_g = hosted_mempool_create(NULL, 0,
			SPI_MEMPOOL_NUM_BLOCKS, SPI_BUFFER_SIZE);
	/* re-use the mempool, as same size, can be seperate, if needed */
	buf_mp_rx_g = buf_mp_tx_g;
	trans_mp_g = hosted_mempool_create(NULL, 0,
			SPI_MEMPOOL_NUM_BLOCKS, sizeof(spi_slave_transaction_t));
#if CONFIG_ESP_CACHE_MALLOC
	assert(buf_mp_tx_g);
	assert(buf_mp_rx_g);
	assert(trans_mp_g);
#endif
}

static inline void spi_mempool_destroy()
{
	hosted_mempool_destroy(buf_mp_tx_g);
	hosted_mempool_destroy(trans_mp_g);
}

static inline void *spi_buffer_tx_alloc(uint need_memset)
{
	return hosted_mempool_alloc(buf_mp_tx_g, SPI_BUFFER_SIZE, need_memset);
}

static inline void *spi_buffer_rx_alloc(uint need_memset)
{
	return hosted_mempool_alloc(buf_mp_rx_g, SPI_BUFFER_SIZE, need_memset);
}

static inline spi_slave_transaction_t *spi_trans_alloc(uint need_memset)
{
	return hosted_mempool_alloc(trans_mp_g, sizeof(spi_slave_transaction_t), need_memset);
}

static inline void spi_buffer_tx_free(void *buf)
{
	hosted_mempool_free(buf_mp_tx_g, buf);
}

static inline void spi_buffer_rx_free(void *buf)
{
	hosted_mempool_free(buf_mp_rx_g, buf);
}

static inline void spi_trans_free(spi_slave_transaction_t *trans)
{
	hosted_mempool_free(trans_mp_g, trans);
}

#define set_handshake_gpio()     gpio_set_level(GPIO_HANDSHAKE, 1);
#define reset_handshake_gpio()   gpio_set_level(GPIO_HANDSHAKE, 0);
#define set_dataready_gpio()     gpio_set_level(GPIO_DATA_READY, 1);
#define reset_dataready_gpio()   gpio_set_level(GPIO_DATA_READY, 0);

uint32_t get_cur_back_pressure(void)
{
	return uxQueueMessagesWaiting(spi_rx_queue[PRIO_Q_OTHERS]);
}

interface_context_t *interface_insert_driver(int (*event_handler)(uint8_t val))
{
	ESP_LOGI(TAG, "Using SPI interface");
	memset(&context, 0, sizeof(context));

	context.type = SPI;
	context.if_ops = &if_ops;
	context.event_handler = event_handler;

	return &context;
}

int interface_remove_driver()
{
	memset(&context, 0, sizeof(context));
	return 0;
}


#ifndef CONFIG_HOSTED_ON_LOW_MEM
static inline int find_wifi_tx_throttling_to_be_set(void)
{
	uint16_t queue_load;
	uint8_t load_percent;

	if (!slv_cfg_g.flow_ctl_start_thres) {
		/* No high threshold set, no throttlling */
		return 0;
	}

	queue_load = uxQueueMessagesWaiting(spi_rx_queue[PRIO_Q_OTHERS]);
#if ESP_PKT_STATS
	/* pkt_stats.slave_wifi_rx_msg_loaded = queue_load; */
#endif

	load_percent = (queue_load*100/SPI_RX_QUEUE_SIZE);

	if (load_percent > slv_cfg_g.flow_ctl_start_thres) {
		slv_state_g.flow_ctl_wifi = 1;
		ESP_LOGV(TAG, "flow ctl started");
	}

	if (load_percent < slv_cfg_g.flow_ctl_clear_thres) {
		slv_state_g.flow_ctl_wifi = 0;
		ESP_LOGV(TAG, "flow ctl stopped");
	}

	return slv_state_g.flow_ctl_wifi;
}
#endif


void generate_startup_event(uint8_t cap)
{
	struct esp_payload_header *header = NULL;
	interface_buffer_handle_t buf_handle = {0};
	struct esp_priv_event *event = NULL;
	uint8_t *pos = NULL;
	uint16_t len = 0;
	uint8_t raw_tp_cap = 0;
	uint32_t total_len = 0;

	buf_handle.payload = spi_buffer_tx_alloc(MEMSET_REQUIRED);

	raw_tp_cap = debug_get_raw_tp_conf();

	assert(buf_handle.payload);
	header = (struct esp_payload_header *) buf_handle.payload;

	header->if_type = ESP_PRIV_IF;
	header->if_num = 0;
	header->offset = htole16(sizeof(struct esp_payload_header));
	header->priv_pkt_type = ESP_PACKET_TYPE_EVENT;
#ifdef CONFIG_HOSTED_ON_LOW_MEM
	header->throttle_cmd = 0;
#else
	header->throttle_cmd = find_wifi_tx_throttling_to_be_set();
#endif

	/* Populate event data */
	event = (struct esp_priv_event *) (buf_handle.payload + sizeof(struct esp_payload_header));

	event->event_type = ESP_PRIV_EVENT_INIT;

	/* Populate TLVs for event */
	pos = event->event_data;

	/* TLVs start */

	/* TLV - Board type */
	ESP_LOGI(TAG, "Slave chip Id[%x]", ESP_PRIV_FIRMWARE_CHIP_ID);
	*pos = ESP_PRIV_FIRMWARE_CHIP_ID;   pos++;len++;
	*pos = LENGTH_1_BYTE;               pos++;len++;
	*pos = CONFIG_IDF_FIRMWARE_CHIP_ID; pos++;len++;

	/* TLV - Capability */
	*pos = ESP_PRIV_CAPABILITY;         pos++;len++;
	*pos = LENGTH_1_BYTE;               pos++;len++;
	*pos = cap;                         pos++;len++;

	*pos = ESP_PRIV_TEST_RAW_TP;        pos++;len++;
	*pos = LENGTH_1_BYTE;               pos++;len++;
	*pos = raw_tp_cap;                  pos++;len++;

	*pos = ESP_PRIV_RX_Q_SIZE;          pos++;len++;
	*pos = LENGTH_1_BYTE;               pos++;len++;
	*pos = SPI_RX_QUEUE_SIZE;           pos++;len++;

	*pos = ESP_PRIV_TX_Q_SIZE;          pos++;len++;
	*pos = LENGTH_1_BYTE;               pos++;len++;
	*pos = SPI_TX_QUEUE_SIZE;           pos++;len++;

	/* TLVs end */

	event->event_len = len;

	/* payload len = Event len + sizeof(event type) + sizeof(event len) */
	len += 2;
	header->len = htole16(len);

	total_len = len + sizeof(struct esp_payload_header);

	if (!IS_SPI_DMA_ALIGNED(total_len)) {
		MAKE_SPI_DMA_ALIGNED(total_len);
	}

	buf_handle.payload_len = total_len;

#if CONFIG_ESP_SPI_CHECKSUM
	header->checksum = htole16(compute_checksum(buf_handle.payload, len + sizeof(struct esp_payload_header)));
#endif

	xQueueSend(spi_tx_queue[PRIO_Q_OTHERS], &buf_handle, portMAX_DELAY);
	xSemaphoreGive(spi_tx_sem);

#if !DUMMY_TRANS_DESIGN
	set_dataready_gpio();
	/* process first data packet here to start transactions */
	queue_next_transaction();
#endif
}


/* Invoked after transaction is queued and ready for pickup by master */
static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t *trans)
{
	/* ESP peripheral ready for spi transaction. Set hadnshake line high. */
	set_handshake_gpio();
}

#if DUMMY_TRANS_DESIGN
static inline int is_valid_trans_buffer(uint8_t *trans_buf)
{
	struct esp_payload_header *header;
	uint16_t len, offset;

	if (!trans_buf) {
		return pdFALSE;
	}

	header = (struct esp_payload_header *) trans_buf;

	len = le16toh(header->len);
	offset = le16toh(header->offset);

	if (!len || (len > SPI_BUFFER_SIZE) ||
	    (offset != sizeof(struct esp_payload_header))) {
		return pdFALSE;
	}

	if ((header->if_type >= ESP_MAX_IF) || (header->if_num)) {
		return pdFALSE;
	}

	return pdTRUE;
}
#endif
/* Invoked after transaction is sent/received.
 * Use this to set the handshake line low */
static void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t *trans)
{
#if DUMMY_TRANS_DESIGN
	if (trans && is_valid_trans_buffer((uint8_t *)trans->tx_buffer)) {
		/* Host has consumed a valid TX buffer
		 * Clear Data ready line and release semaphore */
		reset_dataready_gpio();

		if (spi_sema)
			xSemaphoreGive(spi_sema);
	}
#endif
	/* Clear handshake line */
	reset_handshake_gpio();

}

static uint8_t * get_next_tx_buffer(uint32_t *len)
{
	interface_buffer_handle_t buf_handle = {0};
	esp_err_t ret = ESP_OK;
	uint8_t *sendbuf = NULL;
	struct esp_payload_header *header = NULL;

	/* Get or create new tx_buffer
	 *	1. Check if SPI TX queue has pending buffers. Return if valid buffer is obtained.
	 *	2. Create a new empty tx buffer and return */

	/* Get buffer from SPI Tx queue */
	ret = xSemaphoreTake(spi_tx_sem, 0);
	if (pdTRUE == ret)
		if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_SERIAL], &buf_handle, 0))
			if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_BT], &buf_handle, 0))
				if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_OTHERS], &buf_handle, 0))
					ret = pdFALSE;

	if (ret == pdTRUE && buf_handle.payload) {
		if (len) {
#if ESP_PKT_STATS
			if (buf_handle.if_type == ESP_SERIAL_IF)
				pkt_stats.serial_tx_total++;
#endif
			*len = buf_handle.payload_len;
		}
		/* Return real data buffer from queue */
		return buf_handle.payload;
	}

#if DUMMY_TRANS_DESIGN
	/* Dummy transaction is already queued. Return. */
	if (dummy_queued) {
		if (len)
			*len = 0;
		return NULL;
	}
#else
	/* No real data pending, clear ready line and indicate host an idle state */
	reset_dataready_gpio();
#endif

	/* Create empty dummy buffer */
	sendbuf = spi_buffer_tx_alloc(MEMSET_REQUIRED);
	if (!sendbuf) {
		ESP_LOGE(TAG, "Failed to allocate memory for dummy transaction");
		if (len)
			*len = 0;
		return NULL;
	}

	/* Initialize header */
	header = (struct esp_payload_header *) sendbuf;

	/* Populate header to indicate it as a dummy buffer */
	header->if_type = ESP_MAX_IF;
	header->if_num = 0xF;
	header->len = 0;
#ifdef CONFIG_HOSTED_ON_LOW_MEM
	header->throttle_cmd = 0;
#else
	header->throttle_cmd = find_wifi_tx_throttling_to_be_set();
#endif

	if (len)
		*len = 0;

	return sendbuf;
}

static int process_spi_rx(interface_buffer_handle_t *buf_handle)
{
	struct esp_payload_header *header = NULL;
	uint16_t len = 0, offset = 0;
#if CONFIG_ESP_SPI_CHECKSUM
	uint16_t rx_checksum = 0, checksum = 0;
#endif

	/* Validate received buffer. Drop invalid buffer. */

	if (!buf_handle || !buf_handle->payload) {
		ESP_LOGE(TAG, "%s: Invalid params", __func__);
		return -1;
	}

	header = (struct esp_payload_header *) buf_handle->payload;
	len = le16toh(header->len);
	offset = le16toh(header->offset);

	if (!len)
		return -1;

	if ((len+offset) > SPI_BUFFER_SIZE) {
		ESP_LOGE(TAG, "rx_pkt len[%u]>max[%u], dropping it", len+offset, SPI_BUFFER_SIZE);

		return -1;
	}

#if CONFIG_ESP_SPI_CHECKSUM
	rx_checksum = le16toh(header->checksum);
	header->checksum = 0;

	checksum = compute_checksum(buf_handle->payload, len+offset);

	if (checksum != rx_checksum) {
		ESP_LOGE(TAG, "%s: cal_chksum[%u] != exp_chksum[%u], drop len[%u] offset[%u]",
				__func__, checksum, rx_checksum, len, offset);
		return -1;
	}
#endif

	/* Buffer is valid */
	buf_handle->if_type = header->if_type;
	buf_handle->if_num = header->if_num;
	buf_handle->free_buf_handle = esp_spi_read_done;
	buf_handle->payload_len = len + offset;
	buf_handle->priv_buffer_handle = buf_handle->payload;


#if ESP_PKT_STATS
	if (buf_handle->if_type == ESP_STA_IF)
		pkt_stats.hs_bus_sta_in++;
#endif
	if (header->if_type == ESP_SERIAL_IF) {
		xQueueSend(spi_rx_queue[PRIO_Q_SERIAL], buf_handle, portMAX_DELAY);
	} else if (header->if_type == ESP_HCI_IF) {
		xQueueSend(spi_rx_queue[PRIO_Q_BT], buf_handle, portMAX_DELAY);
	} else {
		xQueueSend(spi_rx_queue[PRIO_Q_OTHERS], buf_handle, portMAX_DELAY);
	}

	xSemaphoreGive(spi_rx_sem);
	return 0;
}

#if DUMMY_TRANS_DESIGN
static void spi_transaction_tx_task(void* pvParameters)
{
	spi_slave_transaction_t *spi_trans;
	esp_err_t ret = 0;
	interface_buffer_handle_t buf_handle;

	for(;;) {
		xSemaphoreTake(spi_tx_sem, portMAX_DELAY);

		if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_SERIAL], &buf_handle, 0))
			if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_BT], &buf_handle, 0))
				if (pdFALSE == xQueueReceive(spi_tx_queue[PRIO_Q_OTHERS], &buf_handle, 0))
					ret = pdFALSE;

		if (ret == pdTRUE && buf_handle.payload) {
			spi_trans = spi_trans_alloc(MEMSET_REQUIRED);
			assert(spi_trans);

			/* Attach Rx Buffer */
			spi_trans->rx_buffer = spi_buffer_rx_alloc(MEMSET_REQUIRED);
			assert(spi_trans->rx_buffer);

			/* Attach Tx Buffer */
			spi_trans->tx_buffer = buf_handle.payload;

			/* Transaction len */
			spi_trans->length = SPI_BUFFER_SIZE * SPI_BITS_PER_WORD;

			/* Execute transaction */
			xSemaphoreTake(spi_sema, portMAX_DELAY);

			spi_slave_queue_trans(ESP_SPI_CONTROLLER, spi_trans,
					portMAX_DELAY);

			/* Set Data ready high */
			set_dataready_gpio();
		}
	}
}

static void queue_dummy_transaction()
{
	spi_slave_transaction_t *spi_trans = NULL;
	esp_err_t ret = ESP_OK;
	uint32_t len = 0;
	uint8_t *tx_buffer = NULL;

	tx_buffer = get_next_tx_buffer(&len);
	if (!tx_buffer) {
		/* No need to queue dummy transaction */
		xSemaphoreGive(spi_sema);
		return;
	}

	spi_trans = spi_trans_alloc(MEMSET_REQUIRED);
	assert(spi_trans);

	/* Attach Rx Buffer */
	spi_trans->rx_buffer = spi_buffer_rx_alloc(MEMSET_REQUIRED);
	assert(spi_trans->rx_buffer);

	/* Attach Tx Buffer */
	spi_trans->tx_buffer = tx_buffer;

	/* Transaction len */
	spi_trans->length = SPI_BUFFER_SIZE * SPI_BITS_PER_WORD;

	ret = spi_slave_queue_trans(ESP_SPI_CONTROLLER, spi_trans, 0);
	if (ret != ESP_OK) {
		free(spi_trans->rx_buffer);
		free((void *)spi_trans->tx_buffer);
		free(spi_trans);
		xSemaphoreGive(spi_sema);
		return;
	}

	if (!len) {
		/* queued dummy transaction, release semaphore */
		dummy_queued = pdTRUE;
		xSemaphoreGive(spi_sema);
	} else {
		/* Queued transaction with valid TX Buffer. Set Data ready high. */
		set_dataready_gpio();
	}
}

#else
static void queue_next_transaction(void)
{
	spi_slave_transaction_t *spi_trans = NULL;
	uint32_t len = 0;
	uint8_t *tx_buffer = get_next_tx_buffer(&len);
	if (!tx_buffer) {
		/* Queue next transaction failed */
		ESP_LOGE(TAG , "Failed to queue new transaction\r\n");
		return;
	}
	ESP_HEXLOGD("spi_tx", tx_buffer, len, len);

	spi_trans = spi_trans_alloc(MEMSET_REQUIRED);
	assert(spi_trans);

	/* Attach Rx Buffer */
	spi_trans->rx_buffer = spi_buffer_rx_alloc(MEMSET_REQUIRED);
	assert(spi_trans->rx_buffer);

	/* Attach Tx Buffer */
	spi_trans->tx_buffer = tx_buffer;

	/* Transaction len */
	spi_trans->length = SPI_BUFFER_SIZE * SPI_BITS_PER_WORD;

	spi_slave_queue_trans(ESP_SPI_CONTROLLER, spi_trans, portMAX_DELAY);
}
#endif

static void spi_transaction_post_process_task(void* pvParameters)
{
	spi_slave_transaction_t *spi_trans = NULL;
#if DUMMY_TRANS_DESIGN
	struct esp_payload_header *header;
#endif
	esp_err_t ret = ESP_OK;
	interface_buffer_handle_t rx_buf_handle;

	for (;;) {
		memset(&rx_buf_handle, 0, sizeof(rx_buf_handle));

		/* Await transmission result, after any kind of transmission a new packet
		 * (dummy or real) must be placed in SPI slave
		 */
		spi_slave_get_trans_result(ESP_SPI_CONTROLLER, &spi_trans,
				portMAX_DELAY);
#if DUMMY_TRANS_DESIGN
		if (spi_trans->tx_buffer) {
			header = (struct esp_payload_header *) spi_trans->tx_buffer;

			if (header->if_type == 0xF && header->if_num == 0xF && header->offset == 0) {
				/* Dummy Tx buffer consumed by host */
				dummy_queued = pdFALSE;
			}

			spi_buffer_tx_free((void *)spi_trans->tx_buffer);
			spi_trans->tx_buffer = NULL;
		}

		/* Check if dummy transaction is needed
		 *
		 * If failed to obtain spi_sema:
		 *    - Transaction is already queued.
		 *    - No need to queue dummy transaction
		 *
		 * If spi_sema is obtained: queue dummy transaction
		 **/

		ret = xSemaphoreTake(spi_sema, 0);
		if (ret == pdTRUE)
			queue_dummy_transaction();
#else
		/* Queue new transaction to get ready as soon as possible */
		queue_next_transaction();
		assert(spi_trans);
#if ESP_PKT_STATS
		struct esp_payload_header *header =
			(struct esp_payload_header *)spi_trans->tx_buffer;
		if (header->if_type == ESP_STA_IF)
			pkt_stats.sta_sh_out++;
#endif

		/* Free any tx buffer, data is not relevant anymore */
		spi_buffer_tx_free((void *)spi_trans->tx_buffer);
#endif

		/* Process received data */
		if (spi_trans->rx_buffer) {
			rx_buf_handle.payload = spi_trans->rx_buffer;

			ret = process_spi_rx(&rx_buf_handle);

			/* free rx_buffer if process_spi_rx returns an error
			 * In success case it will be freed later */
			if (ret != ESP_OK) {
				spi_buffer_rx_free((void *)spi_trans->rx_buffer);
			}
		} else {
			ESP_LOGI(TAG, "no rx_buf");
		}

		/* Free Transfer structure */
		spi_trans_free(spi_trans);
	}
}

static void IRAM_ATTR gpio_disable_hs_isr_handler(void* arg)
{
	reset_handshake_gpio();
}

static void register_hs_disable_pin(uint32_t gpio_num)
{
	if (gpio_num != -1) {
		gpio_reset_pin(gpio_num);

		gpio_config_t slave_disable_hs_pin_conf={
			.intr_type=GPIO_INTR_DISABLE,
			.mode=GPIO_MODE_INPUT,
			.pin_bit_mask=(1ULL<<gpio_num)
		};
		slave_disable_hs_pin_conf.pull_up_en = 1;
		gpio_config(&slave_disable_hs_pin_conf);
		gpio_set_intr_type(gpio_num, H_CS_INTR_TO_CLEAR_HS);
		gpio_install_isr_service(0);
		gpio_isr_handler_add(gpio_num, gpio_disable_hs_isr_handler, NULL);
	}
}

static interface_handle_t * esp_spi_init(void)
{
	esp_err_t ret = ESP_OK;
	uint16_t prio_q_idx = 0;

	/* Configuration for the SPI bus */
	spi_bus_config_t buscfg={
		.mosi_io_num=GPIO_MOSI,
		.miso_io_num=GPIO_MISO,
		.sclk_io_num=GPIO_SCLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = SPI_BUFFER_SIZE,
#if 0
		/*
		 * Moving ESP32 SPI slave interrupts in flash, Keeping it in IRAM gives crash,
		 * While performing flash erase operation.
		 */
		.intr_flags=ESP_INTR_FLAG_IRAM
#endif
	};

	/* Configuration for the SPI slave interface */
	spi_slave_interface_config_t slvcfg={
		.mode=ESP_SPI_MODE,
		.spics_io_num=GPIO_CS,
		.queue_size=SPI_QUEUE_SIZE,
		.flags=0,
		.post_setup_cb=spi_post_setup_cb,
		.post_trans_cb=spi_post_trans_cb
	};

	/* Configuration for the handshake line */
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_OUTPUT,
		.pin_bit_mask=GPIO_MASK_HANDSHAKE
	};

	/* Configuration for data_ready line */
	gpio_config_t io_data_ready_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_OUTPUT,
		.pin_bit_mask=GPIO_MASK_DATA_READY
	};

	spi_mempool_create();

	/* Configure handshake and data_ready lines as output */
	gpio_config(&io_conf);
	gpio_config(&io_data_ready_conf);
	reset_handshake_gpio();
	reset_dataready_gpio();

	/* Enable pull-ups on SPI lines
	 * so that no rogue pulses when no master is connected
	 */
	gpio_set_pull_mode(CONFIG_ESP_SPI_GPIO_HANDSHAKE, H_HS_PULL_REGISTER);
	gpio_set_pull_mode(CONFIG_ESP_SPI_GPIO_DATA_READY, H_DR_PULL_REGISTER);
	gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
	gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
	gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

	ESP_LOGI(TAG, "SPI Ctrl:%u mode: %u, Freq:ConfigAtHost\nGPIOs: MOSI: %u, MISO: %u, CS: %u, CLK: %u HS: %u DR: %u\n",
			ESP_SPI_CONTROLLER, slvcfg.mode,
			GPIO_MOSI, GPIO_MISO, GPIO_CS, GPIO_SCLK,
			GPIO_HANDSHAKE, GPIO_DATA_READY);

	ESP_LOGI(TAG, "Hosted SPI queue size: Tx:%u Rx:%u", SPI_TX_QUEUE_SIZE, SPI_RX_QUEUE_SIZE);
	register_hs_disable_pin(GPIO_CS);

#if !H_HANDSHAKE_ACTIVE_HIGH
	ESP_LOGI(TAG, "Handshake: Active Low");
#endif

#if !H_DATAREADY_ACTIVE_HIGH
	ESP_LOGI(TAG, "DataReady: Active Low");
#endif


	/* Initialize SPI slave interface */
	ret=spi_slave_initialize(ESP_SPI_CONTROLLER, &buscfg, &slvcfg, DMA_CHAN);
	assert(ret==ESP_OK);

	//gpio_set_drive_capability(CONFIG_ESP_SPI_GPIO_HANDSHAKE, GPIO_DRIVE_CAP_3);
	//gpio_set_drive_capability(CONFIG_ESP_SPI_GPIO_DATA_READY, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(GPIO_SCLK, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(GPIO_MISO, GPIO_DRIVE_CAP_3);
	gpio_set_pull_mode(GPIO_MISO, GPIO_PULLDOWN_ONLY);


	memset(&if_handle_g, 0, sizeof(if_handle_g));
	if_handle_g.state = INIT;

	spi_tx_sem = xSemaphoreCreateCounting(SPI_TX_QUEUE_SIZE*3, 0);
	assert(spi_tx_sem != NULL);
	spi_rx_sem = xSemaphoreCreateCounting(SPI_RX_QUEUE_SIZE*3, 0);
	assert(spi_rx_sem != NULL);

	for (prio_q_idx=0; prio_q_idx<MAX_PRIORITY_QUEUES;prio_q_idx++) {
		spi_rx_queue[prio_q_idx] = xQueueCreate(SPI_RX_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(spi_rx_queue[prio_q_idx] != NULL);

		spi_tx_queue[prio_q_idx] = xQueueCreate(SPI_TX_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(spi_tx_queue[prio_q_idx] != NULL);
	}

#if DUMMY_TRANS_DESIGN
	spi_sema = xSemaphoreCreateBinary();
	assert(spi_sema != NULL);
	xSemaphoreGive(spi_sema);

	assert(xTaskCreate(spi_transaction_tx_task , "spi_tx_task" ,
			CONFIG_ESP_DEFAULT_TASK_STACK_SIZE, NULL,
			CONFIG_ESP_DEFAULT_TASK_PRIO+1, NULL) == pdTRUE);
#endif
	assert(xTaskCreate(spi_transaction_post_process_task , "spi_post_process_task" ,
			CONFIG_ESP_DEFAULT_TASK_STACK_SIZE, NULL,
			CONFIG_ESP_DEFAULT_TASK_PRIO, NULL) == pdTRUE);

	usleep(500);

	return &if_handle_g;
}

static int32_t esp_spi_write(interface_handle_t *handle, interface_buffer_handle_t *buf_handle)
{
	int32_t total_len = 0;
	uint16_t offset = 0;
	struct esp_payload_header *header = NULL;
	interface_buffer_handle_t tx_buf_handle = {0};

	if (!handle || !buf_handle) {
		ESP_LOGE(TAG , "Invalid arguments\n");
		return ESP_FAIL;
	}

	if (!buf_handle->payload_len || !buf_handle->payload) {
		ESP_LOGE(TAG , "Invalid arguments, len:%d\n", buf_handle->payload_len);
		return ESP_FAIL;
	}

	total_len = buf_handle->payload_len + sizeof (struct esp_payload_header);

	/* make the adresses dma aligned */
	if (!IS_SPI_DMA_ALIGNED(total_len)) {
		MAKE_SPI_DMA_ALIGNED(total_len);
	}

	if (total_len > SPI_BUFFER_SIZE) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
		ESP_LOGE(TAG, "Max frame length exceeded %ld.. drop it\n", total_len);
#else
		ESP_LOGE(TAG, "Max frame length exceeded %d.. drop it\n", total_len);
#endif
		return ESP_FAIL;
	}

	tx_buf_handle.if_type = buf_handle->if_type;
	tx_buf_handle.if_num = buf_handle->if_num;
	tx_buf_handle.payload_len = total_len;

	tx_buf_handle.payload = spi_buffer_tx_alloc(MEMSET_REQUIRED);
	assert(tx_buf_handle.payload);

	header = (struct esp_payload_header *) tx_buf_handle.payload;

	memset (header, 0, sizeof(struct esp_payload_header));

	/* Initialize header */
	header->if_type = buf_handle->if_type;
	header->if_num = buf_handle->if_num;
	header->len = htole16(buf_handle->payload_len);
	offset = sizeof(struct esp_payload_header);
	header->offset = htole16(offset);
	header->seq_num = htole16(buf_handle->seq_num);
	header->flags = buf_handle->flag;


#ifdef CONFIG_HOSTED_ON_LOW_MEM
	header->throttle_cmd = 0;
#else
	header->throttle_cmd = find_wifi_tx_throttling_to_be_set();
#endif

	/* copy the data from caller */
	memcpy(tx_buf_handle.payload + offset, buf_handle->payload, buf_handle->payload_len);


#if CONFIG_ESP_SPI_CHECKSUM
	header->checksum = htole16(compute_checksum(tx_buf_handle.payload,
				offset+buf_handle->payload_len));
#endif

	if (header->if_type == ESP_SERIAL_IF)
		xQueueSend(spi_tx_queue[PRIO_Q_SERIAL], &tx_buf_handle, portMAX_DELAY);
	else if (header->if_type == ESP_HCI_IF)
		xQueueSend(spi_tx_queue[PRIO_Q_BT], &tx_buf_handle, portMAX_DELAY);
	else
		xQueueSend(spi_tx_queue[PRIO_Q_OTHERS], &tx_buf_handle, portMAX_DELAY);

	xSemaphoreGive(spi_tx_sem);

	/* indicate waiting data on ready pin */
	set_dataready_gpio();

	return buf_handle->payload_len;
}

static void IRAM_ATTR esp_spi_read_done(void *handle)
{
	spi_buffer_rx_free(handle);
}

static int esp_spi_read(interface_handle_t *if_handle, interface_buffer_handle_t *buf_handle)
{
	if (!if_handle) {
		ESP_LOGE(TAG, "Invalid arguments to esp_spi_read\n");
		return ESP_FAIL;
	}

	xSemaphoreTake(spi_rx_sem, portMAX_DELAY);

	if (pdFALSE == xQueueReceive(spi_rx_queue[PRIO_Q_SERIAL], buf_handle, 0))
		if (pdFALSE == xQueueReceive(spi_rx_queue[PRIO_Q_BT], buf_handle, 0))
			if (pdFALSE == xQueueReceive(spi_rx_queue[PRIO_Q_OTHERS], buf_handle, 0)) {
				ESP_LOGI(TAG, "%s No element in rx queue", __func__);
		return ESP_FAIL;
	}

	return buf_handle->payload_len;
}

static esp_err_t esp_spi_reset(interface_handle_t *handle)
{
	esp_err_t ret = ESP_OK;
	ret = spi_slave_free(ESP_SPI_CONTROLLER);
	if (ESP_OK != ret) {
		ESP_LOGE(TAG, "spi slave bus free failed\n");
	}
	return ret;
}

static void esp_spi_deinit(interface_handle_t *handle)
{
	esp_err_t ret = ESP_OK;

	spi_mempool_destroy();
#if DUMMY_TRANS_DESIGN
	if (spi_sema)
		vSemaphoreDelete(spi_sema);
#endif

	ret = spi_slave_free(ESP_SPI_CONTROLLER);
	if (ESP_OK != ret) {
		ESP_LOGE(TAG, "spi slave bus free failed\n");
		return;
	}

	ret = spi_bus_free(ESP_SPI_CONTROLLER);
	if (ESP_OK != ret) {
		ESP_LOGE(TAG, "spi all bus free failed\n");
		return;
	}
}
