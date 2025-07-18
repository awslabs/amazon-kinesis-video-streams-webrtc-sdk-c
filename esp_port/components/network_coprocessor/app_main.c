/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "sys/queue.h"
#include "soc/soc.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <unistd.h>
#ifndef CONFIG_IDF_TARGET_ARCH_RISCV
#include "xtensa/core-macros.h"
#endif
#include "esp_private/wifi.h"
#include "interface.h"
#include "esp_wpa.h"
#include "slave_main.h"
#include "driver/gpio.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "endian.h"

#include <protocomm.h>
#include "protocomm_pserial.h"
#include "slave_control.h"
#if CONFIG_NW_COPROC_BT_ENABLED
#include "slave_bt.h"
#endif
#include "stats.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mempool.h"

#include "lwip/err.h"
#include "lwip/sys.h"
//#include "lwip/ip_addr.h"
#include "lwip/etharp.h"
#include "lwip/prot/iana.h"
#include "lwip/prot/ip.h"
#include "lwip/prot/tcp.h"
#include "lwip/prot/udp.h"
#include "lwip/prot/icmp.h"

#include "host_power_save.h"
#include "esp_hosted_rpc.pb-c.h"

#include "freertos/portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifndef CONFIG_ADAPTER_ONLY_MODE
#include "esp_work_queue.h"
#include "message_utils.h"
#endif

#include "network_coprocessor.h"

static const char *TAG = "fg_mcu_slave";

#if 0
#ifndef CONFIG_HOSTED_ON_LOW_MEM
#define FLOW_CTL_ENABLED 1
#define BYPASS_TX_PRIORITY_Q 1
#endif
#endif

// #define FLOW_CTL_ENABLED 1
#define BYPASS_TX_PRIORITY_Q 1

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#define STATS_TICKS                      pdMS_TO_TICKS(10 * 1000)
#define ARRAY_SIZE_OFFSET                5
#endif

#define UNKNOWN_RPC_MSG_ID               0

#if CONFIG_ESP_SPI_HOST_INTERFACE
  #ifdef CONFIG_IDF_TARGET_ESP32S2
    #define TO_HOST_QUEUE_SIZE           5
  #else
    #define TO_HOST_QUEUE_SIZE           20
  #endif
#else
  #define TO_HOST_QUEUE_SIZE             20
#endif

#define ETH_DATA_LEN                     1500
#define HOST_RESET_TASK_STACK            (5 * 512)

/* Perform DHCP at slave & send IP info at host */
#define H_SLAVE_LWIP_DHCP_AT_SLAVE       1

volatile uint8_t datapath = 0;
volatile uint8_t station_connected = 0;
volatile uint8_t station_got_ip = 0;
volatile uint8_t softap_started = 0;

interface_context_t *if_context = NULL;
interface_handle_t *if_handle = NULL;
slave_config_t slv_cfg_g;
slave_state_t  slv_state_g;

#if !BYPASS_TX_PRIORITY_Q
static QueueHandle_t meta_to_host_queue = NULL;
static QueueHandle_t to_host_queue[MAX_PRIORITY_QUEUES] = {NULL};
#endif
SemaphoreHandle_t host_reset_sem;

// #define DEBUG_PRINT_RECEIVED_PACKETS 1

#ifdef FLOW_CTL_ENABLED
  volatile uint8_t * wifi_flow_ctrl;
  volatile uint8_t * wifi_flow_ctrl_dirty;
  static void flow_ctl_task(void* pvParameters);
  static SemaphoreHandle_t sdio_flow_ctl_sem = NULL;
  #define IS_FLOW_CTL_NEEDED() if(sdio_flow_ctl_sem) xSemaphoreGive(sdio_flow_ctl_sem);
#else
  #define IS_FLOW_CTL_NEEDED()
#endif

esp_netif_t *slave_sta_netif = NULL;

static protocomm_t *pc_pserial;

static struct rx_data {
	uint8_t valid;
	uint16_t cur_seq_no;
	int len;
	uint8_t data[4096];
} r;

uint8_t ap_mac[BSSID_BYTES_SIZE] = {0};
static void user_defined_rpc_s2h_evt(uint8_t usr_evt_num) __attribute__((unused));

static void print_firmware_version()
{
	ESP_LOGI(TAG, "*********************************************************************");
	ESP_LOGI(TAG, "                ESP-Hosted-MCU Slave FW version :: %d.%d.%d                        ",
			(int)PROJECT_VERSION_MAJOR_1, (int)PROJECT_VERSION_MAJOR_2, (int)PROJECT_VERSION_MINOR);
#if CONFIG_ESP_SPI_HOST_INTERFACE
  #if BLUETOOTH_UART
	ESP_LOGI(TAG, "                Transport used :: SPI + UART                    ");
  #else
	ESP_LOGI(TAG, "                Transport used :: SPI only                      ");
  #endif
#else
  #if BLUETOOTH_UART
	ESP_LOGI(TAG, "                Transport used :: SDIO + UART                   ");
  #else
	ESP_LOGI(TAG, "                Transport used :: SDIO only                     ");
  #endif
#endif
	ESP_LOGI(TAG, "*********************************************************************");
}

static uint8_t get_capabilities()
{
	uint8_t cap = 0;

	ESP_LOGI(TAG, "Supported features are:");
#if CONFIG_ESP_SPI_HOST_INTERFACE
	ESP_LOGI(TAG, "- WLAN over SPI");
	cap |= ESP_WLAN_SPI_SUPPORT;
#else
	ESP_LOGI(TAG, "- WLAN over SDIO");
	cap |= ESP_WLAN_SDIO_SUPPORT;
#endif

#if CONFIG_ESP_SPI_CHECKSUM || CONFIG_ESP_SDIO_CHECKSUM
	cap |= ESP_CHECKSUM_ENABLED;
#endif

#if CONFIG_NW_COPROC_BT_ENABLED
	cap |= get_bluetooth_capabilities();
#endif
	ESP_LOGI(TAG, "capabilities: 0x%x", cap);

	return cap;
}

static inline esp_err_t populate_buff_handle(interface_buffer_handle_t *buf_handle,
		uint8_t if_type,
		uint8_t *buf,
		uint16_t len,
		void (*free_buf_func)(void *data),
		void *free_buf_handle,
		uint8_t flag,
		uint8_t if_num,
		uint16_t seq_num)
{
	buf_handle->if_type = if_type;
	buf_handle->payload = buf;
	buf_handle->payload_len = len;
	buf_handle->priv_buffer_handle = free_buf_handle;
	buf_handle->free_buf_handle = free_buf_func;
	buf_handle->flag = flag;
	buf_handle->if_num = if_num;
	buf_handle->seq_num = seq_num;

	return ESP_OK;
}

#define populate_wifi_buffer_handle(Buf_hdL, TypE, BuF, LeN) \
	populate_buff_handle(Buf_hdL, TypE, BuF, LeN, esp_wifi_internal_free_rx_buffer, eb, 0, 0, 0);


