/*
 * Espressif Systems Wireless LAN device driver
 *
 * Copyright (C) 2015-2021 Espressif Systems (Shanghai) PTE LTD
 *
 * This software file (the "File") is distributed by Espressif Systems (Shanghai)
 * PTE LTD under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */


#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rpc_slave_if.h"
#include "string.h"
#include "adapter.h"
#include "os_wrapper.h"
#include "rpc_wrap.h"
#include "rpc_common.h"
#include "esp_log.h"
#include "esp_http_client.h"
#ifdef CONFIG_SLAVE_LWIP_ENABLED
#if !defined(CONFIG_LWIP_TCP_LOCAL_PORT_RANGE_START) || \
    !defined(CONFIG_LWIP_TCP_LOCAL_PORT_RANGE_END) ||   \
    !defined(CONFIG_LWIP_UDP_LOCAL_PORT_RANGE_START) || \
    !defined(CONFIG_LWIP_UDP_LOCAL_PORT_RANGE_END)
#error "LWIP ports at host need to configured, ensure they are exclusive and different from slave"
#endif
#endif
#ifdef CONFIG_HOST_USES_STATIC_NETIF
#include "lwip/err.h"
#include "lwip/sys.h"
//#include "lwip/ip_addr.h"
#include "lwip/etharp.h"
#include "lwip/prot/iana.h"
#include "lwip/prot/ip.h"
#include "lwip/prot/tcp.h"
#include "lwip/prot/udp.h"
#include "esp_check.h"
#include "power_save_drv.h"
#include <inttypes.h>
#endif

DEFINE_LOG_TAG(rpc_wrap);
static char* OTA_TAG = "h_ota";

uint8_t restart_after_slave_ota = 0;

#define WIFI_VENDOR_IE_ELEMENT_ID                         0xDD
#define OFFSET                                            4
#define VENDOR_OUI_0                                      1
#define VENDOR_OUI_1                                      2
#define VENDOR_OUI_2                                      3
#define VENDOR_OUI_TYPE                                   22
#define CHUNK_SIZE                                        1400
#define OTA_BEGIN_RSP_TIMEOUT_SEC                         15
#define WIFI_INIT_RSP_TIMEOUT_SEC                         15
#define OTA_FROM_WEB_URL                                  1



static ctrl_cmd_t * RPC_DEFAULT_REQ(void)
{
  ctrl_cmd_t *new_req = (ctrl_cmd_t*)g_h.funcs->_h_calloc(1, sizeof(ctrl_cmd_t));
  assert(new_req);
  new_req->msg_type = RPC_TYPE__Req;
  new_req->rpc_rsp_cb = NULL;
  new_req->rsp_timeout_sec = DEFAULT_RPC_RSP_TIMEOUT; /* 5 sec */
  /* new_req->wait_prev_cmd_completion = WAIT_TIME_B2B_RPC_REQ; */
  return new_req;
}


#define CLEANUP_RPC(msg) do {                            \
  if (msg) {                                             \
    if (msg->app_free_buff_hdl) {                        \
      if (msg->app_free_buff_func) {                     \
        msg->app_free_buff_func(msg->app_free_buff_hdl); \
        msg->app_free_buff_hdl = NULL;                   \
      }                                                  \
    }                                                    \
    g_h.funcs->_h_free(msg);                             \
    msg = NULL;                                          \
  }                                                      \
} while(0);

#define YES                                               1
#define NO                                                0
#define MIN_TIMESTAMP_STR_SIZE                            75
#define HEARTBEAT_DURATION_SEC                            20

#ifdef CONFIG_HOST_USES_STATIC_NETIF
ESP_EVENT_DECLARE_BASE(IP_EVENT);
#endif

typedef struct {
	int event;
	rpc_rsp_cb_t fun;
} event_callback_table_t;

static void (*usr_evt_cb)(uint8_t usr_evt_num, rpc_usr_t *usr_evt);

int rpc_init(void)
{
	ESP_LOGD(TAG, "%s", __func__);
	return rpc_slaveif_init();
}

int rpc_deinit(void)
{
	ESP_LOGD(TAG, "%s", __func__);
	return rpc_slaveif_deinit();
}

static char * get_timestamp(char *str, uint16_t str_size)
{
	if (str && str_size>=MIN_TIMESTAMP_STR_SIZE) {
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		sprintf(str, "%d-%02d-%02d %02d:%02d:%02d > ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		return str;
	}
	return NULL;
}

#ifdef CONFIG_HOST_USES_STATIC_NETIF
static esp_err_t update_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static void update_static_dhcp_dns(rpc_set_dhcp_dns_status_t *dhcp_dns)
{
	esp_netif_t *netif = NULL;
	ip_event_got_ip_t got_ip_evt = {0};
	ESP_LOGI(TAG, "dhcp_dns_status: iface[%s] net_link_up[%u] dhcp{up[%u] ip[%s] nm[%s] gw[%s]} dns{up[%u] ip[%s] type[%u]}",
			dhcp_dns->iface==WIFI_IF_STA? "sta":"ap", dhcp_dns->net_link_up,
			dhcp_dns->dhcp_up, dhcp_dns->dhcp_ip, dhcp_dns->dhcp_nm, dhcp_dns->dhcp_gw,
			dhcp_dns->dns_up, dhcp_dns->dns_ip, dhcp_dns->dns_type);

	if (! dhcp_dns->net_link_up) {
		esp_event_post(IP_EVENT, IP_EVENT_STA_LOST_IP, NULL, 0, HOSTED_BLOCK_MAX);
		return;
	}

	if (dhcp_dns->iface==WIFI_IF_STA) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	} else if (dhcp_dns->iface==WIFI_IF_AP) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
	} else {
		ESP_LOGE(TAG, "unsupported wifi interface yet");
		return;
	}

	esp_netif_ip_info_t ip = {0};
	ip.ip.addr = ipaddr_addr((char*)dhcp_dns->dhcp_ip);
	ip.netmask.addr = ipaddr_addr((char*)dhcp_dns->dhcp_nm);
	ip.gw.addr = ipaddr_addr((char*)dhcp_dns->dhcp_gw);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip));
	ESP_ERROR_CHECK(update_dns_server(netif,
				ipaddr_addr((char*)dhcp_dns->dns_ip), dhcp_dns->dns_type));

	got_ip_evt.ip_info = ip;
	got_ip_evt.ip_changed = 1;
	got_ip_evt.esp_netif = netif;
	esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_evt, sizeof(ip_event_got_ip_t),
		HOSTED_BLOCK_MAX);
}
#endif

#define IS_VALID_USR_EVT_NUM(x) ( ((x)>=RPC_ID__Event_USR1) && \
								 ((x)<=RPC_ID__Event_USR5))? 1: 0
#define GET_END_USR_EVT_NUM(x) ((x)-RPC_ID__Event_USR1+1)

