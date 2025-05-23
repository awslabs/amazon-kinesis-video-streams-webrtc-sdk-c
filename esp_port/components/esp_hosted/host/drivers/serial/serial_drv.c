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

#include "serial_if.h"
#include "serial_ll_if.h"
#include "esp_log.h"
#include "esp_hosted_log.h"

DEFINE_LOG_TAG(serial);

struct serial_drv_handle_t {
	int handle; /* dummy variable */
};

static serial_ll_handle_t * serial_ll_if_g;
static void * readSemaphore;


static void rpc_rx_indication(void);

/* -------- Serial Drv ---------- */
struct serial_drv_handle_t* serial_drv_open(const char *transport)
{
	struct serial_drv_handle_t* serial_drv_handle = NULL;
	if (!transport) {
		ESP_LOGE(TAG, "Invalid parameter in open");
		return NULL;
	}

	if(serial_drv_handle) {
		ESP_LOGE(TAG, "return orig hndl\n");
		return serial_drv_handle;
	}

	serial_drv_handle = (struct serial_drv_handle_t*) g_h.funcs->_h_calloc
		(1,sizeof(struct serial_drv_handle_t));
	if (!serial_drv_handle) {
		ESP_LOGE(TAG, "Failed to allocate memory \n");
		return NULL;
	}

	return serial_drv_handle;
}

int serial_drv_write (struct serial_drv_handle_t* serial_drv_handle,
	uint8_t* buf, int in_count, int* out_count)
{
	int ret = 0;
	if (!serial_drv_handle || !buf || !in_count || !out_count) {
		ESP_LOGE(TAG,"Invalid parameters in write\n\r");
		return RET_INVALID;
	}

	if( (!serial_ll_if_g) ||
		(!serial_ll_if_g->fops) ||
		(!serial_ll_if_g->fops->write)) {
		ESP_LOGE(TAG,"serial interface not valid\n\r");
		return RET_INVALID;
	}

	ESP_HEXLOGV("serial_write", buf, in_count, 32);
	ret = serial_ll_if_g->fops->write(serial_ll_if_g, buf, in_count);
	if (ret != RET_OK) {
		*out_count = 0;
		ESP_LOGE(TAG,"Failed to write data\n\r");
		return RET_FAIL;
	}

	*out_count = in_count;
	return RET_OK;
}