esp_err_t wlan_ap_rx_callback(void *buffer, uint16_t len, void *eb)
{
	interface_buffer_handle_t buf_handle = {0};

	if (!buffer || !eb || !datapath) {
		if (eb) {
			esp_wifi_internal_free_rx_buffer(eb);
		}
		return ESP_OK;
	}
	ESP_HEXLOGV("AP_Get", buffer, len, 32);

#if 0
	/* Only enable this is you want to avoid multi and bradcast
	 * traffic to be reduced from stations to softap
	 */
	uint8_t * ap_buf = buffer;
	/* Check destination address against self address */
	if (memcmp(ap_buf, ap_mac, BSSID_BYTES_SIZE)) {
		/* Check for multicast or broadcast address */
		if (!(ap_buf[0] & 1))
			goto DONE;
	}
#endif

	populate_wifi_buffer_handle(&buf_handle, ESP_AP_IF, buffer, len);

	if (send_to_host_queue(&buf_handle, PRIO_Q_OTHERS))
		goto DONE;

	return ESP_OK;

DONE:
	esp_wifi_internal_free_rx_buffer(eb);
	return ESP_OK;
}

typedef enum {
	SLAVE_LWIP_BRIDGE,
	HOST_LWIP_BRIDGE,
	BOTH_LWIP_BRIDGE,
	INVALID_BRIDGE,
} hosted_l2_bridge;

#ifdef CONFIG_SLAVE_LWIP_ENABLED
#if defined(CONFIG_ESP_DEFAULT_LWIP_SLAVE)
  #define DEFAULT_LWIP_TO_SEND SLAVE_LWIP_BRIDGE
#elif defined(CONFIG_ESP_DEFAULT_LWIP_HOST)
  #define DEFAULT_LWIP_TO_SEND BOTH_LWIP_BRIDGE
#elif defined(CONFIG_ESP_DEFAULT_LWIP_BOTH)
  #define DEFAULT_LWIP_TO_SEND BOTH_LWIP_BRIDGE
#else
  #error "Select one of the LWIP to forward"
#endif
#endif

#ifdef CONFIG_SLAVE_LWIP_ENABLED

#if defined(CONFIG_ESP_DEFAULT_LWIP_BOTH)
  #define DHCP_LWIP_BRIDGE BOTH_LWIP_BRIDGE
#elif defined(CONFIG_ESP_DEFAULT_LWIP_HOST)
  #define DHCP_LWIP_BRIDGE HOST_LWIP_BRIDGE
#elif defined(CONFIG_ESP_DEFAULT_LWIP_SLAVE)
  #define DHCP_LWIP_BRIDGE SLAVE_LWIP_BRIDGE
#else
  #define DHCP_LWIP_BRIDGE HOST_LWIP_BRIDGE
#endif


#define CONFIG_LWIP_TCP_REMOTE_PORT_RANGE_START 49152
#define CONFIG_LWIP_TCP_REMOTE_PORT_RANGE_END 61439
#define CONFIG_LWIP_UDP_REMOTE_PORT_RANGE_START 49152
#define CONFIG_LWIP_UDP_REMOTE_PORT_RANGE_END 61439

static hosted_l2_bridge find_destination_bridge(void *frame_data, uint16_t frame_length)
{
	struct eth_hdr *ethhdr = (struct eth_hdr *)frame_data;
	struct ip_hdr *iphdr;
	u8_t proto;

	/* Check if the frame contains an IP packet */
	if (lwip_ntohs(ethhdr->type) == ETHTYPE_IP) {
		/* Get the IP header */
		iphdr = (struct ip_hdr *)((u8_t *)frame_data + SIZEOF_ETH_HDR);
		/* Get the protocol from the IP header */
		proto = IPH_PROTO(iphdr);

		if (proto == IP_PROTO_TCP) {
			struct tcp_hdr *tcphdr = (struct tcp_hdr *)((u8_t *)iphdr + IPH_HL(iphdr) * 4);
			u16_t dst_port = lwip_ntohs(tcphdr->dest);

#ifdef DEBUG_PRINT_RECEIVED_PACKETS
			{
				u16_t src_port = lwip_ntohs(tcphdr->src);
				struct ip_hdr *ip4hdr = iphdr;
				ip4_addr_t src_addr;
				src_addr.addr = ip4hdr->src.addr;
				printf("dst_port: %d, src_port: %d to %d\n", dst_port, src_port, (int) DEFAULT_LWIP_TO_SEND);
				printf("tcp src_addr: %u.%u.%u.%u\n",
					(unsigned int) (src_addr.addr) & 0xFF,
					(unsigned int) (src_addr.addr >> 8) & 0xFF,
					(unsigned int) (src_addr.addr >> 16) & 0xFF,
					(unsigned int) (src_addr.addr >> 24) & 0xFF);
			}
#endif
			if (IS_REMOTE_TCP_PORT(dst_port)) {
				return HOST_LWIP_BRIDGE;
			} else if (IS_LOCAL_TCP_PORT(dst_port)) {
				return SLAVE_LWIP_BRIDGE;
			}
		} else if (proto == IP_PROTO_UDP) {
			struct udp_hdr *udphdr = (struct udp_hdr *)((u8_t *)iphdr + IPH_HL(iphdr) * 4);
			u16_t dst_port = lwip_ntohs(udphdr->dest);
#ifdef DEBUG_PRINT_RECEIVED_PACKETS
			{
				u16_t src_port = lwip_ntohs(udphdr->src);
				struct ip_hdr *ip4hdr = iphdr;
				ip4_addr_t src_addr;
				src_addr.addr = ip4hdr->src.addr;
				printf("dst_port: %d, src_port: %d to %d\n", dst_port, src_port, (int) DEFAULT_LWIP_TO_SEND);
				printf("udp src_addr: %u.%u.%u.%u\n",
					(unsigned int) (src_addr.addr) & 0xFF,
					(unsigned int) (src_addr.addr >> 8) & 0xFF,
					(unsigned int) (src_addr.addr >> 16) & 0xFF,
					(unsigned int) (src_addr.addr >> 24) & 0xFF);
			}
#endif
			if (dst_port == LWIP_IANA_PORT_DHCP_CLIENT)
				return DHCP_LWIP_BRIDGE;

			if (IS_REMOTE_UDP_PORT(dst_port)) {
				return HOST_LWIP_BRIDGE;
			}

		} else if (proto == IP_PROTO_ICMP) {
#ifndef ICMP_ECHOREPLY
#define ICMP_ECHOREPLY 0
#endif
			struct icmp_echo_hdr *icmphdr = (struct icmp_echo_hdr *)((u8_t *)iphdr + IPH_HL(iphdr) * 4);
			if (icmphdr->type == ICMP_ECHO) {
				/* ping request */
				return SLAVE_LWIP_BRIDGE;
			} else if (icmphdr->type == ICMP_ECHOREPLY) {
				/* ping response */
				return BOTH_LWIP_BRIDGE;
			}
		}
	} else if (lwip_ntohs(ethhdr->type) == ETHTYPE_ARP) {
		struct etharp_hdr *arphdr = (struct etharp_hdr *)((u8_t *)frame_data + SIZEOF_ETH_HDR);

        if (arphdr->opcode == lwip_htons(ARP_REQUEST))
			return SLAVE_LWIP_BRIDGE;
		else
			return BOTH_LWIP_BRIDGE;
	}

	return DEFAULT_LWIP_TO_SEND;
}
#endif

