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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_private/wifi.h"
#include "slave_control.h"
#include "esp_hosted_rpc.pb-c.h"
#include "esp_ota_ops.h"
#include "rpc_common.h"
#include "adapter.h"
#include "esp_check.h"
#include "lwip/inet.h"
#include "host_power_save.h"
#include "esp_wifi.h"

#define MAC_STR_LEN                 17
#define MAC2STR(a)                  (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR                      "%02x:%02x:%02x:%02x:%02x:%02x"
#define SSID_LENGTH                 32
#define PASSWORD_LENGTH             64
#define MIN_TX_POWER                8
#define MAX_TX_POWER                84

/* Bits for wifi connect event */
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1
#define WIFI_NO_AP_FOUND_BIT        BIT2
#define WIFI_WRONG_PASSWORD_BIT     BIT3
#define WIFI_HOST_REQUEST_BIT       BIT4

#define MAX_STA_CONNECT_ATTEMPTS    5

#define TIMEOUT_IN_MIN              (60*TIMEOUT_IN_SEC)
#define TIMEOUT_IN_HOUR             (60*TIMEOUT_IN_MIN)
#define TIMEOUT                     (2*TIMEOUT_IN_MIN)
#define RESTART_TIMEOUT             (5*TIMEOUT_IN_SEC)

#define MIN_HEARTBEAT_INTERVAL      (10)
#define MAX_HEARTBEAT_INTERVAL      (60*60)

#define mem_free(x)             \
    {                           \
        if (x) {                \
            free(x);            \
            x = NULL;           \
        }                       \
    }


#ifdef CONFIG_SLAVE_LWIP_ENABLED

typedef struct {
    int iface;
    int net_link_up;
    int dhcp_up;
    uint8_t dhcp_ip[64];
    uint8_t dhcp_nm[64];
    uint8_t dhcp_gw[64];
    int dns_up;
    uint8_t dns_ip[64];
    int dns_type;
} rpc_set_dhcp_dns_status_t;

rpc_set_dhcp_dns_status_t s2h_dhcp_dns;

#endif


typedef struct esp_rpc_cmd {
	int req_num;
	esp_err_t (*command_handler)(Rpc *req,
			Rpc *resp, void *priv_data);
} esp_rpc_req_t;


static const char *TAG = "slave_ctrl";
static TimerHandle_t handle_heartbeat_task;
static uint32_t hb_num;

/* FreeRTOS event group to signal when we are connected*/
static esp_event_handler_instance_t instance_any_id;
#ifdef CONFIG_SLAVE_LWIP_ENABLED
static esp_event_handler_instance_t instance_ip;
#endif

static esp_ota_handle_t handle;
const esp_partition_t* update_partition = NULL;
static int ota_msg = 0;

extern esp_err_t wlan_sta_rx_callback(void *buffer, uint16_t len, void *eb);
extern esp_err_t wlan_ap_rx_callback(void *buffer, uint16_t len, void *eb);

extern volatile uint8_t station_connected;
extern volatile uint8_t station_got_ip;
extern volatile uint8_t softap_started;
uint16_t sta_connect_retry;
uint8_t wifi_config_modified;
static wifi_event_sta_connected_t lkg_sta_connected_event = {0};
//static ip_event_got_ip_t lkg_sta_got_ip_event = {0};

/* OTA end timer callback */
void vTimerCallback( TimerHandle_t xTimer )
{
	xTimerDelete(xTimer, 0);
	esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
	esp_restart();
}

/* Function returns mac address of station/softap */
static esp_err_t req_wifi_get_mac(Rpc *req,
		Rpc *resp, void *priv_data)
{
	uint8_t mac[BSSID_BYTES_SIZE] = {0};

	RPC_TEMPLATE_SIMPLE(RpcRespGetMacAddress, resp_get_mac_address,
			RpcReqGetMacAddress, req_get_mac_address,
			rpc__resp__get_mac_address__init);

	RPC_RET_FAIL_IF(esp_wifi_get_mac(req->req_get_mac_address->mode, mac));

	ESP_LOGI(TAG,"mac [" MACSTR "]", MAC2STR(mac));

	RPC_RESP_COPY_BYTES_SRC_UNCHECKED(resp_payload->mac, mac, BSSID_BYTES_SIZE);

	ESP_LOGD(TAG, "resp mac [" MACSTR "]", MAC2STR(resp_payload->mac.data));

	return ESP_OK;
}

/* Function returns wifi mode */
static esp_err_t req_wifi_get_mode(Rpc *req,
		Rpc *resp, void *priv_data)
{
	wifi_mode_t mode = 0;

	RPC_TEMPLATE_SIMPLE(RpcRespGetMode, resp_get_wifi_mode,
			RpcReqGetMode, req_get_wifi_mode,
			rpc__resp__get_mode__init);

	RPC_RET_FAIL_IF(esp_wifi_get_mode(&mode));

	resp_payload->mode = mode;

	return ESP_OK;
}

/* Function sets wifi mode */
static esp_err_t req_wifi_set_mode(Rpc *req,
		Rpc *resp, void *priv_data)
{
	wifi_mode_t num = 0;
	wifi_mode_t cur_mode = WIFI_MODE_NULL;

	RPC_TEMPLATE(RpcRespSetMode, resp_set_wifi_mode,
			RpcReqSetMode, req_set_wifi_mode,
			rpc__resp__set_mode__init);

	num = req_payload->mode;

	RPC_RET_FAIL_IF(esp_wifi_get_mode(&cur_mode));

	if (cur_mode != num) {
		RPC_RET_FAIL_IF(esp_wifi_set_mode(num));
	}

	return ESP_OK;
}

/* Function sets MAC address for station/softap */
static esp_err_t req_wifi_set_mac(Rpc *req,
		Rpc *resp, void *priv_data)
{
	uint8_t * mac = NULL;

	RPC_TEMPLATE(RpcRespSetMacAddress, resp_set_mac_address,
			RpcReqSetMacAddress, req_set_mac_address,
			rpc__resp__set_mac_address__init);

	if (!req_payload->mac.data || (req_payload->mac.len != BSSID_BYTES_SIZE)) {
		ESP_LOGE(TAG, "Invalid MAC address data or len: %d", req->req_set_mac_address->mac.len);
		resp_payload->resp = ESP_ERR_INVALID_ARG;
		goto err;
	}

	mac = req_payload->mac.data;
	ESP_LOGI(TAG, "Set mac: " MACSTR, MAC2STR(mac));

	RPC_RET_FAIL_IF(esp_wifi_set_mac(req_payload->mode, mac));
err:
	return ESP_OK;
}

/* Function sets power save mode */
static esp_err_t req_wifi_set_ps(Rpc *req,
		Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespSetPs, resp_wifi_set_ps,
			RpcReqSetPs, req_wifi_set_ps,
			rpc__resp__set_ps__init);
	RPC_RET_FAIL_IF(esp_wifi_set_ps(req_payload->type));
	return ESP_OK;
}

/* Function returns current power save mode */
static esp_err_t req_wifi_get_ps(Rpc *req,
		Rpc *resp, void *priv_data)
{
	wifi_ps_type_t ps_type = 0;

	RPC_TEMPLATE_SIMPLE(RpcRespGetPs, resp_wifi_get_ps,
			RpcReqGetPs, req_wifi_get_ps,
			rpc__resp__get_ps__init);
	RPC_RET_FAIL_IF(esp_wifi_get_ps(&ps_type));
	resp_payload->type = ps_type;
	return ESP_OK;
}

/* Function OTA begin */
static esp_err_t req_ota_begin_handler (Rpc *req,
		Rpc *resp, void *priv_data)
{
	esp_err_t ret = ESP_OK;
	RpcRespOTABegin *resp_payload = NULL;

	if (!req || !resp) {
		ESP_LOGE(TAG, "Invalid parameters");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "OTA update started");

	resp_payload = (RpcRespOTABegin *)
		calloc(1,sizeof(RpcRespOTABegin));
	if (!resp_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}
	rpc__resp__otabegin__init(resp_payload);
	resp->payload_case = RPC__PAYLOAD_RESP_OTA_BEGIN;
	resp->resp_ota_begin = resp_payload;

	/* Identify next OTA partition */
	update_partition = esp_ota_get_next_update_partition(NULL);
	if (update_partition == NULL) {
		ESP_LOGE(TAG, "Failed to get next update partition");
		ret = -1;
		goto err;
	}

	ESP_LOGI(TAG, "Prepare partition for OTA\n");
	ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &handle);
	if (ret) {
		ESP_LOGE(TAG, "OTA begin failed[%d]", ret);
		goto err;
	}

	ota_msg = 1;

	resp_payload->resp = SUCCESS;
	return ESP_OK;
err:
	resp_payload->resp = ret;
	return ESP_OK;

}

/* Function OTA write */
static esp_err_t req_ota_write_handler (Rpc *req,
		Rpc *resp, void *priv_data)
{
	esp_err_t ret = ESP_OK;
	RpcRespOTAWrite *resp_payload = NULL;

	if (!req || !resp) {
		ESP_LOGE(TAG, "Invalid parameters");
		return ESP_FAIL;
	}

	resp_payload = (RpcRespOTAWrite *)calloc(1,sizeof(RpcRespOTAWrite));
	if (!resp_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}

	if (ota_msg) {
		ESP_LOGI(TAG, "Flashing image\n");
		ota_msg = 0;
	}
	rpc__resp__otawrite__init(resp_payload);
	resp->payload_case = RPC__PAYLOAD_RESP_OTA_WRITE;
	resp->resp_ota_write = resp_payload;

	ret = esp_ota_write( handle, (const void *)req->req_ota_write->ota_data.data,
			req->req_ota_write->ota_data.len);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "OTA write failed with return code 0x%x",ret);
		resp_payload->resp = ret;
		return ESP_OK;
	}
	resp_payload->resp = SUCCESS;
	return ESP_OK;
}

/* Function OTA end */
static esp_err_t req_ota_end_handler (Rpc *req,
		Rpc *resp, void *priv_data)
{
	esp_err_t ret = ESP_OK;
	RpcRespOTAEnd *resp_payload = NULL;
	TimerHandle_t xTimer = NULL;

	if (!req || !resp) {
		ESP_LOGE(TAG, "Invalid parameters");
		return ESP_FAIL;
	}

	resp_payload = (RpcRespOTAEnd *)calloc(1,sizeof(RpcRespOTAEnd));
	if (!resp_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}
	rpc__resp__otaend__init(resp_payload);
	resp->payload_case = RPC__PAYLOAD_RESP_OTA_END;
	resp->resp_ota_end = resp_payload;

	ret = esp_ota_end(handle);
	if (ret != ESP_OK) {
		if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
			ESP_LOGE(TAG, "Image validation failed, image is corrupted");
		} else {
			ESP_LOGE(TAG, "OTA update failed in end (%s)!", esp_err_to_name(ret));
		}
		goto err;
	}

	/* set OTA partition for next boot */
	ret = esp_ota_set_boot_partition(update_partition);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(ret));
		goto err;
	}
	xTimer = xTimerCreate("Timer", RESTART_TIMEOUT , pdFALSE, 0, vTimerCallback);
	if (xTimer == NULL) {
		ESP_LOGE(TAG, "Failed to create timer to restart system");
		ret = -1;
		goto err;
	}
	ret = xTimerStart(xTimer, 0);
	if (ret != pdPASS) {
		ESP_LOGE(TAG, "Failed to start timer to restart system");
		ret = -2;
		goto err;
	}
	ESP_LOGE(TAG, "**** OTA updated successful, ESP32 will reboot in 5 sec ****");
	resp_payload->resp = SUCCESS;
	return ESP_OK;
err:
	resp_payload->resp = ret;
	return ESP_OK;
}

#if 0
/* Function vendor specific ie */
static esp_err_t req_set_softap_vender_specific_ie_handler (Rpc *req,
		Rpc *resp, void *priv_data)
{
	esp_err_t ret = ESP_OK;
	RpcRespSetSoftAPVendorSpecificIE *resp_payload = NULL;
	RpcReqSetSoftAPVendorSpecificIE *p_vsi = req->req_set_softap_vendor_specific_ie;
	RpcReqVendorIEData *p_vid = NULL;
	vendor_ie_data_t *v_data = NULL;

	if (!req || !resp || !p_vsi) {
		ESP_LOGE(TAG, "Invalid parameters");
		return ESP_FAIL;
	}
	p_vid = p_vsi->vendor_ie_data;

	if (!p_vsi->enable) {

		ESP_LOGI(TAG,"Disable softap vendor IE\n");

	} else {

		ESP_LOGI(TAG,"Enable softap vendor IE\n");

		if (!p_vid ||
		    !p_vid->payload.len ||
		    !p_vid->payload.data) {
			ESP_LOGE(TAG, "Invalid parameters");
			return ESP_FAIL;
		}

		v_data = (vendor_ie_data_t*)calloc(1,sizeof(vendor_ie_data_t)+p_vid->payload.len);
		if (!v_data) {
			ESP_LOGE(TAG, "Malloc failed at %s:%u\n", __func__, __LINE__);
			return ESP_FAIL;
		}

		v_data->length = p_vid->length;
		v_data->element_id = p_vid->element_id;
		v_data->vendor_oui_type = p_vid->vendor_oui_type;

		memcpy(v_data->vendor_oui, p_vid->vendor_oui.data, VENDOR_OUI_BUF);

		if (p_vid->payload.len && p_vid->payload.data) {
			memcpy(v_data->payload, p_vid->payload.data, p_vid->payload.len);
		}
	}


	resp_payload = (RpcRespSetSoftAPVendorSpecificIE *)
		calloc(1,sizeof(RpcRespSetSoftAPVendorSpecificIE));
	if (!resp_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		if (v_data)
			mem_free(v_data);
		return ESP_ERR_NO_MEM;
	}

	rpc__resp__set_soft_apvendor_specific_ie__init(resp_payload);
	resp->payload_case = RPC__PAYLOAD_RESP_SET_SOFTAP_VENDOR_SPECIFIC_IE;
	resp->resp_set_softap_vendor_specific_ie = resp_payload;


	ret = esp_wifi_set_vendor_ie(p_vsi->enable,
			p_vsi->type,
			p_vsi->idx,
			v_data);

	if (v_data)
		mem_free(v_data);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set vendor information element %d \n", ret);
		resp_payload->resp = FAILURE;
		return ESP_OK;
	}
	resp_payload->resp = SUCCESS;
	return ESP_OK;
}
#endif

