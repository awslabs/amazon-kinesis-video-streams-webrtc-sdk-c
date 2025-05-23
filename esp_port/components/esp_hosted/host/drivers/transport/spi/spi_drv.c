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


#include "mempool.h"
#include "common.h"
#include "esp_hosted_config.h"
//#include "netdev_if.h"
#include "transport_drv.h"
#include "spi_drv.h"
#include "serial_drv.h"
#include "adapter.h"
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "stats.h"
#include "hci_drv.h"

DEFINE_LOG_TAG(spi);

void * spi_handle = NULL;
semaphore_handle_t spi_trans_ready_sem;

#if DEBUG_HOST_TX_SEMAPHORE
#define H_DEBUG_GPIO_PIN_Host_Tx_Port NULL
#define H_DEBUG_GPIO_PIN_Host_Tx_Pin  -1
#endif

static uint8_t schedule_dummy_rx = 0;

static void * spi_transaction_thread;
/* TODO to move this in transport drv */
extern transport_channel_t *chan_arr[ESP_MAX_IF];

/* Create mempool for cache mallocs */
static struct mempool * buf_mp_g;

/** Exported variables **/

static void * spi_bus_lock;

/* Queue declaration */
static queue_handle_t to_slave_queue[MAX_PRIORITY_QUEUES];
semaphore_handle_t sem_to_slave_queue;
static queue_handle_t from_slave_queue[MAX_PRIORITY_QUEUES];
semaphore_handle_t sem_from_slave_queue;

static void * spi_rx_thread;


/** function declaration **/
/** Exported functions **/
static void spi_transaction_task(void const* pvParameters);
static void spi_process_rx_task(void const* pvParameters);
static uint8_t * get_next_tx_buffer(uint8_t *is_valid_tx_buf, void (**free_func)(void* ptr));

static inline void spi_mempool_create()
{
	MEM_DUMP("spi_mempool_create");
	buf_mp_g = mempool_create(MAX_SPI_BUFFER_SIZE);
#ifdef CONFIG_ESP_CACHE_MALLOC
	assert(buf_mp_g);
#endif
}

static inline void spi_mempool_destroy()
{
	mempool_destroy(buf_mp_g);
}

static inline void *spi_buffer_alloc(uint need_memset)
{
	return mempool_alloc(buf_mp_g, MAX_SPI_BUFFER_SIZE, need_memset);
}

static inline void spi_buffer_free(void *buf)
{
	mempool_free(buf_mp_g, buf);
}


/*
This ISR is called when the handshake or data_ready line goes high.
*/
static void FAST_RAM_ATTR gpio_hs_isr_handler(void* arg)
{
#if 0
	//Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
	//looking at the time between interrupts and refusing any interrupt too close to another one.
	static uint32_t lasthandshaketime_us;
	uint32_t currtime_us = esp_timer_get_time();
	uint32_t diff = currtime_us - lasthandshaketime_us;
	if (diff < 1000) {
		return; //ignore everything <1ms after an earlier irq
	}
	lasthandshaketime_us = currtime_us;
#endif
	g_h.funcs->_h_post_semaphore_from_isr(spi_trans_ready_sem);
	ESP_EARLY_LOGV(TAG, "%s", __func__);
}

/*
This ISR is called when the handshake or data_ready line goes high.
*/
static void FAST_RAM_ATTR gpio_dr_isr_handler(void* arg)
{
	g_h.funcs->_h_post_semaphore_from_isr(spi_trans_ready_sem);
	ESP_EARLY_LOGV(TAG, "%s", __func__);
}

void bus_deinit_internal(void)
{
	/* TODO */
}

/**
  * @brief  transport initializes
  * @param  transport_evt_handler_fp - event handler
  * @retval None
  */