/* This function would check the incoming packet from AP
 * to send it to local lwip or host lwip depending upon the
 * destination port used in the packet
 */
esp_err_t wlan_sta_rx_callback(void *buffer, uint16_t len, void *eb)
{
	interface_buffer_handle_t buf_handle = {0};
	hosted_l2_bridge bridge_to_use = HOST_LWIP_BRIDGE;

	if (!buffer || !eb || !datapath) {
		if (eb) {
			ESP_LOGW(TAG, "free packet");
			esp_wifi_internal_free_rx_buffer(eb);
		}
		return ESP_OK;
	}

	ESP_HEXLOGV("STA_Get", buffer, len, 32);

#if ESP_PKT_STATS
	pkt_stats.sta_lwip_in++;
#endif

#ifdef CONFIG_SLAVE_LWIP_ENABLED
	/* Filtering based on destination port */
	bridge_to_use = find_destination_bridge(buffer, len);
#else
	bridge_to_use = HOST_LWIP_BRIDGE;
#endif

	switch (bridge_to_use) {
		case HOST_LWIP_BRIDGE:
			/* Send to Host */
			ESP_LOGV(TAG, "host packet");
			populate_wifi_buffer_handle(&buf_handle, ESP_STA_IF, buffer, len);

			if (unlikely(send_to_host_queue(&buf_handle, PRIO_Q_OTHERS)))
				goto DONE;

#if ESP_PKT_STATS
			pkt_stats.sta_sh_in++;
			pkt_stats.sta_host_lwip_out++;
#endif
			break;

		case SLAVE_LWIP_BRIDGE:
			/* Send to local LWIP */
			ESP_LOGV(TAG, "slave packet");
			esp_netif_receive(slave_sta_netif, buffer, len, eb);
#if ESP_PKT_STATS
			pkt_stats.sta_slave_lwip_out++;
#endif
			break;

		case BOTH_LWIP_BRIDGE:
			ESP_LOGV(TAG, "slave & host packet");

			void * copy_buff = malloc(len);
			assert(copy_buff);
			memcpy(copy_buff, buffer, len);

			/* slave LWIP */
			esp_netif_receive(slave_sta_netif, buffer, len, eb);

			ESP_LOGV(TAG, "slave & host packet");

			/* Host LWIP, free up wifi buffers */
			populate_buff_handle(&buf_handle, ESP_STA_IF, copy_buff, len, free, copy_buff, 0, 0, 0);
			if (unlikely(send_to_host_queue(&buf_handle, PRIO_Q_OTHERS)))
				goto DONE;

#if ESP_PKT_STATS
			pkt_stats.sta_sh_in++;
			pkt_stats.sta_both_lwip_out++;
#endif
			break;

		default:
			ESP_LOGW(TAG, "Packet filtering failed, drop packet");
			goto DONE;
	}

	return ESP_OK;

DONE:
	esp_wifi_internal_free_rx_buffer(eb);
	return ESP_OK;
}

static void process_tx_pkt(interface_buffer_handle_t *buf_handle)
{
	int host_awake = 1;

	/* Check if data path is not yet open */
	if (!datapath) {
		/* Post processing */
		if (buf_handle->free_buf_handle && buf_handle->priv_buffer_handle) {
			buf_handle->free_buf_handle(buf_handle->priv_buffer_handle);
			buf_handle->priv_buffer_handle = NULL;
		}
		ESP_LOGD(TAG, "Data path stopped");
		usleep(100*1000);
		return;
	}
	if (if_context && if_context->if_ops && if_context->if_ops->write) {

#if 1 /* P4 takes some time to wake-up and get ready. Early wake-up is more useful */
		if (is_host_power_saving() && is_host_wakeup_needed(buf_handle)) {
			ESP_LOGI(TAG, "Host sleeping, trigger wake-up");
			ESP_HEXLOGV("Wakeup_pkt", buf_handle->payload+H_ESP_PAYLOAD_HEADER_OFFSET,
					buf_handle->payload_len, buf_handle->payload_len);
			host_awake = wakeup_host(portMAX_DELAY);
			buf_handle->flag |= FLAG_WAKEUP_PKT;
		}
#endif

		if (host_awake)
			if_context->if_ops->write(if_handle, buf_handle);
		else
			ESP_LOGI(TAG, "Host wakeup failed, drop packet");
	}

	/* Post processing */
	if (buf_handle->free_buf_handle && buf_handle->priv_buffer_handle) {
		buf_handle->free_buf_handle(buf_handle->priv_buffer_handle);
		buf_handle->priv_buffer_handle = NULL;
	}
}

#if !BYPASS_TX_PRIORITY_Q
/* Send data to host */
static void send_task(void* pvParameters)
{
	uint8_t queue_type = 0;
	interface_buffer_handle_t buf_handle = {0};

	while (1) {

		if (!datapath) {
			usleep(100*1000);
			continue;
		}

		if (xQueueReceive(meta_to_host_queue, &queue_type, portMAX_DELAY))
			if (xQueueReceive(to_host_queue[queue_type], &buf_handle, portMAX_DELAY))
				process_tx_pkt(&buf_handle);
	}
}
#endif

static void host_reset_task(void* pvParameters)
{
	uint8_t capa = 0;

	ESP_LOGI(TAG, "host reset handler task started");

	while (1) {

		if (host_reset_sem) {
			xSemaphoreTake(host_reset_sem, portMAX_DELAY);
		} else {
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}

		capa = get_capabilities();
		/* send capabilities to host */
		send_event_to_host(RPC_ID__Event_ESPInit);
		ESP_LOGI(TAG,"host reconfig event");
		generate_startup_event(capa);

#ifdef CONFIG_SLAVE_LWIP_ENABLED
		ESP_LOGI(TAG,"--- Wait for IP ---");
		while (!station_got_ip) {
			vTaskDelay(pdMS_TO_TICKS(50));
		}
		send_dhcp_dns_info_to_host(1);
#endif
	}
}

static void parse_protobuf_req(void)
{
	protocomm_pserial_data_ready(pc_pserial, r.data,
		r.len, UNKNOWN_RPC_MSG_ID);
}

void send_event_to_host(int event_id)
{
#if ESP_PKT_STATS
	pkt_stats.serial_tx_evt++;
#endif
	protocomm_pserial_data_ready(pc_pserial, NULL, 0, event_id);
}

void send_event_data_to_host(int event_id, void *data, int size)
{
#if ESP_PKT_STATS
	pkt_stats.serial_tx_evt++;
#endif
	protocomm_pserial_data_ready(pc_pserial, data, size, event_id);
}