/* Function set wifi maximum TX power */
static esp_err_t req_wifi_set_max_tx_power(Rpc *req,
		Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetMaxTxPower, resp_set_wifi_max_tx_power,
			RpcReqWifiSetMaxTxPower, req_set_wifi_max_tx_power,
			rpc__resp__wifi_set_max_tx_power__init);
	RPC_RET_FAIL_IF(esp_wifi_set_max_tx_power(req_payload->power));
	return ESP_OK;
}

/* Function get wifi TX current power */
static esp_err_t req_wifi_get_max_tx_power(Rpc *req,
		Rpc *resp, void *priv_data)
{
	int8_t power = 0;

	RPC_TEMPLATE_SIMPLE(RpcRespWifiGetMaxTxPower, resp_get_wifi_max_tx_power,
			RpcReqWifiGetMaxTxPower, req_get_wifi_max_tx_power,
			rpc__resp__wifi_get_max_tx_power__init);
	RPC_RET_FAIL_IF(esp_wifi_get_max_tx_power(&power));
	resp_payload->power = power;
	return ESP_OK;
}

static void heartbeat_timer_cb(TimerHandle_t xTimer)
{
	send_event_to_host(RPC_ID__Event_Heartbeat);
	hb_num++;
}

static void stop_heartbeat(void)
{
	if (handle_heartbeat_task &&
	    xTimerIsTimerActive(handle_heartbeat_task)) {
		ESP_LOGI(TAG, "Stopping HB timer");
		xTimerStop(handle_heartbeat_task, portMAX_DELAY);
		xTimerDelete(handle_heartbeat_task, portMAX_DELAY);
		handle_heartbeat_task = NULL;
	}
	hb_num = 0;
}

static esp_err_t start_heartbeat(int duration)
{
	esp_err_t ret = ESP_OK;

	handle_heartbeat_task = xTimerCreate("HB_Timer",
			duration*TIMEOUT_IN_SEC, pdTRUE, 0, heartbeat_timer_cb);
	if (handle_heartbeat_task == NULL) {
		ESP_LOGE(TAG, "Failed to Heartbeat");
		return ESP_FAIL;
	}

	ret = xTimerStart(handle_heartbeat_task, 0);
	if (ret != pdPASS) {
		ESP_LOGE(TAG, "Failed to start Heartbeat");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "HB timer started for %u sec\n", duration);

	return ESP_OK;
}

static esp_err_t configure_heartbeat(bool enable, int hb_duration)
{
	esp_err_t ret = ESP_OK;
	int duration = hb_duration ;

	if (!enable) {
		ESP_LOGI(TAG, "Stop Heatbeat");
		stop_heartbeat();

	} else {
		if (duration < MIN_HEARTBEAT_INTERVAL)
			duration = MIN_HEARTBEAT_INTERVAL;
		if (duration > MAX_HEARTBEAT_INTERVAL)
			duration = MAX_HEARTBEAT_INTERVAL;

		stop_heartbeat();

		ret = start_heartbeat(duration);
	}

	return ret;
}

/* Function to config heartbeat */
static esp_err_t req_config_heartbeat(Rpc *req,
		Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespConfigHeartbeat,
			resp_config_heartbeat,
			RpcReqConfigHeartbeat,
			req_config_heartbeat,
			rpc__resp__config_heartbeat__init);

	RPC_RET_FAIL_IF(configure_heartbeat(req_payload->enable, req_payload->duration));

	return ESP_OK;
}

#ifdef CONFIG_SLAVE_LWIP_ENABLED

void send_dhcp_dns_info_to_host(uint8_t send_wifi_connected)
{
#if 0
	ESP_EARLY_LOGI(TAG, "Send DHCP-DNS status to Host");
	send_event_data_to_host(RPC_ID__Event_SetDhcpDnsStatus,
			&s2h_dhcp_dns, sizeof(rpc_set_dhcp_dns_status_t));

#endif
	if (station_connected) {
		ESP_LOGI(TAG, "-- Send station connected event to host --");
		send_event_data_to_host(RPC_ID__Event_StaConnected,
			&lkg_sta_connected_event, sizeof(wifi_event_sta_connected_t));
	}
}

static void event_handler_ip(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	char ip_s[16] = {0};
	char nm_s[16] = {0};
	char gw_s[16] = {0};
	char dns_ip_s[16] = {0};

	if (event_base == IP_EVENT) {
		switch (event_id) {

		case IP_EVENT_STA_GOT_IP: {
			ip_event_got_ip_t* event = event_data;
			esp_netif_t *netif = event->esp_netif;
			esp_netif_dns_info_t dns = {0};

			//memcpy(&lkg_sta_got_ip_event, event_data, sizeof(ip_event_got_ip_t));
			ESP_ERROR_CHECK(esp_wifi_internal_set_sta_ip());
			ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));

			esp_ip4addr_ntoa(&event->ip_info.ip, ip_s, sizeof(ip_s));
			esp_ip4addr_ntoa(&event->ip_info.netmask, nm_s, sizeof(nm_s));
			esp_ip4addr_ntoa(&event->ip_info.gw, gw_s, sizeof(gw_s));
			esp_ip4addr_ntoa(&dns.ip.u_addr.ip4, dns_ip_s, sizeof(dns_ip_s));

			ESP_LOGI(TAG, "Slave sta dhcp{IP[%s] NM[%s] GW[%s]} dns{type[%u] ip[%s]}",
					ip_s, nm_s, gw_s, dns.ip.type, dns_ip_s);

			s2h_dhcp_dns.net_link_up = 1;
			s2h_dhcp_dns.dhcp_up     = 1;
			s2h_dhcp_dns.dns_up      = 1;
			strlcpy((char*)s2h_dhcp_dns.dhcp_ip, ip_s, sizeof(s2h_dhcp_dns.dhcp_ip));
			strlcpy((char*)s2h_dhcp_dns.dhcp_nm, nm_s, sizeof(s2h_dhcp_dns.dhcp_nm));
			strlcpy((char*)s2h_dhcp_dns.dhcp_gw, gw_s, sizeof(s2h_dhcp_dns.dhcp_gw));
			strlcpy((char*)s2h_dhcp_dns.dns_ip, dns_ip_s, sizeof(s2h_dhcp_dns.dns_ip));
			s2h_dhcp_dns.dns_type = ESP_NETIF_DNS_MAIN;

			send_dhcp_dns_info_to_host(0);
			station_got_ip = 1;
			break;
		} case IP_EVENT_STA_LOST_IP: {
			ESP_LOGI(TAG, "Lost IP address");
			station_got_ip = 0;
			memset(&s2h_dhcp_dns, 0, sizeof(s2h_dhcp_dns));
			send_dhcp_dns_info_to_host(0);
			break;
		}

		}
	}
}
#endif

extern esp_netif_t *slave_sta_netif;
static esp_err_t set_slave_static_ip(wifi_interface_t iface, char *ip, char *nm, char *gw)
{
	esp_netif_ip_info_t ip_info = {0};

	ESP_RETURN_ON_FALSE(iface == WIFI_IF_STA, ESP_FAIL, TAG, "only sta iface supported yet");

	ip_info.ip.addr = ipaddr_addr(ip);
	ip_info.netmask.addr = ipaddr_addr(nm);
	ip_info.gw.addr = ipaddr_addr(gw);

	ESP_LOGI(TAG, "Set static IP addr ip:%s nm:%s gw:%s", ip, nm, gw);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(slave_sta_netif, &ip_info));

	return ESP_OK;
}

esp_err_t set_slave_dns(wifi_interface_t iface, char *ip, uint8_t type)
{
	esp_netif_dns_info_t dns = {0};

	ESP_RETURN_ON_FALSE(iface == WIFI_IF_STA, ESP_FAIL, TAG, "only sta iface supported yet");

	dns.ip.u_addr.ip4.addr = ipaddr_addr(ip);
	dns.ip.type = type;

	ESP_LOGI(TAG, "Set DNS ip:%s type:%u", ip, type);
	ESP_ERROR_CHECK(esp_netif_set_dns_info(slave_sta_netif, ESP_NETIF_DNS_MAIN, &dns));

	return ESP_OK;
}


static void event_handler_wifi(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_AP_STACONNECTED) {
			wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
			ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
					MAC2STR(event->mac), event->aid);
			send_event_data_to_host(RPC_ID__Event_AP_StaConnected,
					event_data, sizeof(wifi_event_ap_staconnected_t));
		} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
			wifi_event_ap_stadisconnected_t *event =
				(wifi_event_ap_stadisconnected_t *) event_data;
			ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
					MAC2STR(event->mac), event->aid);
			send_event_data_to_host(RPC_ID__Event_AP_StaDisconnected,
					event_data, sizeof(wifi_event_ap_stadisconnected_t));
		} else if (event_id == WIFI_EVENT_SCAN_DONE) {
			ESP_LOGI(TAG, "Wi-Fi sta scan done");
			// rpc event receiver expects Scan Done to have this ID
			send_event_data_to_host(RPC_ID__Event_StaScanDone,
					event_data, sizeof(wifi_event_sta_scan_done_t));
		} else if (event_id == WIFI_EVENT_STA_CONNECTED) {
			ESP_LOGI(TAG, "Sta mode connected");
			send_event_data_to_host(RPC_ID__Event_StaConnected,
				event_data, sizeof(wifi_event_sta_connected_t));
			memcpy(&lkg_sta_connected_event, event_data, sizeof(wifi_event_sta_connected_t));
			esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, (wifi_rxcb_t) wlan_sta_rx_callback);
			station_connected = true;
			sta_connect_retry = 0;
		} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
			station_connected = false;
			esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);

#ifdef CONFIG_SLAVE_LWIP_ENABLED
			// rpc_set_dhcp_dns_status_t s2h_dhcp_dns = {0};
			// send_event_data_to_host(RPC_ID__Event_SetDhcpDnsStatus,
			// 		&s2h_dhcp_dns, sizeof(rpc_set_dhcp_dns_status_t));
#endif

			if (sta_connect_retry < MAX_STA_CONNECT_ATTEMPTS) {
				ESP_LOGI(TAG, "**** esp_wifi_connect ***");
				esp_wifi_connect();
			} else {
				send_event_data_to_host(RPC_ID__Event_StaDisconnected,
						event_data, sizeof(wifi_event_sta_disconnected_t));
			}
			ESP_LOGI(TAG, "Sta mode disconnect, retry[%u]", sta_connect_retry);
			sta_connect_retry++;
		} else {
			if (event_id == WIFI_EVENT_AP_START) {
				ESP_LOGI(TAG,"softap started");
				esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, (wifi_rxcb_t) wlan_ap_rx_callback);
				softap_started = 1;
			} else if (event_id == WIFI_EVENT_AP_STOP) {
				ESP_LOGI(TAG,"softap stopped");
				esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, NULL);
				softap_started = 0;
			}

			ESP_LOGI(TAG, "########  Sending Wifi event %d #########", (int)event_id);
			send_event_data_to_host(RPC_ID__Event_WifiEventNoArgs,
					&event_id, sizeof(event_id));
		}
	}
}

esp_err_t esp_hosted_register_event_handlers(void)
{
	ESP_LOGI(TAG, "************ esp_hosted_register_event_handlers ****************");
	esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler_wifi,
		NULL,
		&instance_any_id);

#ifdef CONFIG_SLAVE_LWIP_ENABLED
	esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&event_handler_ip,
		NULL,
		&instance_ip);
	esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_LOST_IP,
		&event_handler_ip,
		NULL,
		&instance_ip);
#endif
	return ESP_OK;
}

static esp_err_t req_wifi_init(Rpc *req, Rpc *resp, void *priv_data)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	RPC_TEMPLATE(RpcRespWifiInit, resp_wifi_init,
			RpcReqWifiInit, req_wifi_init,
			rpc__resp__wifi_init__init);

	RPC_RET_FAIL_IF(!req_payload->cfg);

	if (station_connected) {
		ESP_LOGW(TAG, "Wifi already init");
		return ESP_OK;
	}
	cfg.static_rx_buf_num       = req_payload->cfg->static_rx_buf_num      ;
	cfg.dynamic_rx_buf_num      = req_payload->cfg->dynamic_rx_buf_num     ;
	cfg.tx_buf_type             = req_payload->cfg->tx_buf_type            ;
	cfg.static_tx_buf_num       = req_payload->cfg->static_tx_buf_num      ;
	cfg.dynamic_tx_buf_num      = req_payload->cfg->dynamic_tx_buf_num     ;
	cfg.cache_tx_buf_num        = req_payload->cfg->cache_tx_buf_num       ;
	cfg.csi_enable              = req_payload->cfg->csi_enable             ;
	cfg.ampdu_rx_enable         = req_payload->cfg->ampdu_rx_enable        ;
	cfg.ampdu_tx_enable         = req_payload->cfg->ampdu_tx_enable        ;
	cfg.amsdu_tx_enable         = req_payload->cfg->amsdu_tx_enable        ;
	cfg.nvs_enable              = req_payload->cfg->nvs_enable             ;
	cfg.nano_enable             = req_payload->cfg->nano_enable            ;
	cfg.rx_ba_win               = req_payload->cfg->rx_ba_win              ;
	cfg.wifi_task_core_id       = req_payload->cfg->wifi_task_core_id      ;
	cfg.beacon_max_len          = req_payload->cfg->beacon_max_len         ;
	cfg.mgmt_sbuf_num           = req_payload->cfg->mgmt_sbuf_num          ;
	cfg.feature_caps            = req_payload->cfg->feature_caps           ;
	cfg.sta_disconnected_pm     = req_payload->cfg->sta_disconnected_pm    ;
	cfg.espnow_max_encrypt_num  = req_payload->cfg->espnow_max_encrypt_num ;
	cfg.magic                   = req_payload->cfg->magic                  ;

	ESP_LOGI(TAG, "************ esp_wifi_init ****************");
    RPC_RET_FAIL_IF(esp_wifi_init(&cfg));

	return ESP_OK;
}

static esp_err_t req_wifi_deinit(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiDeinit, resp_wifi_deinit,
			RpcReqWifiDeinit, req_wifi_deinit,
			rpc__resp__wifi_deinit__init);

	ESP_LOGI(TAG, "************ esp_wifi_deinit ****************");
    RPC_RET_FAIL_IF(esp_wifi_deinit());

	return ESP_OK;
}


