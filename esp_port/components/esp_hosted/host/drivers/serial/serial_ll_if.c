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

/** Includes **/
#include "string.h"
#include "serial_ll_if.h"
#include "esp_log.h"
#include "transport_drv.h"
#include "esp_hosted_log.h"
#include "adapter.h"

DEFINE_LOG_TAG(serial_ll);

/** Macros / Constants **/
#define MAX_SERIAL_INTF                   2
#define TO_SERIAL_INFT_QUEUE_SIZE         10

typedef enum {
	INIT,
	ACTIVE,
	DESTROY
} serial_ll_state_e;

static struct rx_data {
	int len;
	uint8_t *data;
} r;

/* data structures needed for serial driver */
static QueueHandle_t to_serial_ll_intf_queue[MAX_SERIAL_INTF];
static serial_ll_handle_t * interface_handle_g[MAX_SERIAL_INTF] = {NULL};
static uint8_t conn_num = 0;

/** Function Declarations **/
static int       serial_ll_open    (serial_ll_handle_t *serial_ll_hdl);
static uint8_t * serial_ll_read    (const serial_ll_handle_t * serial_ll_hdl,
		uint16_t * rlen);
static int       serial_ll_write   (const serial_ll_handle_t * serial_ll_hdl,
		uint8_t * wbuffer, const uint16_t wlen);
static int       serial_ll_close   (serial_ll_handle_t * serial_ll_hdl);


/* define serial interface */
static struct serial_ll_operations serial_ll_fops = {
	.open    = serial_ll_open,
	.read    = serial_ll_read,
	.write   = serial_ll_write,
	.close   = serial_ll_close,
};

/** function definition **/

/** Local Functions **/

/**
  * @brief Open new Serial interface
  * @param  serial_ll_hdl - handle of serial interface
  * @retval 0 if success, -1 on failure
  */
static int serial_ll_open(serial_ll_handle_t *serial_ll_hdl)
{
	if (! serial_ll_hdl) {
		ESP_LOGE(TAG, "serial invalid hdr");
		return STM_FAIL;
	}

	if (serial_ll_hdl->queue) {
		/* clean up earlier queue */
		ESP_LOGW(TAG, "Discarding earlier serial queues");
		vQueueDelete(serial_ll_hdl->queue);
	}

	/* Queue - serial rx */
	serial_ll_hdl->queue = g_h.funcs->_h_create_queue(TO_SERIAL_INFT_QUEUE_SIZE,
		sizeof(interface_buffer_handle_t));

	if (! serial_ll_hdl->queue) {
		serial_ll_close(serial_ll_hdl);
		return STM_FAIL;
	}

	serial_ll_hdl->state  = ACTIVE;
	return STM_OK;
}

/**
  * @brief Get serial handle for iface_num
  * @param  iface_num - serial connection number
  * @retval serial_ll_hdl - output handle of serial interface
  */
static serial_ll_handle_t * get_serial_ll_handle(const uint8_t iface_num)
{
	if ((iface_num < MAX_SERIAL_INTF) &&
		(interface_handle_g[iface_num]) &&
		(interface_handle_g[iface_num]->state == ACTIVE)) {

		return interface_handle_g[iface_num];
	}
	ESP_LOGE(TAG, "%s Failed to get interface handle", __func__);
	return NULL;
}

/**
  * @brief Close serial interface
  * @param  serial_ll_hdl - handle
  * @retval rbuffer - ready buffer read on serial inerface
  */
static int serial_ll_close(serial_ll_handle_t * serial_ll_hdl)
{
	serial_ll_hdl->state = DESTROY;

	if (serial_ll_hdl->queue) {
		ESP_LOGI(TAG, "Cleaning up serial queues");
		vQueueDelete(serial_ll_hdl->queue);
		serial_ll_hdl->queue = NULL;
	}

	/* reset connection */
	if (conn_num > 0) {
		interface_handle_g[--conn_num] = NULL;
	}

	if (serial_ll_hdl) {
		g_h.funcs->_h_free(serial_ll_hdl);
		serial_ll_hdl = NULL;
	}
	return STM_OK;
}


/**
  * @brief  Serial interface read non blocking
  * @param  serial_ll_hdl - handle
  *         rlen - output param, number of bytes read
  * @retval rbuffer - ready buffer read on serial inerface
  */
