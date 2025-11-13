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

/** Includes **/
#include "string.h"
#include "sdio_drv.h"
#include "sdio_reg.h"
#include "serial_drv.h"
#include "stats.h"
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "hci_drv.h"
#include "power_save_drv.h"
#include "esp_wifi.h"

static const char TAG[] = "H_SDIO_DRV";

/** Constants/Macros **/
#define TO_SLAVE_QUEUE_SIZE               CONFIG_ESP_SDIO_TX_Q_SIZE
#define FROM_SLAVE_QUEUE_SIZE             CONFIG_ESP_SDIO_RX_Q_SIZE

#define RX_TASK_STACK_SIZE                4096
#define TX_TASK_STACK_SIZE                4096
#define PROCESS_RX_TASK_STACK_SIZE        4096
#define RX_TIMEOUT_TICKS                  50

/* For sdio_write, slave sdio buffer availability to be checked, before
 * actual write happens. This slave availability check may need retry till slave
 * has sufficient room.
 * BUF_AVAIL_* are config for fetching buffer availability */
#define BUF_AVAIL_MAX_RETRY               45
#define BUF_AVAIL_INTERVAL_US_START       20
#define BUF_AVAIL_INTERVAL_US_MAX         10240
#define BUF_AVAIL_REPEAT_STEP             4

/* Actual data sdio_write max retry */
#define MAX_SDIO_WRITE_RETRY              2

// this locks the sdio transaction at the driver level, instead of at the HAL layer
#define USE_DRIVER_LOCK

#if defined(USE_DRIVER_LOCK)
#define ACQUIRE_LOCK false
#else
#define ACQUIRE_LOCK true
#endif

#if defined(USE_DRIVER_LOCK)
static void * sdio_bus_lock;

#define SDIO_DRV_LOCK()   g_h.funcs->_h_lock_mutex(sdio_bus_lock, portMAX_DELAY);
#define SDIO_DRV_UNLOCK() g_h.funcs->_h_unlock_mutex(sdio_bus_lock);

#else
#define SDIO_DRV_LOCK()
#define SDIO_DRV_UNLOCK()
#endif

/* Create mempool for cache mallocs */
static struct mempool * buf_mp_g;

/* TODO to move this in transport drv */
extern transport_channel_t *chan_arr[ESP_MAX_IF];

static void * sdio_handle = NULL;
static void * sdio_bus_lock;
static void * sdio_read_thread;
static void * sdio_process_rx_thread;
static void * sdio_write_thread;

static queue_handle_t to_slave_queue[MAX_PRIORITY_QUEUES];
semaphore_handle_t sem_to_slave_queue;
static queue_handle_t from_slave_queue[MAX_PRIORITY_QUEUES];
semaphore_handle_t sem_from_slave_queue;

/* Counter to hold the amount of buffers already sent to sdio slave */
static uint32_t sdio_tx_buf_count = 0;

/* Counter to hold the amount of bytes already received from sdio slave */
static uint32_t sdio_rx_byte_count = 0;

// one-time trigger to start write thread
static bool sdio_start_write_thread = false;

#if H_SDIO_HOST_RX_MODE == H_SDIO_HOST_STREAMING_MODE
static uint32_t recv_buf_size = 0;
static uint8_t * recv_buf = NULL;
#endif

static esp_err_t sdio_generate_slave_intr(uint8_t intr_no);

static void sdio_write_task(void const* pvParameters);
static void sdio_read_task(void const* pvParameters);
static void sdio_process_rx_task(void const* pvParameters);
static int get_firmware_data(bool is_lock_needed);

static inline void sdio_mempool_create(void)
{
	MEM_DUMP("sdio_mempool_create");
	buf_mp_g = mempool_create(MAX_SDIO_BUFFER_SIZE);
#ifdef CONFIG_ESP_USE_MEMPOOL
	assert(buf_mp_g);
#endif
}

static inline void sdio_mempool_destroy(void)
{
	mempool_destroy(buf_mp_g);
}

static inline void *sdio_buffer_alloc(uint need_memset)
{
	return mempool_alloc(buf_mp_g, MAX_SDIO_BUFFER_SIZE, need_memset);
}

static inline void sdio_buffer_free(void *buf)
{
	mempool_free(buf_mp_g, buf);
}

void bus_deinit_internal(void)
{
	/* TODO */
}

static int sdio_generate_slave_intr(uint8_t intr_no)
{
	uint8_t intr_mask = BIT(intr_no + ESP_SDIO_CONF_OFFSET);

	if (intr_no >= BIT(ESP_MAX_HOST_INTERRUPT)) {
		ESP_LOGE(TAG,"Invalid slave interrupt number");
		return ESP_ERR_INVALID_ARG;
	}

	return g_h.funcs->_h_sdio_write_reg(HOST_TO_SLAVE_INTR, &intr_mask,
		sizeof(intr_mask), ACQUIRE_LOCK);
}

int send_slave_power_save(int ps)
{
	if (ps)
		return sdio_generate_slave_intr(ESP_POWER_SAVE_ON);
	else
		return sdio_generate_slave_intr(ESP_POWER_SAVE_OFF);
}


