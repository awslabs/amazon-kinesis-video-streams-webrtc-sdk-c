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

/** prevent recursive inclusion **/
#ifndef __COMMON_H
#define __COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/** Includes **/
#include "stdint.h"
#include "stdio.h"
#include "os_wrapper.h"
#include "esp_err.h"


/** Constants/Macros **/
#define MAX_NETWORK_INTERFACES            2
#define STA_INTERFACE                     "ESP_STATION"
#define SOFTAP_INTERFACE                  "ESP_SOFTAP"

#define UNUSED_VAR(x)                     (void)(x);

#define MAX_SPI_BUFFER_SIZE               1600
/* TODO: SDIO buffers to be set same at both, ESP and host side */
#define MAX_SDIO_BUFFER_SIZE              1536

#define MAX_SUPPORTED_SDIO_CLOCK_MHZ      40

#define htole16(x)                        ((uint16_t)(x))
#define le16toh(x)                        ((uint16_t)(x))

#define IP_ADDR_LEN                       4
#define MAC_LEN                           6
#define MIN_MAC_STRING_LEN                17

#ifndef BIT
#define BIT(x)                            (1UL << (x))
#endif

#define FREQ_IN_MHZ(x)                    ((x)*1000000)

#define MHZ_TO_HZ(x) (1000000*(x))

#define SUCCESS 0
#define FAILURE -1

typedef enum stm_ret_s {
	STM_OK                =  0,
	STM_FAIL              = -1,
	STM_FAIL_TIMEOUT      = -2,
	STM_FAIL_INVALID_ARG  = -3,
	STM_FAIL_NO_MEMORY    = -4,
	STM_FAIL_NOT_FOUND    = -5,
	STM_FAIL_NOT_FINISHED = -6,
	STM_FAIL_ALIGNMENT    = -7
}stm_ret_t;

typedef enum {
	TRANSPORT_INACTIVE,
	TRANSPORT_RX_ACTIVE,
	TRANSPORT_TX_ACTIVE,
} transport_drv_events_e;

/** Exported Structures **/
/* interface header */
typedef struct {
	union {
		void *priv_buffer_handle;
	};
	uint8_t if_type;
	uint8_t if_num;
	uint8_t *payload;
	uint8_t flag;
	uint16_t payload_len;
	uint16_t seq_num;
	/* no need of memcpy at different layers */
	uint8_t payload_zcopy;

	void (*free_buf_handle)(void *buf_handle);
} interface_buffer_handle_t;

/** Exported variables **/

/** Exported Functions **/
uint16_t hton_short (uint16_t x);
uint32_t hton_long (uint32_t x);

#define ntoh_long hton_long
#define ntoh_short hton_short

typedef unsigned char   u_char;
typedef unsigned long   u_long;

int min(int x, int y);
#if 0
void hard_delay(int x);
int get_num_from_string(int *val, char *arg);
#endif

#define H_FREE_PTR_WITH_FUNC(FreeFunc, FreePtr) do { \
	if (FreeFunc && FreePtr) {             \
		FreeFunc(FreePtr);                 \
		FreePtr = NULL;                    \
	}                                      \
} while (0);

#ifdef __cplusplus
}
#endif

#endif