static uint8_t * serial_ll_read(const serial_ll_handle_t * serial_ll_hdl,
							 uint16_t * rlen)
{
	/* This is a non-blocking call */
	interface_buffer_handle_t buf_handle = {0};

	/* Initial value */
	*rlen = 0 ;

	/* check if serial interface valid */
	if ((! serial_ll_hdl) || (serial_ll_hdl->state != ACTIVE)) {
		ESP_LOGE(TAG, "serial invalid interface");
		return NULL;
	}

	/* This is **blocking** receive.
	 *
	 * Although not needed in normal circumstances,
	 * User can convert it to non blocking using below steps:
	 *
	 * To make it non blocking:
	 *   As an another design option, serial_rx_callback can also be
	 *   thought of incoming data indication, i.e. asynchronous rx
	 *   indication, which can be used by higher layer in seperate
	 *   dedicated rx task to receive and process rx data.
	 *
	 * In our example, first approach of blocking read is used.
	 */
	ESP_LOGV(TAG, "before deQ for ll_read");
	if (g_h.funcs->_h_dequeue_item(serial_ll_hdl->queue, &buf_handle, HOSTED_BLOCK_MAX)) {
		ESP_LOGE(TAG, "serial queue recv failed ");
		return NULL;
	}
	ESP_LOGV(TAG, "after deQ for ll_read");

	/* proceed only if payload and length are sane */
	if (!buf_handle.payload || !buf_handle.payload_len) {
		ESP_LOGE(TAG, "%s: Dequeue result in empty buffer",__func__);
		return NULL;
	}

	*rlen = buf_handle.payload_len;

	ESP_HEXLOGV("ll_read", buf_handle.payload, buf_handle.payload_len, 32);

	return buf_handle.payload;
}

/**
  * @brief Serial interface write
  * @param  serial_ll_hdl - handle
  *         wlen - number of bytes to write
  *         wbuffer - buffer to send
  * @retval STM_FAIL/STM_OK
  */
static int serial_ll_write(const serial_ll_handle_t * serial_ll_hdl,
	uint8_t * wbuffer, const uint16_t wlen)
{

	if ((! serial_ll_hdl) || (serial_ll_hdl->state != ACTIVE)) {
		ESP_LOGE(TAG, "serial invalid interface for write");
		return STM_FAIL;
	}

	return esp_hosted_tx(serial_ll_hdl->if_type,
		serial_ll_hdl->if_num, wbuffer, wlen, H_BUFF_NO_ZEROCOPY, H_DEFLT_FREE_FUNC);
}

/**
  * @brief Serial rx handler is called by spi driver when there
  *        is incoming data with interface type is Serial.
  * @param  if_num - interface instance
  *         rxbuff - buffer from spi driver
  *         rx_len - size of rxbuff
  *         seq_num - serial sequence number
  *         flag_more_frags - Flags for fragmentation
  * @retval 0 on success, else failure
  */