static inline int sdio_get_intr(uint32_t *interrupts)
{
	return g_h.funcs->_h_sdio_read_reg(ESP_SLAVE_INT_RAW_REG, (uint8_t *)interrupts,
		sizeof(uint32_t), ACQUIRE_LOCK);
}

static inline int sdio_clear_intr(uint32_t interrupts)
{
	return g_h.funcs->_h_sdio_write_reg(ESP_SLAVE_INT_CLR_REG, (uint8_t *)&interrupts,
		sizeof(uint32_t), ACQUIRE_LOCK);
}

static int sdio_get_tx_buffer_num(uint32_t *tx_num, bool is_lock_needed)
{
	uint32_t len = 0;
	int ret = 0;

	ret = g_h.funcs->_h_sdio_read_reg(ESP_SLAVE_TOKEN_RDATA, (uint8_t *)&len,
		sizeof(len), is_lock_needed);

	if (ret) {
		ESP_LOGE(TAG, "%s: err: %d", __func__, ret);
		return ret;
	}

	len = (len >> 16) & ESP_TX_BUFFER_MASK;
	len = (len + ESP_TX_BUFFER_MAX - sdio_tx_buf_count) % ESP_TX_BUFFER_MAX;

	*tx_num = len;

	return ret;
}

#if H_SDIO_HOST_RX_MODE != H_SDIO_ALWAYS_HOST_RX_MAX_TRANSPORT_SIZE

static int sdio_get_len_from_slave(uint32_t *rx_size, bool is_lock_needed)
{
	uint32_t len;
	uint32_t temp;
	int ret = 0;

	if (!rx_size)
		return ESP_FAIL;
	*rx_size = 0;

	ret = g_h.funcs->_h_sdio_read_reg(ESP_SLAVE_PACKET_LEN_REG,
		(uint8_t *)&len, sizeof(len), is_lock_needed);

	if (ret) {
		ESP_LOGE(TAG, "len read err: %d", ret);
		return ret;
	}

	len &= ESP_SLAVE_LEN_MASK;

	if (len >= sdio_rx_byte_count)
		len = (len + ESP_RX_BYTE_MAX - sdio_rx_byte_count) % ESP_RX_BYTE_MAX;
	else {
		/* Handle a case of roll over */
		temp = ESP_RX_BYTE_MAX - sdio_rx_byte_count;
		len = temp + len;

#if H_SDIO_HOST_RX_MODE != H_SDIO_HOST_STREAMING_MODE
		if (len > ESP_RX_BUFFER_SIZE) {
			ESP_LOGI(TAG, "%s: Len from slave[%ld] exceeds max [%d]",
					__func__, len, ESP_RX_BUFFER_SIZE);
		}
#endif
	}
	*rx_size = len;

	return 0;
}
#endif

static int sdio_is_write_buffer_available(uint32_t buf_needed)
{
    static uint32_t buf_available = 0;
    uint16_t retry = 0;
    uint32_t interval_us = 0;
	uint32_t max_retry_sdio_not_responding = 2;

    do {
        if (buf_available >= buf_needed) {
            /* Slave has sufficient sdio buffers */
            break;
        }

        /* If buffer needed are less than buffer available,
		 * then only read for available buffer number from slave */
        if (0x107 == sdio_get_tx_buffer_num(&buf_available, ACQUIRE_LOCK) ) {
			max_retry_sdio_not_responding--;

			if (!max_retry_sdio_not_responding) {
				esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
				esp_restart();
			}
			continue;
		}

        /* Retry until slave side sdio buffer gets available.
         * Incrementally add usleep & retry to get current slave rx sdio buffers
         *
         * MAX_RETRY          : Max retry before returning failure
         * INTERVAL_US_START  : usleep interval to add exponentially
         * INTERVAL_US_MAX    : max saturation of usleep interval
         * REPEAT_STEP        : repeat usleep for curr val before exponential increase
         */
        if (retry && (retry % BUF_AVAIL_REPEAT_STEP == 0)) {
            if (interval_us == 0)
                /* Retries without sleep exhausted, start with sleep interval */
                interval_us = BUF_AVAIL_INTERVAL_US_START;
            else
                /* Increase interval exponentially */
                interval_us <<= 1;
        }

        if (interval_us > BUF_AVAIL_INTERVAL_US_MAX)
            /* Max saturation of usleep interval */
            interval_us = BUF_AVAIL_INTERVAL_US_MAX;

        if (interval_us) {
            ESP_LOGV(TAG,"Retry[%u] with usleep[%lu]", retry, interval_us);
            g_h.funcs->_h_usleep(interval_us);
        } else {
            ESP_LOGV(TAG,"Retry[%u]", retry);
		}

        retry++;
    } while(retry < BUF_AVAIL_MAX_RETRY);

    if (buf_available >= buf_needed) {
        buf_available -= buf_needed;
		return 1;
	}

	/* No buffer available at slave */
	return 0;
}