static int usr_event_cb(ctrl_cmd_t * app_event)
{
	RpcId msg_id = app_event->msg_id;

	if (!app_event || (app_event->msg_type != RPC_TYPE__Event)) {
		if (app_event)
			ESP_LOGE(TAG, "Msg type is not event[%u]",app_event->msg_type);
		goto fail_parsing;
	}

	if ((msg_id <= RPC_ID__Event_Base) ||
	    (msg_id >= RPC_ID__Event_Max)) {
		ESP_LOGE(TAG, "Event Msg ID[%u] is not correct",msg_id);
		goto fail_parsing;
	}

	if (!IS_VALID_USR_EVT_NUM(msg_id)) {
		ESP_LOGE(TAG, "Event Msg ID[%u] is incorrect",msg_id);
		goto fail_parsing;
	}

	if (usr_evt_cb)
		usr_evt_cb(GET_END_USR_EVT_NUM(msg_id), &app_event->u.rpc_usr);

#if 0
	switch(msg_id) {
		case RPC_ID__Event_USR1: {
			rpc_usr_t * p_e = &app_event->u.rpc_usr;
			ESP_LOGI(TAG, "Evt USR1: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_e->int_1, p_e->int_2, p_e->uint_1, p_e->uint_2, p_e->data_len, p_e->data);
			break;
		} case RPC_ID__Event_USR2: {
			rpc_usr_t * p_e = &app_event->u.rpc_usr;
			ESP_LOGI(TAG, "Evt USR2: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_e->int_1, p_e->int_2, p_e->uint_1, p_e->uint_2, p_e->data_len, p_e->data);
			break;
		} case RPC_ID__Event_USR3: {
			rpc_usr_t * p_e = &app_event->u.rpc_usr;
			ESP_LOGI(TAG, "Evt USR3: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_e->int_1, p_e->int_2, p_e->uint_1, p_e->uint_2, p_e->data_len, p_e->data);
			break;
		} case RPC_ID__Event_USR4: {
			rpc_usr_t * p_e = &app_event->u.rpc_usr;
			ESP_LOGI(TAG, "Evt USR4: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_e->int_1, p_e->int_2, p_e->uint_1, p_e->uint_2, p_e->data_len, p_e->data);
			break;
		} case RPC_ID__Event_USR5: {
			rpc_usr_t * p_e = &app_event->u.rpc_usr;
			ESP_LOGI(TAG, "Evt USR5: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_e->int_1, p_e->int_2, p_e->uint_1, p_e->uint_2, p_e->data_len, p_e->data);
			break;
		} default: {
			ESP_LOGW(TAG, "Invalid event[%u] to parse\n\r", msg_id);
			break;
		}
	}
#endif

	CLEANUP_RPC(app_event);
	return SUCCESS;

fail_parsing:
	CLEANUP_RPC(app_event);
	return FAILURE;
}

static int rpc_event_callback(ctrl_cmd_t * app_event)
{
	char ts[MIN_TIMESTAMP_STR_SIZE] = {'\0'};

	ESP_LOGV(TAG, "%u",app_event->msg_id);
	if (!app_event || (app_event->msg_type != RPC_TYPE__Event)) {
		if (app_event)
			ESP_LOGE(TAG, "Recvd msg [0x%x] is not event",app_event->msg_type);
		goto fail_parsing;
	}

	if ((app_event->msg_id <= RPC_ID__Event_Base) ||
	    (app_event->msg_id >= RPC_ID__Event_Max)) {
		ESP_LOGE(TAG, "Event Msg ID[0x%x] is not correct",app_event->msg_id);
		goto fail_parsing;
	}

	switch(app_event->msg_id) {

		case RPC_ID__Event_ESPInit: {
			ESP_LOGI(TAG, "Received Slave ESP Init");
			break;
		} case RPC_ID__Event_Heartbeat: {
			ESP_LOGV(TAG, "%s App EVENT: Heartbeat event [%lu]",
				get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE),
					(long unsigned int)app_event->u.e_heartbeat.hb_num);
			break;
		} case RPC_ID__Event_AP_StaConnected: {
			wifi_event_ap_staconnected_t *p_e = &app_event->u.e_wifi_ap_staconnected;

			if (strlen((char*)p_e->mac)) {
				ESP_LOGV(TAG, "%s App EVENT: SoftAP mode: connected station",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE));
				g_h.funcs->_h_wifi_event_handler(WIFI_EVENT_AP_STACONNECTED,
					p_e, sizeof(wifi_event_ap_staconnected_t), HOSTED_BLOCK_MAX);
			}
			break;
		} case RPC_ID__Event_AP_StaDisconnected: {
			wifi_event_ap_stadisconnected_t *p_e = &app_event->u.e_wifi_ap_stadisconnected;
			if (strlen((char*)p_e->mac)) {
				ESP_LOGV(TAG, "%s App EVENT: SoftAP mode: disconnected MAC",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE));
				g_h.funcs->_h_wifi_event_handler(WIFI_EVENT_AP_STADISCONNECTED,
					p_e, sizeof(wifi_event_ap_stadisconnected_t), HOSTED_BLOCK_MAX);
			}
			break;
		} case RPC_ID__Event_StaConnected: {
			ESP_LOGV(TAG, "%s App EVENT: Station mode: Connected",
				get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE));
			wifi_event_sta_connected_t *p_e = &app_event->u.e_wifi_sta_connected;
			g_h.funcs->_h_wifi_event_handler(WIFI_EVENT_STA_CONNECTED,
				p_e, sizeof(wifi_event_sta_connected_t), HOSTED_BLOCK_MAX);
			break;
		} case RPC_ID__Event_StaDisconnected: {
			ESP_LOGV(TAG, "%s App EVENT: Station mode: Disconnected",
				get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE));
			wifi_event_sta_disconnected_t *p_e = &app_event->u.e_wifi_sta_disconnected;
			g_h.funcs->_h_wifi_event_handler(WIFI_EVENT_STA_DISCONNECTED,
				p_e, sizeof(wifi_event_sta_disconnected_t), HOSTED_BLOCK_MAX);
			break;
		} case RPC_ID__Event_WifiEventNoArgs: {
			int wifi_event_id = app_event->u.e_wifi_simple.wifi_event_id;

			switch (wifi_event_id) {

			case WIFI_EVENT_STA_START:
				ESP_LOGV(TAG, "%s App EVENT: WiFi Event[%s]",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE), "WIFI_EVENT_STA_START");
				break;
			case WIFI_EVENT_STA_STOP:
				ESP_LOGV(TAG, "%s App EVENT: WiFi Event[%s]",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE), "WIFI_EVENT_STA_STOP");
				break;

			case WIFI_EVENT_AP_START:
				ESP_LOGI(TAG,"App Event: softap started");
				break;

			case WIFI_EVENT_AP_STOP:
				ESP_LOGI(TAG,"App Event: softap stopped");
				break;

			default:
				ESP_LOGV(TAG, "%s App EVENT: WiFi Event[%x]",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE), wifi_event_id);
				break;
			} /* inner switch case */
			g_h.funcs->_h_wifi_event_handler(wifi_event_id, 0, 0, HOSTED_BLOCK_MAX);

			break;
		} case RPC_ID__Event_StaScanDone: {
			wifi_event_sta_scan_done_t *p_e = &app_event->u.e_wifi_sta_scan_done;
			ESP_LOGV(TAG, "%s App EVENT: StaScanDone",
					get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE));
			ESP_LOGV(TAG, "scan: status: %lu number:%u scan_id:%u", p_e->status, p_e->number, p_e->scan_id);
			g_h.funcs->_h_wifi_event_handler(WIFI_EVENT_SCAN_DONE,
				p_e, sizeof(wifi_event_sta_scan_done_t), HOSTED_BLOCK_MAX);
			break;
		} case RPC_ID__Event_SetDhcpDnsStatus: {
#ifdef CONFIG_HOST_USES_STATIC_NETIF
			rpc_set_dhcp_dns_status_t *p_e = &app_event->u.slave_dhcp_dns_status;
			update_static_dhcp_dns(p_e);
#endif
			//TODO: Need to make modular. RPC events could be handled as new event_base and get handled in os_wrapper
			break;
		} default: {
			ESP_LOGW(TAG, "%s Invalid event[0x%x] to parse\n\r",
				get_timestamp(ts, MIN_TIMESTAMP_STR_SIZE), app_event->msg_id);
			break;
		}
	}
	CLEANUP_RPC(app_event);
	return SUCCESS;

fail_parsing:
	CLEANUP_RPC(app_event);
	return FAILURE;
}

