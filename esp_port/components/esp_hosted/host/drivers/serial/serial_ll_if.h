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

/*prevent recursive inclusion */
#ifndef __SERIAL_LL_IF_H
#define __SERIAL_LL_IF_H

#ifdef __cplusplus
extern "C" {
#endif

/** includes **/
#include "transport_drv.h"
#include "os_wrapper.h"

struct serial_ll_operations;

/* serial interface handle */
typedef struct serial_handle_s {
	QueueHandle_t queue;
	uint8_t if_type;
	uint8_t if_num;
	struct serial_ll_operations *fops;
	uint8_t state;
	void (*serial_rx_callback) (void);
} serial_ll_handle_t;

/* serial interface */
struct serial_ll_operations {
	/**
	 * @brief Open new Serial interface
	 * @param  serial_ll_hdl - handle of serial interface
	 * @retval 0 if success, -1 on failure
	 */
	int        (*open)  (serial_ll_handle_t *serial_ll_hdl);


	/**
	 * @brief  Serial interface read non blocking
	 *         This is non blocking receive
	 *         In case higher layer using serial interface needs to make
	 *         blocking read, it should register serial_rx_callback through
	 *         serial_ll_init.
	 *
	 *         serial_rx_callback is notification mechanism to implementer of
	 *         serial interface. Higher layer would understand there is data
	 *         is ready through this notification. Then higer layer should do
	 *         serial_read API to receive actual data.
	 *
	 *         As an another design option, serial_rx_callback can also be
	 *         thought of incoming data indication, i.e. asynchronous rx
	 *         indication, which can be used by higher layer in seperate
	 *         dedicated rx task to receive and process rx data.
	 *
	 * @param  serial_ll_hdl - handle
	 *         rlen - output param, number of bytes read
	 *
	 * @retval rbuffer - ready buffer read on serial inerface
	 */
	uint8_t *  (*read)  (const serial_ll_handle_t * serial_ll_hdl,
		uint16_t * rlen);


	/**
	 * @brief Serial interface write
	 * @param  serial_ll_hdl - handle
	 *         wlen - number of bytes to write
	 *         wbuffer - buffer to send
	 * @retval STM_FAIL/STM_OK
	 */
	int        (*write) (const serial_ll_handle_t * serial_ll_hdl,
		uint8_t * wbuffer, const uint16_t wlen);


	/**
	 * @brief close - Close serial interface
	 * @param  serial_ll_hdl - handle
	 * @retval rbuffer - ready buffer read on serial inerface
	 */
	int        (*close) (serial_ll_handle_t * serial_ll_hdl);
};

/**
  * @brief serial_ll_init - create and return new serial interface
  * @param  serial_rx_callback - callback to be invoked on rx data
  * @retval serial_ll_hdl - output handle of serial interface
  */
serial_ll_handle_t * serial_ll_init(void(*rx_data_ind)(void));

stm_ret_t serial_ll_rx_handler(interface_buffer_handle_t * buf_handle);
#ifdef __cplusplus
}
#endif

#endif /* __SERIAL_LL_IF_H */