void bus_init_internal(void)
{
	uint8_t prio_q_idx;

	spi_bus_lock = g_h.funcs->_h_create_mutex();
	assert(spi_bus_lock);


	sem_to_slave_queue = g_h.funcs->_h_create_semaphore(TO_SLAVE_QUEUE_SIZE*MAX_PRIORITY_QUEUES);
	assert(sem_to_slave_queue);
	sem_from_slave_queue = g_h.funcs->_h_create_semaphore(FROM_SLAVE_QUEUE_SIZE*MAX_PRIORITY_QUEUES);
	assert(sem_from_slave_queue);

	for (prio_q_idx=0; prio_q_idx<MAX_PRIORITY_QUEUES;prio_q_idx++) {
		/* Queue - rx */
		from_slave_queue[prio_q_idx] = g_h.funcs->_h_create_queue(FROM_SLAVE_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(from_slave_queue[prio_q_idx]);

		/* Queue - tx */
		to_slave_queue[prio_q_idx] = g_h.funcs->_h_create_queue(TO_SLAVE_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
		assert(to_slave_queue[prio_q_idx]);
	}

	spi_mempool_create();

	/* Creates & Give sem for next spi trans */
	spi_trans_ready_sem = g_h.funcs->_h_create_semaphore(1);
	assert(spi_trans_ready_sem);

	spi_handle = g_h.funcs->_h_bus_init();
	if (!spi_handle) {
		ESP_LOGE(TAG, "could not create spi handle, exiting\n");
		assert(spi_handle);
	}

	/* Task - SPI transaction (full duplex) */
	spi_transaction_thread = g_h.funcs->_h_thread_create("spi_trans", DFLT_TASK_PRIO,
			DFLT_TASK_STACK_SIZE, spi_transaction_task, NULL);
	assert(spi_transaction_thread);

	/* Task - RX processing */
	spi_rx_thread = g_h.funcs->_h_thread_create("spi_rx", DFLT_TASK_PRIO,
			DFLT_TASK_STACK_SIZE, spi_process_rx_task, NULL);
	assert(spi_rx_thread);
}


/**
  * @brief  Schedule SPI transaction if -
  * a. valid TX buffer is ready at SPI host (STM)
  * b. valid TX buffer is ready at SPI peripheral (ESP)
  * c. Dummy transaction is expected from SPI peripheral (ESP)
  * @param  argument: Not used
  * @retval None
  */
static int process_spi_rx_buf(uint8_t * rxbuff)
{
	struct  esp_payload_header *payload_header;
	uint16_t rx_checksum = 0, checksum = 0;
	interface_buffer_handle_t buf_handle = {0};
	uint16_t len, offset;
	int ret = 0;
	uint8_t pkt_prio = PRIO_Q_OTHERS;

	if (!rxbuff)
		return -1;

	/* create buffer rx handle, used for processing */
	payload_header = (struct esp_payload_header *) rxbuff;

	/* Fetch length and offset from payload header */
	len = le16toh(payload_header->len);
	offset = le16toh(payload_header->offset);

	ESP_HEXLOGV("h_spi_rx", rxbuff, len, 32);

	if (ESP_MAX_IF == payload_header->if_type)
		schedule_dummy_rx = 0;

	if (!len) {
		wifi_tx_throttling = payload_header->throttle_cmd;
		ret = -5;
		goto done;
	}

	if ((len > MAX_PAYLOAD_SIZE) ||
		(offset != sizeof(struct esp_payload_header))) {
		ESP_LOGI(TAG, "rx packet ignored: len [%u], rcvd_offset[%u], exp_offset[%u]\n",
				len, offset, sizeof(struct esp_payload_header));

		/* 1. no payload to process
		 * 2. input packet size > driver capacity
		 * 3. payload header size mismatch,
		 * wrong header/bit packing?
		 * */
		ret = -2;
		goto done;

	} else {
		rx_checksum = le16toh(payload_header->checksum);
		payload_header->checksum = 0;

		checksum = compute_checksum(rxbuff, len+offset);
		//TODO: checksum needs to be configurable from menuconfig
		if (checksum == rx_checksum) {
			buf_handle.priv_buffer_handle = rxbuff;
			buf_handle.free_buf_handle = spi_buffer_free;
			buf_handle.payload_len = len;
			buf_handle.if_type     = payload_header->if_type;
			buf_handle.if_num      = payload_header->if_num;
			buf_handle.payload     = rxbuff + offset;
			buf_handle.seq_num     = le16toh(payload_header->seq_num);
			buf_handle.flag        = payload_header->flags;
			wifi_tx_throttling     = payload_header->throttle_cmd;
#if 0
#if CONFIG_H_LOWER_MEMCOPY
			if ((buf_handle.if_type == ESP_STA_IF) ||
					(buf_handle.if_type == ESP_AP_IF))
				buf_handle.payload_zcopy = 1;
#endif
#endif
#if ESP_PKT_STATS
			if (buf_handle.if_type == ESP_STA_IF)
				pkt_stats.sta_rx_in++;
#endif
			if (buf_handle.if_type == ESP_SERIAL_IF)
				pkt_prio = PRIO_Q_SERIAL;
			else if (buf_handle.if_type == ESP_HCI_IF)
				pkt_prio = PRIO_Q_BT;
			/* else OTHERS by default */

			g_h.funcs->_h_queue_item(from_slave_queue[pkt_prio], &buf_handle, portMAX_DELAY);
			g_h.funcs->_h_post_semaphore(sem_from_slave_queue);

		} else {
			ESP_LOGI(TAG, "rcvd_crc[%u] != exp_crc[%u], drop pkt\n",checksum, rx_checksum);
			ret = -4;
			goto done;
		}
	}

	return ret;

done:
	/* error cases, abort */
	if (rxbuff) {
		spi_buffer_free(rxbuff);
#if H_MEM_STATS
		h_stats_g.spi_mem_stats.rx_freed++;
#endif
		rxbuff = NULL;
	}

	return ret;
}

static int check_and_execute_spi_transaction(void)
{
	uint8_t *txbuff = NULL;
	uint8_t *rxbuff = NULL;
	uint8_t is_valid_tx_buf = 0;
	void (*tx_buff_free_func)(void* ptr) = NULL;
	struct esp_payload_header *h = NULL;
	static uint8_t schedule_dummy_tx = 0;

	uint32_t ret = 0;
	struct hosted_transport_context_t spi_trans = {0};
	gpio_pin_state_t gpio_handshake = H_HS_VAL_INACTIVE;
	gpio_pin_state_t gpio_rx_data_ready = H_DR_VAL_INACTIVE;

	g_h.funcs->_h_lock_mutex(spi_bus_lock, portMAX_DELAY);

	/* handshake line SET -> slave ready for next transaction */
	gpio_handshake = g_h.funcs->_h_read_gpio(H_GPIO_HANDSHAKE_Port,
			H_GPIO_HANDSHAKE_Pin);

	/* data ready line SET -> slave wants to send something */
	gpio_rx_data_ready = g_h.funcs->_h_read_gpio(H_GPIO_DATA_READY_Port,
			H_GPIO_DATA_READY_Pin);

	if (gpio_handshake == H_HS_VAL_ACTIVE) {

		/* Get next tx buffer to be sent */
		txbuff = get_next_tx_buffer(&is_valid_tx_buf, &tx_buff_free_func);

		if ( (gpio_rx_data_ready == H_DR_VAL_ACTIVE) ||
				(is_valid_tx_buf) || schedule_dummy_tx || schedule_dummy_rx ) {

			if (!txbuff) {
				/* Even though, there is nothing to send,
				 * valid reseted txbuff is needed for SPI driver
				 */
				txbuff = spi_buffer_alloc(MEMSET_REQUIRED);
				assert(txbuff);

				h = (struct esp_payload_header *) txbuff;
				h->if_type = ESP_MAX_IF;
#if H_MEM_STATS
				h_stats_g.spi_mem_stats.tx_dummy_alloc++;
#endif
				tx_buff_free_func = spi_buffer_free;
				schedule_dummy_tx = 0;
			} else {
				schedule_dummy_tx = 1;
				ESP_HEXLOGV("h_spi_tx", txbuff, 32, 32);
			}

			ESP_LOGV(TAG, "dr %u tx_valid %u\n", gpio_rx_data_ready, is_valid_tx_buf);
			/* Allocate rx buffer */
			rxbuff = spi_buffer_alloc(MEMSET_REQUIRED);
			assert(rxbuff);
			//heap_caps_dump_all();
#if H_MEM_STATS
			h_stats_g.spi_mem_stats.rx_alloc++;
#endif

			spi_trans.tx_buf = txbuff;
			spi_trans.tx_buf_size = MAX_SPI_BUFFER_SIZE;
			spi_trans.rx_buf = rxbuff;

#if ESP_PKT_STATS
			struct  esp_payload_header *payload_header =
				(struct esp_payload_header *) txbuff;
			if (payload_header->if_type == ESP_STA_IF)
				pkt_stats.sta_tx_out++;
#endif

			/* Execute transaction only if EITHER holds true-
			 * a. A valid tx buffer to be transmitted towards slave
			 * b. Slave wants to send something (Rx for host)
			 */
			ret = g_h.funcs->_h_do_bus_transfer(&spi_trans);

			if (!ret)
				process_spi_rx_buf(spi_trans.rx_buf);
		}

		if (txbuff && tx_buff_free_func) {
			tx_buff_free_func(txbuff);
#if H_MEM_STATS
			if (tx_buff_free_func == spi_buffer_free)
				h_stats_g.spi_mem_stats.tx_freed++;
			else
				h_stats_g.others.tx_others_freed++;
#endif
		}
	}
	if ((gpio_handshake != H_HS_VAL_ACTIVE) || schedule_dummy_tx || schedule_dummy_rx)
		g_h.funcs->_h_post_semaphore(spi_trans_ready_sem);

	g_h.funcs->_h_unlock_mutex(spi_bus_lock);

	return ret;
}



/**
  * @brief  Send to slave via SPI
  * @param  iface_type -type of interface
  *         iface_num - interface number
  *         wbuffer - tx buffer
  *         wlen - size of wbuffer
  * @retval sendbuf - Tx buffer
  */
int esp_hosted_tx(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen, uint8_t buff_zcopy, void (*free_wbuf_fun)(void* ptr))
{
	interface_buffer_handle_t buf_handle = {0};
	void (*free_func)(void* ptr) = NULL;
	uint8_t transport_up = is_transport_tx_ready();
	uint8_t pkt_prio = PRIO_Q_OTHERS;

	if (free_wbuf_fun)
		free_func = free_wbuf_fun;

	if (!wbuffer || !wlen || (wlen > MAX_PAYLOAD_SIZE) || !transport_up) {
		ESP_LOGE(TAG, "write fail: trans_ready[%u] buff(%p) 0? OR (0<len(%u)<=max_poss_len(%u))?",
				transport_up, wbuffer, wlen, MAX_PAYLOAD_SIZE);
		H_FREE_PTR_WITH_FUNC(free_func, wbuffer);
		return STM_FAIL;
	}
	//g_h.funcs->_h_memset(&buf_handle, 0, sizeof(buf_handle));
	buf_handle.payload_zcopy = buff_zcopy;
	buf_handle.if_type = iface_type;
	buf_handle.if_num = iface_num;
	buf_handle.payload_len = wlen;
	buf_handle.payload = wbuffer;
	buf_handle.priv_buffer_handle = wbuffer;
	buf_handle.free_buf_handle = free_func;

	ESP_LOGV(TAG, "ifype: %u wbuff:%p, free: %p wlen:%u ", iface_type, wbuffer, free_func, wlen);

	if (buf_handle.if_type == ESP_SERIAL_IF)
		pkt_prio = PRIO_Q_SERIAL;
	else if (buf_handle.if_type == ESP_HCI_IF)
		pkt_prio = PRIO_Q_BT;
	/* else OTHERS by default */

	g_h.funcs->_h_queue_item(to_slave_queue[pkt_prio], &buf_handle, portMAX_DELAY);
	g_h.funcs->_h_post_semaphore(sem_to_slave_queue);

#if ESP_PKT_STATS
	if (buf_handle.if_type == ESP_STA_IF)
		pkt_stats.sta_tx_in_pass++;
#endif

#if DEBUG_HOST_TX_SEMAPHORE
	if (H_DEBUG_GPIO_PIN_Host_Tx_Pin != -1)
		g_h.funcs->_h_write_gpio(H_DEBUG_GPIO_PIN_Host_Tx_Port, H_DEBUG_GPIO_PIN_Host_Tx_Pin, H_GPIO_HIGH);
#endif
	g_h.funcs->_h_post_semaphore(spi_trans_ready_sem);

	return STM_OK;
}


/** Local Functions **/


/**
  * @brief  Task for SPI transaction
  * @param  argument: Not used
  * @retval None
  */
static void spi_transaction_task(void const* pvParameters)
{

	ESP_LOGD(TAG, "Staring SPI task");
#if DEBUG_HOST_TX_SEMAPHORE
	if (H_DEBUG_GPIO_PIN_Host_Tx_Pin != -1)
		g_h.funcs->_h_config_gpio(H_DEBUG_GPIO_PIN_Host_Tx_Port, H_DEBUG_GPIO_PIN_Host_Tx_Pin, H_GPIO_MODE_DEF_OUTPUT);
#endif

	g_h.funcs->_h_config_gpio_as_interrupt(H_GPIO_HANDSHAKE_Port, H_GPIO_HANDSHAKE_Pin,
			H_HS_INTR_EDGE, gpio_hs_isr_handler);

	g_h.funcs->_h_config_gpio_as_interrupt(H_GPIO_DATA_READY_Port, H_GPIO_DATA_READY_Pin,
			H_DR_INTR_EDGE, gpio_dr_isr_handler);

#if !H_HANDSHAKE_ACTIVE_HIGH
	ESP_LOGI(TAG, "Handshake: Active Low");
#endif
#if !H_DATAREADY_ACTIVE_HIGH
	ESP_LOGI(TAG, "DataReady: Active Low");
#endif

	ESP_LOGD(TAG, "SPI GPIOs configured");

	create_debugging_tasks();

	for (;;) {

		if ((!is_transport_rx_ready()) ||
			(!spi_trans_ready_sem)) {
			g_h.funcs->_h_msleep(300);
			continue;
		}

		/* Do SPI transactions unless first event is received.
		 * Then onward only do transactions if ESP sends interrupt
		 * on Either Data ready and Handshake pin
		 */

		if (!g_h.funcs->_h_get_semaphore(spi_trans_ready_sem, portMAX_DELAY)) {
#if DEBUG_HOST_TX_SEMAPHORE
			if (H_DEBUG_GPIO_PIN_Host_Tx_Pin != -1)
				g_h.funcs->_h_write_gpio(H_DEBUG_GPIO_PIN_Host_Tx_Port, H_DEBUG_GPIO_PIN_Host_Tx_Pin, H_GPIO_LOW);
#endif

			check_and_execute_spi_transaction();
		}
	}
}

/**
  * @brief  RX processing task
  * @param  argument: Not used
  * @retval None
  */
static void spi_process_rx_task(void const* pvParameters)
{
	interface_buffer_handle_t buf_handle_l = {0};
	interface_buffer_handle_t *buf_handle = NULL;
	int ret = 0;

	while (1) {

		g_h.funcs->_h_get_semaphore(sem_from_slave_queue, portMAX_DELAY);

		if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_SERIAL], &buf_handle_l, 0))
			if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_BT], &buf_handle_l, 0))
				if (g_h.funcs->_h_dequeue_item(from_slave_queue[PRIO_Q_OTHERS], &buf_handle_l, 0)) {
					ESP_LOGI(TAG, "No element in any queue found");
					continue;
				}

		buf_handle = &buf_handle_l;

		struct esp_priv_event *event = NULL;

		/* process received buffer for all possible interface types */
		if (buf_handle->if_type == ESP_SERIAL_IF) {

			/* serial interface path */
			serial_rx_handler(buf_handle);

		} else if((buf_handle->if_type == ESP_STA_IF) ||
				(buf_handle->if_type == ESP_AP_IF)) {
			schedule_dummy_rx = 1;
#if 1
			if (chan_arr[buf_handle->if_type] && chan_arr[buf_handle->if_type]->rx) {
				/* TODO : Need to abstract heap_caps_malloc */
				uint8_t * copy_payload = (uint8_t *)g_h.funcs->_h_malloc(buf_handle->payload_len);
				assert(copy_payload);
				memcpy(copy_payload, buf_handle->payload, buf_handle->payload_len);
				H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle, buf_handle->priv_buffer_handle);

				ret = chan_arr[buf_handle->if_type]->rx(chan_arr[buf_handle->if_type]->api_chan,
						copy_payload, copy_payload, buf_handle->payload_len);
				if (unlikely(ret))
					HOSTED_FREE(copy_payload);

				H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle, buf_handle->priv_buffer_handle);
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

			event = (struct esp_priv_event *) (buf_handle->payload);
			if (event->event_type != ESP_PRIV_EVENT_INIT) {
				/* User can re-use this type of transaction */
			}
		} else if (buf_handle->if_type == ESP_HCI_IF) {
			hci_rx_handler(buf_handle);
		} else if (buf_handle->if_type == ESP_TEST_IF) {
#if TEST_RAW_TP
			update_test_raw_tp_rx_len(buf_handle->payload_len+H_ESP_PAYLOAD_HEADER_OFFSET);
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
			H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle,buf_handle->priv_buffer_handle);