static esp_err_t req_wifi_start(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiStart, resp_wifi_start,
			RpcReqWifiStart, req_wifi_start,
			rpc__resp__wifi_start__init);

	if (station_connected) {
		ESP_LOGW(TAG, "Wifi is already started");
		int event_id = WIFI_EVENT_STA_START;
		send_event_data_to_host(RPC_ID__Event_WifiEventNoArgs,
				&event_id, sizeof(event_id));
#ifdef CONFIG_SLAVE_LWIP_ENABLED
		// Send DHCP DNS info to host
		send_dhcp_dns_info_to_host(1);
#endif
		return ESP_OK;
	}
	ESP_LOGI(TAG, "************ wifi_start ****************");
    RPC_RET_FAIL_IF(esp_wifi_start());
	return ESP_OK;
}

static esp_err_t req_wifi_stop(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiStop, resp_wifi_stop,
			RpcReqWifiStop, req_wifi_stop,
			rpc__resp__wifi_stop__init);

	ESP_LOGI(TAG, "************ esp_wifi_stop ****************");
    RPC_RET_FAIL_IF(esp_wifi_stop());

	return ESP_OK;
}

static esp_err_t req_wifi_connect(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiConnect, resp_wifi_connect,
			RpcReqWifiConnect, req_wifi_connect,
			rpc__resp__wifi_connect__init);

	if (wifi_config_modified || !station_connected) {
		ESP_LOGI(TAG, "************ connect ****************");
		RPC_RET_FAIL_IF(esp_wifi_connect());
		wifi_config_modified = 0;
	} else {
		send_event_data_to_host(RPC_ID__Event_StaConnected,
				&lkg_sta_connected_event, sizeof(wifi_event_sta_connected_t));
		ESP_LOGI(TAG, "Already connected");

#ifdef CONFIG_SLAVE_LWIP_ENABLED
		send_dhcp_dns_info_to_host(1);
#endif
	}

	return ESP_OK;
}

static esp_err_t req_wifi_disconnect(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiDisconnect, resp_wifi_disconnect,
			RpcReqWifiDisconnect, req_wifi_disconnect,
			rpc__resp__wifi_disconnect__init);

	ESP_LOGI(TAG, "************ esp_wifi_disconnect ****************");
    RPC_RET_FAIL_IF(esp_wifi_disconnect());

	return ESP_OK;
}


static esp_err_t req_wifi_set_config(Rpc *req, Rpc *resp, void *priv_data)
{
	static wifi_config_t prev_config = {0};
	wifi_config_t cfg = {0};

	RPC_TEMPLATE(RpcRespWifiSetConfig, resp_wifi_set_config,
			RpcReqWifiSetConfig, req_wifi_set_config,
			rpc__resp__wifi_set_config__init);

	RPC_RET_FAIL_IF((req_payload->iface != WIFI_IF_STA) &&
	                 (req_payload->iface != WIFI_IF_AP));

	RPC_RET_FAIL_IF(!req_payload->cfg);
	if (req_payload->iface == WIFI_IF_STA) {

		wifi_sta_config_t * p_a_sta = &(cfg.sta);
		WifiStaConfig * p_c_sta = req_payload->cfg->sta;
		RPC_RET_FAIL_IF(!req_payload->cfg->sta);
		RPC_REQ_COPY_STR(p_a_sta->ssid, p_c_sta->ssid, SSID_LENGTH);
		if (strlen((char*)p_a_sta->ssid))
			ESP_LOGI(TAG, "STA set config: SSID:%s", p_a_sta->ssid);
		RPC_REQ_COPY_STR(p_a_sta->password, p_c_sta->password, PASSWORD_LENGTH);
		p_a_sta->scan_method = p_c_sta->scan_method;
		p_a_sta->bssid_set = p_c_sta->bssid_set;

		if (p_a_sta->bssid_set)
			RPC_REQ_COPY_BYTES(p_a_sta->bssid, p_c_sta->bssid, BSSID_BYTES_SIZE);

		p_a_sta->channel = p_c_sta->channel;
		p_a_sta->listen_interval = p_c_sta->listen_interval;
		p_a_sta->sort_method = p_c_sta->sort_method;
		p_a_sta->threshold.rssi = p_c_sta->threshold->rssi;
		p_a_sta->threshold.authmode = p_c_sta->threshold->authmode;
		//p_a_sta->ssid_hidden = p_c_sta->ssid_hidden;
		//p_a_sta->max_connections = p_c_sta->max_connections;
		p_a_sta->pmf_cfg.capable = p_c_sta->pmf_cfg->capable;
		p_a_sta->pmf_cfg.required = p_c_sta->pmf_cfg->required;

		p_a_sta->rm_enabled = H_GET_BIT(STA_RM_ENABLED_BIT, p_c_sta->bitmask);
		p_a_sta->btm_enabled = H_GET_BIT(STA_BTM_ENABLED_BIT, p_c_sta->bitmask);
		p_a_sta->mbo_enabled = H_GET_BIT(STA_MBO_ENABLED_BIT, p_c_sta->bitmask);
		p_a_sta->ft_enabled = H_GET_BIT(STA_FT_ENABLED_BIT, p_c_sta->bitmask);
		p_a_sta->owe_enabled = H_GET_BIT(STA_OWE_ENABLED_BIT, p_c_sta->bitmask);
		p_a_sta->transition_disable = H_GET_BIT(STA_TRASITION_DISABLED_BIT, p_c_sta->bitmask);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
		p_a_sta->reserved = WIFI_CONFIG_STA_GET_RESERVED_VAL(p_c_sta->bitmask);
#else
		p_a_sta->reserved1 = WIFI_CONFIG_STA_GET_RESERVED_VAL(p_c_sta->bitmask);
#endif

		p_a_sta->sae_pwe_h2e = p_c_sta->sae_pwe_h2e;
		p_a_sta->failure_retry_cnt = p_c_sta->failure_retry_cnt;

		p_a_sta->he_dcm_set = H_GET_BIT(WIFI_HE_STA_CONFIG_he_dcm_set_BIT, p_c_sta->he_bitmask);
		// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx is two bits wide
		p_a_sta->he_dcm_max_constellation_tx = (p_c_sta->he_bitmask >> WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx_BITS) & 0x03;
		// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx is two bits wide
		p_a_sta->he_dcm_max_constellation_rx = (p_c_sta->he_bitmask >> WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx_BITS) & 0x03;
		p_a_sta->he_mcs9_enabled = H_GET_BIT(WIFI_HE_STA_CONFIG_he_mcs9_enabled_BIT, p_c_sta->he_bitmask);
		p_a_sta->he_su_beamformee_disabled = H_GET_BIT(WIFI_HE_STA_CONFIG_he_su_beamformee_disabled_BIT, p_c_sta->he_bitmask);
		p_a_sta->he_trig_su_bmforming_feedback_disabled = H_GET_BIT(WIFI_HE_STA_CONFIG_he_trig_su_bmforming_feedback_disabled_BIT, p_c_sta->bitmask);
		p_a_sta->he_trig_mu_bmforming_partial_feedback_disabled = H_GET_BIT(WIFI_HE_STA_CONFIG_he_trig_mu_bmforming_partial_feedback_disabled_BIT, p_c_sta->bitmask);
		p_a_sta->he_trig_cqi_feedback_disabled = H_GET_BIT(WIFI_HE_STA_CONFIG_he_trig_cqi_feedback_disabled_BIT, p_c_sta->bitmask);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
		p_a_sta->he_reserved = WIFI_HE_STA_GET_RESERVED_VAL(p_c_sta->bitmask);
#endif

		/* Avoid using fast scan, which leads to faster SSID selection,
		 * but faces data throughput issues when same SSID broadcasted by weaker AP
		 */
		p_a_sta->scan_method = WIFI_ALL_CHANNEL_SCAN;
		p_a_sta->sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

		RPC_REQ_COPY_STR(p_a_sta->sae_h2e_identifier, p_c_sta->sae_h2e_identifier, SAE_H2E_IDENTIFIER_LEN);
	} else if (req_payload->iface == WIFI_IF_AP) {
		wifi_ap_config_t * p_a_ap = &(cfg.ap);
		WifiApConfig * p_c_ap = req_payload->cfg->ap;
		RPC_RET_FAIL_IF(!req_payload->cfg->ap);
		/* esp_wifi_types.h says SSID should be NULL terminated if ssid_len is 0 */
		RPC_REQ_COPY_STR(p_a_ap->ssid, p_c_ap->ssid, SSID_LENGTH);
		p_a_ap->ssid_len = p_c_ap->ssid_len;
		RPC_REQ_COPY_STR(p_a_ap->password, p_c_ap->password, PASSWORD_LENGTH);
		p_a_ap->channel = p_c_ap->channel;
		p_a_ap->authmode = p_c_ap->authmode;
		p_a_ap->ssid_hidden = p_c_ap->ssid_hidden;
		p_a_ap->max_connection = p_c_ap->max_connection;
		p_a_ap->beacon_interval = p_c_ap->beacon_interval;
		p_a_ap->pairwise_cipher = p_c_ap->pairwise_cipher;
		p_a_ap->ftm_responder = p_c_ap->ftm_responder;
		p_a_ap->pmf_cfg.capable = p_c_ap->pmf_cfg->capable;
		p_a_ap->pmf_cfg.required = p_c_ap->pmf_cfg->required;
		p_a_ap->sae_pwe_h2e = p_c_ap->sae_pwe_h2e;
	}

	if (0 != memcmp(&cfg, &prev_config, sizeof(wifi_config_t))) {
		ESP_LOGI(TAG, "************ esp_wifi_set_config ****************");
		RPC_RET_FAIL_IF(esp_wifi_set_config(req_payload->iface, &cfg));
		wifi_config_modified = 1;
	} else {
		wifi_config_modified = 0;
	}

	return ESP_OK;
}

static esp_err_t req_wifi_get_config(Rpc *req, Rpc *resp, void *priv_data)
{
	wifi_interface_t iface;
	wifi_config_t cfg = {0};

	RPC_TEMPLATE(RpcRespWifiGetConfig, resp_wifi_get_config,
			RpcReqWifiGetConfig, req_wifi_get_config,
			rpc__resp__wifi_get_config__init);

	iface = req_payload->iface;
	resp_payload->iface = iface;
	RPC_RET_FAIL_IF(iface > WIFI_IF_AP);
	RPC_RET_FAIL_IF(esp_wifi_get_config(iface, &cfg));

	RPC_ALLOC_ELEMENT(WifiConfig, resp_payload->cfg, wifi_config__init);
	switch (iface) {

	case WIFI_IF_STA: {
		wifi_sta_config_t * p_a_sta = &(cfg.sta);
		resp_payload->cfg->u_case = WIFI_CONFIG__U_STA;

		RPC_ALLOC_ELEMENT(WifiStaConfig, resp_payload->cfg->sta, wifi_sta_config__init);

		WifiStaConfig * p_c_sta = resp_payload->cfg->sta;
		RPC_RESP_COPY_STR(p_c_sta->ssid, p_a_sta->ssid, SSID_LENGTH);
		RPC_RESP_COPY_STR(p_c_sta->password, p_a_sta->password, PASSWORD_LENGTH);
		p_c_sta->scan_method = p_a_sta->scan_method;
		p_c_sta->bssid_set = p_a_sta->bssid_set;

		//TODO: Expected to break python for bssid
		if (p_c_sta->bssid_set)
			RPC_RESP_COPY_BYTES(p_c_sta->bssid, p_a_sta->bssid, BSSID_BYTES_SIZE);

		p_c_sta->channel = p_a_sta->channel;
		p_c_sta->listen_interval = p_a_sta->listen_interval;
		p_c_sta->sort_method = p_a_sta->sort_method;
		RPC_ALLOC_ELEMENT(WifiScanThreshold, p_c_sta->threshold, wifi_scan_threshold__init);
		p_c_sta->threshold->rssi = p_a_sta->threshold.rssi;
		p_c_sta->threshold->authmode = p_a_sta->threshold.authmode;
		RPC_ALLOC_ELEMENT(WifiPmfConfig, p_c_sta->pmf_cfg, wifi_pmf_config__init);
		p_c_sta->pmf_cfg->capable = p_a_sta->pmf_cfg.capable;
		p_c_sta->pmf_cfg->required = p_a_sta->pmf_cfg.required;

		if (p_a_sta->rm_enabled)
			H_SET_BIT(STA_RM_ENABLED_BIT, p_c_sta->bitmask);

		if (p_a_sta->btm_enabled)
			H_SET_BIT(STA_BTM_ENABLED_BIT, p_c_sta->bitmask);

		if (p_a_sta->mbo_enabled)
			H_SET_BIT(STA_MBO_ENABLED_BIT, p_c_sta->bitmask);

		if (p_a_sta->ft_enabled)
			H_SET_BIT(STA_FT_ENABLED_BIT, p_c_sta->bitmask);

		if (p_a_sta->owe_enabled)
			H_SET_BIT(STA_OWE_ENABLED_BIT, p_c_sta->bitmask);

		if (p_a_sta->transition_disable)
			H_SET_BIT(STA_TRASITION_DISABLED_BIT, p_c_sta->bitmask);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
		WIFI_CONFIG_STA_SET_RESERVED_VAL(p_a_sta->reserved, p_c_sta->bitmask);
#else
		WIFI_CONFIG_STA_SET_RESERVED_VAL(p_a_sta->reserved1, p_c_sta->bitmask);
#endif

		p_c_sta->sae_pwe_h2e = p_a_sta->sae_pwe_h2e;
		p_c_sta->failure_retry_cnt = p_a_sta->failure_retry_cnt;
		break;
	}
	case WIFI_IF_AP: {
		wifi_ap_config_t * p_a_ap = &(cfg.ap);
		resp_payload->cfg->u_case = WIFI_CONFIG__U_AP;

		RPC_ALLOC_ELEMENT(WifiApConfig, resp_payload->cfg->ap, wifi_ap_config__init);
		WifiApConfig * p_c_ap = resp_payload->cfg->ap;
		RPC_RESP_COPY_STR(p_c_ap->password, p_a_ap->password, PASSWORD_LENGTH);
		p_c_ap->ssid_len = p_a_ap->ssid_len;
		p_c_ap->channel = p_a_ap->channel;
		p_c_ap->authmode = p_a_ap->authmode;
		p_c_ap->ssid_hidden = p_a_ap->ssid_hidden;
		p_c_ap->max_connection = p_a_ap->max_connection;
		p_c_ap->beacon_interval = p_a_ap->beacon_interval;
		p_c_ap->pairwise_cipher = p_a_ap->pairwise_cipher;
		p_c_ap->ftm_responder = p_a_ap->ftm_responder;
		RPC_ALLOC_ELEMENT(WifiPmfConfig, p_c_ap->pmf_cfg, wifi_pmf_config__init);
		p_c_ap->pmf_cfg->capable = p_a_ap->pmf_cfg.capable;
		p_c_ap->pmf_cfg->required = p_a_ap->pmf_cfg.required;
		if (p_c_ap->ssid_len)
			RPC_RESP_COPY_STR(p_c_ap->ssid, p_a_ap->ssid, SSID_LENGTH);
		p_c_ap->sae_pwe_h2e = p_a_ap->sae_pwe_h2e;
		break;
	}
	default:
        ESP_LOGE(TAG, "Unsupported WiFi interface[%u]\n", iface);
	} //switch

err:
	return ESP_OK;
}