static void sdio_write_task(void const* pvParameters)
{
	uint16_t len = 0;
	uint8_t *sendbuf = NULL;
	void (*free_func)(void* ptr) = NULL;
	struct esp_payload_header * payload_header = NULL;
	uint8_t * payload  = NULL;
	interface_buffer_handle_t buf_handle = {0};
	uint8_t retry = 0;

	int ret = 0;
	uint8_t *pos = NULL;
	uint32_t data_left;
	uint32_t len_to_send;
	uint32_t buf_needed;
	uint8_t tx_needed = 1;

	while (!sdio_start_write_thread)
		g_h.funcs->_h_msleep(10);

	for (;;) {
		/* Check if higher layers have anything to transmit */
		g_h.funcs->_h_get_semaphore(sem_to_slave_queue, HOSTED_BLOCK_MAX);

		/* Tx msg is present as per sem */
		if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_SERIAL], &buf_handle, 0))
			if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_BT], &buf_handle, 0))
				if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_OTHERS], &buf_handle, 0)) {
					tx_needed = 0; /* No Tx msg */
				}

		if (tx_needed)
			len = buf_handle.payload_len;

		if (!len) {
			ESP_LOGE(TAG, "%s: Empty len", __func__);
			goto done;
		}
#if ESP_PKT_STATS
		if (buf_handle.if_type == ESP_STA_IF)
			pkt_stats.sta_tx_trans_in++;
#endif

		if (!buf_handle.payload_zcopy) {
			sendbuf = sdio_buffer_alloc(MEMSET_REQUIRED);
			assert(sendbuf);
			free_func = sdio_buffer_free;
		} else {
			sendbuf = buf_handle.payload;
			free_func = buf_handle.free_buf_handle;
		}

		if (!sendbuf) {
			ESP_LOGE(TAG, "sdio buff malloc failed");
			free_func = NULL;
			goto done;
		}

		if (buf_handle.payload_len > MAX_SDIO_BUFFER_SIZE - sizeof(struct esp_payload_header)) {
			ESP_LOGE(TAG, "Pkt len [%u] > Max [%u]. Drop",
					buf_handle.payload_len, MAX_SDIO_BUFFER_SIZE - sizeof(struct esp_payload_header));
			goto done;
		}

		/* Form Tx header */
		payload_header = (struct esp_payload_header *) sendbuf;
		payload  = sendbuf + sizeof(struct esp_payload_header);

		payload_header->len = htole16(len);
		payload_header->offset = htole16(sizeof(struct esp_payload_header));
		payload_header->if_type = buf_handle.if_type;
		payload_header->if_num = buf_handle.if_num;
		payload_header->seq_num = htole16(buf_handle.seq_num);
		payload_header->flags = buf_handle.flag;
		UPDATE_HEADER_TX_PKT_NO(payload_header);

		if (payload_header->if_type == ESP_HCI_IF) {
			// special handling for HCI
			if (!buf_handle.payload_zcopy) {
				// copy first byte of payload into header
				payload_header->hci_pkt_type = buf_handle.payload[0];
				// adjust actual payload len
				payload_header->len = htole16(len - 1);
				g_h.funcs->_h_memcpy(payload, &buf_handle.payload[1], len - 1);
			}
		} else
		if (!buf_handle.payload_zcopy)
			g_h.funcs->_h_memcpy(payload, buf_handle.payload, len);

#if CONFIG_ESP_SDIO_CHECKSUM
		payload_header->checksum = htole16(compute_checksum(sendbuf,
			sizeof(struct esp_payload_header) + len));
#endif

		buf_needed = (len + sizeof(struct esp_payload_header) + ESP_RX_BUFFER_SIZE - 1)
			/ ESP_RX_BUFFER_SIZE;

		SDIO_DRV_LOCK();

		if (! sdio_is_write_buffer_available(buf_needed)) {
			ESP_LOGV(TAG, "no SDIO write buffers on slave device");
#if ESP_PKT_STATS
		if (payload_header->if_type == ESP_STA_IF)
			pkt_stats.sta_tx_out_drop++;
#endif
			goto unlock_done;
		}

		pos = sendbuf;
		data_left = len + sizeof(struct esp_payload_header);

		ESP_HEXLOGV("bus_TX", sendbuf, data_left, 32);

		len_to_send = 0;
		retry = 0;
		do {
			len_to_send = data_left;

			ret = g_h.funcs->_h_sdio_write_block(ESP_SLAVE_CMD53_END_ADDR - data_left,
				pos, len_to_send, ACQUIRE_LOCK);
			if (ret) {
				ESP_LOGE(TAG, "%s: %d: Failed to send data: %d %ld %ld", __func__,
					retry, ret, len_to_send, data_left);
				retry++;
				if (retry < MAX_SDIO_WRITE_RETRY) {
					ESP_LOGD(TAG, "retry");
					get_firmware_data(false);
					continue;
				} else {
					SDIO_DRV_UNLOCK();
					ESP_LOGE(TAG, "Unrecoverable host sdio state, reset host mcu");
					esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
					esp_restart();
					goto done;
				}
			}

			data_left -= len_to_send;
			pos += len_to_send;
		} while (data_left);

		sdio_tx_buf_count += buf_needed;
		sdio_tx_buf_count = sdio_tx_buf_count % ESP_TX_BUFFER_MAX;