#if H_MEM_STATS
			if (buf_handle->free_buf_handle && buf_handle->priv_buffer_handle) {
				if (spi_buffer_free == buf_handle->free_buf_handle)
					h_stats_g.spi_mem_stats.rx_freed++;
				else
					h_stats_g.others.tx_others_freed++;
			}
#endif
		}
	}
}


/**
  * @brief  Next TX buffer in SPI transaction
  * @param  argument: Not used
  * @retval sendbuf - Tx buffer
  */
static uint8_t * get_next_tx_buffer(uint8_t *is_valid_tx_buf, void (**free_func)(void* ptr))
{
	struct  esp_payload_header *payload_header;
	uint8_t *sendbuf = NULL;
	uint8_t *payload = NULL;
	uint16_t len = 0;
	interface_buffer_handle_t buf_handle = {0};
	uint8_t tx_needed = 1;

	assert(is_valid_tx_buf);
	assert(free_func);

	*is_valid_tx_buf = 0;

	/* Check if higher layers have anything to transmit, non blocking.
	 * If nothing is expected to send, queue receive will fail.
	 * In that case only payload header with zero payload
	 * length would be transmitted.
	 */

	if (!g_h.funcs->_h_get_semaphore(sem_to_slave_queue, 0)) {

		/* Tx msg is present as per sem */
		if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_SERIAL], &buf_handle, 0))
			if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_BT], &buf_handle, 0))
				if (g_h.funcs->_h_dequeue_item(to_slave_queue[PRIO_Q_OTHERS], &buf_handle, 0)) {
					tx_needed = 0; /* No Tx msg */
				}

		if (tx_needed)
			len = buf_handle.payload_len;
	}

	if (len) {

		ESP_HEXLOGD("h_spi_tx", buf_handle.payload, len, 16);

		if (!buf_handle.payload_zcopy) {
			sendbuf = spi_buffer_alloc(MEMSET_REQUIRED);
			assert(sendbuf);
#if H_MEM_STATS
			h_stats_g.spi_mem_stats.tx_alloc++;
#endif
			*free_func = spi_buffer_free;
		} else {
			sendbuf = buf_handle.payload;
			*free_func = buf_handle.free_buf_handle;
		}

		if (!sendbuf) {
			ESP_LOGE(TAG, "spi buff malloc failed");
			*free_func = NULL;
			goto done;
		}

		//g_h.funcs->_h_memset(sendbuf, 0, MAX_SPI_BUFFER_SIZE);

		*is_valid_tx_buf = 1;

		/* Form Tx header */
		payload_header = (struct esp_payload_header *) sendbuf;
		payload = sendbuf + sizeof(struct esp_payload_header);
		payload_header->len     = htole16(len);
		payload_header->offset  = htole16(sizeof(struct esp_payload_header));
		payload_header->if_type = buf_handle.if_type;
		payload_header->if_num  = buf_handle.if_num;

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
			g_h.funcs->_h_memcpy(payload, buf_handle.payload, min(len, MAX_PAYLOAD_SIZE));

		//TODO: checksum should be configurable from menuconfig
		payload_header->checksum = htole16(compute_checksum(sendbuf,
				sizeof(struct esp_payload_header)+len));
	}

done:
	if (len && !buf_handle.payload_zcopy) {
		/* free allocated buffer, only if zerocopy is not requested */
		H_FREE_PTR_WITH_FUNC(buf_handle.free_buf_handle, buf_handle.priv_buffer_handle);
#if H_MEM_STATS
		if (buf_handle.free_buf_handle &&
			buf_handle.priv_buffer_handle &&
			((buf_handle.if_type == ESP_STA_IF) || (buf_handle.if_type == ESP_AP_IF))) {
			h_stats_g.nw_mem_stats.tx_freed++;
		}
#endif

	}

	return sendbuf;
}