static esp_err_t req_wifi_scan_start(Rpc *req, Rpc *resp, void *priv_data)
{
	wifi_scan_config_t scan_conf = {0};
	WifiScanConfig *p_c = NULL;
	WifiScanTime *p_c_st = NULL;
	wifi_scan_config_t * p_a = &scan_conf;
	wifi_scan_time_t *p_a_st = &p_a->scan_time;

    RPC_TEMPLATE(RpcRespWifiScanStart, resp_wifi_scan_start,
			RpcReqWifiScanStart, req_wifi_scan_start,
			rpc__resp__wifi_scan_start__init);

	p_c = req_payload->config;

	if (!req_payload->config || !req_payload->config_set) {
		p_a = NULL;
	} else {
		//RPC_REQ_COPY_STR(p_a->ssid, p_c->ssid, SSID_LENGTH);
		//RPC_REQ_COPY_STR(p_a->bssid, p_c->ssid, MAC_SIZE_BYTES);

		/* Note these are only pointers, not allocating memory for that */
		if (p_c->ssid.len)
			p_a->ssid = p_c->ssid.data;
		if (p_c->bssid.len)
			p_a->bssid = p_c->bssid.data;

		p_a->channel = p_c->channel;
		p_a->show_hidden = p_c->show_hidden;
		p_a->scan_type = p_c->scan_type;

		p_c_st = p_c->scan_time;

		p_a_st->passive = p_c_st->passive;
		p_a_st->active.min = p_c_st->active->min ;
		p_a_st->active.max = p_c_st->active->max ;

		p_a->home_chan_dwell_time = p_c->home_chan_dwell_time;
	}

    RPC_RET_FAIL_IF(esp_wifi_scan_start(p_a, req_payload->block));

	return ESP_OK;
}



static esp_err_t req_wifi_set_protocol(Rpc *req, Rpc *resp, void *priv_data)
{
	uint8_t protocol_bitmap = 0;
	RPC_TEMPLATE(RpcRespWifiSetProtocol, resp_wifi_set_protocol,
			RpcReqWifiSetProtocol, req_wifi_set_protocol,
			rpc__resp__wifi_set_protocol__init);

	RPC_RET_FAIL_IF(esp_wifi_get_protocol(req_payload->ifx, &protocol_bitmap));

	if (protocol_bitmap != req_payload->protocol_bitmap) {
		RPC_RET_FAIL_IF(esp_wifi_set_protocol(req_payload->ifx,
			req_payload->protocol_bitmap));
	}
	return ESP_OK;
}

static esp_err_t req_wifi_get_protocol(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiGetProtocol, resp_wifi_get_protocol,
			RpcReqWifiGetProtocol, req_wifi_get_protocol,
			rpc__resp__wifi_get_protocol__init);

	uint8_t protocol_bitmap = 0;
	RPC_RET_FAIL_IF(esp_wifi_get_protocol(req_payload->ifx, &protocol_bitmap));

	resp_payload->protocol_bitmap = protocol_bitmap;
	return ESP_OK;
}

static esp_err_t req_wifi_scan_stop(Rpc *req, Rpc *resp, void *priv_data)
{
    RPC_TEMPLATE_SIMPLE(RpcRespWifiScanStop, resp_wifi_scan_stop,
			RpcReqWifiScanStop, req_wifi_scan_stop,
			rpc__resp__wifi_scan_stop__init);

    RPC_RET_FAIL_IF(esp_wifi_scan_stop());
	return ESP_OK;
}

static esp_err_t req_wifi_scan_get_ap_num(Rpc *req, Rpc *resp, void *priv_data)
{
	uint16_t number = 0;
	int ret = 0;

    RPC_TEMPLATE_SIMPLE(RpcRespWifiScanGetApNum, resp_wifi_scan_get_ap_num,
			RpcReqWifiScanGetApNum, req_wifi_scan_get_ap_num,
			rpc__resp__wifi_scan_get_ap_num__init);

	ret = esp_wifi_scan_get_ap_num(&number);
    RPC_RET_FAIL_IF(ret);

	resp_payload->number = number;

	return ESP_OK;
}

static esp_err_t req_wifi_scan_get_ap_records(Rpc *req, Rpc *resp, void *priv_data)
{
	uint16_t number = 0;
	uint16_t ap_count = 0;
	int ret = 0;
	uint16_t i;

	wifi_ap_record_t *p_a_ap_list = NULL;
	WifiApRecord *p_c_ap_record = NULL;
	WifiCountry * p_c_country = NULL;
	wifi_country_t * p_a_country = NULL;

    RPC_TEMPLATE_SIMPLE(RpcRespWifiScanGetApRecords, resp_wifi_scan_get_ap_records,
			RpcReqWifiScanGetApRecords, req_wifi_scan_get_ap_records,
			rpc__resp__wifi_scan_get_ap_records__init);

	number = req->req_wifi_scan_get_ap_records->number;
	ESP_LOGD(TAG,"n_elem_scan_list predicted: %u\n", number);



	p_a_ap_list = (wifi_ap_record_t *)calloc(number, sizeof(wifi_ap_record_t));
	RPC_RET_FAIL_IF(!p_a_ap_list);


	ret = esp_wifi_scan_get_ap_num(&ap_count);
	if (ret || !ap_count) {
		ESP_LOGE(TAG,"esp_wifi_scan_get_ap_num: ret: %d num_ap_scanned:%u", ret, number);
		goto err;
	}
	if (number < ap_count) {
		ESP_LOGI(TAG,"n_elem_scan_list wants to return: %u Limit to %u\n", ap_count, number);
	}

	ret = esp_wifi_scan_get_ap_records(&number, p_a_ap_list);
    if(ret) {
		ESP_LOGE(TAG,"Failed to scan ap records");
		goto err;
	}


	resp_payload->number = number;
	resp_payload->ap_records = (WifiApRecord**)calloc(number, sizeof(WifiApRecord *));
	if (!resp_payload->ap_records) {
		ESP_LOGE(TAG,"resp: malloc failed for resp_payload->ap_records");
		goto err;
	}

	for (i=0;i<number;i++) {
		ESP_LOGD(TAG, "ap_record[%u]:", i+1);
		RPC_ALLOC_ELEMENT(WifiApRecord, resp_payload->ap_records[i], wifi_ap_record__init);
		RPC_ALLOC_ELEMENT(WifiCountry, resp_payload->ap_records[i]->country, wifi_country__init);
		p_c_ap_record = resp_payload->ap_records[i];
		p_c_country = p_c_ap_record->country;
		p_a_country = &p_a_ap_list[i].country;
		ESP_LOGD(TAG, "Ssid: %s, Bssid: " MACSTR, p_a_ap_list[i].ssid, MAC2STR(p_a_ap_list[i].bssid));
		ESP_LOGD(TAG, "Primary: %u Second: %u Rssi: %d Authmode: %u",
			p_a_ap_list[i].primary, p_a_ap_list[i].second,
			p_a_ap_list[i].rssi, p_a_ap_list[i].authmode
			);
		ESP_LOGD(TAG, "PairwiseCipher: %u Groupcipher: %u Ant: %u",
			p_a_ap_list[i].pairwise_cipher, p_a_ap_list[i].group_cipher,
			p_a_ap_list[i].ant
			);
		ESP_LOGD(TAG, "Bitmask: 11b:%u g:%u n:%u ax: %u lr:%u wps:%u ftm_resp:%u ftm_ini:%u res: %u",
			p_a_ap_list[i].phy_11b, p_a_ap_list[i].phy_11g,
			p_a_ap_list[i].phy_11n,  p_a_ap_list[i].phy_11ax, p_a_ap_list[i].phy_lr,
			p_a_ap_list[i].wps, p_a_ap_list[i].ftm_responder,
			p_a_ap_list[i].ftm_initiator, p_a_ap_list[i].reserved
			);
		RPC_RESP_COPY_STR(p_c_ap_record->ssid, p_a_ap_list[i].ssid, SSID_LENGTH);
		RPC_RESP_COPY_BYTES(p_c_ap_record->bssid, p_a_ap_list[i].bssid, BSSID_BYTES_SIZE);
		p_c_ap_record->primary = p_a_ap_list[i].primary;
		p_c_ap_record->second = p_a_ap_list[i].second;
		p_c_ap_record->rssi = p_a_ap_list[i].rssi;
		p_c_ap_record->authmode = p_a_ap_list[i].authmode;
		p_c_ap_record->pairwise_cipher = p_a_ap_list[i].pairwise_cipher;
		p_c_ap_record->group_cipher = p_a_ap_list[i].group_cipher;
		p_c_ap_record->ant = p_a_ap_list[i].ant;

		/*Bitmask*/
		if (p_a_ap_list[i].phy_11b)
			H_SET_BIT(WIFI_SCAN_AP_REC_phy_11b_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].phy_11g)
			H_SET_BIT(WIFI_SCAN_AP_REC_phy_11g_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].phy_11n)
			H_SET_BIT(WIFI_SCAN_AP_REC_phy_11n_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].phy_lr)
			H_SET_BIT(WIFI_SCAN_AP_REC_phy_lr_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].phy_11ax)
			H_SET_BIT(WIFI_SCAN_AP_REC_phy_11ax_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].wps)
			H_SET_BIT(WIFI_SCAN_AP_REC_wps_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].ftm_responder)
			H_SET_BIT(WIFI_SCAN_AP_REC_ftm_responder_BIT,p_c_ap_record->bitmask);

		if (p_a_ap_list[i].ftm_initiator)
			H_SET_BIT(WIFI_SCAN_AP_REC_ftm_initiator_BIT,p_c_ap_record->bitmask);

		WIFI_SCAN_AP_SET_RESERVED_VAL(p_a_ap_list[i].reserved, p_c_ap_record->bitmask);

		/* country */
		RPC_RESP_COPY_BYTES(p_c_country->cc, p_a_country->cc, sizeof(p_a_country->cc));
		p_c_country->schan = p_a_country->schan;
		p_c_country->nchan = p_a_country->nchan;
		p_c_country->max_tx_power = p_a_country->max_tx_power;
		p_c_country->policy = p_a_country->policy;

		ESP_LOGD(TAG, "Country cc:%c%c schan: %u nchan: %u max_tx_pow: %d policy: %u",
			p_a_country->cc[0], p_a_country->cc[1], p_a_country->schan, p_a_country->nchan,
			p_a_country->max_tx_power,p_a_country->policy);

		/* he_ap */
		RPC_ALLOC_ELEMENT(WifiHeApInfo, resp_payload->ap_records[i]->he_ap, wifi_he_ap_info__init);
		WifiHeApInfo * p_c_he_ap = p_c_ap_record->he_ap;
		wifi_he_ap_info_t * p_a_he_ap = &p_a_ap_list[i].he_ap;

		// bss_color uses six bits
		p_c_he_ap->bitmask = (p_a_he_ap->bss_color & WIFI_HE_AP_INFO_BSS_COLOR_BITS);

		if (p_a_he_ap->partial_bss_color)
			H_SET_BIT(WIFI_HE_AP_INFO_partial_bss_color_BIT,p_c_he_ap->bitmask);

		if (p_a_he_ap->bss_color_disabled)
			H_SET_BIT(WIFI_HE_AP_INFO_bss_color_disabled_BIT,p_c_he_ap->bitmask);

		p_c_he_ap->bssid_index = p_a_he_ap->bssid_index;

		ESP_LOGD(TAG, "HE_AP: bss_color %d, partial_bss_color %d, bss_color_disabled %d",
			p_a_he_ap->bss_color, p_a_he_ap->bss_color_disabled, p_a_he_ap->bss_color_disabled);

		/* increment num of records in rpc msg */
		resp_payload->n_ap_records++;
	}

err:
	mem_free(p_a_ap_list);
	return ESP_OK;
}

static esp_err_t req_wifi_clear_ap_list(Rpc *req, Rpc *resp, void *priv_data)
{
    RPC_TEMPLATE_SIMPLE(RpcRespWifiClearApList, resp_wifi_clear_ap_list,
			RpcReqWifiClearApList, req_wifi_clear_ap_list,
			rpc__resp__wifi_clear_ap_list__init);

    RPC_RET_FAIL_IF(esp_wifi_clear_ap_list());
	return ESP_OK;
}

static esp_err_t req_wifi_restore(Rpc *req, Rpc *resp, void *priv_data)
{
    RPC_TEMPLATE_SIMPLE(RpcRespWifiRestore, resp_wifi_restore,
			RpcReqWifiRestore, req_wifi_restore,
			rpc__resp__wifi_restore__init);

    RPC_RET_FAIL_IF(esp_wifi_restore());
	return ESP_OK;
}

static esp_err_t req_wifi_clear_fast_connect(Rpc *req, Rpc *resp, void *priv_data)
{
    RPC_TEMPLATE_SIMPLE(RpcRespWifiClearFastConnect, resp_wifi_clear_fast_connect,
			RpcReqWifiClearFastConnect, req_wifi_clear_fast_connect,
			rpc__resp__wifi_clear_fast_connect__init);

    RPC_RET_FAIL_IF(esp_wifi_clear_fast_connect());
	return ESP_OK;
}