#if ESP_PKT_STATS
		if (payload_header->if_type == ESP_STA_IF)
			pkt_stats.sta_tx_out++;
#endif

unlock_done:
		SDIO_DRV_UNLOCK();
done:
		if (len && !buf_handle.payload_zcopy) {
			/* free allocated buffer, only if zerocopy is not requested */
			H_FREE_PTR_WITH_FUNC(buf_handle.free_buf_handle, buf_handle.priv_buffer_handle);
		}
		H_FREE_PTR_WITH_FUNC(free_func, sendbuf);
	}
}

static int is_valid_sdio_rx_packet(uint8_t *rxbuff_a, uint16_t *len_a, uint16_t *offset_a)
{
	struct esp_payload_header * h = (struct esp_payload_header *)rxbuff_a;
	uint16_t len = 0, offset = 0;
#if CONFIG_ESP_SDIO_CHECKSUM
	uint16_t rx_checksum = 0, checksum = 0;
#endif
	uint8_t is_wakeup_pkt = 0;

	UPDATE_HEADER_RX_PKT_NO(h);
	if (!h || !len_a || !offset_a)
		return 0;

	/* Fetch length and offset from payload header */
	len = le16toh(h->len);
	offset = le16toh(h->offset);
	is_wakeup_pkt = h->flags & FLAG_WAKEUP_PKT;

	if (is_wakeup_pkt) {
		ESP_LOGI(TAG, "Host wakeup triggered, len: %u ", len);
		ESP_HEXLOGV("Wakeup_pkt", rxbuff_a+offset, len, len);
	}

	if ((!len) ||
		(len > MAX_PAYLOAD_SIZE) ||
		(offset != sizeof(struct esp_payload_header))) {

		/* Free up buffer, as one of following -
		 * 1. no payload to process
		 * 2. input packet size > driver capacity
		 * 3. payload header size mismatch,
		 * wrong header/bit packing?
		 * */
		if (len) {
			ESP_LOGE(TAG, "len[%u]>max[%u] OR offset[%u] != exp[%u], Drop",
				len, MAX_PAYLOAD_SIZE, offset, sizeof(struct esp_payload_header));
		}
		return 0;

	}

#if CONFIG_ESP_SDIO_CHECKSUM
	rx_checksum = le16toh(h->checksum);
	h->checksum = 0;
	checksum = compute_checksum((uint8_t*)h, len + offset);

	if (checksum != rx_checksum) {
		ESP_LOGE(TAG, "SDIO RX rx_chksum[%u] != checksum[%u]. Drop.",
				checksum, rx_checksum);
		return 0;
	}
#endif

#if ESP_PKT_STATS
	if (h->if_type == ESP_STA_IF)
		pkt_stats.sta_rx_in++;
#endif

	*len_a = len;
	*offset_a = offset;

	return 1;
}

// pushes received packet data on to rx queue
static esp_err_t sdio_push_pkt_to_queue(uint8_t * rxbuff, uint16_t len, uint16_t offset)
{
	uint8_t pkt_prio = PRIO_Q_OTHERS;
	struct esp_payload_header *h= NULL;
	interface_buffer_handle_t buf_handle;

	h = (struct esp_payload_header *)rxbuff;

	memset(&buf_handle, 0, sizeof(interface_buffer_handle_t));

	buf_handle.priv_buffer_handle = rxbuff;
	buf_handle.free_buf_handle    = sdio_buffer_free;
	buf_handle.payload_len        = len;
	buf_handle.if_type            = h->if_type;
	buf_handle.if_num             = h->if_num;
	buf_handle.payload            = rxbuff + offset;
	buf_handle.seq_num            = le16toh(h->seq_num);
	buf_handle.flag               = h->flags;

	if (buf_handle.if_type == ESP_SERIAL_IF)
		pkt_prio = PRIO_Q_SERIAL;
	else if (buf_handle.if_type == ESP_HCI_IF)
		pkt_prio = PRIO_Q_BT;
	/* else OTHERS by default */

	g_h.funcs->_h_queue_item(from_slave_queue[pkt_prio], &buf_handle, portMAX_DELAY);
	g_h.funcs->_h_post_semaphore(sem_from_slave_queue);

	return ESP_OK;
}

#if !H_SDIO_HOST_STREAMING_MODE
static void update_flow_ctl(uint8_t *rxbuff)
{
#if defined(H_H2S_WIFI_FLOW_CTRL)
	struct esp_payload_header * h = (struct esp_payload_header *)rxbuff;
	if (h->throttle_cmd) {

		ESP_EARLY_LOGV(TAG, "Start sta_tx flow control[%u]", h->throttle_cmd);

		if (h->throttle_cmd == H_FLOW_CTL_ON) {
			/* Flow control started -> block/drop wifi traffic.
			 * So allow_wifi bit is cleared
			 */
			ESP_LOGV(TAG, "h_fc_on");
#if ESP_PKT_STATS
			pkt_stats.sta_flow_ctrl_on++;
#endif
			g_h.funcs->_h_reset_event_bit(eg_h2s_wifi_flow_ctrl,
					H_EVTGRP_BIT_FC_ALLOW_WIFI);
		}
		if (h->throttle_cmd == H_FLOW_CTL_OFF) {
			/* Flow control stopped/no more required -> allow wifi traffic.
			 * So allow_wifi bit is set
			 */
			ESP_LOGV(TAG, "h_fc_off");
#if ESP_PKT_STATS
			pkt_stats.sta_flow_ctrl_off++;
#endif
			g_h.funcs->_h_set_event_bit(eg_h2s_wifi_flow_ctrl,
				H_EVTGRP_BIT_FC_ALLOW_WIFI);
		}
	}
#endif
}
#endif
/**
 * These function definitions depend on whether we are in SDIO
 * streaming mode or not.
 */