static void process_serial_rx_pkt(uint8_t *buf)
{
	struct esp_payload_header *header = NULL;
	uint16_t payload_len = 0;
	uint8_t *payload = NULL;
	int rem_buff_size;

	header = (struct esp_payload_header *) buf;
	payload_len = le16toh(header->len);
	payload = buf + le16toh(header->offset);
	rem_buff_size = sizeof(r.data) - r.len;

	ESP_HEXLOGV("serial_rx", payload, payload_len, 32);

	while (r.valid)
	{
		ESP_LOGI(TAG,"More segment: %u curr seq: %u header seq: %u\n",
			header->flags & MORE_FRAGMENT, r.cur_seq_no, header->seq_num);
		vTaskDelay(10);
	}

	if (!r.len) {
		/* New Buffer */
		r.cur_seq_no = le16toh(header->seq_num);
	}

	if (header->seq_num != r.cur_seq_no) {
		/* Sequence number mismatch */
		r.valid = 1;
		parse_protobuf_req();
		return;
	}

	memcpy((r.data + r.len), payload, min(payload_len, rem_buff_size));
	r.len += min(payload_len, rem_buff_size);

	if (!(header->flags & MORE_FRAGMENT)) {
		/* Received complete buffer */
		r.valid = 1;
		parse_protobuf_req();
	}
}


static int host_to_slave_reconfig(uint8_t *evt_buf, uint16_t len)
{
	uint8_t len_left = len, tag_len;
	uint8_t *pos;

	if (!evt_buf)
		return ESP_FAIL;

	pos = evt_buf;
	ESP_LOGD(TAG, "Init event length: %u", len);
	if (len > 64) {
		ESP_LOGE(TAG, "Init event length: %u", len);
#if CONFIG_ESP_SPI_HOST_INTERFACE
		ESP_LOGE(TAG, "Seems incompatible SPI mode try changing SPI mode. Asserting for now.");
#endif
		assert(len < 64);
	}

	while (len_left) {
		tag_len = *(pos + 1);

		if (*pos == HOST_CAPABILITIES) {

			ESP_LOGI(TAG, "Host capabilities: %2x", *pos);

		} else if (*pos == RCVD_ESP_FIRMWARE_CHIP_ID) {

			if (CONFIG_IDF_FIRMWARE_CHIP_ID != *(pos+2)) {
				ESP_LOGE(TAG, "Chip id returned[%u] doesn't match with chip id sent[%u]",
						*(pos+2), CONFIG_IDF_FIRMWARE_CHIP_ID);
			}

		} else if (*pos == SLV_CONFIG_TEST_RAW_TP) {
#if TEST_RAW_TP
			switch (*(pos + 2)) {

			case ESP_TEST_RAW_TP__ESP_TO_HOST:
				ESP_LOGI(TAG, "Raw TP ESP --> Host");
				/* TODO */
			break;

			case ESP_TEST_RAW_TP__HOST_TO_ESP:
				ESP_LOGI(TAG, "Raw TP ESP <-- Host");
				/* TODO */
			break;

			case ESP_TEST_RAW_TP__BIDIRECTIONAL:
				ESP_LOGI(TAG, "Raw TP ESP <--> Host");
				/* TODO */
			break;

			default:
				ESP_LOGW(TAG, "Unsupported Raw TP config");
			}

			process_test_capabilities(*(pos + 2));
#else
			if (*(pos + 2))
				ESP_LOGW(TAG, "Host requested raw throughput testing, but not enabled in slave");
#endif
		} else if (*pos == SLV_CFG_FLOW_CTL_START_THRESHOLD) {

			slv_cfg_g.flow_ctl_start_thres = *(pos + 2);
			ESP_LOGI(TAG, "ESP<-Host wifi flow ctl start thres [%u%%]",
					slv_cfg_g.flow_ctl_start_thres);
#ifdef FLOW_CTL_ENABLED
			if (slv_cfg_g.flow_ctl_start_thres) {
				assert(xTaskCreate(flow_ctl_task, "flow_ctl_task" ,
						CONFIG_ESP_DEFAULT_TASK_STACK_SIZE, NULL ,
						CONFIG_ESP_DEFAULT_TASK_PRIO, NULL) == pdTRUE);
			}
#endif
#ifdef CONFIG_SLAVE_LWIP_ENABLED
		send_dhcp_dns_info_to_host(1);
#endif
		} else if (*pos == SLV_CFG_FLOW_CTL_CLEAR_THRESHOLD) {

			slv_cfg_g.flow_ctl_clear_thres = *(pos + 2);
			ESP_LOGI(TAG, "ESP<-Host wifi flow ctl clear thres [%u%%]",
					slv_cfg_g.flow_ctl_clear_thres);

		} else {

			ESP_LOGD(TAG, "Unsupported H->S config: %2x", *pos);

		}

		pos += (tag_len+2);
		len_left -= (tag_len+2);
	}

	return ESP_OK;
}

static void process_priv_pkt(uint8_t *payload, uint16_t payload_len)
{
	int ret = 0;
	struct esp_priv_event *event;

	if (!payload || !payload_len)
		return;

	event = (struct esp_priv_event *) payload;

	if (event->event_type == ESP_PRIV_EVENT_INIT) {

		ESP_LOGI(TAG, "Slave init_config received from host");
		ESP_HEXLOGD("init_config", event->event_data, event->event_len, 32);

		ret = host_to_slave_reconfig(event->event_data, event->event_len);
		if (ret) {
			ESP_LOGE(TAG, "failed to init event\n\r");
		}
	} else if (event->event_type == ESP_PACKET_TYPE_DATA) {
		//process_pri
		ESP_LOGW(TAG, "Drop unknown event\n\r");
	} else {
		ESP_LOGW(TAG, "Drop unknown event\n\r");
	}
}

static void process_rx_pkt(interface_buffer_handle_t *buf_handle)
{
	struct esp_payload_header *header = NULL;
	uint8_t *payload = NULL;
	uint16_t payload_len = 0;
	int ret = 0;
	int retry = 0;
	uint32_t delay = 0;

	header = (struct esp_payload_header *) buf_handle->payload;
	payload = buf_handle->payload + le16toh(header->offset);
	payload_len = le16toh(header->len);

	ESP_HEXLOGV("bus_RX", buf_handle->payload, buf_handle->payload_len, 8);

#define WIFI_TX_REPEAT_STEP       4
#define WIFI_TX_INTERVAL_START    5
#define WIFI_TX_INTERVAL_CAP      10240
#define WIFI_TX_MAX_RETRY         100

	if (buf_handle->if_type == ESP_STA_IF && station_connected) {

		ESP_HEXLOGV("STA_Put", payload, payload_len, 32);

		/* Forward data to WLAN driver */
		do {
			ret = esp_wifi_internal_tx(ESP_IF_WIFI_STA, payload, payload_len);

			/* Delay only if flow control is enabled */
			if (ret && slv_cfg_g.flow_ctl_start_thres) {
				if (retry % WIFI_TX_REPEAT_STEP == 0) {
					if (delay == 0)
						/* Retries without sleep exhausted */
						delay = WIFI_TX_INTERVAL_START;
					else
						/* Increase interval exponentially */
						delay <<= 1;
				}

				if (delay > WIFI_TX_INTERVAL_CAP)
					/* Max saturation of delay interval */
					delay = WIFI_TX_INTERVAL_CAP;

				if (delay)
					vTaskDelay(delay);
			}

			retry++;
		} while (ret && (retry < WIFI_TX_MAX_RETRY));

#if ESP_PKT_STATS
		if (retry >= WIFI_TX_MAX_RETRY)
			pkt_stats.hs_bus_sta_fail++;
		else
			pkt_stats.hs_bus_sta_out++;
#endif

		IS_FLOW_CTL_NEEDED();
	} else if (buf_handle->if_type == ESP_AP_IF && softap_started) {
		/* Forward data to wlan driver */
		esp_wifi_internal_tx(ESP_IF_WIFI_AP, payload, payload_len);
		ESP_HEXLOGV("AP_Put", payload, payload_len, 32);
	} else if (buf_handle->if_type == ESP_SERIAL_IF) {
#if ESP_PKT_STATS
			pkt_stats.serial_rx++;
#endif
		process_serial_rx_pkt(buf_handle->payload);
	} else if (buf_handle->if_type == ESP_PRIV_IF) {
		process_priv_pkt(payload, payload_len);
	}
#if CONFIG_NW_COPROC_BT_ENABLED
#if defined(CONFIG_BT_ENABLED) && BLUETOOTH_HCI
	else if (buf_handle->if_type == ESP_HCI_IF) {
		process_hci_rx_pkt(payload, payload_len);
	}
#endif
#endif
#if TEST_RAW_TP
	else if (buf_handle->if_type == ESP_TEST_IF) {
		debug_update_raw_tp_rx_count(payload_len);
	}
#endif

	/* Free buffer handle */
	if (buf_handle->free_buf_handle && buf_handle->priv_buffer_handle) {
		buf_handle->free_buf_handle(buf_handle->priv_buffer_handle);
		buf_handle->priv_buffer_handle = NULL;
	}
}