static esp_err_t req_wifi_sta_get_ap_info(Rpc *req, Rpc *resp, void *priv_data)
{
	wifi_ap_record_t p_a_ap_info = {0};
	WifiApRecord *p_c_ap_record = NULL;
	WifiCountry * p_c_country = NULL;
	wifi_country_t * p_a_country = NULL;

    RPC_TEMPLATE_SIMPLE(RpcRespWifiStaGetApInfo, resp_wifi_sta_get_ap_info,
			RpcReqWifiStaGetApInfo, req_wifi_sta_get_ap_info,
			rpc__resp__wifi_sta_get_ap_info__init);


    RPC_RET_FAIL_IF(esp_wifi_sta_get_ap_info(&p_a_ap_info));
	RPC_ALLOC_ELEMENT(WifiApRecord, resp_payload->ap_records, wifi_ap_record__init);
	RPC_ALLOC_ELEMENT(WifiCountry, resp_payload->ap_records->country, wifi_country__init);
	p_c_ap_record = resp_payload->ap_records;
	p_c_country = p_c_ap_record->country;
	p_a_country = &p_a_ap_info.country;

	printf("Ssid: %s, Bssid: " MACSTR "\n", p_a_ap_info.ssid, MAC2STR(p_a_ap_info.bssid));
	printf("Primary: %u\nSecond: %u\nRssi: %d\nAuthmode: %u\nPairwiseCipher: %u\nGroupcipher: %u\nAnt: %u\nBitmask: 11b:%u g:%u n:%u lr:%u wps:%u ftm_resp:%u ftm_ini:%u res: %u\n",
			p_a_ap_info.primary, p_a_ap_info.second,
			p_a_ap_info.rssi, p_a_ap_info.authmode,
			p_a_ap_info.pairwise_cipher, p_a_ap_info.group_cipher,
			p_a_ap_info.ant, p_a_ap_info.phy_11b, p_a_ap_info.phy_11g,
			p_a_ap_info.phy_11n, p_a_ap_info.phy_lr,
			p_a_ap_info.wps, p_a_ap_info.ftm_responder,
			p_a_ap_info.ftm_initiator, p_a_ap_info.reserved);

	RPC_RESP_COPY_STR(p_c_ap_record->ssid, p_a_ap_info.ssid, SSID_LENGTH);
	RPC_RESP_COPY_BYTES(p_c_ap_record->bssid, p_a_ap_info.bssid, BSSID_BYTES_SIZE);
	p_c_ap_record->primary = p_a_ap_info.primary;
	p_c_ap_record->second = p_a_ap_info.second;
	p_c_ap_record->rssi = p_a_ap_info.rssi;
	p_c_ap_record->authmode = p_a_ap_info.authmode;
	p_c_ap_record->pairwise_cipher = p_a_ap_info.pairwise_cipher;
	p_c_ap_record->group_cipher = p_a_ap_info.group_cipher;
	p_c_ap_record->ant = p_a_ap_info.ant;

	/*Bitmask*/
	if (p_a_ap_info.phy_11b)
		H_SET_BIT(WIFI_SCAN_AP_REC_phy_11b_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.phy_11g)
		H_SET_BIT(WIFI_SCAN_AP_REC_phy_11g_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.phy_11n)
		H_SET_BIT(WIFI_SCAN_AP_REC_phy_11n_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.phy_lr)
		H_SET_BIT(WIFI_SCAN_AP_REC_phy_lr_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.phy_11ax)
		H_SET_BIT(WIFI_SCAN_AP_REC_phy_11ax_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.wps)
		H_SET_BIT(WIFI_SCAN_AP_REC_wps_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.ftm_responder)
		H_SET_BIT(WIFI_SCAN_AP_REC_ftm_responder_BIT,p_c_ap_record->bitmask);

	if (p_a_ap_info.ftm_initiator)
		H_SET_BIT(WIFI_SCAN_AP_REC_ftm_initiator_BIT,p_c_ap_record->bitmask);

	WIFI_SCAN_AP_SET_RESERVED_VAL(p_a_ap_info.reserved, p_c_ap_record->bitmask);

	/* country */
	RPC_RESP_COPY_BYTES(p_c_country->cc, p_a_country->cc, sizeof(p_a_country->cc));
	p_c_country->schan = p_a_country->schan;
	p_c_country->nchan = p_a_country->nchan;
	p_c_country->max_tx_power = p_a_country->max_tx_power;
	p_c_country->policy = p_a_country->policy;

	printf("Country: cc:%c%c schan: %u nchan: %u max_tx_pow: %d policy: %u\n",
			p_a_country->cc[0], p_a_country->cc[1], p_a_country->schan, p_a_country->nchan,
			p_a_country->max_tx_power,p_a_country->policy);

	/* he_ap */
	RPC_ALLOC_ELEMENT(WifiHeApInfo, resp_payload->ap_records->he_ap, wifi_he_ap_info__init);
	WifiHeApInfo * p_c_he_ap = p_c_ap_record->he_ap;
	wifi_he_ap_info_t * p_a_he_ap = &p_a_ap_info.he_ap;

	// bss_color uses six bits
	p_c_he_ap->bitmask = (p_a_he_ap->bss_color & WIFI_HE_AP_INFO_BSS_COLOR_BITS);

	if (p_a_he_ap->partial_bss_color)
		H_SET_BIT(WIFI_HE_AP_INFO_partial_bss_color_BIT,p_c_he_ap->bitmask);

	if (p_a_he_ap->bss_color_disabled)
		H_SET_BIT(WIFI_HE_AP_INFO_bss_color_disabled_BIT,p_c_he_ap->bitmask);

	p_c_he_ap->bssid_index = p_a_he_ap->bssid_index;

	printf("HE_AP: bss_color %d, partial_bss_color %d, bss_color_disabled %d\n",
		p_a_he_ap->bss_color, p_a_he_ap->bss_color_disabled, p_a_he_ap->bss_color_disabled);
	/* increment num of records in rpc msg */

err:
	return ESP_OK;
}


static esp_err_t req_wifi_deauth_sta(Rpc *req, Rpc *resp, void *priv_data)
{
    RPC_TEMPLATE(RpcRespWifiDeauthSta, resp_wifi_deauth_sta,
			RpcReqWifiDeauthSta, req_wifi_deauth_sta,
			rpc__resp__wifi_deauth_sta__init);

    RPC_RET_FAIL_IF(esp_wifi_deauth_sta(req_payload->aid));
	return ESP_OK;
}

static esp_err_t req_wifi_set_storage(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetStorage, resp_wifi_set_storage,
			RpcReqWifiSetStorage, req_wifi_set_storage,
			rpc__resp__wifi_set_storage__init);
	ESP_LOGI(TAG, "storage set: %lu", req_payload->storage);

	RPC_RET_FAIL_IF(esp_wifi_set_storage(req_payload->storage));
	return ESP_OK;
}

static esp_err_t req_wifi_set_bandwidth(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetBandwidth, resp_wifi_set_bandwidth,
			RpcReqWifiSetBandwidth, req_wifi_set_bandwidth,
			rpc__resp__wifi_set_bandwidth__init);

	wifi_bandwidth_t bw = 0;
	RPC_RET_FAIL_IF(esp_wifi_get_bandwidth(req_payload->ifx, &bw));
	if (bw != req_payload->bw) {
		RPC_RET_FAIL_IF(esp_wifi_set_bandwidth(req_payload->ifx, req_payload->bw));
	}

	return ESP_OK;
}

static esp_err_t req_wifi_get_bandwidth(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiGetBandwidth, resp_wifi_get_bandwidth,
			RpcReqWifiGetBandwidth, req_wifi_get_bandwidth,
			rpc__resp__wifi_get_bandwidth__init);

	wifi_bandwidth_t bw = 0;
	RPC_RET_FAIL_IF(esp_wifi_get_bandwidth(req_payload->ifx, &bw));

	resp_payload->bw = bw;
	return ESP_OK;
}

static esp_err_t req_wifi_set_channel(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetChannel, resp_wifi_set_channel,
			RpcReqWifiSetChannel, req_wifi_set_channel,
			rpc__resp__wifi_set_channel__init);

	uint8_t primary = 0;
	wifi_second_chan_t second = 0;
	RPC_RET_FAIL_IF(esp_wifi_get_channel(&primary, &second));

	if ((primary != req_payload->primary) ||
	    (second != req_payload->second)) {
		RPC_RET_FAIL_IF(esp_wifi_set_channel(req_payload->primary, req_payload->second));
	}
	return ESP_OK;
}

static esp_err_t req_wifi_get_channel(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiGetChannel, resp_wifi_get_channel,
			RpcReqWifiGetChannel, req_wifi_get_channel,
			rpc__resp__wifi_get_channel__init);

	uint8_t primary = 0;
	wifi_second_chan_t second = 0;
	RPC_RET_FAIL_IF(esp_wifi_get_channel(&primary, &second));

	resp_payload->primary = primary;
	resp_payload->second = second;
	return ESP_OK;
}

static esp_err_t req_wifi_set_country_code(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetCountryCode, resp_wifi_set_country_code,
			RpcReqWifiSetCountryCode, req_wifi_set_country_code,
			rpc__resp__wifi_set_country_code__init);

	char cc[3] = {0}; // country code
	RPC_RET_FAIL_IF(!req_payload->country.data);
	RPC_REQ_COPY_STR(&cc[0], req_payload->country, 2); // only copy the first two chars

	RPC_RET_FAIL_IF(esp_wifi_set_country_code(&cc[0],
			req_payload->ieee80211d_enabled));

	return ESP_OK;
}

static esp_err_t req_wifi_get_country_code(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiGetCountryCode, resp_wifi_get_country_code,
			RpcReqWifiGetCountryCode, req_wifi_get_country_code,
			rpc__resp__wifi_get_country_code__init);

	char cc[3] = {0}; // country code
	RPC_RET_FAIL_IF(esp_wifi_get_country_code(&cc[0]));

	RPC_RESP_COPY_STR(resp_payload->country, &cc[0], sizeof(cc));

	return ESP_OK;
}

static esp_err_t req_wifi_set_country(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiSetCountry, resp_wifi_set_country,
			RpcReqWifiSetCountry, req_wifi_set_country,
			rpc__resp__wifi_set_country__init);

	RPC_RET_FAIL_IF(!req_payload->country);

	wifi_country_t country = {0};
	WifiCountry * p_c_country = req_payload->country;
	RPC_REQ_COPY_BYTES(&country.cc[0], p_c_country->cc, sizeof(country.cc));
	country.schan        = p_c_country->schan;
	country.nchan        = p_c_country->nchan;
	country.max_tx_power = p_c_country->max_tx_power;
	country.policy       = p_c_country->policy;

	RPC_RET_FAIL_IF(esp_wifi_set_country(&country));

	return ESP_OK;
}

static esp_err_t req_wifi_get_country(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiGetCountry, resp_wifi_get_country,
			RpcReqWifiGetCountry, req_wifi_get_country,
			rpc__resp__wifi_get_country__init);

	wifi_country_t country = {0};
	RPC_RET_FAIL_IF(esp_wifi_get_country(&country));

	RPC_ALLOC_ELEMENT(WifiCountry, resp_payload->country, wifi_country__init);
	WifiCountry * p_c_country = resp_payload->country;
	RPC_RESP_COPY_BYTES(p_c_country->cc, &country.cc[0], sizeof(country.cc));
	p_c_country->schan        = country.schan;
	p_c_country->nchan        = country.nchan;
	p_c_country->max_tx_power = country.max_tx_power;
	p_c_country->policy       = country.policy;

err:
	return ESP_OK;
}

static esp_err_t req_wifi_ap_get_sta_list(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiApGetStaList, resp_wifi_ap_get_sta_list,
			RpcReqWifiApGetStaList, req_wifi_ap_get_sta_list,
			rpc__resp__wifi_ap_get_sta_list__init);

	wifi_sta_list_t sta;
	RPC_RET_FAIL_IF(esp_wifi_ap_get_sta_list(&sta));

	RPC_ALLOC_ELEMENT(WifiStaList, resp_payload->sta_list, wifi_sta_list__init);
	WifiStaList * p_c_sta_list = resp_payload->sta_list;

	resp_payload->sta_list->sta = (WifiStaInfo**)calloc(ESP_WIFI_MAX_CONN_NUM, sizeof(WifiStaInfo *));
	if (!resp_payload->sta_list->sta) {
		ESP_LOGE(TAG,"resp: malloc failed for resp_payload->sta_list->sta");
		goto err;
	}

	for (int i = 0; i < ESP_WIFI_MAX_CONN_NUM; i++) {
		RPC_ALLOC_ELEMENT(WifiStaInfo, p_c_sta_list->sta[i], wifi_sta_info__init);
		WifiStaInfo * p_c_sta_info = p_c_sta_list->sta[i];

		RPC_RESP_COPY_BYTES(p_c_sta_info->mac, &sta.sta[i].mac[0], sizeof(sta.sta[i].mac));
		p_c_sta_info->rssi = sta.sta[i].rssi;

		if (sta.sta[i].phy_11b)
			H_SET_BIT(WIFI_STA_INFO_phy_11b_BIT, p_c_sta_info->bitmask);

		if (sta.sta[i].phy_11g)
			H_SET_BIT(WIFI_STA_INFO_phy_11g_BIT, p_c_sta_info->bitmask);

		if (sta.sta[i].phy_11n)
			H_SET_BIT(WIFI_STA_INFO_phy_11n_BIT, p_c_sta_info->bitmask);

		if (sta.sta[i].phy_lr)
			H_SET_BIT(WIFI_STA_INFO_phy_lr_BIT, p_c_sta_info->bitmask);

		if (sta.sta[i].phy_11ax)
			H_SET_BIT(WIFI_STA_INFO_phy_11ax_BIT, p_c_sta_info->bitmask);

		if (sta.sta[i].is_mesh_child)
			H_SET_BIT(WIFI_STA_INFO_is_mesh_child_BIT, p_c_sta_info->bitmask);

		WIFI_STA_INFO_SET_RESERVED_VAL(sta.sta[i].reserved, p_c_sta_info->bitmask);
	}
	// number of sta records in the list
	resp_payload->sta_list->n_sta = ESP_WIFI_MAX_CONN_NUM;

	p_c_sta_list->num = sta.num;

err:
	return ESP_OK;
}