#if H_SDIO_HOST_RX_MODE != H_SDIO_HOST_STREAMING_MODE
// SDIO packet mode
// return a buffer big enough to contain the data
static uint8_t * sdio_rx_get_buffer(uint32_t len)
{
	// return mempool allocated buffer
	return sdio_buffer_alloc(MEMSET_REQUIRED);
}

// this frees the buffer *before* it is queued
static void sdio_rx_free_buffer(uint8_t * buf)
{
	sdio_buffer_free(buf);
}

// push buffer on to the queue
static esp_err_t sdio_push_data_to_queue(uint8_t * buf, uint32_t buf_len)
{
	uint16_t len = 0;
	uint16_t offset = 0;

	/* Drop packet if no processing needed */
	if (!is_valid_sdio_rx_packet(buf, &len, &offset)) {
		/* Free up buffer, as one of following -
		 * 1. no payload to process
		 * 2. input packet size > driver capacity
		 * 3. payload header size mismatch,
		 * wrong header/bit packing?
		 * */
		ESP_LOGW(TAG, "Dropping packet");
		sdio_buffer_free(buf);
		return ESP_FAIL;
	}

	if (sdio_push_pkt_to_queue(buf, len, offset)) {
		ESP_LOGE(TAG, "Failed to push Rx packet to queue");
		return ESP_FAIL;
	}

	return ESP_OK;
}
#else // H_SDIO_HOST_STREAMING_MODE
// SDIO streaming mode
// return a buffer big enough to contain the data
static uint8_t * sdio_rx_get_buffer(uint32_t len)
{
	// (re)allocate a buffer big enough to contain the data stream
	if (len > recv_buf_size) {
		if (recv_buf) {
			// free already allocated memory
			g_h.funcs->_h_free(recv_buf);
		}
		recv_buf = (uint8_t *)MEM_ALLOC(len);
		assert(recv_buf);
		recv_buf_size = len;
		ESP_LOGD(TAG, "recv_buf_size %ld", recv_buf_size);
	}
	return recv_buf;
}

// this frees the buffer *before* it is queued
static void sdio_rx_free_buffer(uint8_t * buf)
{
	// no op - keep the allocated static buffer as it is
}

// extract packets from the stream and push on to the queue
static esp_err_t sdio_push_data_to_queue(uint8_t * buf, uint32_t buf_len)
{
	uint8_t * pkt_rxbuff = NULL;
	uint16_t len = 0;
	uint16_t offset = 0;
	uint32_t packet_size;

	// break up the data stream into packets to send to the queue
	do {
		update_flow_ctl(buf);

		if (!is_valid_sdio_rx_packet(buf, &len, &offset)) {
			/* Have to drop packets in the stream as we cannot decode
			 * them after this error */
			ESP_LOGV(TAG, "Dropping packet(s) from stream");
			return ESP_FAIL;
		}
		/* Allocate rx buffer */
		pkt_rxbuff = sdio_buffer_alloc(MEMSET_REQUIRED);
		assert(pkt_rxbuff);

		packet_size = len + offset;
		if (packet_size > buf_len) {
			ESP_LOGE(TAG, "packet size[%lu]>[%lu] too big for remaining stream data",
					packet_size, buf_len);
			return ESP_FAIL;
		}
		memcpy(pkt_rxbuff, buf, packet_size);


		if (sdio_push_pkt_to_queue(pkt_rxbuff, len, offset)) {
			ESP_LOGI(TAG, "Failed to push a packet to queue from stream");
		}

		// move to the next packet in the stream
		buf_len -= packet_size;
		buf     += packet_size;
	} while (buf_len);

	return ESP_OK;
}
#endif

static int get_firmware_data(bool is_lock_needed)
{
    uint32_t len;
    int ret = 0;

	ret = g_h.funcs->_h_sdio_read_reg(ESP_SLAVE_PACKET_LEN_REG,
		(uint8_t *)&len, sizeof(len), is_lock_needed);
	if (ret) {
		ESP_LOGE(TAG, "len read err: %d", ret);
		return ret;
	}

    /* Initialize rx_byte_count */
    ESP_LOGI(TAG,"Rx Pre ====== %lu", sdio_rx_byte_count);
    sdio_rx_byte_count = len & ESP_SLAVE_LEN_MASK;
    ESP_LOGI(TAG,"Rx Pos ======  %lu", sdio_rx_byte_count);

	ret = g_h.funcs->_h_sdio_read_reg(ESP_SLAVE_TOKEN_RDATA, (uint8_t *)&len,
		sizeof(len), is_lock_needed);
	if (ret) {
		ESP_LOGE(TAG, "%s: err: %d", __func__, ret);
		return ret;
	}

	len = (len >> 16) & ESP_TX_BUFFER_MASK;

    ESP_LOGI(TAG,"Tx Pre ======  %lu", sdio_tx_buf_count);
    if (len >= ESP_TX_BUFFER_MAX)
        sdio_tx_buf_count = len - ESP_TX_BUFFER_MASK;
    else
        sdio_tx_buf_count = 0;
    ESP_LOGI(TAG,"Tx Pos ======  %lu", sdio_tx_buf_count);

    return ret;
}