stm_ret_t serial_ll_rx_handler(interface_buffer_handle_t * buf_handle)
{

#define SERIAL_ALLOC_REALLOC_RDATA() \
	do { \
		if(!r.data) { \
			r.data = (uint8_t *)g_h.funcs->_h_malloc(buf_handle->payload_len); \
		} else { \
			r.data = (uint8_t *)g_h.funcs->_h_realloc(r.data, r.len + buf_handle->payload_len); \
		} \
		if (!r.data) { \
			ESP_LOGE(TAG, "Failed to allocate serial data"); \
			goto serial_buff_cleanup; \
		} \
	} while(0);

	serial_ll_handle_t * serial_ll_hdl = NULL;
	uint8_t *serial_buf = NULL;
	interface_buffer_handle_t new_buf_handle = {0};

	/* Check valid handle and length */
	if (!buf_handle || !buf_handle->payload_len) {
		ESP_LOGE(TAG, "%s:%u Invalid parameters", __func__, __LINE__);
		goto serial_buff_cleanup;
	}

	serial_ll_hdl = get_serial_ll_handle(buf_handle->if_num);

	/* Is serial interface up */
	if ((! serial_ll_hdl) || (serial_ll_hdl->state != ACTIVE)) {
		ESP_LOGE(TAG, "Serial interface not registered yet");
		goto serial_buff_cleanup;
	}


	/* Accumulate fragments */
	if (buf_handle->flag & MORE_FRAGMENT) {

		ESP_LOGD(TAG, "Fragment!!!");
		SERIAL_ALLOC_REALLOC_RDATA();

		g_h.funcs->_h_memcpy((r.data + r.len), buf_handle->payload, buf_handle->payload_len);
		r.len += buf_handle->payload_len;
		return STM_OK;
	}

	SERIAL_ALLOC_REALLOC_RDATA();

	/* No or last fragment */
	g_h.funcs->_h_memcpy((r.data + r.len), buf_handle->payload, buf_handle->payload_len);
	r.len += buf_handle->payload_len;

	serial_buf = (uint8_t *)g_h.funcs->_h_malloc(r.len);
	if(!serial_buf) {
		ESP_LOGE(TAG, "Malloc failed, drop pkt");
		goto serial_buff_cleanup;
	}
	g_h.funcs->_h_memcpy(serial_buf, r.data, r.len);

	/* form new buf handle for processing of serial msg */
	new_buf_handle.if_type = ESP_SERIAL_IF;
	new_buf_handle.if_num = buf_handle->if_num;
	new_buf_handle.payload_len = r.len;
	new_buf_handle.payload = serial_buf;
	new_buf_handle.priv_buffer_handle = serial_buf;
	new_buf_handle.free_buf_handle = g_h.funcs->_h_free;

	/* clear old buf handle */
	//H_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle, buf_handle->priv_buffer_handle);


	r.len = 0;
	g_h.funcs->_h_free(r.data);
	r.data = NULL;

	ESP_LOGV(TAG, "before ENQ for ll_read");
	/* send to serial queue */
	if (g_h.funcs->_h_queue_item(serial_ll_hdl->queue,
		    &new_buf_handle, HOSTED_BLOCK_MAX)) {
		ESP_LOGE(TAG, "Failed send serialif queue[%u]", new_buf_handle.if_num);
		goto serial_buff_cleanup;
	}
	ESP_LOGV(TAG, "after ENQ for ll_read");

	/* Indicate higher layer about data ready for consumption */
	if (serial_ll_hdl->serial_rx_callback) {
		(*serial_ll_hdl->serial_rx_callback) ();
	} else {
		goto serial_buff_cleanup;
	}

	return STM_OK;

serial_buff_cleanup:

	ESP_LOGE(TAG, "Err occured, discard current buffer");

	r.len = 0;

	H_FREE_PTR_WITH_FUNC(new_buf_handle.free_buf_handle, new_buf_handle.priv_buffer_handle);

	g_h.funcs->_h_free(r.data);
	return STM_FAIL;
}

/** Exported Functions **/

/**
  * @brief create and return new serial interface
  * @param  serial_rx_callback - callback to be invoked on rx data
  * @retval serial_ll_hdl - output handle of serial interface
  */
serial_ll_handle_t * serial_ll_init(void(*serial_rx_callback)(void))
{
	serial_ll_handle_t  * serial_ll_hdl = NULL;

	/* Check if more serial interfaces be created */
	if ((conn_num+1) < MAX_SERIAL_INTF) {

		serial_ll_hdl = (serial_ll_handle_t *)g_h.funcs->_h_malloc(sizeof(serial_ll_handle_t));
		if (! serial_ll_hdl) {
			ESP_LOGE(TAG, "Serial interface - malloc failed");
			return NULL;
		}

		serial_ll_hdl->if_type = ESP_SERIAL_IF;
		serial_ll_hdl->if_num  = conn_num;
		serial_ll_hdl->queue   = to_serial_ll_intf_queue[conn_num];
		serial_ll_hdl->state   = INIT;
		serial_ll_hdl->fops    = &serial_ll_fops;
		serial_ll_hdl->serial_rx_callback   = serial_rx_callback;
		interface_handle_g[conn_num] = serial_ll_hdl;
		conn_num++;

	} else {
		ESP_LOGE(TAG, "Number of serial interface connections overflow");
		return NULL;
	}

	return serial_ll_hdl;
}