static esp_err_t req_wifi_ap_get_sta_aid(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespWifiApGetStaAid, resp_wifi_ap_get_sta_aid,
			RpcReqWifiApGetStaAid, req_wifi_ap_get_sta_aid,
			rpc__resp__wifi_ap_get_sta_aid__init);

	uint8_t mac[6];
	uint16_t aid;

	RPC_REQ_COPY_BYTES(mac, req_payload->mac, sizeof(mac));
	ESP_LOGI(TAG, "mac: " MACSTR, MAC2STR(mac));
	RPC_RET_FAIL_IF(esp_wifi_ap_get_sta_aid(mac, &aid));

	resp_payload->aid = aid;

	return ESP_OK;
}

static esp_err_t req_wifi_sta_get_rssi(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE_SIMPLE(RpcRespWifiStaGetRssi, resp_wifi_sta_get_rssi,
			RpcReqWifiStaGetRssi, req_wifi_sta_get_rssi,
			rpc__resp__wifi_sta_get_rssi__init);

	int rssi;
	RPC_RET_FAIL_IF(esp_wifi_sta_get_rssi(&rssi));

	resp_payload->rssi = rssi;

	return ESP_OK;
}

static esp_err_t req_set_dhcp_dns_status(Rpc *req, Rpc *resp, void *priv_data)
{
	RPC_TEMPLATE(RpcRespSetDhcpDnsStatus, resp_set_dhcp_dns,
			RpcReqSetDhcpDnsStatus, req_set_dhcp_dns,
			rpc__resp__set_dhcp_dns_status__init);
	uint8_t iface = req_payload->iface;
	uint8_t net_link_up = req_payload->net_link_up;
	uint8_t dhcp_up = req_payload->dhcp_up;
	uint8_t dns_up = req_payload->dns_up;
	uint8_t dns_type = req_payload->dns_type;

	char dhcp_ip[64] = {0};
	char dhcp_nm[64] = {0};
	char dhcp_gw[64] = {0};
	char dns_ip[64] = {0};

	ESP_LOGI(TAG, "iface: %u link_up:%u dhcp_up:%u dns_up:%u dns_type:%u",
			iface, net_link_up, dhcp_up, dns_up, dns_type);

	if (req_payload->dhcp_ip.len)
		ESP_LOGI(TAG, "dhcp ip: %s" , req_payload->dhcp_ip.data);
	if (req_payload->dhcp_nm.len)
		ESP_LOGI(TAG, "dhcp nm: %s" , req_payload->dhcp_nm.data);
	if (req_payload->dhcp_gw.len)
		ESP_LOGI(TAG, "dhcp gw: %s" , req_payload->dhcp_gw.data);
	if (req_payload->dns_ip.len)
		ESP_LOGI(TAG, "dns ip: %s" , req_payload->dns_ip.data);

	RPC_REQ_COPY_BYTES(dhcp_ip, req_payload->dhcp_ip, sizeof(dhcp_ip));
	RPC_REQ_COPY_BYTES(dhcp_nm, req_payload->dhcp_nm, sizeof(dhcp_nm));
	RPC_REQ_COPY_BYTES(dhcp_gw, req_payload->dhcp_gw, sizeof(dhcp_gw));
	RPC_REQ_COPY_BYTES(dns_ip, req_payload->dns_ip, sizeof(dns_ip));

	if (dhcp_up)
		set_slave_static_ip(iface, dhcp_ip, dhcp_nm, dhcp_gw);

	if (dns_up)
		set_slave_dns(iface, dns_ip, dns_type);

	return ESP_OK;
}

static esp_err_t req_USR1(Rpc *req, Rpc *resp, void *priv_data)
{
	int ret = 0;
	RPC_TEMPLATE_SIMPLE(RpcRespUSR, resp_usr1,
			RpcReqUSR, req_usr1,
			rpc__resp__usr__init);

	if (priv_data) {
		esp_err_t (*user_defined_rpc_h2s_req_handler) (Rpc *, Rpc *) = (esp_err_t (*)(Rpc*,Rpc*)) priv_data;
		ret = user_defined_rpc_h2s_req_handler(req, resp);
		if (ret)
			resp_payload->resp = ret;
	}

	return ESP_OK;
}

static esp_err_t req_USR2(Rpc *req, Rpc *resp, void *priv_data)
{
	int ret = 0;
	RPC_TEMPLATE_SIMPLE(RpcRespUSR, resp_usr2,
			RpcReqUSR, req_usr2,
			rpc__resp__usr__init);

	if (priv_data) {
		esp_err_t (*user_defined_rpc_h2s_req_handler) (Rpc *, Rpc *) = (esp_err_t (*)(Rpc*,Rpc*)) priv_data;
		ret = user_defined_rpc_h2s_req_handler(req, resp);
		if (ret)
			resp_payload->resp = ret;
	}

	return ESP_OK;
}

static esp_err_t req_USR3(Rpc *req, Rpc *resp, void *priv_data)
{
	int ret = 0;
	RPC_TEMPLATE_SIMPLE(RpcRespUSR, resp_usr3,
			RpcReqUSR, req_usr3,
			rpc__resp__usr__init);

	if (priv_data) {
		esp_err_t (*user_defined_rpc_h2s_req_handler) (Rpc *, Rpc *) = (esp_err_t (*)(Rpc*,Rpc*)) priv_data;
		ret = user_defined_rpc_h2s_req_handler(req, resp);
		if (ret)
			resp_payload->resp = ret;
	}

	return ESP_OK;
}

static esp_err_t req_USR4(Rpc *req, Rpc *resp, void *priv_data)
{
	int ret = 0;
	RPC_TEMPLATE_SIMPLE(RpcRespUSR, resp_usr4,
			RpcReqUSR, req_usr4,
			rpc__resp__usr__init);

	if (priv_data) {
		esp_err_t (*user_defined_rpc_h2s_req_handler) (Rpc *, Rpc *) = (esp_err_t (*)(Rpc*,Rpc*)) priv_data;
		ret = user_defined_rpc_h2s_req_handler(req, resp);
		if (ret)
			resp_payload->resp = ret;
	}

	return ESP_OK;
}

static esp_err_t req_USR5(Rpc *req, Rpc *resp, void *priv_data)
{
	int ret = 0;
	RPC_TEMPLATE_SIMPLE(RpcRespUSR, resp_usr5,
			RpcReqUSR, req_usr5,
			rpc__resp__usr__init);

	if (priv_data) {
		esp_err_t (*user_defined_rpc_h2s_req_handler) (Rpc *, Rpc *) = (esp_err_t (*)(Rpc*,Rpc*)) priv_data;
		ret = user_defined_rpc_h2s_req_handler(req, resp);
		if (ret)
			resp_payload->resp = ret;
	}

	return ESP_OK;
}

static esp_rpc_req_t req_table[] = {
	{
		.req_num = RPC_ID__Req_GetMACAddress ,
		.command_handler = req_wifi_get_mac
	},
	{
		.req_num = RPC_ID__Req_GetWifiMode,
		.command_handler = req_wifi_get_mode
	},
	{
		.req_num = RPC_ID__Req_SetWifiMode,
		.command_handler = req_wifi_set_mode
	},
	{
		.req_num = RPC_ID__Req_SetMacAddress,
		.command_handler = req_wifi_set_mac
	},
	{
		.req_num = RPC_ID__Req_WifiSetPs,
		.command_handler = req_wifi_set_ps
	},
	{
		.req_num = RPC_ID__Req_WifiGetPs,
		.command_handler = req_wifi_get_ps
	},
	{
		.req_num = RPC_ID__Req_OTABegin,
		.command_handler = req_ota_begin_handler
	},
	{
		.req_num = RPC_ID__Req_OTAWrite,
		.command_handler = req_ota_write_handler
	},
	{
		.req_num = RPC_ID__Req_OTAEnd,
		.command_handler = req_ota_end_handler
	},
	{
		.req_num = RPC_ID__Req_WifiSetMaxTxPower,
		.command_handler = req_wifi_set_max_tx_power
	},
	{
		.req_num = RPC_ID__Req_WifiGetMaxTxPower,
		.command_handler = req_wifi_get_max_tx_power
	},
	{
		.req_num = RPC_ID__Req_ConfigHeartbeat,
		.command_handler = req_config_heartbeat
	},
	{
		.req_num = RPC_ID__Req_WifiInit,
		.command_handler = req_wifi_init
	},
	{
		.req_num = RPC_ID__Req_WifiDeinit,
		.command_handler = req_wifi_deinit
	},
	{
		.req_num = RPC_ID__Req_WifiStart,
		.command_handler = req_wifi_start
	},
	{
		.req_num = RPC_ID__Req_WifiStop,
		.command_handler = req_wifi_stop
	},
	{
		.req_num = RPC_ID__Req_WifiConnect,
		.command_handler = req_wifi_connect
	},
	{
		.req_num = RPC_ID__Req_WifiDisconnect,
		.command_handler = req_wifi_disconnect
	},
	{
		.req_num = RPC_ID__Req_WifiSetConfig,
		.command_handler = req_wifi_set_config
	},
	{
		.req_num = RPC_ID__Req_WifiGetConfig,
		.command_handler = req_wifi_get_config
	},
	{
		.req_num = RPC_ID__Req_WifiScanStart,
		.command_handler = req_wifi_scan_start
	},
	{
		.req_num = RPC_ID__Req_WifiScanStop,
		.command_handler = req_wifi_scan_stop
	},
	{
		.req_num = RPC_ID__Req_WifiScanGetApNum,
		.command_handler = req_wifi_scan_get_ap_num
	},
	{
		.req_num = RPC_ID__Req_WifiScanGetApRecords,
		.command_handler = req_wifi_scan_get_ap_records
	},
	{
		.req_num = RPC_ID__Req_WifiClearApList,
		.command_handler = req_wifi_clear_ap_list
	},
	{
		.req_num = RPC_ID__Req_WifiRestore,
		.command_handler = req_wifi_restore
	},
	{
		.req_num = RPC_ID__Req_WifiClearFastConnect,
		.command_handler = req_wifi_clear_fast_connect
	},
	{
		.req_num = RPC_ID__Req_WifiStaGetApInfo,
		.command_handler = req_wifi_sta_get_ap_info
	},
	{
		.req_num = RPC_ID__Req_WifiDeauthSta,
		.command_handler = req_wifi_deauth_sta
	},
	{
		.req_num = RPC_ID__Req_WifiSetStorage,
		.command_handler = req_wifi_set_storage
	},
	{
		.req_num = RPC_ID__Req_WifiSetProtocol,
		.command_handler = req_wifi_set_protocol
	},
	{
		.req_num = RPC_ID__Req_WifiGetProtocol,
		.command_handler = req_wifi_get_protocol
	},
	{
		.req_num = RPC_ID__Req_WifiSetBandwidth,
		.command_handler = req_wifi_set_bandwidth
	},
	{
		.req_num = RPC_ID__Req_WifiGetBandwidth,
		.command_handler = req_wifi_get_bandwidth
	},
	{
		.req_num = RPC_ID__Req_WifiSetChannel,
		.command_handler = req_wifi_set_channel
	},
	{
		.req_num = RPC_ID__Req_WifiGetChannel,
		.command_handler = req_wifi_get_channel
	},
	{
		.req_num = RPC_ID__Req_WifiSetCountryCode,
		.command_handler = req_wifi_set_country_code
	},
	{
		.req_num = RPC_ID__Req_WifiGetCountryCode,
		.command_handler = req_wifi_get_country_code
	},
	{
		.req_num = RPC_ID__Req_WifiSetCountry,
		.command_handler = req_wifi_set_country
	},
	{
		.req_num = RPC_ID__Req_WifiGetCountry,
		.command_handler = req_wifi_get_country
	},
	{
		.req_num = RPC_ID__Req_WifiApGetStaList,
		.command_handler = req_wifi_ap_get_sta_list
	},
	{
		.req_num = RPC_ID__Req_WifiApGetStaAid,
		.command_handler = req_wifi_ap_get_sta_aid
	},
	{
		.req_num = RPC_ID__Req_WifiStaGetRssi,
		.command_handler = req_wifi_sta_get_rssi
	},
	{
		.req_num = RPC_ID__Req_SetDhcpDnsStatus,
		.command_handler = req_set_dhcp_dns_status
	},
	{
		.req_num = RPC_ID__Req_USR1,
		.command_handler = req_USR1
	},
	{
		.req_num = RPC_ID__Req_USR2,
		.command_handler = req_USR2
	},
	{
		.req_num = RPC_ID__Req_USR3,
		.command_handler = req_USR3
	},
	{
		.req_num = RPC_ID__Req_USR4,
		.command_handler = req_USR4
	},
	{
		.req_num = RPC_ID__Req_USR5,
		.command_handler = req_USR5
	},

};


static int lookup_req_handler(int req_id)
{
	for (int i = 0; i < sizeof(req_table)/sizeof(esp_rpc_req_t); i++) {
		if (req_table[i].req_num == req_id) {
			return i;
		}
	}
	return -1;
}