static void sdio_read_task(void const* pvParameters)
{
	esp_err_t res;
	uint8_t *rxbuff = NULL;
	int ret;
	uint32_t len_from_slave;

	uint32_t data_left;
	uint32_t len_to_read;
	uint8_t *pos;
	uint32_t interrupts;

	assert(sdio_handle);

	// wait for transport to be in reset state
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(100));
		if (is_transport_rx_ready()) {
			break;
		}
	}

	res = g_h.funcs->_h_sdio_card_init();
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "sdio card init failed");
		return;
	}

	create_debugging_tasks();

	// display which SDIO mode we are operating in
#if H_SDIO_HOST_RX_MODE == H_SDIO_HOST_STREAMING_MODE
	ESP_LOGI(TAG, "SDIO Host operating in STREAMING MODE");
#else
	ESP_LOGI(TAG, "SDIO Host operating in PACKET MODE");
#endif

	ESP_LOGI(TAG, "generate slave intr");

	set_host_wakeup_reason(esp_sleep_get_wakeup_cause());
	if (is_host_reboot_due_to_deep_sleep()) {
		ESP_LOGW(TAG, "Host Woke-up");
		get_firmware_data(ACQUIRE_LOCK);
		sdio_generate_slave_intr(ESP_POWER_SAVE_OFF);
	} else {
		/* inform the slave device that we are ready */
		//sdio_generate_slave_intr(ESP_POWER_SAVE_OFF);
		sdio_generate_slave_intr(ESP_OPEN_DATA_PATH);
		get_firmware_data(ACQUIRE_LOCK);
	}

	for (;;) {

		// wait for sdio interrupt from slave
		// call will block until there is an interrupt, timeout or error
		ESP_LOGV(TAG, "--- Wait for SDIO intr ---");
		res = g_h.funcs->_h_sdio_wait_slave_intr(portMAX_DELAY);

		if (res != ESP_OK) {
			ESP_LOGE(TAG, "wait_slave_intr error: %d", res);
			continue;
		}

		SDIO_DRV_LOCK();

		// clear slave interrupts
		if (sdio_get_intr(&interrupts)) {
			ESP_LOGE(TAG, "failed to read interrupt register");

			SDIO_DRV_UNLOCK();
			ESP_LOGI(TAG, "Host is reseting itself, to avoid any sdio race condition");
			esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
			esp_restart();
			continue;
		}
		sdio_clear_intr(interrupts);

		ESP_LOGV(TAG, "Intr: %08"PRIX32, interrupts);

#if 0 //defined(H_H2S_WIFI_FLOW_CTRL)
		/* Check all supported interrupts */
		if (BIT(SDIO_S2H_INTR_FLOW_CTRL_ON) & interrupts) {

			ESP_LOGV(TAG, "Start sta_tx flow control");
			#if ESP_PKT_STATS
				pkt_stats.sta_flow_ctrl_on++;
			#endif

			g_h.funcs->_h_reset_event_bit(eg_h2s_wifi_flow_ctrl,
					H_EVTGRP_BIT_FC_ALLOW_WIFI);
			/* Flow control started -> block/drop wifi traffic.
			 * So allow_wifi bit is cleared
			 */

		}


		if (BIT(SDIO_S2H_INTR_FLOW_CTRL_OFF) & interrupts) {

			#if ESP_PKT_STATS
				pkt_stats.sta_flow_ctrl_off++;
			#endif
			ESP_LOGV(TAG, "Stop sta_tx flow control");
			g_h.funcs->_h_set_event_bit(eg_h2s_wifi_flow_ctrl,
					H_EVTGRP_BIT_FC_ALLOW_WIFI);
			/* Flow control stopped/no more required -> allow wifi traffic.
			 * So allow_wifi bit is set
			 */
		}
#endif

		if (!(BIT(SDIO_S2H_INTR_NEW_PKT) & interrupts)) {

			SDIO_DRV_UNLOCK();
			continue;
		}

#if H_SDIO_HOST_RX_MODE == H_SDIO_ALWAYS_HOST_RX_MAX_TRANSPORT_SIZE
		/* Bypass the check to find the bytes to be read from slave to host
		 * always assume max transport size to be read.
		 * slave sdio driver will automatically pad the remaining bytes after
		 * actual written bytes till requested size from host
		 * This typically improves throughput for larger packet sizes
		 **/
		len_from_slave = MAX_TRANSPORT_BUFFER_SIZE;
#else
		/* check the bytes to be read */
		ret = sdio_get_len_from_slave(&len_from_slave, ACQUIRE_LOCK);
		if (ret || !len_from_slave) {
			ESP_LOGW(TAG, "invalid ret or len_from_slave: %d %ld", ret, len_from_slave);

			SDIO_DRV_UNLOCK();
			continue;
		}
#endif

		/* Allocate rx buffer */
		rxbuff = sdio_rx_get_buffer(len_from_slave);
		assert(rxbuff);

		data_left = len_from_slave;
		pos = rxbuff;

		do {
			len_to_read = data_left;

			ret = g_h.funcs->_h_sdio_read_block(
					ESP_SLAVE_CMD53_END_ADDR - data_left,
					pos, len_to_read, ACQUIRE_LOCK);
			if (ret) {
				ESP_LOGE(TAG, "%s: Failed to read data - %d %ld %ld",
					__func__, ret, len_to_read, data_left);
				sdio_rx_free_buffer(rxbuff);
				break;
			}
			data_left -= len_to_read;
			pos += len_to_read;
		} while (data_left);

		SDIO_DRV_UNLOCK();

		//TODO: unclear, on failure case
		//sdio_rx_byte_count += (len_from_slave-data_left);
		sdio_rx_byte_count += len_from_slave;
		sdio_rx_byte_count = sdio_rx_byte_count % ESP_RX_BYTE_MAX;

		if (ret)
			continue;
#if !H_SDIO_HOST_STREAMING_MODE
		update_flow_ctl(rxbuff);
#endif

		ESP_LOGV(TAG, "Data to process with len[%"PRIu32"]", len_from_slave);
		if (sdio_push_data_to_queue(rxbuff, len_from_slave))
			ESP_LOGV(TAG, "Failed to push data to rx queue");
	}
}