/* Get data from host */
static void recv_task(void* pvParameters)
{
	interface_buffer_handle_t buf_handle = {0};

	for (;;) {

		if (!datapath) {
			/* Datapath is not enabled by host yet*/
			usleep(100*1000);
			continue;
		}

		/* receive data from transport layer */
		if (if_context && if_context->if_ops && if_context->if_ops->read) {
			int len = if_context->if_ops->read(if_handle, &buf_handle);
			if (len <= 0) {
				usleep(10*1000);
				continue;
			}
		}

		process_rx_pkt(&buf_handle);
	}
}

static ssize_t serial_read_data(uint8_t *data, ssize_t len)
{
	len = min(len, r.len);
	if (r.valid) {
		memcpy(data, r.data, len);
		r.valid = 0;
		r.len = 0;
		r.cur_seq_no = 0;
	} else {
		ESP_LOGI(TAG,"No data to be read, len %d", len);
	}
	return len;
}

int send_to_host_queue(interface_buffer_handle_t *buf_handle, uint8_t queue_type)
{
#if BYPASS_TX_PRIORITY_Q
	process_tx_pkt(buf_handle);
	return ESP_OK;
#else
	int ret = xQueueSend(to_host_queue[queue_type], buf_handle, portMAX_DELAY);
	if (ret != pdTRUE) {
		ESP_LOGE(TAG, "Failed to send buffer into queue[%u]\n",queue_type);
		return ESP_FAIL;
	}
	if (queue_type == PRIO_Q_SERIAL)
		ret = xQueueSendToFront(meta_to_host_queue, &queue_type, portMAX_DELAY);
	else
		ret = xQueueSend(meta_to_host_queue, &queue_type, portMAX_DELAY);

	if (ret != pdTRUE) {
		ESP_LOGE(TAG, "Failed to send buffer into meta queue[%u]\n",queue_type);
		return ESP_FAIL;
	}

	return ESP_OK;
#endif
}

static esp_err_t serial_write_data(uint8_t* data, ssize_t len)
{
	uint8_t *pos = data;
	int32_t left_len = len;
	int32_t frag_len = 0;
	static uint16_t seq_num = 0;

	do {
		interface_buffer_handle_t buf_handle = {0};

		seq_num++;

		buf_handle.if_type = ESP_SERIAL_IF;
		buf_handle.if_num = 0;
		buf_handle.seq_num = seq_num;

		if (left_len > ETH_DATA_LEN) {
			frag_len = ETH_DATA_LEN;
			buf_handle.flag = MORE_FRAGMENT;
		} else {
			frag_len = left_len;
			buf_handle.flag = 0;
			buf_handle.priv_buffer_handle = data;
			buf_handle.free_buf_handle = free;
		}

		buf_handle.payload = pos;
		buf_handle.payload_len = frag_len;

		if (send_to_host_queue(&buf_handle, PRIO_Q_SERIAL)) {
			if (data) {
				free(data);
				data = NULL;
			}
			return ESP_FAIL;
		}

		ESP_HEXLOGV("serial_tx_create", data, frag_len, 32);

		left_len -= frag_len;
		pos += frag_len;
	} while(left_len);

	return ESP_OK;
}

int event_handler(uint8_t val)
{
	switch(val) {
		case ESP_OPEN_DATA_PATH:
			if (if_handle) {
				if_handle->state = ACTIVE;
				datapath = 1;
				ESP_EARLY_LOGI(TAG, "Start Data Path");
				if (host_reset_sem) {
					xSemaphoreGive(host_reset_sem);
				}
			} else {
				ESP_EARLY_LOGI(TAG, "Failed to Start Data Path");
			}
			break;

		case ESP_CLOSE_DATA_PATH:
			datapath = 0;
			if (if_handle) {
				ESP_EARLY_LOGI(TAG, "Stop Data Path");
				if_handle->state = DEACTIVE;
			} else {
				ESP_EARLY_LOGI(TAG, "Failed to Stop Data Path");
			}
			break;

		case ESP_POWER_SAVE_ON:
			host_power_save_alert(ESP_POWER_SAVE_ON);
			if_handle->state = ACTIVE;
			break;

		case ESP_POWER_SAVE_OFF:
			if_handle->state = ACTIVE;
			datapath = 1;
			if (host_reset_sem) {
				xSemaphoreGive(host_reset_sem);
			}
			host_power_save_alert(ESP_POWER_SAVE_OFF);
			break;
	}
	return 0;
}

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
/* These functions are only for debugging purpose
 * Please do not enable in production environments
 */