uint8_t * serial_drv_read(struct serial_drv_handle_t *serial_drv_handle,
		uint32_t *out_nbyte)
{
	uint16_t init_read_len = 0;
	uint16_t rx_buf_len = 0;
	uint8_t* read_buf = NULL;
	int ret = 0;
	/* Any of `RPC_EP_NAME_EVT` and `RPC_EP_NAME_RSP` could be used,
	 * as both have same strlen in adapter.h */
	const char* ep_name = RPC_EP_NAME_RSP;
	uint8_t *buf = NULL;
	uint32_t buf_len = 0;


	if (!serial_drv_handle || !out_nbyte) {
		ESP_LOGE(TAG,"Invalid parameters in read\n\r");
		return NULL;
	}

	*out_nbyte = 0;

	if(!readSemaphore) {
		ESP_LOGE(TAG,"Semaphore not initialized\n\r");
		return NULL;
	}

	ESP_LOGV(TAG, "Wait for serial_ll_semaphore");
	g_h.funcs->_h_get_semaphore(readSemaphore, HOSTED_BLOCK_MAX);

	if( (!serial_ll_if_g) ||
		(!serial_ll_if_g->fops) ||
		(!serial_ll_if_g->fops->read)) {
		ESP_LOGE(TAG,"serial interface refusing to read\n\r");
		return NULL;
	}
	ESP_LOGV(TAG, "Starting serial_ll read");

	/* Get buffer from serial interface */
	read_buf = serial_ll_if_g->fops->read(serial_ll_if_g, &rx_buf_len);
	if ((!read_buf) || (!rx_buf_len)) {
		ESP_LOGE(TAG,"serial read failed\n\r");
		return NULL;
	}
	ESP_HEXLOGV("serial_read", read_buf, rx_buf_len, 32);

/*
 * Read Operation happens in two steps because total read length is unknown
 * at first read.
 *      1) Read fixed length of RX data
 *      2) Read variable length of RX data
 *
 * (1) Read fixed length of RX data :
 * Read fixed length of received data in below format:
 * ----------------------------------------------------------------------------
 *  Endpoint Type | Endpoint Length | Endpoint Value  | Data Type | Data Length
 * ----------------------------------------------------------------------------
 *
 *  Bytes used per field as follows:
 *  ---------------------------------------------------------------------------
 *      1         |       2         | Endpoint Length |     1     |     2     |
 *  ---------------------------------------------------------------------------
 *
 *  int_read_len = 1 + 2 + Endpoint length + 1 + 2
 */

	init_read_len = SIZE_OF_TYPE + SIZE_OF_LENGTH + strlen(ep_name) +
		SIZE_OF_TYPE + SIZE_OF_LENGTH;

	if(rx_buf_len < init_read_len) {
		HOSTED_FREE(read_buf);
		ESP_LOGE(TAG,"Incomplete serial buff, return\n");
		return NULL;
	}

	HOSTED_CALLOC(uint8_t,buf,init_read_len,free_bufs);

	g_h.funcs->_h_memcpy(buf, read_buf, init_read_len);

	/* parse_tlv function returns variable payload length
	 * of received data in buf_len
	 **/
	ret = parse_tlv(buf, &buf_len);
	if (ret || !buf_len) {
		HOSTED_FREE(buf);
		ESP_LOGE(TAG,"Failed to parse RX data \n\r");
		goto free_bufs;
	}
	ESP_LOGV(TAG, "TLV parsed");

	if (rx_buf_len < (init_read_len + buf_len)) {
		ESP_LOGE(TAG,"Buf read on serial iface is smaller than expected len\n");
		HOSTED_FREE(buf);
		goto free_bufs;
	}

	if (rx_buf_len > (init_read_len + buf_len)) {
		ESP_LOGE(TAG,"Buf read on serial iface is smaller than expected len\n");
	}

	HOSTED_FREE(buf);
/*
 * (2) Read variable length of RX data:
 */
	HOSTED_CALLOC(uint8_t,buf,buf_len,free_bufs);

	g_h.funcs->_h_memcpy((buf), read_buf+init_read_len, buf_len);

	HOSTED_FREE(read_buf);

	*out_nbyte = buf_len;
	ESP_LOGV(TAG, "sizeof real serial buf: %" PRIu32, *out_nbyte);
	return buf;

free_bufs:
	HOSTED_FREE(read_buf);
	HOSTED_FREE(buf);
	return NULL;
}

int serial_drv_close(struct serial_drv_handle_t** serial_drv_handle)
{
	if (!serial_drv_handle || !(*serial_drv_handle)) {
		ESP_LOGE(TAG,"Invalid parameter in close \n\r");
		if (serial_drv_handle)
			HOSTED_FREE(serial_drv_handle);
		return RET_INVALID;
	}
	HOSTED_FREE(*serial_drv_handle);
	return RET_OK;
}

int rpc_platform_init(void)
{
	/* rpc semaphore */
	readSemaphore = g_h.funcs->_h_create_semaphore(CONFIG_ESP_MAX_SIMULTANEOUS_SYNC_RPC_REQUESTS +
			CONFIG_ESP_MAX_SIMULTANEOUS_ASYNC_RPC_REQUESTS);
	assert(readSemaphore);

	/* grab the semaphore, so that task will be mandated to wait on semaphore */
	g_h.funcs->_h_get_semaphore(readSemaphore, 0);

	serial_ll_if_g = serial_ll_init(rpc_rx_indication);
	if (!serial_ll_if_g) {
		ESP_LOGE(TAG,"Serial interface creation failed\n\r");
		assert(serial_ll_if_g);
		return RET_FAIL;
	}
	if (RET_OK != serial_ll_if_g->fops->open(serial_ll_if_g)) {
		ESP_LOGE(TAG,"Serial interface open failed\n\r");
		return RET_FAIL;
	}
	return RET_OK;
}

/* TODO: Why this is not called in transport_pserial_close() */
int rpc_platform_deinit(void)
{
	if (RET_OK != serial_ll_if_g->fops->close(serial_ll_if_g)) {
		ESP_LOGE(TAG,"Serial interface close failed\n\r");
		return RET_FAIL;
	}
	return RET_OK;
}

static void rpc_rx_indication(void)
{
	/* heads up to rpc for read */
	if(readSemaphore) {
		g_h.funcs->_h_post_semaphore(readSemaphore);
	}
}


