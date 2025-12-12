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

#include <stdio.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <protocomm.h>
#include <protocomm_priv.h>
#include "protocomm_pserial.h"
#include "adapter.h"

static const char TAG[] = "protocomm_pserial";

#define EPNAME_MAX                   16
#if defined(CONFIG_IDF_TARGET_ESP32C2)
  #define REQ_Q_MAX                  3
#else
  #define REQ_Q_MAX                  10
#endif

#define SIZE_OF_TYPE                  1
#define SIZE_OF_LENGTH                2

#define PROTO_PSER_TLV_T_EPNAME       1
#define PROTO_PSER_TLV_T_DATA         2
#define PSERIAL_TASK_STACK_SIZE       (6 * 1024)

struct pserial_config {
	pserial_xmit    xmit;
	pserial_recv    recv;
	QUEUE_HANDLE    req_queue;
};

typedef struct {
	int len;
	uint8_t *data;
	int msg_id;
} serial_arg_t;

static esp_err_t parse_tlv(uint8_t **buf, size_t *total_len,
		int *type, size_t *len, uint8_t **ptr)
{
	uint8_t *b = *buf;
	uint16_t *out_len = NULL;

	if (*total_len == 0) {
		return ESP_FAIL;
	}

	*type = b[0];
	out_len = (uint16_t *)(b + 1);
	*len = *out_len;
	*ptr = (uint8_t *) (b + 3);
	/*printf("*len %d \n", *len); */
	*total_len -= (*len + 1 + 2);
	*buf = b + 1 + 2 + (*len);
	return ESP_OK;
}

static esp_err_t compose_tlv(char *epname, uint8_t **out, size_t *outlen)
{
	uint16_t len = 0;
	uint16_t ep_len = strlen(epname);
	/*
	 * TLV (Type - Length - Value) structure is as follows:
	 * --------------------------------------------------------------------------------------------
	 *  Endpoint Type | Endpoint Length | Endpoint Value  | Data Type | Data Length | Data Value  |
	 * --------------------------------------------------------------------------------------------
	 *
	 *  Bytes used per field as follows:
	 * --------------------------------------------------------------------------------------------
	 *       1        |        2        | Endpoint length |     1     |      2      | Data length |
	 * --------------------------------------------------------------------------------------------
	 */
	uint32_t buf_len = SIZE_OF_TYPE + SIZE_OF_LENGTH +
		ep_len + SIZE_OF_TYPE + SIZE_OF_LENGTH + *outlen;
	uint8_t *buf = (uint8_t *)calloc(1, buf_len);
	if (buf == NULL) {
		ESP_LOGE(TAG,"%s Mem Alloc Failed [%d]bytes", __func__, (int) buf_len);
		return ESP_FAIL;
	}
	buf[len] = PROTO_PSER_TLV_T_EPNAME;
	len++;
	buf[len] = (ep_len & 0xFF);
	len++;
	buf[len] = ((ep_len >> 8) & 0xFF);
	len++;
	memcpy(&buf[len], epname, strlen(epname));
	len = len + strlen(epname) ;
	buf[len] = PROTO_PSER_TLV_T_DATA;
	len++;
	buf[len] = (*outlen & 0xFF);
	len++;
	buf[len] = ((*outlen >> 8) & 0xFF);
	len++;
	buf_len = len + *outlen;
	memcpy(&buf[len], (*out), *outlen);
	free(*out);
	*out = buf;
	*outlen = buf_len;
	return ESP_OK;
}

static esp_err_t rpc_req_handler(protocomm_t *pc,
		uint8_t *in, size_t in_len)
{
	uint8_t *buf = in;
	size_t total_len = 0, len = 0;
	int type = 0, ret = 0;
	uint8_t *ptr = NULL;

	char epname[EPNAME_MAX] = {0};
	uint8_t *data = NULL;
	size_t data_len = 0;

	uint8_t *out = NULL;
	size_t outlen = 0;
	struct pserial_config *pserial_cfg = NULL;

	total_len = in_len;

	while (parse_tlv(&buf, &total_len, &type, &len, &ptr) == 0) {
		/*ESP_LOGI(TAG, "Parsed type %d len %d", type, len); */
		switch(type) {
			case PROTO_PSER_TLV_T_EPNAME:
				if (len >= EPNAME_MAX - 1) {
					ESP_LOGE(TAG, "EP Name bigger than supported");
					return ESP_FAIL;
				}
				memcpy(epname, ptr, len);
				/*ESP_LOGI(TAG, "Found ep %s", epname); */
				break;
			case PROTO_PSER_TLV_T_DATA:
				data = ptr;
				data_len = len;
				break;
			default:
				ESP_LOGE(TAG, "Invalid type found in the packet");
				return ESP_FAIL;
		}
	}

	if (data == NULL || data_len == 0 || strlen(epname) == 0) {
		ESP_LOGE(TAG, "TLV components not complete for parsing");
		return ESP_FAIL;
	}
	ret = protocomm_req_handle(pc, epname, 0, data,
			data_len, &out, (ssize_t *) &outlen);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Error in handling protocomm request %d", ret);
		return ESP_FAIL;
	}

	pserial_cfg = pc->priv;
	ret = compose_tlv(RPC_EP_NAME_RSP, &out, &outlen);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to compose tlv");
		return ESP_FAIL;
	}

	/*ESP_LOG_BUFFER_HEXDUMP("serial_tx", out, outlen<16?outlen:16, ESP_LOG_INFO); */
	ret = (pserial_cfg->xmit)(out, (ssize_t) outlen);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to transmit data");
		return ESP_FAIL;
	}
	return ESP_OK;
}