static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
	TaskStatus_t *start_array = NULL, *end_array = NULL;
	UBaseType_t start_array_size, end_array_size;
	uint32_t start_run_time, end_run_time;
	esp_err_t ret;

	/* Allocate array to store current task states */
	start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
	start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
	if (start_array == NULL) {
		ret = ESP_ERR_NO_MEM;
		goto exit;
	}
	/* Get current task states */
	start_array_size = uxTaskGetSystemState(start_array,
			start_array_size, &start_run_time);
	if (start_array_size == 0) {
		ret = ESP_ERR_INVALID_SIZE;
		goto exit;
	}

	vTaskDelay(xTicksToWait);

	/* Allocate array to store tasks states post delay */
	end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
	end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
	if (end_array == NULL) {
		ret = ESP_ERR_NO_MEM;
		goto exit;
	}
	/* Get post delay task states */
	end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
	if (end_array_size == 0) {
		ret = ESP_ERR_INVALID_SIZE;
		goto exit;
	}

	/* Calculate total_elapsed_time in units of run time stats clock period */
	uint32_t total_elapsed_time = (end_run_time - start_run_time);
	if (total_elapsed_time == 0) {
		ret = ESP_ERR_INVALID_STATE;
		goto exit;
	}

	ESP_LOGI(TAG,"| Task | Run Time | Percentage");
	/* Match each task in start_array to those in the end_array */
	for (int i = 0; i < start_array_size; i++) {
		int k = -1;
		for (int j = 0; j < end_array_size; j++) {
			if (start_array[i].xHandle == end_array[j].xHandle) {
				k = j;
				/* Mark that task have been matched by overwriting their handles */
				start_array[i].xHandle = NULL;
				end_array[j].xHandle = NULL;
				break;
			}
		}
		/* Check if matching task found */
		if (k >= 0) {
			uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter -
				start_array[i].ulRunTimeCounter;
			uint32_t percentage_time = (task_elapsed_time * 100UL) /
				(total_elapsed_time * portNUM_PROCESSORS);
			ESP_LOGI(TAG,"| %s | %d | %d%%", start_array[i].pcTaskName,
					(int)task_elapsed_time, (int)percentage_time);
		}
	}

	/* Print unmatched tasks */
	for (int i = 0; i < start_array_size; i++) {
		if (start_array[i].xHandle != NULL) {
			ESP_LOGI(TAG,"| %s | Deleted", start_array[i].pcTaskName);
		}
	}
	for (int i = 0; i < end_array_size; i++) {
		if (end_array[i].xHandle != NULL) {
			ESP_LOGI(TAG,"| %s | Created", end_array[i].pcTaskName);
		}
	}
	ret = ESP_OK;

exit:    /* Common return path */
	if (start_array)
		free(start_array);
	if (end_array)
		free(end_array);
	return ret;
}