static int process_failed_responses(ctrl_cmd_t *app_msg)
{
	uint8_t request_failed_flag = true;
	int result = app_msg->resp_event_status;

	/* Identify general issue, common for all control requests */
	/* Map results to a matching ESP_ERR_ code */
	switch (app_msg->resp_event_status) {
		case RPC_ERR_REQ_IN_PROG:
			ESP_LOGE(TAG, "Error reported: Command In progress, Please wait");
			break;
		case RPC_ERR_REQUEST_TIMEOUT:
			ESP_LOGE(TAG, "Error reported: Response Timeout");
			break;
		case RPC_ERR_MEMORY_FAILURE:
			ESP_LOGE(TAG, "Error reported: Memory allocation failed");
			break;
		case RPC_ERR_UNSUPPORTED_MSG:
			ESP_LOGE(TAG, "Error reported: Unsupported control msg");
			break;
		case RPC_ERR_INCORRECT_ARG:
			ESP_LOGE(TAG, "Error reported: Invalid or out of range parameter values");
			break;
		case RPC_ERR_PROTOBUF_ENCODE:
			ESP_LOGE(TAG, "Error reported: Protobuf encode failed");
			break;
		case RPC_ERR_PROTOBUF_DECODE:
			ESP_LOGE(TAG, "Error reported: Protobuf decode failed");
			break;
		case RPC_ERR_SET_ASYNC_CB:
			ESP_LOGE(TAG, "Error reported: Failed to set aync callback");
			break;
		case RPC_ERR_TRANSPORT_SEND:
			ESP_LOGE(TAG, "Error reported: Problem while sending data on serial driver");
			break;
		case RPC_ERR_SET_SYNC_SEM:
			ESP_LOGE(TAG, "Error reported: Failed to set sync sem");
			break;
		default:
			request_failed_flag = false;
			break;
	}

	/* if control request failed, no need to proceed for response checking */
	if (request_failed_flag)
		return result;

	/* Identify control request specific issue */
	switch (app_msg->msg_id) {

		case RPC_ID__Resp_OTAEnd:
		case RPC_ID__Resp_OTABegin:
		case RPC_ID__Resp_OTAWrite: {
			/* intentional fallthrough */
			ESP_LOGE(TAG, "OTA procedure failed");
			break;
		} default: {
			ESP_LOGE(TAG, "Failed Control Response");
			break;
		}
	}
	return result;
}


int rpc_unregister_event_callbacks(void)
{
	int ret = SUCCESS;
	int evt = 0;
	for (evt=RPC_ID__Event_Base+1; evt<RPC_ID__Event_Max; evt++) {
		if (CALLBACK_SET_SUCCESS != reset_event_callback(evt) ) {
			ESP_LOGV(TAG, "reset event callback failed for event[%u]", evt);
			ret = FAILURE;
		}
	}
	return ret;
}

int rpc_register_event_callbacks(void)
{
	int ret = SUCCESS;
	int evt = 0;

	event_callback_table_t events[] = {
		{ RPC_ID__Event_ESPInit,                   rpc_event_callback },
#if 0
		{ RPC_ID__Event_Heartbeat,                 rpc_event_callback },
#endif
		{ RPC_ID__Event_AP_StaConnected,           rpc_event_callback },
		{ RPC_ID__Event_AP_StaDisconnected,        rpc_event_callback },
		{ RPC_ID__Event_WifiEventNoArgs,           rpc_event_callback },
		{ RPC_ID__Event_StaScanDone,               rpc_event_callback },
		{ RPC_ID__Event_StaConnected,              rpc_event_callback },
		{ RPC_ID__Event_StaDisconnected,           rpc_event_callback },
		{ RPC_ID__Event_SetDhcpDnsStatus,          rpc_event_callback },
		{ RPC_ID__Event_USR1,                      usr_event_cb },
		{ RPC_ID__Event_USR2,                      usr_event_cb },
		{ RPC_ID__Event_USR3,                      usr_event_cb },
		{ RPC_ID__Event_USR4,                      usr_event_cb },
		{ RPC_ID__Event_USR5,                      usr_event_cb },
	};

	for (evt=0; evt<sizeof(events)/sizeof(event_callback_table_t); evt++) {
		if (CALLBACK_SET_SUCCESS != set_event_callback(events[evt].event, events[evt].fun) ) {
			ESP_LOGE(TAG, "event callback register failed for event[%u]\n\r", events[evt].event);
			ret = FAILURE;
			break;
		}
	}
	return ret;
}

int print_cleanup_usr_resp(ctrl_cmd_t * app_resp)
{
	int response = ESP_FAIL; // default response

	if (!app_resp || (app_resp->msg_type != RPC_TYPE__Resp)) {
		if (app_resp)
			ESP_LOGE(TAG, "Msg type is not response[%u]",app_resp->msg_type);
		goto fail_resp;
	}

	if ((app_resp->msg_id <= RPC_ID__Resp_Base) || (app_resp->msg_id >= RPC_ID__Resp_Max)) {
		ESP_LOGE(TAG, "Response Msg ID[%x] is not correct",app_resp->msg_id);
		goto fail_resp;
	}

	if (app_resp->resp_event_status != SUCCESS) {
		ESP_LOGI(TAG, "%s: err_resp[%"PRId32"] for req[%u]",
				__func__, app_resp->resp_event_status, app_resp->msg_id);
		response = app_resp->resp_event_status;
		goto fail_resp;
	}

	switch(app_resp->msg_id) {

	case RPC_ID__Resp_USR1: {
		/*ESP_LOGI(TAG, "Rcvd_cust_rpc_resp[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			1, app_resp->u.rpc_usr.int_1, app_resp->u.rpc_usr.int_2,
			app_resp->u.rpc_usr.uint_1, app_resp->u.rpc_usr.uint_2,
			app_resp->u.rpc_usr.data_len, app_resp->u.rpc_usr.data);*/
		break;
	} case RPC_ID__Resp_USR2: {
		/*ESP_LOGI(TAG, "Rcvd_cust_rpc_resp[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			2, app_resp->u.rpc_usr.int_1, app_resp->u.rpc_usr.int_2,
			app_resp->u.rpc_usr.uint_1, app_resp->u.rpc_usr.uint_2,
			app_resp->u.rpc_usr.data_len, app_resp->u.rpc_usr.data);*/
		break;
	} case RPC_ID__Resp_USR3: {
		/*ESP_LOGI(TAG, "Rcvd_cust_rpc_resp[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			3, app_resp->u.rpc_usr.int_1, app_resp->u.rpc_usr.int_2,
			app_resp->u.rpc_usr.uint_1, app_resp->u.rpc_usr.uint_2,
			app_resp->u.rpc_usr.data_len, app_resp->u.rpc_usr.data);*/
		break;
	} case RPC_ID__Resp_USR4: {
		/*ESP_LOGI(TAG, "Rcvd_cust_rpc_resp[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			4, app_resp->u.rpc_usr.int_1, app_resp->u.rpc_usr.int_2,
			app_resp->u.rpc_usr.uint_1, app_resp->u.rpc_usr.uint_2,
			app_resp->u.rpc_usr.data_len, app_resp->u.rpc_usr.data);*/
		break;
	} case RPC_ID__Resp_USR5: {
		/*ESP_LOGI(TAG, "Rcvd_cust_rpc_resp[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			5, app_resp->u.rpc_usr.int_1, app_resp->u.rpc_usr.int_2,
			app_resp->u.rpc_usr.uint_1, app_resp->u.rpc_usr.uint_2,
			app_resp->u.rpc_usr.data_len, app_resp->u.rpc_usr.data);*/
		break;
	}

	}
	CLEANUP_RPC(app_resp);
	return ESP_OK;

fail_resp:
	CLEANUP_RPC(app_resp);
	return response;
}