static esp_err_t esp_rpc_command_dispatcher(
		Rpc *req, Rpc *resp,
		void *priv_data)
{
	esp_err_t ret = ESP_OK;
	int req_index = 0;

	if (!req || !resp) {
		ESP_LOGE(TAG, "Invalid parameters in command");
		return ESP_FAIL;
	}

	if ((req->msg_id <= RPC_ID__Req_Base) ||
		(req->msg_id >= RPC_ID__Req_Max)) {
		ESP_LOGE(TAG, "Invalid command request lookup");
	}

	ESP_LOGI(TAG, "Received Req [0x%x]", req->msg_id);

	req_index = lookup_req_handler(req->msg_id);
	if (req_index < 0) {
		ESP_LOGE(TAG, "Invalid command handler lookup");
		return ESP_FAIL;
	}

	ret = req_table[req_index].command_handler(req, resp, priv_data);
	if (ret) {
		ESP_LOGE(TAG, "Error executing command handler");
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* TODO: Is this really required? Can't just rpc__free_unpacked(resp, NULL); would do? */
static void esp_rpc_cleanup(Rpc *resp)
{
	if (!resp) {
		return;
	}

	switch (resp->msg_id) {
		case (RPC_ID__Resp_GetMACAddress ) : {
			mem_free(resp->resp_get_mac_address->mac.data);
			mem_free(resp->resp_get_mac_address);
			break;
		} case (RPC_ID__Resp_GetWifiMode) : {
			mem_free(resp->resp_get_wifi_mode);
			break;
		} case (RPC_ID__Resp_SetWifiMode ) : {
			mem_free(resp->resp_set_wifi_mode);
			break;
		} case (RPC_ID__Resp_SetMacAddress) : {
			mem_free(resp->resp_set_mac_address);
			break;
		} case (RPC_ID__Resp_WifiSetPs) : {
			mem_free(resp->resp_wifi_set_ps);
			break;
		} case (RPC_ID__Resp_WifiGetPs) : {
			mem_free(resp->resp_wifi_get_ps);
			break;
		} case (RPC_ID__Resp_OTABegin) : {
			mem_free(resp->resp_ota_begin);
			break;
		} case (RPC_ID__Resp_OTAWrite) : {
			mem_free(resp->resp_ota_write);
			break;
		} case (RPC_ID__Resp_OTAEnd) : {
			mem_free(resp->resp_ota_end);
			break;
#if 0
		} case (RPC_ID__Resp_SetSoftAPVendorSpecificIE) : {
			mem_free(resp->resp_set_softap_vendor_specific_ie);
			break;
#endif
		} case (RPC_ID__Resp_WifiSetMaxTxPower) : {
			mem_free(resp->resp_set_wifi_max_tx_power);
			break;
		} case (RPC_ID__Resp_WifiGetMaxTxPower) : {
			mem_free(resp->resp_get_wifi_max_tx_power);
			break;
		} case (RPC_ID__Resp_ConfigHeartbeat) : {
			mem_free(resp->resp_config_heartbeat);
			break;
		} case (RPC_ID__Resp_WifiInit) : {
			mem_free(resp->resp_wifi_init);
			break;
		} case (RPC_ID__Resp_WifiDeinit) : {
			mem_free(resp->resp_wifi_deinit);
			break;
		} case (RPC_ID__Resp_WifiStart) : {
			mem_free(resp->resp_wifi_start);
			break;
		} case (RPC_ID__Resp_WifiStop) : {
			mem_free(resp->resp_wifi_stop);
			break;
		} case (RPC_ID__Resp_WifiConnect) : {
			mem_free(resp->resp_wifi_connect);
			break;
		} case (RPC_ID__Resp_WifiDisconnect) : {
			mem_free(resp->resp_wifi_disconnect);
			break;
		} case (RPC_ID__Resp_WifiSetConfig) : {
			mem_free(resp->resp_wifi_set_config);
			break;
		} case (RPC_ID__Resp_WifiGetConfig) : {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_get_config->cfg) {
				if (resp->resp_wifi_get_config->iface == WIFI_IF_STA) {
					mem_free(resp->resp_wifi_get_config->cfg->sta->ssid.data);
					mem_free(resp->resp_wifi_get_config->cfg->sta->password.data);
					mem_free(resp->resp_wifi_get_config->cfg->sta->bssid.data);
					mem_free(resp->resp_wifi_get_config->cfg->sta->threshold);
					mem_free(resp->resp_wifi_get_config->cfg->sta->pmf_cfg);
					mem_free(resp->resp_wifi_get_config->cfg->sta);
				} else if (resp->resp_wifi_get_config->iface == WIFI_IF_AP) {
					mem_free(resp->resp_wifi_get_config->cfg->ap->ssid.data);
					mem_free(resp->resp_wifi_get_config->cfg->ap->password.data);
					mem_free(resp->resp_wifi_get_config->cfg->ap->pmf_cfg);
					mem_free(resp->resp_wifi_get_config->cfg->ap);
				}
				mem_free(resp->resp_wifi_get_config->cfg);
			}
			mem_free(resp->resp_wifi_get_config);
			break;

		} case RPC_ID__Resp_WifiScanStart: {
			mem_free(resp->resp_wifi_scan_start);
			break;
		} case RPC_ID__Resp_WifiScanStop: {
			mem_free(resp->resp_wifi_scan_stop);
			break;
		} case RPC_ID__Resp_WifiScanGetApNum: {
			mem_free(resp->resp_wifi_scan_get_ap_num);
			break;
		} case RPC_ID__Resp_WifiScanGetApRecords: {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_scan_get_ap_records->ap_records)
				for (int i=0 ; i<resp->resp_wifi_scan_get_ap_records->n_ap_records; i++) {
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]->ssid.data);
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]->bssid.data);
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]->country->cc.data);
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]->country);
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]->he_ap);
					mem_free(resp->resp_wifi_scan_get_ap_records->ap_records[i]);
				}
			if (resp->resp_wifi_scan_get_ap_records->ap_records)
				mem_free(resp->resp_wifi_scan_get_ap_records->ap_records);
			mem_free(resp->resp_wifi_scan_get_ap_records);
			break;
		} case RPC_ID__Resp_WifiClearApList: {
			mem_free(resp->resp_wifi_clear_ap_list);
			break;
		} case RPC_ID__Resp_WifiRestore: {
			mem_free(resp->resp_wifi_restore);
			break;
		} case RPC_ID__Resp_WifiClearFastConnect: {
			mem_free(resp->resp_wifi_clear_fast_connect);
			break;
		} case RPC_ID__Resp_WifiStaGetApInfo: {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_sta_get_ap_info->ap_records) {
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records->ssid.data);
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records->bssid.data);
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records->country->cc.data);
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records->country);
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records->he_ap);
				mem_free(resp->resp_wifi_sta_get_ap_info->ap_records);
			}
			mem_free(resp->resp_wifi_sta_get_ap_info);
			break;
		} case RPC_ID__Resp_WifiDeauthSta: {
			mem_free(resp->resp_wifi_deauth_sta);
			break;
		} case RPC_ID__Resp_WifiSetStorage: {
			mem_free(resp->resp_wifi_set_storage);
			break;
		} case RPC_ID__Resp_WifiSetProtocol: {
			mem_free(resp->resp_wifi_set_protocol);
			break;
		} case RPC_ID__Resp_WifiGetProtocol: {
			mem_free(resp->resp_wifi_get_protocol);
			break;
		} case RPC_ID__Resp_WifiSetBandwidth: {
			mem_free(resp->resp_wifi_set_bandwidth);
			break;
		} case RPC_ID__Resp_WifiGetBandwidth: {
			mem_free(resp->resp_wifi_get_bandwidth);
			break;
		} case RPC_ID__Resp_WifiSetChannel: {
			mem_free(resp->resp_wifi_set_channel);
			break;
		} case RPC_ID__Resp_WifiGetChannel: {
			mem_free(resp->resp_wifi_get_channel);
			break;
		} case RPC_ID__Resp_WifiSetCountryCode: {
			mem_free(resp->resp_wifi_set_country_code);
			break;
		} case RPC_ID__Resp_WifiGetCountryCode: {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_get_country_code->country.data)
				mem_free(resp->resp_wifi_get_country_code->country.data);
			mem_free(resp->resp_wifi_get_country_code);
			break;
		} case RPC_ID__Resp_WifiSetCountry: {
			mem_free(resp->resp_wifi_set_country);
			break;
		} case RPC_ID__Resp_WifiGetCountry: {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_get_country->country)
				mem_free(resp->resp_wifi_get_country->country);
			mem_free(resp->resp_wifi_get_country);
			break;
		} case RPC_ID__Resp_WifiApGetStaList: {
			// if req failed, internals of resp msg may not have been allocated
			if (resp->resp_wifi_ap_get_sta_list->sta_list) {
				for(int i = 0; i < ESP_WIFI_MAX_CONN_NUM; i++) {
					mem_free(resp->resp_wifi_ap_get_sta_list->sta_list->sta[i]->mac.data);
					mem_free(resp->resp_wifi_ap_get_sta_list->sta_list->sta[i]);
				}
				mem_free(resp->resp_wifi_ap_get_sta_list->sta_list);
			}
			mem_free(resp->resp_wifi_ap_get_sta_list);
			break;
		} case RPC_ID__Resp_WifiApGetStaAid: {
			mem_free(resp->resp_wifi_ap_get_sta_aid);
			break;
		} case RPC_ID__Resp_WifiStaGetRssi: {
			mem_free(resp->resp_wifi_sta_get_rssi);
			break;
		} case RPC_ID__Resp_SetDhcpDnsStatus: {
			mem_free(resp->resp_set_dhcp_dns);
			break;
		} case RPC_ID__Resp_USR1: {
			mem_free(resp->resp_usr1->data.data);
			mem_free(resp->resp_usr1);
			break;
		} case RPC_ID__Resp_USR2: {
			mem_free(resp->resp_usr2->data.data);
			mem_free(resp->resp_usr2);
			break;
		} case RPC_ID__Resp_USR3: {
			mem_free(resp->resp_usr3->data.data);
			mem_free(resp->resp_usr3);
			break;
		} case RPC_ID__Resp_USR4: {
			mem_free(resp->resp_usr4->data.data);
			mem_free(resp->resp_usr4);
			break;
		} case RPC_ID__Resp_USR5: {
			mem_free(resp->resp_usr5->data.data);
			mem_free(resp->resp_usr5);
			break;
		} case (RPC_ID__Event_ESPInit) : {
			mem_free(resp->event_esp_init);
			break;
		} case (RPC_ID__Event_Heartbeat) : {
			mem_free(resp->event_heartbeat);
			break;
		} case (RPC_ID__Event_AP_StaConnected) : {
			//mem_free(resp->event_ap_sta_connected->mac.data);
			mem_free(resp->event_ap_sta_connected);
			break;
		} case (RPC_ID__Event_AP_StaDisconnected) : {
			//mem_free(resp->event_ap_sta_disconnected->mac.data);
			mem_free(resp->event_ap_sta_disconnected);
			break;
		} case (RPC_ID__Event_StaScanDone) : {
			mem_free(resp->event_sta_scan_done->scan_done);
			mem_free(resp->event_sta_scan_done);
			break;
		} case (RPC_ID__Event_StaConnected) : {
			mem_free(resp->event_sta_connected->sta_connected);
			mem_free(resp->event_sta_connected);
			break;
		} case (RPC_ID__Event_StaDisconnected) : {
			mem_free(resp->event_sta_disconnected->sta_disconnected);
			mem_free(resp->event_sta_disconnected);
			break;
		} case RPC_ID__Event_WifiEventNoArgs: {
			mem_free(resp->event_wifi_event_no_args);
			break;
		} case RPC_ID__Event_SetDhcpDnsStatus: {
			mem_free(resp->event_set_dhcp_dns);
			break;
		} case RPC_ID__Event_USR1: {
			mem_free(resp->event_usr1);
			break;
		} case RPC_ID__Event_USR2: {
			mem_free(resp->event_usr2);
			break;
		} case RPC_ID__Event_USR3: {
			mem_free(resp->event_usr3);
			break;
		} case RPC_ID__Event_USR4: {
			mem_free(resp->event_usr4);
			break;
		} case RPC_ID__Event_USR5: {
			mem_free(resp->event_usr5);
			break;
		} default: {
			ESP_LOGE(TAG, "Unsupported Rpc type[%u]",resp->msg_id);
			break;
		}
	}
}

esp_err_t data_transfer_handler(uint32_t session_id,const uint8_t *inbuf,
		ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
	Rpc *req = NULL, resp = {0};
	esp_err_t ret = ESP_OK;

	if (!inbuf || !outbuf || !outlen) {
		ESP_LOGE(TAG,"Buffers are NULL");
		return ESP_FAIL;
	}

	req = rpc__unpack(NULL, inlen, inbuf);
	if (!req) {
		ESP_LOGE(TAG, "Unable to unpack config data");
		return ESP_FAIL;
	}

	rpc__init (&resp);
	resp.msg_type = RPC_TYPE__Resp;
	resp.msg_id = req->msg_id - RPC_ID__Req_Base + RPC_ID__Resp_Base;
	resp.uid = req->uid;
	resp.payload_case = resp.msg_id;
	ESP_LOGI(TAG, "Resp_MSGId for req[0x%x] is [0x%x], uid %ld", req->msg_id, resp.msg_id, resp.uid);
	ret = esp_rpc_command_dispatcher(req, &resp, priv_data);
	if (ret) {
		ESP_LOGE(TAG, "Command dispatching not happening");
		goto err;
	}

	rpc__free_unpacked(req, NULL);

	*outlen = rpc__get_packed_size (&resp);
	if (*outlen <= 0) {
		ESP_LOGE(TAG, "Invalid encoding for response");
		goto err;
	}

	*outbuf = (uint8_t *)calloc(1, *outlen);
	if (!*outbuf) {
		ESP_LOGE(TAG, "No memory allocated for outbuf");
		esp_rpc_cleanup(&resp);
		return ESP_ERR_NO_MEM;
	}

	rpc__pack (&resp, *outbuf);

	//printf("Resp outbuf:\n");
	//ESP_LOG_BUFFER_HEXDUMP("Resp outbuf", *outbuf, *outlen, ESP_LOG_INFO);

	esp_rpc_cleanup(&resp);
	return ESP_OK;

err:
	esp_rpc_cleanup(&resp);
	return ESP_FAIL;
}

/* Function ESPInit Notification */
static esp_err_t rpc_evt_ESPInit(Rpc *ntfy)
{
	RpcEventESPInit *ntfy_payload = NULL;

	ESP_LOGI(TAG,"event ESPInit");
	ntfy_payload = (RpcEventESPInit *)
		calloc(1,sizeof(RpcEventESPInit));
	if (!ntfy_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}
	rpc__event__espinit__init(ntfy_payload);
	ntfy->payload_case = RPC__PAYLOAD_EVENT_ESP_INIT;
	ntfy->event_esp_init = ntfy_payload;

	return ESP_OK;
}

static esp_err_t rpc_evt_heartbeat(Rpc *ntfy)
{
	RpcEventHeartbeat *ntfy_payload = NULL;


	ntfy_payload = (RpcEventHeartbeat*)
		calloc(1,sizeof(RpcEventHeartbeat));
	if (!ntfy_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}
	rpc__event__heartbeat__init(ntfy_payload);

	ntfy_payload->hb_num = hb_num;

	ntfy->payload_case = RPC__PAYLOAD_EVENT_HEARTBEAT;
	ntfy->event_heartbeat = ntfy_payload;

	return ESP_OK;

}