static void print_mem_stats()
{
    uint32_t freeSize = esp_get_free_heap_size();
    printf("The available total size of heap:%" PRIu32 "\n", freeSize);

    printf("\tDescription\tInternal\tSPIRAM\n");
    printf("Current Free Memory\t%d\t\t%d\n",
           heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("Largest Free Block\t%d\t\t%d\n",
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("Min. Ever Free Size\t%d\t\t%d\n",
           heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

void task_runtime_stats_task(void* pvParameters)
{
	while (1) {
		ESP_LOGI(TAG,"\n\nGetting real time stats over %d ticks", (int) STATS_TICKS);
		if (print_real_time_stats(STATS_TICKS) == ESP_OK) {
			ESP_LOGI(TAG,"Real time stats obtained");
		} else {
			ESP_LOGI(TAG,"Error getting real time stats");
		}
		print_mem_stats();
		vTaskDelay(STATS_TICKS);
	}
}
#endif

static void IRAM_ATTR gpio_resetpin_isr_handler(void* arg)
{

	ESP_EARLY_LOGI(TAG, "*********");
	if (CONFIG_ESP_GPIO_SLAVE_RESET == -1) {
		ESP_EARLY_LOGI(TAG, "%s: using EN pin for slave reset", __func__);
		return;
	}

	static uint32_t lasthandshaketime_us;
	uint32_t currtime_us = esp_timer_get_time();

	if (gpio_get_level(CONFIG_ESP_GPIO_SLAVE_RESET) == 0) {
		lasthandshaketime_us = currtime_us;
	} else {
		uint32_t diff = currtime_us - lasthandshaketime_us;
		ESP_EARLY_LOGI(TAG, "%s Diff: %u", __func__, diff);
		if (diff < 500) {
			return; //ignore everything < half ms after an earlier irq
		} else {
			ESP_EARLY_LOGI(TAG, "Host triggered slave reset");
			esp_restart();
		}
	}
}

static void register_reset_pin(uint32_t gpio_num)
{
	if (gpio_num != -1) {
		ESP_LOGI(TAG, "Using GPIO [%lu] as slave reset pin", gpio_num);
		gpio_reset_pin(gpio_num);

		gpio_config_t slave_reset_pin_conf={
			.intr_type=GPIO_INTR_DISABLE,
			.mode=GPIO_MODE_INPUT,
			.pull_up_en=1,
			.pin_bit_mask=(1<<gpio_num)
		};

		gpio_config(&slave_reset_pin_conf);
		gpio_set_intr_type(gpio_num, GPIO_INTR_ANYEDGE);
		gpio_install_isr_service(0);
		gpio_isr_handler_add(gpio_num, gpio_resetpin_isr_handler, NULL);
	}
}


#ifdef CONFIG_SLAVE_LWIP_ENABLED
void create_slave_sta_netif(uint8_t dhcp_at_slave)
{
    /* Create "almost" default station, but with un-flagged DHCP client */
	esp_netif_inherent_config_t netif_cfg;
	memcpy(&netif_cfg, ESP_NETIF_BASE_DEFAULT_WIFI_STA, sizeof(netif_cfg));

	if (!dhcp_at_slave)
		netif_cfg.flags &= ~ESP_NETIF_DHCP_CLIENT;

	esp_netif_config_t cfg_sta = {
		.base = &netif_cfg,
		.stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
	};
	esp_netif_t *netif_sta = esp_netif_new(&cfg_sta);
	assert(netif_sta);

	ESP_ERROR_CHECK(esp_netif_attach_wifi_station(netif_sta));
	ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());

	if (!dhcp_at_slave)
		ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif_sta));

	slave_sta_netif = netif_sta;
}
#endif

extern void mqtt_example_start(void);
extern void slave_dhcp_request_example(void);


#ifdef FLOW_CTL_ENABLED
static inline esp_err_t s2h_intimate_flow_ctl_status(void)
{
	interface_buffer_handle_t buf_handle = {0};

	// TODO: add if_type, to separate flow ctl for sta and ap if
	//buf_handle->if_type = if_type;

	if (*wifi_flow_ctrl)
		buf_handle.flow_ctl_en = H_FLOW_CTL_ON;
	else
		buf_handle.flow_ctl_en = H_FLOW_CTL_OFF;

	ESP_LOGV(TAG, "flow_ctl %u", buf_handle.flow_ctl_en);
	send_to_host_queue(&buf_handle, PRIO_Q_SERIAL);

	return ESP_OK;
}

static void toggle_wifi_ctl_if_needed(void)
{
	uint32_t queue_load;
	uint8_t load_percent;

	/* Check if needs to disable wifi flow ctrl */
	queue_load = get_cur_back_pressure();

	load_percent = (queue_load*100/CONFIG_ESP_RX_Q_SIZE);

	if (load_percent > slv_cfg_g.flow_ctl_start_thres) {
		ESP_LOGV(TAG, "start wifi_flow_ctrl at host");
		*wifi_flow_ctrl = 1;
	}

	if (load_percent < slv_cfg_g.flow_ctl_clear_thres) {
		ESP_LOGV(TAG, "stop wifi flow ctrl at host");
		*wifi_flow_ctrl = 0;
	}

	//*wifi_flow_ctrl_dirty = 1;
	if (*wifi_flow_ctrl != *wifi_flow_ctrl_dirty) {

#if ESP_PKT_STATS
		if (*wifi_flow_ctrl)
			pkt_stats.sta_flowctrl_on++;
		else
			pkt_stats.sta_flowctrl_off++;
#endif
		*wifi_flow_ctrl_dirty = *wifi_flow_ctrl;
		s2h_intimate_flow_ctl_status();
	}
}

/* TODO: LOW MEM solution */
static void flow_ctl_task(void* pvParameters)
{
	ESP_LOGI(TAG, "Flow control task started");
	sdio_flow_ctl_sem = xSemaphoreCreateBinary();
	assert(sdio_flow_ctl_sem);

	for(;;) {
		xSemaphoreTake(sdio_flow_ctl_sem, portMAX_DELAY);
		toggle_wifi_ctl_if_needed();
	}
}
#endif

#ifndef CONFIG_ADAPTER_ONLY_MODE
static SemaphoreHandle_t mutex;

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#define RECEIVED_MSG_BUF_SIZE 10000

static received_msg_t *received_msg;

__attribute__((weak))
int app_common_queryServer_get_by_idx(int index, uint8_t **data, int *len, bool *have_more)
{
    return -1;
}

extern bool kvswebrtc_is_time_sync_done();

/* Function pointer for WebRTC message handler */
static webrtc_message_callback_t webrtc_msg_callback = NULL;

/* Register a callback for WebRTC messages */
void network_coprocessor_register_webrtc_callback(webrtc_message_callback_t callback)
{
    webrtc_msg_callback = callback;
    ESP_LOGI(TAG, "WebRTC message callback registered");
}

/* Default message handler if no callback is registered */
extern void on_webrtc_bridge_msg_received(void *data, int len);

static void handle_on_message_received(void *priv_data)
{
    received_msg_t *received_msg = (received_msg_t *) priv_data;

    /* Use registered callback if available, otherwise use default handler */
    if (webrtc_msg_callback) {
        webrtc_msg_callback(received_msg->buf, received_msg->data_size);
    } else {
        on_webrtc_bridge_msg_received((void *) received_msg->buf, received_msg->data_size);
    }

    /* Done! Free the buffer now */
    free(received_msg->buf);
    free(received_msg);
}
#endif

static esp_err_t user_defined_rpc_h2s_req_handler(Rpc *req, Rpc *resp)
{
	RpcReqUSR *req_usr = NULL;
	RpcRespUSR *resp_usr = NULL;

	if (!req || !resp) {
		ESP_LOGE(TAG, "Failed to process rpc, req[%p] resp[%p]", req, resp);
		return ESP_FAIL;
	}

	/* This function is just a template/place_holder.
	 * It provides 5 default hooks for user spacific handling.
	 * users can change this function to handle USR1..USR5 requests as they wish
	 */
	switch (req->msg_id) {
		case RPC_ID__Req_USR1:
			ESP_LOGI(TAG, "Received req_USR1, sending resp resp_USR1");
			req_usr = req->req_usr1;
			resp_usr = resp->resp_usr1;
			break;
		case RPC_ID__Req_USR2:
			ESP_LOGI(TAG, "Received req_USR2, sending resp resp_USR2");
			req_usr = req->req_usr2;
			resp_usr = resp->resp_usr2;
			break;
		case RPC_ID__Req_USR3:
			ESP_LOGI(TAG, "Received req_USR3, sending resp resp_USR3");
			req_usr = req->req_usr3;
			resp_usr = resp->resp_usr3;
			break;
		case RPC_ID__Req_USR4:
			ESP_LOGI(TAG, "Received req_USR4, sending resp resp_USR4");
			req_usr = req->req_usr4;
			resp_usr = resp->resp_usr4;
			break;
		case RPC_ID__Req_USR5:
			ESP_LOGI(TAG, "Received req_USR5, sending resp resp_USR5");
			req_usr = req->req_usr5;
			resp_usr = resp->resp_usr5;
			break;
		default:
			ESP_LOGE(TAG, "Unhandled RPC req[%p]", req);
			return ESP_FAIL;
	}

	// ESP_LOGI(TAG, "req params: %" PRIi32 ",%" PRIi32 ",%" PRIu32 ",%" PRIu32 ", %d,%s: Loopback to response!",
	// 	req_usr->int_1, req_usr->int_2,
	// 	req_usr->uint_1, req_usr->uint_2, (int) req_usr->data.len,
	// 	req_usr->data.len? (char*)req_usr->data.data: "null");

	// /* Form dummy loopback response as demonstration.
	//  * Users can fetch values and add their logic and send response*/
	resp_usr->int_1  = req_usr->int_1;
	resp_usr->int_2  = req_usr->int_2;
	resp_usr->uint_1 = req_usr->uint_1;
	resp_usr->uint_2 = req_usr->uint_2;

#ifndef CONFIG_ADAPTER_ONLY_MODE
	if (req->msg_id == RPC_ID__Req_USR2) {
		struct timeval tv = {};
		gettimeofday(&tv, NULL);
		resp_usr->data.len = sizeof(struct timeval);
		resp_usr->data.data = calloc(1, 100);
		memcpy(resp_usr->data.data, &tv, sizeof(struct timeval));
		resp_usr->resp = SUCCESS;
		return ESP_OK;
	} else if (req->msg_id == RPC_ID__Req_USR3) {
		int len = 0;
		bool have_more = false;
		int index = req_usr->int_1;
		resp_usr->data.data = NULL;
		app_common_queryServer_get_by_idx(index, &resp_usr->data.data, &len, &have_more);
		resp_usr->data.len = (size_t) len;
		resp_usr->uint_1 = have_more;
		resp_usr->resp = SUCCESS;
		return ESP_OK;
	}
#endif

#if 0
	if (req_usr->data.data && req_usr->data.len) {
		resp_usr->data.data = (uint8_t *)calloc(1, req_usr->data.len);
		if (!resp_usr->data.data) {
			ESP_LOGI(TAG, "%s: Failed to allocate", __func__);
			received_msg->data_size = 0;
			resp_usr->resp = FAILURE;
			xSemaphoreGive(mutex);
			return ESP_OK;
		}
		resp_usr->data.len = req_usr->data.len;
		memcpy(resp_usr->data.data, req_usr->data.data, req_usr->data.len);
	}
#else /* Maybe if we do below method, it makes more sense, the data is also less */
	resp_usr->data.data = (uint8_t *)calloc(1, 10);
	memcpy(resp_usr->data.data, "Some Text", sizeof("Some Text"));
	resp_usr->data.len = 10; /* Just some response and no data echo */
#endif

#ifndef CONFIG_ADAPTER_ONLY_MODE
	if (!received_msg) {
		if (req_usr->uint_1 == 0) {
			int required_size = req_usr->uint_2 ? req_usr->uint_2 : req_usr->data.len;
			received_msg = esp_webrtc_create_buffer_for_msg(required_size);
		}
		if (!received_msg) {
			ESP_LOGE(TAG, "Memory issue or wrong seq number");
			resp_usr->resp = SUCCESS;
			return ESP_OK;
		}
	}

	bool is_fin = req_usr->int_2;

	xSemaphoreTake(mutex, 5000);

	esp_err_t append_ret = esp_webrtc_append_msg_to_existing(received_msg, req_usr->data.data, req_usr->data.len, is_fin);
	if (append_ret == ESP_OK) {
		/* We do not want to delay the response. Handle this after */
	} else if (append_ret == ESP_FAIL) {
		ESP_LOGW(TAG, "Failed to put the message into buffer...");
		free(received_msg->buf);
		free(received_msg);
		received_msg = NULL;
	} else {
		ESP_LOGW(TAG, "Waiting for the next part...");
	}

	if (append_ret == ESP_OK) {
        /* Process the message now */
        esp_work_queue_add_task(&handle_on_message_received, (void *) received_msg);
        received_msg = NULL;
	}
	xSemaphoreGive(mutex);
#endif
	resp_usr->resp = SUCCESS;
	return ESP_OK;
}

static void user_defined_rpc_s2h_evt(uint8_t usr_evt_num)
{
	struct rpc_user_specific_event_t usr_evt = {0};
	uint8_t evt_data_bytes[RPC_USER_SPECIFIC_EVENT_DATA_SIZE] ="USR defined event num: ";
	uint16_t evt_data_bytes_len = 0;
	int rpc_msg_id = 0;

	/* five user specific event hooks are pre-added for user convenience
	 * user can change the functionality as per their use-case
	 * strlcpy, etc are just to demo */
	switch(usr_evt_num) {
		case 1:
			rpc_msg_id = RPC_ID__Event_USR1;
			strlcat((char*)evt_data_bytes, "(1)", sizeof(evt_data_bytes));
			break;
		case 2:
			rpc_msg_id = RPC_ID__Event_USR2;
			strlcat((char*)evt_data_bytes, "(2)", sizeof(evt_data_bytes));
			break;
		case 3:
			rpc_msg_id = RPC_ID__Event_USR3;
			strlcat((char*)evt_data_bytes, "(3)", sizeof(evt_data_bytes));
			break;
		case 4:
			rpc_msg_id = RPC_ID__Event_USR4;
			strlcat((char*)evt_data_bytes, "(4)", sizeof(evt_data_bytes));
			break;
		case 5:
			rpc_msg_id = RPC_ID__Event_USR5;
			strlcat((char*)evt_data_bytes, "(5)", sizeof(evt_data_bytes));
			break;
		default:
			ESP_LOGE(TAG, "Unknown/unsupported USR event[%u]", usr_evt_num);
			return;
	}

	evt_data_bytes_len = strlen((const char*)evt_data_bytes) + 1;
	usr_evt.data_len = evt_data_bytes_len;

	usr_evt.int_1 = 1;
	usr_evt.int_2 = 2;
	usr_evt.uint_1 = 3;
	usr_evt.uint_2 = 4;
	usr_evt.resp = SUCCESS;
	strlcpy((char*)usr_evt.data, (const char*)evt_data_bytes, evt_data_bytes_len);
	usr_evt.data_len = evt_data_bytes_len;

	send_event_data_to_host(rpc_msg_id,
			&usr_evt, sizeof(struct rpc_user_specific_event_t));
}

void custom_rpc_events_demo()
{
	uint8_t usr_evt_num = 1; /* custom events 1..5 */
	while (1) {
		/* Demonstrate custom RPC events
		 * User can use one of them as per their choice */
		user_defined_rpc_s2h_evt(usr_evt_num);
		usr_evt_num++;
		if (usr_evt_num>5)
			usr_evt_num = 1;
		sleep(5);
	}
}

static void host_wakeup_callback(void)
{
#if H_HOST_PS_ALLOWED
	ESP_EARLY_LOGI(TAG, "Sending DHCP status to host");
	send_dhcp_dns_info_to_host(1);
#endif
}

void network_coprocessor_init()
{
#ifndef CONFIG_ADAPTER_ONLY_MODE
	mutex = xSemaphoreCreateMutex();
#endif
	assert(host_reset_sem = xSemaphoreCreateBinary());

	print_firmware_version();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	register_reset_pin(CONFIG_ESP_GPIO_SLAVE_RESET);

	host_power_save_init(host_wakeup_callback);

#if CONFIG_NW_COPROC_BT_ENABLED
	initialise_bluetooth();
#endif

	pc_pserial = protocomm_new();
	if (pc_pserial == NULL) {
		ESP_LOGE(TAG,"Failed to allocate memory for new instance of protocomm ");
		return;
	}

	/* Endpoint for control command responses */
	if (protocomm_add_endpoint(pc_pserial, RPC_EP_NAME_RSP,
				data_transfer_handler, user_defined_rpc_h2s_req_handler) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add enpoint");
		return;
	}

	/* Endpoint for control notifications for events subscribed by user */
	if (protocomm_add_endpoint(pc_pserial, RPC_EP_NAME_EVT,
				rpc_evt_handler, NULL) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add enpoint");
		return;
	}

	protocomm_pserial_start(pc_pserial, serial_write_data, serial_read_data);

	if_context = interface_insert_driver(event_handler);

#if CONFIG_ESP_SPI_HOST_INTERFACE
	datapath = 1;
#endif

	if (!if_context || !if_context->if_ops) {
		ESP_LOGE(TAG, "Failed to insert driver\n");
		return;
	}

	if_handle = if_context->if_ops->init();

#ifdef FLOW_CTL_ENABLED
	wifi_flow_ctrl = &slv_state_g.flow_ctl_wifi;
	wifi_flow_ctrl_dirty = &slv_state_g.flow_ctl_wifi_to_send;
#endif

	if (!if_handle) {
		ESP_LOGE(TAG, "Failed to initialize driver\n");
		return;
	}

	assert(xTaskCreate(recv_task , "recv_task" ,
			CONFIG_ESP_DEFAULT_TASK_STACK_SIZE, NULL ,
			CONFIG_ESP_DEFAULT_TASK_PRIO, NULL) == pdTRUE);

#if !BYPASS_TX_PRIORITY_Q
	meta_to_host_queue = xQueueCreate(TO_HOST_QUEUE_SIZE*3, sizeof(uint8_t));
	assert(meta_to_host_queue);
	for (uint8_t prio_q_idx=0; prio_q_idx<MAX_PRIORITY_QUEUES; prio_q_idx++) {
		to_host_queue[prio_q_idx] = xQueueCreate(TO_HOST_QUEUE_SIZE,
				sizeof(interface_buffer_handle_t));
		assert(to_host_queue[prio_q_idx]);
	}
	assert(xTaskCreate(send_task , "send_task" ,
			CONFIG_ESP_DEFAULT_TASK_STACK_SIZE, NULL ,
			CONFIG_ESP_DEFAULT_TASK_PRIO, NULL) == pdTRUE);
#endif
	// create_debugging_tasks();

	while(!datapath) {
		vTaskDelay(10);
	}

#ifdef CONFIG_SLAVE_LWIP_ENABLED
	create_slave_sta_netif(H_SLAVE_LWIP_DHCP_AT_SLAVE);

	ESP_LOGI(TAG, "Default LWIP post filtering packets to send: %s",
#if defined(CONFIG_ESP_DEFAULT_LWIP_SLAVE)
			"slave. Host need to use **static netif** only"
#elif defined(CONFIG_ESP_DEFAULT_LWIP_HOST)
			"host"
#elif defined(CONFIG_ESP_DEFAULT_LWIP_BOTH)
			"host+slave"
#endif
			);
#endif

	assert(xTaskCreate(host_reset_task, "host_reset_task" ,
			HOST_RESET_TASK_STACK, NULL ,
			CONFIG_ESP_DEFAULT_TASK_PRIO, NULL) == pdTRUE);
}