int rpc_rsp_callback(ctrl_cmd_t * app_resp)
{
	int response = ESP_FAIL; // default response

	uint16_t i = 0;
	if (!app_resp || (app_resp->msg_type != RPC_TYPE__Resp)) {
		if (app_resp)
			ESP_LOGE(TAG, "Recvd Msg[0x%x] is not response",app_resp->msg_type);
		goto fail_resp;
	}

	if ((app_resp->msg_id <= RPC_ID__Resp_Base) || (app_resp->msg_id >= RPC_ID__Resp_Max)) {
		ESP_LOGE(TAG, "Response Msg ID[0x%x] is not correct",app_resp->msg_id);
		goto fail_resp;
	}

	if (app_resp->resp_event_status != SUCCESS) {
		response = process_failed_responses(app_resp);
		goto fail_resp;
	}

	switch(app_resp->msg_id) {

	case RPC_ID__Resp_GetMACAddress: {
		ESP_LOGV(TAG, "mac address is [" MACSTR "]", MAC2STR(app_resp->u.wifi_mac.mac));
		break;
	} case RPC_ID__Resp_SetMacAddress : {
		ESP_LOGV(TAG, "MAC address is set");
		break;
	} case RPC_ID__Resp_GetWifiMode : {
		ESP_LOGV(TAG, "wifi mode is : ");
		switch (app_resp->u.wifi_mode.mode) {
			case WIFI_MODE_STA:     ESP_LOGV(TAG, "station");        break;
			case WIFI_MODE_AP:      ESP_LOGV(TAG, "softap");         break;
			case WIFI_MODE_APSTA:   ESP_LOGV(TAG, "station+softap"); break;
			case WIFI_MODE_NULL:    ESP_LOGV(TAG, "none");           break;
			default:                ESP_LOGV(TAG, "unknown");        break;
		}
		break;
	} case RPC_ID__Resp_SetWifiMode : {
		ESP_LOGV(TAG, "wifi mode is set");
		break;
	} case RPC_ID__Resp_WifiSetPs: {
		ESP_LOGV(TAG, "Wifi power save mode set");
		break;
	} case RPC_ID__Resp_WifiGetPs: {
		ESP_LOGV(TAG, "Wifi power save mode is: ");

		switch(app_resp->u.wifi_ps.ps_mode) {
			case WIFI_PS_MIN_MODEM:
				ESP_LOGV(TAG, "Min");
				break;
			case WIFI_PS_MAX_MODEM:
				ESP_LOGV(TAG, "Max");
				break;
			default:
				ESP_LOGV(TAG, "Invalid");
				break;
		}
		break;
	} case RPC_ID__Resp_OTABegin : {
		ESP_LOGV(TAG, "OTA begin success");
		break;
	} case RPC_ID__Resp_OTAWrite : {
		ESP_LOGV(TAG, "OTA write success");
		break;
	} case RPC_ID__Resp_OTAEnd : {
		ESP_LOGV(TAG, "OTA end success");
		break;
	} case RPC_ID__Resp_WifiSetMaxTxPower: {
		ESP_LOGV(TAG, "Set wifi max tx power success");
		break;
	} case RPC_ID__Resp_WifiGetMaxTxPower: {
		ESP_LOGV(TAG, "wifi curr tx power : %d",
				app_resp->u.wifi_tx_power.power);
		break;
	} case RPC_ID__Resp_ConfigHeartbeat: {
		ESP_LOGV(TAG, "Heartbeat operation successful");
		break;
	} case RPC_ID__Resp_WifiScanGetApNum: {
		ESP_LOGV(TAG, "Num Scanned APs: %u",
				app_resp->u.wifi_scan_ap_list.number);
		break;
	} case RPC_ID__Resp_WifiScanGetApRecords: {
		wifi_scan_ap_list_t * p_a = &app_resp->u.wifi_scan_ap_list;
		wifi_ap_record_t *list = p_a->out_list;

		if (!p_a->number) {
			ESP_LOGV(TAG, "No AP info found");
			goto finish_resp;
		}
		ESP_LOGV(TAG, "Num AP records: %u",
				app_resp->u.wifi_scan_ap_list.number);
		if (!list) {
			ESP_LOGV(TAG, "Failed to get scanned AP list");
			goto fail_resp;
		} else {

			ESP_LOGV(TAG, "Number of available APs is %d", p_a->number);
			for (i=0; i<p_a->number; i++) {
				ESP_LOGV(TAG, "%d) ssid \"%s\" bssid \"%s\" rssi \"%d\" channel \"%d\" auth mode \"%d\"",\
						i, list[i].ssid, list[i].bssid, list[i].rssi,
						list[i].primary, list[i].authmode);
			}
		}
		break;
	} case RPC_ID__Resp_USR1: {
		/*rpc_usr_t *p_a = &app_resp->u.rpc_usr;
		ESP_LOGI(TAG, "USR1: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_a->int_1, p_a->int_2, p_a->uint_1, p_a->uint_2, p_a->data_len, p_a->data);*/
		break;
	} case RPC_ID__Resp_USR2: {
		/*rpc_usr_t *p_a = &app_resp->u.rpc_usr;
		ESP_LOGI(TAG, "USR2: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_a->int_1, p_a->int_2, p_a->uint_1, p_a->uint_2, p_a->data_len, p_a->data);*/
		break;
	} case RPC_ID__Resp_USR3: {
		/*rpc_usr_t *p_a = &app_resp->u.rpc_usr;
		ESP_LOGI(TAG, "USR3: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_a->int_1, p_a->int_2, p_a->uint_1, p_a->uint_2, p_a->data_len, p_a->data);*/
		break;
	} case RPC_ID__Resp_USR4: {
		/*rpc_usr_t *p_a = &app_resp->u.rpc_usr;
		ESP_LOGI(TAG, "USR4: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_a->int_1, p_a->int_2, p_a->uint_1, p_a->uint_2, p_a->data_len, p_a->data);*/
		break;
	} case RPC_ID__Resp_USR5: {
		/*rpc_usr_t *p_a = &app_resp->u.rpc_usr;
		ESP_LOGI(TAG, "USR5: int_1:%"PRId32" int_2:%"PRId32" uint_1:%"PRIu32" uint_2:%"PRIu32" data_len:%u data:%s",
					p_a->int_1, p_a->int_2, p_a->uint_1, p_a->uint_2, p_a->data_len, p_a->data);*/
		break;
	}
	case RPC_ID__Resp_WifiInit:
	case RPC_ID__Resp_WifiDeinit:
	case RPC_ID__Resp_WifiStart:
	case RPC_ID__Resp_WifiStop:
	case RPC_ID__Resp_WifiConnect:
	case RPC_ID__Resp_WifiDisconnect:
	case RPC_ID__Resp_WifiGetConfig:
	case RPC_ID__Resp_WifiScanStart:
	case RPC_ID__Resp_WifiScanStop:
	case RPC_ID__Resp_WifiClearApList:
	case RPC_ID__Resp_WifiRestore:
	case RPC_ID__Resp_WifiClearFastConnect:
	case RPC_ID__Resp_WifiDeauthSta:
	case RPC_ID__Resp_WifiStaGetApInfo:
	case RPC_ID__Resp_WifiSetConfig:
	case RPC_ID__Resp_WifiSetStorage:
	case RPC_ID__Resp_WifiSetBandwidth:
	case RPC_ID__Resp_WifiGetBandwidth:
	case RPC_ID__Resp_WifiSetChannel:
	case RPC_ID__Resp_WifiGetChannel:
	case RPC_ID__Resp_WifiSetCountryCode:
	case RPC_ID__Resp_WifiGetCountryCode:
	case RPC_ID__Resp_WifiSetCountry:
	case RPC_ID__Resp_WifiGetCountry:
	case RPC_ID__Resp_WifiApGetStaList:
	case RPC_ID__Resp_WifiApGetStaAid:
	case RPC_ID__Resp_WifiStaGetRssi:
	case RPC_ID__Resp_WifiSetProtocol:
	case RPC_ID__Resp_WifiGetProtocol:
	case RPC_ID__Resp_SetDhcpDnsStatus: {
		/* Intended fallthrough */
		break;
	} default: {
		ESP_LOGE(TAG, "Invalid Response[0x%x] to parse", app_resp->msg_id);
		goto fail_resp;
	}

	} //switch

finish_resp:
	// extract response from app_resp
	response = app_resp->resp_event_status;
	CLEANUP_RPC(app_resp);
	return response;

fail_resp:
	CLEANUP_RPC(app_resp);
	return response;
}

