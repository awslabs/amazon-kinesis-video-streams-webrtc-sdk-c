// Copyright 2013 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0-only OR Apache-2.0 */
/*
 * Holds RPC defines common to Hosted Slave and Master
 */

#ifndef __HOSTED_RPC_COMMON__H
#define __HOSTED_RPC_COMMON__H

/*
 * check components/esp_common/src/esp_err_to_name.c for
 * unused error range
 */
#define ESP_ERR_HOSTED_BASE (0x2f00)

enum {
	RPC_ERR_BASE = ESP_ERR_HOSTED_BASE,
	RPC_ERR_NOT_CONNECTED,
	RPC_ERR_NO_AP_FOUND,
	RPC_ERR_INVALID_PASSWORD,
	RPC_ERR_INVALID_ARGUMENT,
	RPC_ERR_OUT_OF_RANGE,
	RPC_ERR_MEMORY_FAILURE,
	RPC_ERR_UNSUPPORTED_MSG,
	RPC_ERR_INCORRECT_ARG,
	RPC_ERR_PROTOBUF_ENCODE,
	RPC_ERR_PROTOBUF_DECODE,
	RPC_ERR_SET_ASYNC_CB,
	RPC_ERR_TRANSPORT_SEND,
	RPC_ERR_REQUEST_TIMEOUT,
	RPC_ERR_REQ_IN_PROG,
	RPC_ERR_SET_SYNC_SEM,
};

#endif