static esp_err_t rpc_evt_handler(protocomm_t *pc,
		uint8_t *in, size_t in_len, int msg_id)
{
	int ret = 0;

	char epname[EPNAME_MAX] = RPC_EP_NAME_EVT;

	uint8_t *out = NULL;
	size_t outlen = 0;
	struct pserial_config *pserial_cfg = NULL;

	ret = protocomm_req_handle(pc, epname, msg_id,
			in, in_len, &out, (ssize_t *) &outlen);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Error in handling protocomm request %d", ret);
		return ESP_FAIL;
	}

	pserial_cfg = pc->priv;
	ret = compose_tlv(RPC_EP_NAME_EVT, &out, &outlen);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to compose tlv");
		return ESP_FAIL;
	}

	ret = (pserial_cfg->xmit)(out, (ssize_t) outlen);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to transmit data");
		return ESP_FAIL;
	}
	return ESP_OK;
}

esp_err_t protocomm_pserial_data_ready(protocomm_t *pc,
		uint8_t *in, int len, int msg_id)
{
	struct pserial_config *pserial_cfg = NULL;
	serial_arg_t arg = {0};
	uint8_t *buf = NULL;

	pserial_cfg = (struct pserial_config *) pc->priv;
	if (!pserial_cfg) {
		ESP_LOGE(TAG, "Unexpected. No pserial_cfg found");
		return ESP_FAIL;
	}

	if (len) {
		buf = (uint8_t *)malloc(len);
		if (buf == NULL) {
			ESP_LOGE(TAG,"%s Failed to allocate memory", __func__);
			return ESP_FAIL;
		}
		memcpy(buf, in, len);
	}
	arg.msg_id = msg_id;
	arg.len = len;
	arg.data = buf;

	if (xQueueSend(pserial_cfg->req_queue, &arg, portMAX_DELAY) != pdTRUE) {
		ESP_LOGE(TAG, "Failed to indicate data ready");
		if (buf)
			free(buf);
		return ESP_FAIL;
	}

	return ESP_OK;
}

static esp_err_t rpc_add_ep(const char *ep_name,
		protocomm_req_handler_t req_handler, void *priv_data)
{
	ESP_LOGD(TAG, "Adding endpoint for rpc");
	return ESP_OK;
}

static esp_err_t rpc_remove_ep(const char *ep_name)
{
	ESP_LOGD(TAG, "Removing endpoint for rpc");
	return ESP_OK;
}

static void pserial_task(void *params)
{
	protocomm_t *pc = (protocomm_t *) params;
	struct pserial_config *pserial_cfg = NULL;
	int len = 0, ret = 0;
	serial_arg_t arg = {0};

	pserial_cfg = (struct pserial_config *) pc->priv;
	if (!pserial_cfg) {
		ESP_LOGE(TAG, "Unexpected. No pserial_cfg found");
		return;
	}

	while (xQueueReceive(pserial_cfg->req_queue, &arg, portMAX_DELAY) == pdTRUE) {
		if ((arg.msg_id > RPC_ID__Event_Base) &&
				(arg.msg_id < RPC_ID__Event_Max)) {
			/* Events */
			ret = rpc_evt_handler(pc, arg.data, arg.len, arg.msg_id);
			if (ret)
				ESP_LOGI(TAG, "protobuf rpc event handling failed %d\n", ret);
		} else {
			/* Request */
			len = pserial_cfg->recv(arg.data, arg.len);
			if (len) {
				/*ESP_LOG_BUFFER_HEXDUMP("serial_rx", arg.data, len<16?len:16, ESP_LOG_INFO);*/
				ret = rpc_req_handler(pc, arg.data, len);
				if (ret)
					ESP_LOGI(TAG, "protocom rpc req handling failed %d\n", ret);
			}
		}
		if (arg.data)
			free(arg.data);
	}

	ESP_LOGI(TAG, "Unexpected termination of pserial task");
}

esp_err_t protocomm_pserial_start(protocomm_t *pc,
		pserial_xmit xmit, pserial_recv recv)
{
	struct pserial_config *pserial_cfg = NULL;

	if (pc == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	pc->add_endpoint = rpc_add_ep;
	pc->remove_endpoint = rpc_remove_ep;

	pserial_cfg = (struct pserial_config *) malloc(sizeof(struct pserial_config));
	if (pserial_cfg == NULL) {
		ESP_LOGE(TAG,"%s Failed to allocate memory", __func__);
		return ESP_ERR_NO_MEM;
	}
	pserial_cfg->xmit = xmit;
	pserial_cfg->recv = recv;
	pserial_cfg->req_queue = xQueueCreate(REQ_Q_MAX, sizeof(serial_arg_t));

	pc->priv = pserial_cfg;

	xTaskCreate(pserial_task, "pserial_task", PSERIAL_TASK_STACK_SIZE,
			(void *) pc, CONFIG_ESP_DEFAULT_TASK_PRIO, NULL);

	return ESP_OK;
}

esp_err_t protocomm_pserial_stop(protocomm_t *pc)
{
	struct pserial_config *pserial_cfg = NULL;
	if (pc->priv) {
		pserial_cfg = (struct pserial_config *) pc->priv;
		vQueueDelete(pserial_cfg->req_queue);
		free(pserial_cfg);
		pc->priv = NULL;
	}

	return ESP_OK;
}
