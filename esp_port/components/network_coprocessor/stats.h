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

#ifndef __STATS__H__
#define __STATS__H__

#include <stdint.h>
#include "adapter.h"
#include "endian.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define SEC_TO_MSEC(x)                 (x*1000)
#define MSEC_TO_USEC(x)                (x*1000)
#define SEC_TO_USEC(x)                 (x*1000*1000)


/* Stats CONFIG:
 *
 * 1. CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
 *    These are debug stats to show the CPU utilization by all tasks
 *    This is set through sdkconfig
 *
 * 2. TEST_RAW_TP
 *    These are debug stats which show the raw throughput
 *    performance of transport like SPI or SDIO
 *    When this enabled, it will measure throughput will be measured from ESP to Host
 *    and Host to ESP throughput using raw packets.
 *    The intention is to check the maximum transport capacity.
 *
 *    These tests do not replace iperf stats as iperf operates in network layer.
 *    To tune the packet size, use TEST_RAW_TP__BUF_SIZE
 */
#define TEST_RAW_TP                    CONFIG_ESP_RAW_THROUGHPUT_TRANSPORT

#ifdef CONFIG_ESP_PKT_STATS
#define ESP_PKT_STATS 1
#endif

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
  /* Stats to show task wise CPU utilization */
  #define STATS_TICKS                  pdMS_TO_TICKS(10 * 1000)
  #define ARRAY_SIZE_OFFSET            5

void debug_runtime_stats_task(void* pvParameters);
#endif

/* TEST_RAW_TP is disabled on production.
 * This is only to test the throughout over transport
 * like SPI or SDIO. In this testing, dummy task will
 * push the packets over transport.
 * Currently this testing is possible on one direction
 * at a time
 */

#if TEST_RAW_TP || ESP_PKT_STATS
#include "esp_timer.h"
#include "interface.h"

typedef struct {
	esp_timer_handle_t timer;
	size_t cur_interval;
	int64_t t_start;
	SemaphoreHandle_t done;
} test_args_t;
#endif

#if TEST_RAW_TP

/* Raw throughput is supported only one direction
 * at a time
 * i.e. ESP to Host OR
 * Host to ESP
 */

/* You can optimize this value to understand the behaviour for smaller packet size
 * Intention of Raw throughout test is to assess the transport stability.
 *
 * If you want to compare with iperf performance with raw throughut, we suggest
 * to change TEST_RAW_TP__BUF_SIZE as:
 *
 * UDP : Max unfragmented packet size: 1472.
 * H_ESP_PAYLOAD_HEADER_OFFSET is not included into the calulations.
 *
 * TCP: Assess MSS and decide similar to above
 */
#define TEST_RAW_TP__BUF_SIZE        CONFIG_ESP_RAW_TP_ESP_TO_HOST_PKT_LEN
#define TEST_RAW_TP__TIMEOUT         CONFIG_ESP_RAW_TP_REPORT_INTERVAL

void debug_update_raw_tp_rx_count(uint16_t len);
#endif

#if ESP_PKT_STATS
struct pkt_stats_t {
	uint32_t sta_sh_in;
	uint32_t sta_sh_out;
	uint32_t hs_bus_sta_in;
	uint32_t hs_bus_sta_out;
	uint32_t hs_bus_sta_fail;
	uint32_t serial_rx;
	uint32_t serial_tx_total;
	uint32_t serial_tx_evt;
	uint32_t sta_flowctrl_on;
	uint32_t sta_flowctrl_off;
	uint32_t sta_lwip_in;
	uint32_t sta_slave_lwip_out;
	uint32_t sta_host_lwip_out;
	uint32_t sta_both_lwip_out;
	uint16_t tx_pkt_num;
	uint16_t exp_rx_pkt_num;
};

extern struct pkt_stats_t pkt_stats;

#define UPDATE_HEADER_TX_PKT_NO(h) h->pkt_num = htole16(pkt_stats.tx_pkt_num++)
#define UPDATE_HEADER_RX_PKT_NO(h)                                              \
	do {                                                                        \
		uint16_t rcvd_pkt_num = le16toh(header->pkt_num);                       \
		if (pkt_stats.exp_rx_pkt_num != rcvd_pkt_num) {                         \
			ESP_LOGI(TAG, "exp_pkt_num[%u], rx_pkt_num[%u]",                    \
					pkt_stats.exp_rx_pkt_num, rcvd_pkt_num);                    \
			pkt_stats.exp_rx_pkt_num = rcvd_pkt_num;                            \
		}                                                                       \
		pkt_stats.exp_rx_pkt_num++;                                             \
	} while(0);
#else
#define UPDATE_HEADER_TX_PKT_NO(h)
#define UPDATE_HEADER_RX_PKT_NO(h)
#endif


void process_test_capabilities(uint8_t capabilities);
void create_debugging_tasks(void);
uint8_t debug_get_raw_tp_conf(void);
#endif  /*__STATS__H__*/