/**
 * TODO: unify sdio_process_rx_task() and spi_process_rx_task()
 */
static void sdio_process_rx_task(void const* pvParameters)
{
	interface_buffer_handle_t buf_handle_l = {0};
	interface_buffer_handle_t *buf_handle = NULL;
	int ret = 0;

	struct esp_priv_event *event = NULL;

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(100));
		if (is_transport_rx_ready()) {
			break;
		}
	}
	ESP_LOGI(TAG, "Starting SDIO process rx task");

	while (1) {
		g_h.funcs->_h_get_semaphore(sem_from_slave_queue, portMAX_DELAY);

		if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_SERIAL], &buf_handle_l, 0))
			if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_BT], &buf_handle_l, 0))
				if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_OTHERS], &buf_handle_l, 0)) {
					ESP_LOGI(TAG, "No element in any queue found");
					continue;
				}

		buf_handle = &buf_handle_l;

		ESP_LOGV(TAG, "bus_rx: iftype:%d", (int)buf_handle->if_type);
		ESP_HEXLOGV("bus_rx", buf_handle->priv_buffer_handle,
				buf_handle->payload_len+H_ESP_PAYLOAD_HEADER_OFFSET, 32);

		if (buf_handle->if_type == ESP_SERIAL_IF) {
			/* serial interface path */
			serial_rx_handler(buf_handle);
		} else if((buf_handle->if_type == ESP_STA_IF) ||
				(buf_handle->if_type == ESP_AP_IF)) {
#if 1
			if (chan_arr[buf_handle->if_type] && chan_arr[buf_handle->if_type]->rx) {
				/* TODO : Need to abstract heap_caps_malloc */
				uint8_t * copy_payload = (uint8_t *)g_h.funcs->_h_malloc(buf_handle->payload_len);
				assert(copy_payload);
				assert(buf_handle->payload_len);
				assert(buf_handle->payload);
				memcpy(copy_payload, buf_handle->payload, buf_handle->payload_len);
				H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle, buf_handle->priv_buffer_handle);

#if ESP_PKT_STATS
				if (buf_handle->if_type == ESP_STA_IF)
					pkt_stats.sta_rx_out++;
#endif
				ret = chan_arr[buf_handle->if_type]->rx(chan_arr[buf_handle->if_type]->api_chan,
						copy_payload, copy_payload, buf_handle->payload_len);
				if (unlikely(ret))
					HOSTED_FREE(copy_payload);
			}
#else
			if (chan_arr[buf_handle->if_type] && chan_arr[buf_handle->if_type]->rx) {
				chan_arr[buf_handle->if_type]->rx(chan_arr[buf_handle->if_type]->api_chan,
						buf_handle->payload, NULL, buf_handle->payload_len);
			}