static esp_err_t rpc_evt_sta_scan_done(Rpc *ntfy,
		const uint8_t *data, ssize_t len, int event_id)
{
	WifiEventStaScanDone *p_c_scan = NULL;
	wifi_event_sta_scan_done_t * p_a = (wifi_event_sta_scan_done_t*)data;
	NTFY_TEMPLATE(RPC_ID__Event_StaScanDone,
			RpcEventStaScanDone, event_sta_scan_done,
			rpc__event__sta_scan_done__init);

	NTFY_ALLOC_ELEMENT(WifiEventStaScanDone, ntfy_payload->scan_done,
			wifi_event_sta_scan_done__init);
	p_c_scan = ntfy_payload->scan_done;

	p_c_scan->status = p_a->status;
	p_c_scan->number = p_a->number;
	p_c_scan->scan_id = p_a->scan_id;

err:
	return ESP_OK;
}

static esp_err_t rpc_evt_sta_connected(Rpc *ntfy,
		const uint8_t *data, ssize_t len, int event_id)
{
	WifiEventStaConnected *p_c = NULL;
	wifi_event_sta_connected_t * p_a = (wifi_event_sta_connected_t*)data;
	NTFY_TEMPLATE(RPC_ID__Event_StaConnected,
			RpcEventStaConnected, event_sta_connected,
			rpc__event__sta_connected__init);

	NTFY_ALLOC_ELEMENT(WifiEventStaConnected, ntfy_payload->sta_connected,
			wifi_event_sta_connected__init);

	p_c = ntfy_payload->sta_connected;

	// alloc not needed for ssid
	p_c->ssid.data = p_a->ssid;
	p_c->ssid.len = sizeof(p_a->ssid);

	p_c->ssid_len = p_a->ssid_len;

	// alloc not needed for bssid
	p_c->bssid.data = p_a->bssid;
	p_c->bssid.len = sizeof(p_a->bssid);

	p_c->channel = p_a->channel;
	p_c->authmode = p_a->authmode;
	p_c->aid = p_a->aid;

err:
	return ESP_OK;
}

static esp_err_t rpc_evt_sta_disconnected(Rpc *ntfy,
		const uint8_t *data, ssize_t len, int event_id)
{
	WifiEventStaDisconnected *p_c = NULL;
	wifi_event_sta_disconnected_t * p_a = (wifi_event_sta_disconnected_t*)data;
	NTFY_TEMPLATE(RPC_ID__Event_StaDisconnected,
			RpcEventStaDisconnected, event_sta_disconnected,
			rpc__event__sta_disconnected__init);

	NTFY_ALLOC_ELEMENT(WifiEventStaDisconnected, ntfy_payload->sta_disconnected,
			wifi_event_sta_disconnected__init);

	p_c = ntfy_payload->sta_disconnected;

	// alloc not needed for ssid
	p_c->ssid.data = p_a->ssid;
	p_c->ssid.len = sizeof(p_a->ssid);

	p_c->ssid_len = p_a->ssid_len;

	// alloc not needed for bssid:
	p_c->bssid.data = p_a->bssid;
	p_c->bssid.len = sizeof(p_a->bssid);

	p_c->reason = p_a->reason;
	p_c->rssi = p_a->rssi;

err:
	return ESP_OK;
}

static esp_err_t rpc_evt_ap_staconn_conn_disconn(Rpc *ntfy,
		const uint8_t *data, ssize_t len, int event_id)
{
	/* TODO: use NTFY_TEMPLATE */
	ESP_LOGD(TAG, "%s event:%u",__func__,event_id);
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {

		RpcEventAPStaConnected *ntfy_payload = NULL;
		wifi_event_ap_staconnected_t * p_a = (wifi_event_ap_staconnected_t *)data;

		ntfy_payload = (RpcEventAPStaConnected*)
			calloc(1,sizeof(RpcEventAPStaConnected));
		if (!ntfy_payload) {
			ESP_LOGE(TAG,"Failed to allocate memory");
			return ESP_ERR_NO_MEM;
		}
		rpc__event__ap__sta_connected__init(ntfy_payload);

		ntfy->payload_case = RPC__PAYLOAD_EVENT_AP_STA_CONNECTED;
		ntfy->event_ap_sta_connected = ntfy_payload;
		ntfy_payload->aid = p_a->aid;
		ntfy_payload->mac.len = BSSID_BYTES_SIZE;
		ntfy_payload->is_mesh_child = p_a->is_mesh_child;

		ntfy_payload->mac.data = p_a->mac;
		//ntfy_payload->mac.data = (uint8_t *)calloc(1, BSSID_BYTES_SIZE);
		//memcpy(ntfy_payload->mac.data,p_a->mac,BSSID_BYTES_SIZE);
		ntfy_payload->resp = SUCCESS;
		return ESP_OK;

	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		RpcEventAPStaDisconnected *ntfy_payload = NULL;
		wifi_event_ap_stadisconnected_t * p_a = (wifi_event_ap_stadisconnected_t *)data;

		ntfy_payload = (RpcEventAPStaDisconnected*)
			calloc(1,sizeof(RpcEventAPStaDisconnected));
		if (!ntfy_payload) {
			ESP_LOGE(TAG,"Failed to allocate memory");
			return ESP_ERR_NO_MEM;
		}
		rpc__event__ap__sta_disconnected__init(ntfy_payload);

		ntfy->payload_case = RPC__PAYLOAD_EVENT_AP_STA_DISCONNECTED;
		ntfy->event_ap_sta_disconnected = ntfy_payload;
		ntfy_payload->aid = p_a->aid;
		ntfy_payload->mac.len = BSSID_BYTES_SIZE;
		ntfy_payload->is_mesh_child = p_a->is_mesh_child;
		ntfy_payload->reason = p_a->reason;

		//Note: alloc is not needed in this case
		//ntfy_payload->mac.data = (uint8_t *)calloc(1, BSSID_BYTES_SIZE);
		//memcpy(ntfy_payload->mac.data,p_a->mac,BSSID_BYTES_SIZE);
		ntfy_payload->mac.data = p_a->mac;
		ntfy_payload->resp = SUCCESS;
		return ESP_OK;
	}
	return ESP_FAIL;
}

static esp_err_t rpc_evt_Event_WifiEventNoArgs(Rpc *ntfy,
		const uint8_t *data, ssize_t len)
{
	RpcEventWifiEventNoArgs *ntfy_payload = NULL;
	int32_t event_id = 0;

	ntfy_payload = (RpcEventWifiEventNoArgs*)
		calloc(1,sizeof(RpcEventWifiEventNoArgs));
	if (!ntfy_payload) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}
	rpc__event__wifi_event_no_args__init(ntfy_payload);

	ntfy->payload_case = RPC__PAYLOAD_EVENT_WIFI_EVENT_NO_ARGS;
	ntfy->event_wifi_event_no_args = ntfy_payload;

	event_id = (int32_t)*data;
	ESP_LOGI(TAG, "Sending Wi-Fi event [%ld]", event_id);

	ntfy_payload->event_id = event_id;

	ntfy_payload->resp = SUCCESS;
	return ESP_OK;
}


#ifdef CONFIG_SLAVE_LWIP_ENABLED
static esp_err_t rpc_evt_Event_SetDhcpDnsStatus(Rpc *ntfy,
		const uint8_t *data, ssize_t len)
{
	rpc_set_dhcp_dns_status_t * p_a = (rpc_set_dhcp_dns_status_t*)data;
	RpcEventSetDhcpDnsStatus *p_c = NULL;

	p_c = (RpcEventSetDhcpDnsStatus*)
		calloc(1,sizeof(RpcEventSetDhcpDnsStatus));
	if (!p_c) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}

	rpc__event__set_dhcp_dns_status__init(p_c);

	ntfy->payload_case = RPC__PAYLOAD_EVENT_SET_DHCP_DNS;
	ntfy->event_set_dhcp_dns = p_c;

	p_c->iface = p_a->iface;
	p_c->net_link_up = p_a->net_link_up;
	p_c->dhcp_up = p_a->dhcp_up;
	p_c->dns_up = p_a->dns_up;
	p_c->dns_type = p_a->dns_type;

	p_c->dhcp_ip.data = p_a->dhcp_ip;
	p_c->dhcp_ip.len = sizeof(p_a->dhcp_ip);
	p_c->dhcp_nm.data = p_a->dhcp_nm;
	p_c->dhcp_nm.len = sizeof(p_a->dhcp_nm);
	p_c->dhcp_gw.data = p_a->dhcp_gw;
	p_c->dhcp_gw.len = sizeof(p_a->dhcp_gw);
	p_c->dns_ip.data = p_a->dns_ip;
	p_c->dns_ip.len = sizeof(p_a->dns_ip);

	p_c->resp = SUCCESS;
	return ESP_OK;
}
#endif

static esp_err_t rpc_evt_Event_USR(Rpc *ntfy,
		const uint8_t *data, ssize_t len, uint8_t usr_msg_num)
{
	struct rpc_user_specific_event_t * p_a = (struct rpc_user_specific_event_t*) data;
	RpcEventUSR *p_c = NULL;

	p_c = (RpcEventUSR*)
		calloc(1,sizeof(RpcEventUSR));
	if (!p_c) {
		ESP_LOGE(TAG,"Failed to allocate memory");
		return ESP_ERR_NO_MEM;
	}

	rpc__event__usr__init(p_c);

	switch (usr_msg_num) {
		case 1:
			ntfy->payload_case = RPC__PAYLOAD_EVENT_USR1;
			ntfy->event_usr1 = p_c;
			break;
		case 2:
			ntfy->payload_case = RPC__PAYLOAD_EVENT_USR2;
			ntfy->event_usr2 = p_c;
			break;
		case 3:
			ntfy->payload_case = RPC__PAYLOAD_EVENT_USR3;
			ntfy->event_usr3 = p_c;
			break;
		case 4:
			ntfy->payload_case = RPC__PAYLOAD_EVENT_USR4;
			ntfy->event_usr4 = p_c;
			break;
		case 5:
			ntfy->payload_case = RPC__PAYLOAD_EVENT_USR5;
			ntfy->event_usr5 = p_c;
			break;
		default:
			break;
	}

	p_c->int_1 = p_a->int_1;
	p_c->int_2 = p_a->int_2;
	p_c->uint_1 = p_a->uint_1;
	p_c->uint_2 = p_a->uint_2;
	p_c->resp = p_a->resp;
	p_c->data.data = p_a->data;
	p_c->data.len = p_a->data_len;

	return ESP_OK;
}

esp_err_t rpc_evt_handler(uint32_t session_id,const uint8_t *inbuf,
		ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
	Rpc ntfy = {0};
	int ret = SUCCESS;

	if (!outbuf || !outlen) {
		ESP_LOGE(TAG,"Buffers are NULL");
		return ESP_FAIL;
	}

	rpc__init (&ntfy);
	ntfy.msg_id = session_id;
	ntfy.msg_type = RPC_TYPE__Event;

	switch ((int)ntfy.msg_id) {
		case RPC_ID__Event_ESPInit : {
			ret = rpc_evt_ESPInit(&ntfy);
			break;
		} case RPC_ID__Event_Heartbeat: {
			ret = rpc_evt_heartbeat(&ntfy);
			break;
		} case RPC_ID__Event_AP_StaConnected: {
			ret = rpc_evt_ap_staconn_conn_disconn(&ntfy, inbuf, inlen, WIFI_EVENT_AP_STACONNECTED);
			break;
		} case RPC_ID__Event_AP_StaDisconnected: {
			ret = rpc_evt_ap_staconn_conn_disconn(&ntfy, inbuf, inlen, WIFI_EVENT_AP_STADISCONNECTED);
			break;
		} case RPC_ID__Event_StaScanDone: {
			ret = rpc_evt_sta_scan_done(&ntfy, inbuf, inlen, WIFI_EVENT_SCAN_DONE);
			break;
		} case RPC_ID__Event_StaConnected: {
			ret = rpc_evt_sta_connected(&ntfy, inbuf, inlen, WIFI_EVENT_STA_CONNECTED);
			break;
		} case RPC_ID__Event_StaDisconnected: {
			ret = rpc_evt_sta_disconnected(&ntfy, inbuf, inlen, WIFI_EVENT_STA_DISCONNECTED);
			break;
		} case RPC_ID__Event_WifiEventNoArgs: {
			ret = rpc_evt_Event_WifiEventNoArgs(&ntfy, inbuf, inlen);
			break;
#ifdef CONFIG_SLAVE_LWIP_ENABLED
		} case RPC_ID__Event_SetDhcpDnsStatus: {
			ret = rpc_evt_Event_SetDhcpDnsStatus(&ntfy, inbuf, inlen);
			break;
#endif
		} case RPC_ID__Event_USR1: {
			ret = rpc_evt_Event_USR(&ntfy, inbuf, inlen, 1);
			break;
		} case RPC_ID__Event_USR2: {
			ret = rpc_evt_Event_USR(&ntfy, inbuf, inlen, 2);
			break;
		} case RPC_ID__Event_USR3: {
			ret = rpc_evt_Event_USR(&ntfy, inbuf, inlen, 3);
			break;
		} case RPC_ID__Event_USR4: {
			ret = rpc_evt_Event_USR(&ntfy, inbuf, inlen, 4);
			break;
		} case RPC_ID__Event_USR5: {
			ret = rpc_evt_Event_USR(&ntfy, inbuf, inlen, 5);
			break;
		} default: {
			ESP_LOGE(TAG, "Incorrect/unsupported Ctrl Notification[%u]\n",ntfy.msg_id);
			goto err;
			break;
		}
	}

	if (ret) {
		ESP_LOGI(TAG, "notification[%u] not sent\n", ntfy.msg_id);
		goto err;
	}

	*outlen = rpc__get_packed_size (&ntfy);
	if (*outlen <= 0) {
		ESP_LOGE(TAG, "Invalid encoding for notify");
		goto err;
	}

	*outbuf = (uint8_t *)calloc(1, *outlen);
	if (!*outbuf) {
		ESP_LOGE(TAG, "No memory allocated for outbuf");
		esp_rpc_cleanup(&ntfy);
		return ESP_ERR_NO_MEM;
	}

	rpc__pack (&ntfy, *outbuf);

	//printf("event outbuf:\n");
	//ESP_LOG_BUFFER_HEXDUMP("event outbuf", *outbuf, *outlen, ESP_LOG_INFO);

	esp_rpc_cleanup(&ntfy);
	return ESP_OK;

err:
	if (!*outbuf) {
		free(*outbuf);
		*outbuf = NULL;
	}
	esp_rpc_cleanup(&ntfy);
	return ESP_FAIL;
}