int rpc_get_wifi_mode(void)
{
	/* implemented Asynchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();

	/* register callback for reply */
	req->rpc_rsp_cb = rpc_rsp_callback;

	wifi_get_mode(req);

	return SUCCESS;
}


int rpc_set_wifi_mode(wifi_mode_t mode)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_mode.mode = mode;
	resp = wifi_set_mode(req);

	return rpc_rsp_callback(resp);
}

int rpc_set_wifi_mode_station(void)
{
	return rpc_set_wifi_mode(WIFI_MODE_STA);
}

int rpc_set_wifi_mode_softap(void)
{
	return rpc_set_wifi_mode(WIFI_MODE_AP);
}

int rpc_set_wifi_mode_station_softap(void)
{
	return rpc_set_wifi_mode(WIFI_MODE_APSTA);
}

int rpc_set_wifi_mode_none(void)
{
	return rpc_set_wifi_mode(WIFI_MODE_NULL);
}

int rpc_wifi_get_mac(wifi_interface_t mode, uint8_t out_mac[6])
{
	ctrl_cmd_t *resp = NULL;

	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();

	req->u.wifi_mac.mode = mode;
	resp = wifi_get_mac(req);

	if (resp && resp->resp_event_status == SUCCESS) {

		g_h.funcs->_h_memcpy(out_mac, resp->u.wifi_mac.mac, BSSID_BYTES_SIZE);
		ESP_LOGI(TAG, "%s mac address is [" MACSTR "]",
			mode==WIFI_IF_STA? "sta":"ap", MAC2STR(out_mac));
	}
	return rpc_rsp_callback(resp);
}

int rpc_station_mode_get_mac(uint8_t mac[6])
{
	return rpc_wifi_get_mac(WIFI_MODE_STA, mac);
}

int rpc_wifi_set_mac(wifi_interface_t mode, const uint8_t mac[6])
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_mac.mode = mode;
	g_h.funcs->_h_memcpy(req->u.wifi_mac.mac, mac, BSSID_BYTES_SIZE);

	resp = wifi_set_mac(req);
	return rpc_rsp_callback(resp);
}


int rpc_softap_mode_get_mac_addr(uint8_t mac[6])
{
	return rpc_wifi_get_mac(WIFI_MODE_AP, mac);
}