#endif
		} else if (buf_handle->if_type == ESP_PRIV_IF) {
			process_priv_communication(buf_handle);
			hci_drv_show_configuration();
			/* priv transaction received */
			ESP_LOGI(TAG, "Received INIT event");
			sdio_start_write_thread = true;

			event = (struct esp_priv_event *) (buf_handle->payload);
			if (event->event_type != ESP_PRIV_EVENT_INIT) {
				/* User can re-use this type of transaction */
			}
		} else if (buf_handle->if_type == ESP_HCI_IF) {
			hci_rx_handler(buf_handle);
		} else if (buf_handle->if_type == ESP_TEST_IF) {
#if TEST_RAW_TP
			update_test_raw_tp_rx_len(buf_handle->payload_len +
				H_ESP_PAYLOAD_HEADER_OFFSET);
#endif
		} else {
			ESP_LOGW(TAG, "unknown type %d ", buf_handle->if_type);
		}

		/* Free buffer handle */
		/* When buffer offloaded to other module, that module is
		 * responsible for freeing buffer. In case not offloaded or
		 * failed to offload, buffer should be freed here.
		 */
		if (!buf_handle->payload_zcopy) {
			H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle,
				buf_handle->priv_buffer_handle);
		}
	}
}

void bus_init_internal(void)
{
	uint8_t prio_q_idx = 0;
	/* register callback */

	sdio_bus_lock = g_h.funcs->_h_create_mutex();
	assert(sdio_bus_lock);

	sem_to_slave_queue = g_h.funcs->_h_create_semaphore(TO_SLAVE_QUEUE_SIZE*MAX_PRIORITY_QUEUES);
	assert(sem_to_slave_queue);
	g_h.funcs->_h_get_semaphore(sem_to_slave_queue, 0);

	sem_from_slave_queue = g_h.funcs->_h_create_semaphore(FROM_SLAVE_QUEUE_SIZE*MAX_PRIORITY_QUEUES);
	assert(sem_from_slave_queue);
	g_h.funcs->_h_get_semaphore(sem_from_slave_queue, 0);

	/* cleanup the semaphores */


	for (prio_q_idx=0; prio_q_idx<MAX_PRIORITY_QUEUES;prio_q_idx++) {
		/* Queue - rx */
		from_slave_queue[prio_q_idx] = g_h.funcs->_h_create_queue(FROM_SLAVE_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(from_slave_queue[prio_q_idx]);

		/* Queue - tx */
		to_slave_queue[prio_q_idx] = g_h.funcs->_h_create_queue(TO_SLAVE_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(to_slave_queue[prio_q_idx]);
	}

	sdio_mempool_create();

	/* initialise SDMMC before starting read/write threads
	 * which depend on SDMMC*/
	sdio_handle = g_h.funcs->_h_bus_init();
	if (!sdio_handle) {
		ESP_LOGE(TAG, "could not create sdio handle, exiting");
		assert(sdio_handle);
	}

	sdio_read_thread = g_h.funcs->_h_thread_create("sdio_read",
		DFLT_TASK_PRIO, DFLT_TASK_STACK_SIZE, sdio_read_task, NULL);

	sdio_process_rx_thread = g_h.funcs->_h_thread_create("sdio_process_rx",
		DFLT_TASK_PRIO, DFLT_TASK_STACK_SIZE, sdio_process_rx_task, NULL);

	sdio_write_thread = g_h.funcs->_h_thread_create("sdio_write",
		DFLT_TASK_PRIO, DFLT_TASK_STACK_SIZE, sdio_write_task, NULL);

#if defined(USE_DRIVER_LOCK)
	// initialise mutex for bus locking
	sdio_bus_lock = g_h.funcs->_h_create_mutex();
	assert(sdio_bus_lock);
#endif
	ESP_LOGD(TAG, "sdio bus init done");
}

int esp_hosted_tx(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen, uint8_t buff_zcopy,
		void (*free_wbuf_fun)(void* ptr))
{
	interface_buffer_handle_t buf_handle = {0};
	void (*free_func)(void* ptr) = NULL;
	uint8_t pkt_prio = PRIO_Q_OTHERS;
	uint8_t transport_up = is_transport_tx_ready();

	if (free_wbuf_fun)
		free_func = free_wbuf_fun;

	if (!wbuffer || !wlen ||
		(wlen > MAX_PAYLOAD_SIZE) ||
		!transport_up) {
		ESP_LOGE(TAG, "tx fail: NULL buff, invalid len (%u) or len > max len (%u), transport_up(%u))",
				wlen, MAX_PAYLOAD_SIZE, transport_up);
		H_FREE_PTR_WITH_FUNC(free_func, wbuffer);
		return ESP_FAIL;
	}
	buf_handle.payload_zcopy = buff_zcopy;
	buf_handle.if_type = iface_type;
	buf_handle.if_num = iface_num;
	buf_handle.payload_len = wlen;
	buf_handle.payload = wbuffer;
	buf_handle.priv_buffer_handle = wbuffer;
	buf_handle.free_buf_handle = free_func;

	if (buf_handle.if_type == ESP_SERIAL_IF)
		pkt_prio = PRIO_Q_SERIAL;
	else if (buf_handle.if_type == ESP_HCI_IF)
		pkt_prio = PRIO_Q_BT;
	/* else OTHERS by default */

#if ESP_PKT_STATS
	if (buf_handle.if_type == ESP_STA_IF)
		pkt_stats.sta_tx_in_pass++;
#endif

	g_h.funcs->_h_queue_item(to_slave_queue[pkt_prio], &buf_handle, portMAX_DELAY);
	g_h.funcs->_h_post_semaphore(sem_to_slave_queue);


	return ESP_OK;
}
