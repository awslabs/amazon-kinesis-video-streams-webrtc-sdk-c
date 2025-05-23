// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0-only OR Apache-2.0 */

/** Includes **/
#include <string.h>
#include "os_wrapper.h"
#include "serial_if.h"
#include "serial_drv.h"
#include "esp_log.h"

DEFINE_LOG_TAG(serial_if);

/** Constants/Macros **/
#define SUCCESS                           0
#define FAILURE                          -1


#define PROTO_PSER_TLV_T_EPNAME           0x01
#define PROTO_PSER_TLV_T_DATA             0x02

#if 0
#ifdef MCU_SYS
#define command_log(format, ...)          printf(format "\r", ##__VA_ARGS__);
#else
#define command_log(...)                  printf("%s:%u ",__func__,__LINE__); \
                                          printf(__VA_ARGS__);
#endif
#endif

#if 0
#define HOSTED_CALLOC(buff,nbytes) do {                           \
    buff = (uint8_t *)g_h.funcs->_h_calloc(1, nbytes);       \
    if (!buff) {                                                  \
        printf("%s, Failed to allocate memory \n", __func__);     \
        goto free_bufs;                                           \
    }                                                             \
} while(0);
#endif

/** Exported variables **/
struct serial_drv_handle_t* serial_handle = NULL;

/*
 * The data written on serial driver file, `SERIAL_IF_FILE` from adapter.h
 * In TLV i.e. Type Length Value format, to transfer data between host and ESP32
 *  | type | length | value |
 * Types are 0x01 : for endpoint name
 *           0x02 : for data
 * length is respective value field's data length in 16 bits
 * value is actual data to be transferred
 */

uint16_t compose_tlv(uint8_t* buf, uint8_t* data, uint16_t data_length)
{
	char* ep_name = RPC_EP_NAME_RSP;
	uint16_t ep_length = strlen(ep_name);
	uint16_t count = 0;
	uint8_t idx;

	buf[count] = PROTO_PSER_TLV_T_EPNAME;
	count++;
	buf[count] = (ep_length & 0xFF);
	count++;
	buf[count] = ((ep_length >> 8) & 0xFF);
	count++;

	for (idx = 0; idx < ep_length; idx++) {
		buf[count] = ep_name[idx];
		count++;
	}

	buf[count]= PROTO_PSER_TLV_T_DATA;
	count++;
	buf[count] = (data_length & 0xFF);
	count++;
	buf[count] = ((data_length >> 8) & 0xFF);
	count++;
	g_h.funcs->_h_memcpy(&buf[count], data, data_length);
	count = count + data_length;
	return count;
}

uint8_t parse_tlv(uint8_t* data, uint32_t* pro_len)
{
	char* ep_name = RPC_EP_NAME_RSP;
	char* ep_name2 = RPC_EP_NAME_EVT;
	uint64_t len = 0;
	uint16_t val_len = 0;
	if (data[len] == PROTO_PSER_TLV_T_EPNAME) {
		len++;
		val_len = data[len];
		len++;
		val_len = (data[len] << 8) + val_len;
		len++;
		/* Both RPC_EP_NAME_RSP and RPC_EP_NAME_EvT
		 * are expected to have exactly same length
		 **/
		if (val_len == strlen(ep_name)) {
			if ((strncmp((char* )&data[len],ep_name,strlen(ep_name)) == 0) ||
			    (strncmp((char* )&data[len],ep_name2,strlen(ep_name2)) == 0)) {
				len = len + strlen(ep_name);
				if (data[len] == PROTO_PSER_TLV_T_DATA) {
					len++;
					val_len = data[len];
					len++;
					val_len = (data[len] << 8) + val_len;
					len++;
					*pro_len = val_len;
					return SUCCESS;
				} else {
					ESP_LOGE(TAG, "Data Type not matched, exp %d, recvd %d\n",
							PROTO_PSER_TLV_T_DATA, data[len]);
				}
			} else {
				ESP_LOGE(TAG, "Endpoint Name not matched, exp [%s] or [%s], recvd [%s]\n",
						ep_name, ep_name2, (char* )&data[len]);
			}
		} else {
			ESP_LOGE(TAG, "Endpoint length not matched, exp [For %s, %lu OR For %s, %lu], recvd %d\n",
					ep_name, (long unsigned int)(strlen(ep_name)),
					ep_name2, (long unsigned int)(strlen(ep_name2)), val_len);
		}
	} else {
		ESP_LOGE(TAG, "Endpoint type not matched, exp %d, recvd %d\n",
				PROTO_PSER_TLV_T_EPNAME, data[len]);
	}
	return FAILURE;
}

int transport_pserial_close(void)
{
	int ret = serial_drv_close(&serial_handle);
	if (ret) {
		ESP_LOGE(TAG, "Failed to close driver interface\n");
		return FAILURE;
	}
	serial_handle = NULL;
	return ret;
}

int transport_pserial_open(void)
{
	int ret = SUCCESS;
	const char* transport = SERIAL_IF_FILE;

	if (serial_handle) {
		printf("Already opened returned\n");
		return ret;
	}

	serial_handle = serial_drv_open(transport);
	if (!serial_handle) {
		printf("serial interface open failed, Is the driver loaded?\n");
		return FAILURE;
	}

	ret = rpc_platform_init();
	if (ret != SUCCESS) {
		printf("Platform init failed\n");
		transport_pserial_close();
	}

	return ret;
}


int transport_pserial_send(uint8_t* data, uint16_t data_length)
{
	char* ep_name = RPC_EP_NAME_RSP;
	int count = 0, ret = 0;
	uint16_t buf_len = 0;
	uint8_t *write_buf = NULL;

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
	buf_len = SIZE_OF_TYPE + SIZE_OF_LENGTH + strlen(ep_name) +
		SIZE_OF_TYPE + SIZE_OF_LENGTH + data_length;

	HOSTED_CALLOC(uint8_t,write_buf,buf_len,free_bufs);

	if (!serial_handle) {
		ESP_LOGE(TAG, "Serial connection closed?\n");
		goto free_bufs;
	}

	count = compose_tlv(write_buf, data, data_length);
	if (!count) {
		ESP_LOGE(TAG, "Failed to compose TX data\n");
		goto free_bufs;
	}

	ret = serial_drv_write(serial_handle, write_buf, count, &count);
	if (ret != SUCCESS) {
		ESP_LOGE(TAG, "Failed to write TX data\n");
		goto free_bufs;
	}
	return SUCCESS;
free_bufs:
	if (write_buf) {
		g_h.funcs->_h_free(write_buf);
	}

	return FAILURE;
}

uint8_t * transport_pserial_read(uint32_t *out_nbyte)
{
	/* Two step parsing TLV is moved in serial_drv_read */
	return serial_drv_read(serial_handle, out_nbyte);
}