//int rpc_async_station_mode_connect(char *ssid, char *pwd, char *bssid,
//		int is_wpa3_supported, int listen_interval)
//{
//	/* implemented Asynchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//
//	strcpy((char *)&req->u.hosted_ap_config.ssid, ssid);
//	strcpy((char *)&req->u.hosted_ap_config.pwd, pwd);
//	strcpy((char *)&req->u.hosted_ap_config.bssid, bssid);
//	req->u.hosted_ap_config.is_wpa3_supported = is_wpa3_supported;
//	req->u.hosted_ap_config.listen_interval = listen_interval;
//
//	/* register callback for handling reply asynch-ly */
//	req->rpc_rsp_cb = rpc_rsp_callback;
//
//	wifi_connect_ap(req);
//
//	return SUCCESS;
//}
//
//int rpc_station_mode_connect(char *ssid, char *pwd, char *bssid,
//		int is_wpa3_supported, int listen_interval)
//{
//	/* implemented Asynchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	strcpy((char *)&req->u.hosted_ap_config.ssid, ssid);
//	strcpy((char *)&req->u.hosted_ap_config.pwd, pwd);
//	strcpy((char *)&req->u.hosted_ap_config.bssid, bssid);
//	req->u.hosted_ap_config.is_wpa3_supported = is_wpa3_supported;
//	req->u.hosted_ap_config.listen_interval = listen_interval;
//
//	resp = wifi_connect_ap(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_station_mode_get_info(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_get_ap_config(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_get_available_wifi(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	req->rsp_timeout_sec = 300;
//
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_ap_scan_list(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_station_mode_disconnect(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_disconnect_ap(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_softap_mode_start(char *ssid, char *pwd, int channel,
//		int encryption_mode, int max_conn, int ssid_hidden, int bw)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	strncpy((char *)&req->u.wifi_softap_config.ssid,
//			ssid, MAX_MAC_STR_LEN-1);
//	strncpy((char *)&req->u.wifi_softap_config.pwd,
//			pwd, MAX_MAC_STR_LEN-1);
//	req->u.wifi_softap_config.channel = channel;
//	req->u.wifi_softap_config.encryption_mode = encryption_mode;
//	req->u.wifi_softap_config.max_connections = max_conn;
//	req->u.wifi_softap_config.ssid_hidden = ssid_hidden;
//	req->u.wifi_softap_config.bandwidth = bw;
//
//	resp = wifi_start_softap(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_softap_mode_get_info(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_get_softap_config(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_softap_mode_connected_clients_info(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_get_softap_connected_station_list(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_softap_mode_stop(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//
//	resp = wifi_stop_softap(req);
//
//	return rpc_rsp_callback(resp);
//}

int rpc_set_wifi_power_save_mode(int psmode)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_ps.ps_mode = psmode;
	resp = wifi_set_power_save_mode(req);

	return rpc_rsp_callback(resp);
}

int rpc_set_wifi_power_save_mode_max(void)
{
	return rpc_set_wifi_power_save_mode(WIFI_PS_MAX_MODEM);
}

int rpc_set_wifi_power_save_mode_min(void)
{
	return rpc_set_wifi_power_save_mode(WIFI_PS_MIN_MODEM);
}

int rpc_get_wifi_power_save_mode(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_get_power_save_mode(req);

	return rpc_rsp_callback(resp);
}

//int rpc_reset_vendor_specific_ie(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//	char *data = "Example vendor IE data";
//
//	char *v_data = (char*)g_h.funcs->_h_calloc(1, strlen(data));
//	if (!v_data) {
//		ESP_LOGE(TAG, "Failed to allocate memory \n");
//		return FAILURE;
//	}
//	g_h.funcs->_h_memcpy(v_data, data, strlen(data));
//
//	req->u.wifi_softap_vendor_ie.enable = false;
//	req->u.wifi_softap_vendor_ie.type   = WIFI_VND_IE_TYPE_BEACON;
//	req->u.wifi_softap_vendor_ie.idx    = WIFI_VND_IE_ID_0;
//	req->u.wifi_softap_vendor_ie.vnd_ie.element_id = WIFI_VENDOR_IE_ELEMENT_ID;
//	req->u.wifi_softap_vendor_ie.vnd_ie.length = strlen(data)+OFFSET;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[0] = VENDOR_OUI_0;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[1] = VENDOR_OUI_1;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[2] = VENDOR_OUI_2;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui_type = VENDOR_OUI_TYPE;
//	req->u.wifi_softap_vendor_ie.vnd_ie.payload = (uint8_t *)v_data;
//	//req->u.wifi_softap_vendor_ie.vnd_ie.payload_len = strlen(data);
//
//	req->app_free_buff_func = g_h.funcs->_h_free;
//	req->app_free_buff_hdl = v_data;
//
//	resp = wifi_set_vendor_specific_ie(req);
//
//	return rpc_rsp_callback(resp);
//}
//
//int rpc_set_vendor_specific_ie(void)
//{
//	/* implemented synchronous */
//	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
//	ctrl_cmd_t *resp = NULL;
//	char *data = "Example vendor IE data";
//
//	char *v_data = (char*)g_h.funcs->_h_calloc(1, strlen(data));
//	if (!v_data) {
//		ESP_LOGE(TAG, "Failed to allocate memory \n");
//		return FAILURE;
//	}
//	g_h.funcs->_h_memcpy(v_data, data, strlen(data));
//
//	req->u.wifi_softap_vendor_ie.enable = true;
//	req->u.wifi_softap_vendor_ie.type   = WIFI_VND_IE_TYPE_BEACON;
//	req->u.wifi_softap_vendor_ie.idx    = WIFI_VND_IE_ID_0;
//	req->u.wifi_softap_vendor_ie.vnd_ie.element_id = WIFI_VENDOR_IE_ELEMENT_ID;
//	req->u.wifi_softap_vendor_ie.vnd_ie.length = strlen(data)+OFFSET;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[0] = VENDOR_OUI_0;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[1] = VENDOR_OUI_1;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui[2] = VENDOR_OUI_2;
//	req->u.wifi_softap_vendor_ie.vnd_ie.vendor_oui_type = VENDOR_OUI_TYPE;
//	req->u.wifi_softap_vendor_ie.vnd_ie.payload = (uint8_t *)v_data;
//	//req->u.wifi_softap_vendor_ie.vnd_ie.payload_len = strlen(data);
//
//	req->app_free_buff_func = g_h.funcs->_h_free;
//	req->app_free_buff_hdl = v_data;
//
//	resp = wifi_set_vendor_specific_ie(req);
//
//	return rpc_rsp_callback(resp);
//}

int rpc_ota_begin(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	/* OTA begin takes some time to clear the partition */
	req->rsp_timeout_sec = OTA_BEGIN_RSP_TIMEOUT_SEC;

	resp = ota_begin(req);

	return rpc_rsp_callback(resp);
}

int rpc_ota_write(uint8_t* ota_data, uint32_t ota_data_len)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.ota_write.ota_data = ota_data;
	req->u.ota_write.ota_data_len = ota_data_len;

	resp = ota_write(req);

	return rpc_rsp_callback(resp);
}

int rpc_ota_end(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = ota_end(req);

	return rpc_rsp_callback(resp);
}

#if !OTA_FROM_WEB_URL
/* This assumes full slave binary is present locally */
int rpc_ota(char* image_path)
{
	FILE* f = NULL;
	char ota_chunk[CHUNK_SIZE] = {0};
	int ret = rpc_ota_begin();
	if (ret == SUCCESS) {
		f = fopen(image_path,"rb");
		if (f == NULL) {
			ESP_LOGE(OTA_TAG, "Failed to open file %s", image_path);
			return FAILURE;
		} else {
			ESP_LOGV(OTA_TAG, "Success in opening %s file", image_path);
		}
		while (!feof(f)) {
			fread(&ota_chunk, CHUNK_SIZE, 1, f);
			ret = rpc_ota_write((uint8_t* )&ota_chunk, CHUNK_SIZE);
			if (ret) {
				ESP_LOGE(OTA_TAG, "OTA procedure failed!!");
				/* TODO: Do we need to do OTA end irrespective of success/failure? */
				rpc_ota_end();
				return FAILURE;
			}
		}
		ret = rpc_ota_end();
		if (ret) {
			return FAILURE;
		}
	} else {
		return FAILURE;
	}
	ESP_LOGE(OTA_TAG, "ESP32 will restart after 5 sec");
	return SUCCESS;
	ESP_LOGE(OTA_TAG, "For OTA, user need to integrate HTTP client lib and then invoke OTA");
	return FAILURE;
}
#else
uint8_t http_err = 0;
static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
	switch(evt->event_id) {

	case HTTP_EVENT_ERROR:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_ERROR");
		http_err = 1;
		break;
	case HTTP_EVENT_ON_CONNECTED:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_ON_CONNECTED");
		break;
	case HTTP_EVENT_HEADER_SENT:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_HEADER_SENT");
		break;
	case HTTP_EVENT_ON_HEADER:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
		break;
	case HTTP_EVENT_ON_DATA:
		/* Nothing to handle here */
		break;
	case HTTP_EVENT_ON_FINISH:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_ON_FINISH");
		break;
	case HTTP_EVENT_DISCONNECTED:
		ESP_LOGI(OTA_TAG, "HTTP_EVENT_DISCONNECTED");
		break;
	case HTTP_EVENT_REDIRECT:
		ESP_LOGW(TAG, "HTTP_EVENT_REDIRECT");
		break;

	default:
		ESP_LOGW(TAG, "received HTTP event %u ignored", evt->event_id);
		break;
	}

	return ESP_OK;
}

static esp_err_t _rpc_ota(const char* image_url)
{
	uint8_t *ota_chunk = NULL;
	esp_err_t err = 0;
	int data_read = 0;
	int ota_failed = 0;

	if (image_url == NULL) {
		ESP_LOGE(TAG, "Invalid image URL");
		return FAILURE;
	}

	/* Initialize HTTP client configuration */
	esp_http_client_config_t config = {
		.url = image_url,
		.timeout_ms = 5000,
		.event_handler = http_client_event_handler,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	ESP_LOGI(OTA_TAG, "http_open");
	if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
		ESP_LOGE(OTA_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		ESP_LOGE(OTA_TAG, "Check if URL is correct and connectable: %s", image_url);
		esp_http_client_cleanup(client);
		return FAILURE;
	}

	if (http_err) {
		ESP_LOGE(TAG, "Exiting OTA, due to http failure");
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		http_err = 0;
		return FAILURE;
	}

	ESP_LOGI(OTA_TAG, "http_fetch_headers");
	int64_t content_length = esp_http_client_fetch_headers(client);
	if (content_length <= 0) {
		ESP_LOGE(OTA_TAG, "HTTP client fetch headers failed");
		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		return FAILURE;
	}

	ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
			esp_http_client_get_status_code(client),
			esp_http_client_get_content_length(client));

	ESP_LOGW(OTA_TAG, "********* Started Slave OTA *******************");
	ESP_LOGI(TAG, "*** Please wait for 5 mins to let slave OTA complete ***");

	ESP_LOGI(OTA_TAG, "Preparing OTA");
	if ((err = rpc_ota_begin())) {
		ESP_LOGW(OTA_TAG, "********* Slave OTA Begin Failed *******************");
		ESP_LOGI(OTA_TAG, "esp_ota_begin failed, error=%s", esp_err_to_name(err));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		return FAILURE;
	}

	ota_chunk = (uint8_t*)g_h.funcs->_h_calloc(1, CHUNK_SIZE);
	if (!ota_chunk) {
		ESP_LOGE(OTA_TAG, "Failed to allocate otachunk mem\n");
		err = -ENOMEM;
	}

	ESP_LOGI(OTA_TAG, "Starting OTA");

	if (!err) {
		while ((data_read = esp_http_client_read(client, (char*)ota_chunk, CHUNK_SIZE)) > 0) {

			ESP_LOGV(OTA_TAG, "Read image length %d", data_read);
			if ((err = rpc_ota_write(ota_chunk, data_read))) {
				ESP_LOGI(OTA_TAG, "rpc_ota_write failed");
				ota_failed = err;
				break;
			}
		}
	}

	g_h.funcs->_h_free(ota_chunk);
	if (err) {
		ESP_LOGW(OTA_TAG, "********* Slave OTA Failed *******************");
		ESP_LOGI(OTA_TAG, "esp_ota_write failed, error=%s", esp_err_to_name(err));
		ota_failed = -1;
	}

	if (data_read < 0) {
		ESP_LOGE(OTA_TAG, "Error: SSL data read error");
		ota_failed = -2;
	}

	if ((err = rpc_ota_end())) {
		ESP_LOGW(OTA_TAG, "********* Slave OTA Failed *******************");
		ESP_LOGI(OTA_TAG, "esp_ota_end failed, error=%s", esp_err_to_name(err));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		ota_failed = err;
		return FAILURE;
	}

	esp_http_client_cleanup(client);
	if (!ota_failed) {
		ESP_LOGW(OTA_TAG, "********* Slave OTA Complete *******************");
		ESP_LOGI(OTA_TAG, "OTA Successful, Slave will restart in while");
		ESP_LOGE(TAG, "Need to restart host after slave OTA is complete, to avoid sync issues");
		sleep(5);
		ESP_LOGE(OTA_TAG, "********* Restarting Host **********************");
		restart_after_slave_ota = 1;
		esp_restart();
	}
	return ota_failed;
}

esp_err_t rpc_ota(const char* image_url)
{
	uint8_t ota_retry = 2;
	int ret = 0;

	do {
		ret = _rpc_ota(image_url);

		ota_retry--;
		if (ota_retry && ret)
			ESP_LOGI(OTA_TAG, "OTA retry left: %u\n", ota_retry);
	} while (ota_retry && ret);

	return ret;
}
#endif

int rpc_wifi_set_max_tx_power(int8_t in_power)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_tx_power.power = in_power;
	resp = wifi_set_max_tx_power(req);

	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_max_tx_power(int8_t *power)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_get_max_tx_power(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		*power = resp->u.wifi_tx_power.power;
	}
	return rpc_rsp_callback(resp);
}

int rpc_config_heartbeat(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *resp = NULL;
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	req->u.e_heartbeat.enable = YES;
	req->u.e_heartbeat.duration = HEARTBEAT_DURATION_SEC;

	resp = config_heartbeat(req);

	return rpc_rsp_callback(resp);
}

int rpc_disable_heartbeat(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *resp = NULL;
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	req->u.e_heartbeat.enable = NO;

	resp = config_heartbeat(req);

	return rpc_rsp_callback(resp);
}

int rpc_wifi_init(const wifi_init_config_t *arg)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->rsp_timeout_sec = WIFI_INIT_RSP_TIMEOUT_SEC;

	if (!arg)
		return FAILURE;

	g_h.funcs->_h_memcpy(&req->u.wifi_init_config, (void*)arg, sizeof(wifi_init_config_t));
	resp = wifi_init(req);

	return rpc_rsp_callback(resp);
}

int rpc_wifi_deinit(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_deinit(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_mode(wifi_mode_t mode)
{
	return rpc_set_wifi_mode(mode);
}

int rpc_wifi_get_mode(wifi_mode_t* mode)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!mode)
		return FAILURE;

	resp = wifi_get_mode(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		*mode = resp->u.wifi_mode.mode;
	}

	return SUCCESS;
}

int rpc_wifi_start(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_start(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_stop(void)
{
	if (restart_after_slave_ota)
		return 0;

	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_stop(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_connect(void)
{
#if 1
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_connect(req);
	return rpc_rsp_callback(resp);
	return 0;
#else
	/* implemented asynchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->rpc_rsp_cb = rpc_rsp_callback;
	ESP_LOGE(TAG, "Async call registerd: %p", rpc_rsp_callback);

	wifi_connect(req);

	return SUCCESS;
#endif
}

int rpc_wifi_disconnect(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_disconnect(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!conf)
		return FAILURE;

	g_h.funcs->_h_memcpy(&req->u.wifi_config.u, conf, sizeof(wifi_config_t));

	req->u.wifi_config.iface = interface;
	resp = wifi_set_config(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!conf)
		return FAILURE;

	req->u.wifi_config.iface = interface;

	resp = wifi_get_config(req);

	g_h.funcs->_h_memcpy(conf, &resp->u.wifi_config.u, sizeof(wifi_config_t));

	return rpc_rsp_callback(resp);
}

int rpc_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (config) {
		g_h.funcs->_h_memcpy(&req->u.wifi_scan_config.cfg, config, sizeof(wifi_scan_config_t));
		req->u.wifi_scan_config.cfg_set = 1;
	}

	req->u.wifi_scan_config.block = block;

	resp = wifi_scan_start(req);

	return rpc_rsp_callback(resp);
}

int rpc_wifi_scan_stop(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;
	ESP_LOGV(TAG, "scan stop");

	resp = wifi_scan_stop(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_scan_get_ap_num(uint16_t *number)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!number)
		return FAILURE;

	resp = wifi_scan_get_ap_num(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		*number = resp->u.wifi_scan_ap_list.number;
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!number || !*number || !ap_records)
		return FAILURE;

	g_h.funcs->_h_memset(ap_records, 0, (*number)*sizeof(wifi_ap_record_t));

	req->u.wifi_scan_ap_list.number = *number;
	resp = wifi_scan_get_ap_records(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		ESP_LOGV(TAG, "num: %u",resp->u.wifi_scan_ap_list.number);

		g_h.funcs->_h_memcpy(ap_records, resp->u.wifi_scan_ap_list.out_list,
				resp->u.wifi_scan_ap_list.number * sizeof(wifi_ap_record_t));
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_clear_ap_list(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_clear_ap_list(req);
	return rpc_rsp_callback(resp);
}


int rpc_wifi_restore(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_restore(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_clear_fast_connect(void)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_clear_fast_connect(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_deauth_sta(uint16_t aid)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_deauth_sta.aid = aid;
	resp = wifi_deauth_sta(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!ap_info)
		return FAILURE;

	resp = wifi_sta_get_ap_info(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		g_h.funcs->_h_memcpy(ap_info, resp->u.wifi_scan_ap_list.out_list,
				sizeof(wifi_ap_record_t));
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_ps(wifi_ps_type_t type)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (type > WIFI_PS_MAX_MODEM)
		return FAILURE;

	req->u.wifi_ps.ps_mode = type;

	resp = wifi_set_ps(req);

	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_ps(wifi_ps_type_t *type)
{
	if (!type)
		return FAILURE;

	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!type)
		return FAILURE;

	resp = wifi_get_ps(req);

	*type = resp->u.wifi_ps.ps_mode;

	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_storage(wifi_storage_t storage)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_storage = storage;
	resp = wifi_set_storage(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_bandwidth.ifx = ifx;
	req->u.wifi_bandwidth.bw = bw;
	resp = wifi_set_bandwidth(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!bw)
		return FAILURE;

	req->u.wifi_bandwidth.ifx = ifx;
	resp = wifi_get_bandwidth(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		*bw = resp->u.wifi_bandwidth.bw;
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_channel(uint8_t primary, wifi_second_chan_t second)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_channel.primary = primary;
	req->u.wifi_channel.second = second;
	resp = wifi_set_channel(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if ((!primary) || (!second))
		return FAILURE;

	resp = wifi_get_channel(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		*primary = resp->u.wifi_channel.primary;
		*second = resp->u.wifi_channel.second;
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_country_code(const char *country, bool ieee80211d_enabled)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!country)
		return FAILURE;

	memcpy(&req->u.wifi_country_code.cc[0], country, sizeof(req->u.wifi_country_code.cc));
	req->u.wifi_country_code.ieee80211d_enabled = ieee80211d_enabled;
	resp = wifi_set_country_code(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_country_code(char *country)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!country)
		return FAILURE;

	resp = wifi_get_country_code(req);

	if (resp && resp->resp_event_status == SUCCESS) {
		memcpy(country, &resp->u.wifi_country_code.cc[0], sizeof(resp->u.wifi_country_code.cc));
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_country(const wifi_country_t *country)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!country)
		return FAILURE;

	memcpy(&req->u.wifi_country.cc[0], &country->cc[0], sizeof(country->cc));
	req->u.wifi_country.schan        = country->schan;
	req->u.wifi_country.nchan        = country->nchan;
	req->u.wifi_country.max_tx_power = country->max_tx_power;
	req->u.wifi_country.policy       = country->policy;

	resp = wifi_set_country(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_country(wifi_country_t *country)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!country)
		return FAILURE;

	resp = wifi_get_country(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		memcpy(&country->cc[0], &resp->u.wifi_country.cc[0], sizeof(resp->u.wifi_country.cc));
		country->schan        = resp->u.wifi_country.schan;
		country->nchan        = resp->u.wifi_country.nchan;
		country->max_tx_power = resp->u.wifi_country.max_tx_power;
		country->policy       = resp->u.wifi_country.policy;
	}
	return rpc_rsp_callback(resp);
}

int rpc_wifi_ap_get_sta_list(wifi_sta_list_t *sta)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!sta)
		return FAILURE;

	resp = wifi_ap_get_sta_list(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		for (int i = 0; i < ESP_WIFI_MAX_CONN_NUM; i++) {
			memcpy(sta->sta[i].mac, resp->u.wifi_ap_sta_list.sta[i].mac, 6);
			sta->sta[i].rssi = resp->u.wifi_ap_sta_list.sta[i].rssi;
			sta->sta[i].phy_11b = resp->u.wifi_ap_sta_list.sta[i].phy_11b;
			sta->sta[i].phy_11g = resp->u.wifi_ap_sta_list.sta[i].phy_11g;
			sta->sta[i].phy_11n = resp->u.wifi_ap_sta_list.sta[i].phy_11n;
			sta->sta[i].phy_lr = resp->u.wifi_ap_sta_list.sta[i].phy_lr;
			sta->sta[i].phy_11ax = resp->u.wifi_ap_sta_list.sta[i].phy_11ax;
			sta->sta[i].is_mesh_child = resp->u.wifi_ap_sta_list.sta[i].is_mesh_child;
			sta->sta[i].reserved = resp->u.wifi_ap_sta_list.sta[i].reserved;

		}
	}
	sta->num = resp->u.wifi_ap_sta_list.num;

	return rpc_rsp_callback(resp);
}

int rpc_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!mac || !aid)
		return FAILURE;

	memcpy(&req->u.wifi_ap_get_sta_aid.mac[0], &mac[0], MAC_SIZE_BYTES);

	resp = wifi_ap_get_sta_aid(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		*aid = resp->u.wifi_ap_get_sta_aid.aid;
	}

	return rpc_rsp_callback(resp);
}

int rpc_wifi_sta_get_rssi(int *rssi)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	if (!rssi)
		return FAILURE;

	resp = wifi_sta_get_rssi(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		*rssi = resp->u.wifi_sta_get_rssi.rssi;
	}

	return rpc_rsp_callback(resp);
}

int rpc_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	req->u.wifi_protocol.ifx = ifx;
	req->u.wifi_protocol.protocol_bitmap = protocol_bitmap;

	resp = wifi_set_protocol(req);
	return rpc_rsp_callback(resp);
}

int rpc_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
	/* implemented synchronous */
	if (!protocol_bitmap)
		return FAILURE;

	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	resp = wifi_get_protocol(req);
	if (resp && resp->resp_event_status == SUCCESS) {
		*protocol_bitmap = resp->u.wifi_protocol.protocol_bitmap;
	}

	return rpc_rsp_callback(resp);
}

esp_err_t rpc_set_dhcp_dns_status(wifi_interface_t ifx, uint8_t link_up,
		uint8_t dhcp_up, char *dhcp_ip, char *dhcp_nm, char *dhcp_gw,
		uint8_t dns_up, char *dns_ip, uint8_t dns_type)
{
	/* implemented synchronous */
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
	ctrl_cmd_t *resp = NULL;

	ESP_LOGI(TAG, "iface:%u link_up:%u dhcp_up:%u dns_up:%u dns_type:%u",
			ifx, link_up, dhcp_up, dns_up, dns_type);
	ESP_LOGI(TAG, "dhcp ip:%s nm:%s gw:%s dns ip:%s",
			dhcp_ip, dhcp_nm, dhcp_gw, dns_ip);
	req->u.slave_dhcp_dns_status.iface = ifx;
	req->u.slave_dhcp_dns_status.net_link_up = link_up;
	req->u.slave_dhcp_dns_status.dhcp_up = dhcp_up;
	req->u.slave_dhcp_dns_status.dns_up = dns_up;
	req->u.slave_dhcp_dns_status.dns_type = dns_type;

	if (dhcp_ip)
		strlcpy((char *)req->u.slave_dhcp_dns_status.dhcp_ip, dhcp_ip, 64);
	if (dhcp_nm)
		strlcpy((char *)req->u.slave_dhcp_dns_status.dhcp_nm, dhcp_nm, 64);
	if (dhcp_gw)
		strlcpy((char *)req->u.slave_dhcp_dns_status.dhcp_gw, dhcp_gw, 64);

	if (dns_ip)
		strlcpy((char *)req->u.slave_dhcp_dns_status.dns_ip, dns_ip, 64);


	resp = set_slave_dhcp_dns_status(req);
	return rpc_rsp_callback(resp);
}

esp_err_t rpc_send_usr_request(uint8_t usr_req_num, rpc_usr_t *usr_req, rpc_usr_t *usr_resp)
{
	ctrl_cmd_t *req = RPC_DEFAULT_REQ();
    ctrl_cmd_t *resp = NULL;
	int ret = ESP_OK;
	ctrl_cmd_t *(*ll_usr_req_handler) (ctrl_cmd_t *req) = NULL;

	if (!usr_req || !usr_resp) {
		ESP_LOGE(TAG, "one of usr_req[%p] usr_resp[%p] is not valid memory", usr_req, usr_resp);
		return ESP_FAIL;
	}


	/*ESP_LOGI(TAG, "Send_cust_rpc_req[%u] Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			usr_req_num, usr_req->int_1, usr_req->int_2, usr_req->uint_1, usr_req->uint_2, usr_req->data_len, usr_req->data_len? (char*)usr_req->data: "null");*/

	memcpy(&req->u.rpc_usr, usr_req, sizeof(rpc_usr_t));

	switch (usr_req_num)
	{
		case 1:
			ll_usr_req_handler = rpc_usr1_req_resp;
			break;
		case 2:
			ll_usr_req_handler = rpc_usr2_req_resp;
			break;
		case 3:
			ll_usr_req_handler = rpc_usr3_req_resp;
			break;
		case 4:
			ll_usr_req_handler = rpc_usr4_req_resp;
			break;
		case 5:
			ll_usr_req_handler = rpc_usr5_req_resp;
			break;
		default:
			ESP_LOGE(TAG, "unhandled usr_req[%u]", usr_req_num);
			return ESP_FAIL;
	}

	if (ll_usr_req_handler)
		resp = ll_usr_req_handler(req);

	if (!resp) {
		ESP_LOGI(TAG, "NULL response");
		return ESP_FAIL;
	}

	ret = resp->resp_event_status;
	memcpy(usr_resp, &resp->u.rpc_usr, sizeof(rpc_usr_t));
	print_cleanup_usr_resp(resp);
	return ret;
}

esp_err_t rpc_register_usr_event_callback( void (*usr_evt_cb_a)(uint8_t usr_evt_num, rpc_usr_t *usr_evt))
{
	usr_evt_cb = usr_evt_cb_a;
	return 0;
}
