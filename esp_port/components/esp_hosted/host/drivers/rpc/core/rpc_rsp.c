// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#include "rpc_core.h"
#include "rpc_slave_if.h"
#include "adapter.h"
#include "esp_log.h"

DEFINE_LOG_TAG(rpc_rsp);

/* RPC response is result of remote function invokation at slave from host
 * The response will contain the return values of the RPC procedure
 * Return values typically will be simple integer return value of rpc call
 * for simple procedures. For function call with return value as a parameter,
 * RPC will contain full structure returned for that parameter and wrapper
 * level above will return these in expected pointer
 *
 * Responses will typically have two levels:
 * 1. protobuf level response received
 * 2. parse the response so that Ctrl_cmd_t app structure will be populated
 * or parsed from protobuf level response.
 *
 * For new RPC request, add up switch case for your message
 * For altogether new RPC function addition, please check
 * esp_hosted_fg/common/proto/esp_hosted_config.proto as a start point
 */

#define RPC_ERR_IN_RESP(msGparaM)                                             \
    if (rpc_msg->msGparaM->resp) {                                            \
        app_resp->resp_event_status = rpc_msg->msGparaM->resp;                \
        ESP_LOGE(TAG, "Failure resp/event: possibly precondition not met");   \
        goto fail_parse_rpc_msg;                                              \
    }


#define RPC_RSP_COPY_BYTES(dst,src) {                                         \
    if (src.data && src.len) {                                                \
        g_h.funcs->_h_memcpy(dst, src.data, src.len);                         \
    }                                                                         \
}

/* This will copy rpc response from `Rpc` into
 * application structure `ctrl_cmd_t`
 * This function is called after protobuf decoding is successful
 **/
int rpc_parse_rsp(Rpc *rpc_msg, ctrl_cmd_t *app_resp)
{
	uint16_t i = 0;

	/* 1. Check non NULL */
	if (!rpc_msg || !app_resp) {
		ESP_LOGE(TAG, "NULL rpc resp or NULL App Resp");
		goto fail_parse_rpc_msg;
	}

	/* 2. update basic fields */
	app_resp->msg_type = RPC_TYPE__Resp;
	app_resp->msg_id = rpc_msg->msg_id;
	app_resp->uid = rpc_msg->uid;
	ESP_LOGI(TAG, " --> RPC_Resp [0x%x], uid %ld", app_resp->msg_id, app_resp->uid);

	/* 3. parse Rpc into ctrl_cmd_t */
	switch (rpc_msg->msg_id) {

	case RPC_ID__Resp_GetMACAddress : {
		RPC_FAIL_ON_NULL(resp_get_mac_address);
		RPC_ERR_IN_RESP(resp_get_mac_address);
		RPC_FAIL_ON_NULL(resp_get_mac_address->mac.data);

		RPC_RSP_COPY_BYTES(app_resp->u.wifi_mac.mac, rpc_msg->resp_get_mac_address->mac);
		ESP_LOGD(TAG, "Mac addr: "MACSTR, MAC2STR(app_resp->u.wifi_mac.mac));
		break;
	} case RPC_ID__Resp_SetMacAddress : {
		RPC_FAIL_ON_NULL(resp_set_mac_address);
		RPC_ERR_IN_RESP(resp_set_mac_address);
		break;
	} case RPC_ID__Resp_GetWifiMode : {
		RPC_FAIL_ON_NULL(resp_get_wifi_mode);
		RPC_ERR_IN_RESP(resp_get_wifi_mode);

		app_resp->u.wifi_mode.mode = rpc_msg->resp_get_wifi_mode->mode;
		break;
	} case RPC_ID__Resp_SetWifiMode : {
		RPC_FAIL_ON_NULL(resp_set_wifi_mode);
		RPC_ERR_IN_RESP(resp_set_wifi_mode);
		break;
	} case RPC_ID__Resp_WifiSetPs: {
		RPC_FAIL_ON_NULL(resp_wifi_set_ps);
		RPC_ERR_IN_RESP(resp_wifi_set_ps);
		break;
	} case RPC_ID__Resp_WifiGetPs : {
		RPC_FAIL_ON_NULL(resp_wifi_get_ps);
		RPC_ERR_IN_RESP(resp_wifi_get_ps);
		app_resp->u.wifi_ps.ps_mode = rpc_msg->resp_wifi_get_ps->type;
		break;
	} case RPC_ID__Resp_OTABegin : {
		RPC_FAIL_ON_NULL(resp_ota_begin);
		RPC_ERR_IN_RESP(resp_ota_begin);
		if (rpc_msg->resp_ota_begin->resp) {
			ESP_LOGE(TAG, "OTA Begin Failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_OTAWrite : {
		RPC_FAIL_ON_NULL(resp_ota_write);
		RPC_ERR_IN_RESP(resp_ota_write);
		if (rpc_msg->resp_ota_write->resp) {
			ESP_LOGE(TAG, "OTA write failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_OTAEnd: {
		RPC_FAIL_ON_NULL(resp_ota_end);
		if (rpc_msg->resp_ota_end->resp) {
			ESP_LOGE(TAG, "OTA write failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_WifiSetMaxTxPower: {
		RPC_FAIL_ON_NULL(resp_set_wifi_max_tx_power);
		RPC_ERR_IN_RESP(resp_set_wifi_max_tx_power);
		break;
	} case RPC_ID__Resp_WifiGetMaxTxPower: {
		RPC_FAIL_ON_NULL(resp_get_wifi_max_tx_power);
		RPC_ERR_IN_RESP(resp_get_wifi_max_tx_power);
		app_resp->u.wifi_tx_power.power =
			rpc_msg->resp_get_wifi_max_tx_power->power;
		break;
	} case RPC_ID__Resp_ConfigHeartbeat: {
		RPC_FAIL_ON_NULL(resp_config_heartbeat);
		RPC_ERR_IN_RESP(resp_config_heartbeat);
		break;
	} case RPC_ID__Resp_WifiInit: {
		RPC_FAIL_ON_NULL(resp_wifi_init);
		RPC_ERR_IN_RESP(resp_wifi_init);
		break;
	} case RPC_ID__Resp_WifiDeinit: {
		RPC_FAIL_ON_NULL(resp_wifi_deinit);
		RPC_ERR_IN_RESP(resp_wifi_deinit);
		break;
	} case RPC_ID__Resp_WifiStart: {
		RPC_FAIL_ON_NULL(resp_wifi_start);
		RPC_ERR_IN_RESP(resp_wifi_start);
		break;
	} case RPC_ID__Resp_WifiStop: {
		RPC_FAIL_ON_NULL(resp_wifi_stop);
		RPC_ERR_IN_RESP(resp_wifi_stop);
		break;
	} case RPC_ID__Resp_WifiConnect: {
		RPC_FAIL_ON_NULL(resp_wifi_connect);
		RPC_ERR_IN_RESP(resp_wifi_connect);
		break;
	} case RPC_ID__Resp_WifiDisconnect: {
		RPC_FAIL_ON_NULL(resp_wifi_disconnect);
		RPC_ERR_IN_RESP(resp_wifi_disconnect);
		break;
    } case RPC_ID__Resp_WifiSetConfig: {
		RPC_FAIL_ON_NULL(resp_wifi_set_config);
		RPC_ERR_IN_RESP(resp_wifi_set_config);
		break;
    } case RPC_ID__Resp_WifiGetConfig: {
		RPC_FAIL_ON_NULL(resp_wifi_set_config);
		RPC_ERR_IN_RESP(resp_wifi_set_config);

		app_resp->u.wifi_config.iface = rpc_msg->resp_wifi_get_config->iface;

		switch (app_resp->u.wifi_config.iface) {

		case WIFI_IF_STA: {
			wifi_sta_config_t * p_a_sta = &(app_resp->u.wifi_config.u.sta);
			WifiStaConfig * p_c_sta = rpc_msg->resp_wifi_get_config->cfg->sta;
			RPC_RSP_COPY_BYTES(p_a_sta->ssid, p_c_sta->ssid);
			RPC_RSP_COPY_BYTES(p_a_sta->password, p_c_sta->password);
			p_a_sta->scan_method = p_c_sta->scan_method;
			p_a_sta->bssid_set = p_c_sta->bssid_set;

			if (p_a_sta->bssid_set)
				RPC_RSP_COPY_BYTES(p_a_sta->bssid, p_c_sta->bssid);

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
			p_a_sta->reserved = WIFI_CONFIG_STA_GET_RESERVED_VAL(p_c_sta->bitmask);

			p_a_sta->sae_pwe_h2e = p_c_sta->sae_pwe_h2e;
			p_a_sta->failure_retry_cnt = p_c_sta->failure_retry_cnt;
			break;
		}
		case WIFI_IF_AP: {
			wifi_ap_config_t * p_a_ap = &(app_resp->u.wifi_config.u.ap);
			WifiApConfig * p_c_ap = rpc_msg->resp_wifi_get_config->cfg->ap;

			RPC_RSP_COPY_BYTES(p_a_ap->ssid, p_c_ap->ssid);
			RPC_RSP_COPY_BYTES(p_a_ap->password, p_c_ap->password);
			p_a_ap->ssid_len = p_c_ap->ssid_len;
			p_a_ap->channel = p_c_ap->channel;
			p_a_ap->authmode = p_c_ap->authmode;
			p_a_ap->ssid_hidden = p_c_ap->ssid_hidden;
			p_a_ap->max_connection = p_c_ap->max_connection;
			p_a_ap->beacon_interval = p_c_ap->beacon_interval;
			p_a_ap->pairwise_cipher = p_c_ap->pairwise_cipher;
			p_a_ap->ftm_responder = p_c_ap->ftm_responder;
			p_a_ap->pmf_cfg.capable = p_c_ap->pmf_cfg->capable;
			p_a_ap->pmf_cfg.required = p_c_ap->pmf_cfg->required;
			break;
		}
		default:
			ESP_LOGE(TAG, "Unsupported WiFi interface[%u]", app_resp->u.wifi_config.iface);
		} //switch

		break;

    } case RPC_ID__Resp_WifiScanStart: {
		RPC_FAIL_ON_NULL(resp_wifi_scan_start);
		RPC_ERR_IN_RESP(resp_wifi_scan_start);
		break;
    } case RPC_ID__Resp_WifiScanStop: {
		RPC_FAIL_ON_NULL(resp_wifi_scan_stop);
		RPC_ERR_IN_RESP(resp_wifi_scan_stop);
		break;
    } case RPC_ID__Resp_WifiScanGetApNum: {
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);
		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_num);
		RPC_ERR_IN_RESP(resp_wifi_scan_get_ap_num);

		p_a->number = rpc_msg->resp_wifi_scan_get_ap_num->number;
		break;
    } case RPC_ID__Resp_WifiScanGetApRecords: {
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);
		wifi_ap_record_t *list = NULL;
		WifiApRecord **p_c_list = NULL;

		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_records);
		RPC_ERR_IN_RESP(resp_wifi_scan_get_ap_records);
		p_c_list = rpc_msg->resp_wifi_scan_get_ap_records->ap_records;

		p_a->number = rpc_msg->resp_wifi_scan_get_ap_records->number;

		if (!p_a->number) {
			ESP_LOGI(TAG, "No AP found");
			goto fail_parse_rpc_msg2;
		}
		ESP_LOGD(TAG, "Num AP records: %u",
				app_resp->u.wifi_scan_ap_list.number);

		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_records->ap_records);

		list = (wifi_ap_record_t*)g_h.funcs->_h_calloc(p_a->number,
				sizeof(wifi_ap_record_t));
		p_a->out_list = list;

		RPC_FAIL_ON_NULL_PRINT(list, "Malloc Failed");

		app_resp->app_free_buff_func = g_h.funcs->_h_free;
		app_resp->app_free_buff_hdl = list;

		ESP_LOGD(TAG, "Number of available APs is %d", p_a->number);
		for (i=0; i<p_a->number; i++) {

			WifiCountry *p_c_cntry = p_c_list[i]->country;
			wifi_country_t *p_a_cntry = &list[i].country;

			ESP_LOGD(TAG, "ap_record[%u]:", i+1);
			ESP_LOGD(TAG, "ssid len: %u", p_c_list[i]->ssid.len);
			RPC_RSP_COPY_BYTES(list[i].ssid, p_c_list[i]->ssid);
			RPC_RSP_COPY_BYTES(list[i].bssid, p_c_list[i]->bssid);
			list[i].primary = p_c_list[i]->primary;
			list[i].second = p_c_list[i]->second;
			list[i].rssi = p_c_list[i]->rssi;
			list[i].authmode = p_c_list[i]->authmode;
			list[i].pairwise_cipher = p_c_list[i]->pairwise_cipher;
			list[i].group_cipher = p_c_list[i]->group_cipher;
			list[i].ant = p_c_list[i]->ant;
			list[i].phy_11b       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11b_BIT, p_c_list[i]->bitmask);
			list[i].phy_11g       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11g_BIT, p_c_list[i]->bitmask);
			list[i].phy_11n       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11n_BIT, p_c_list[i]->bitmask);
			list[i].phy_lr        = H_GET_BIT(WIFI_SCAN_AP_REC_phy_lr_BIT, p_c_list[i]->bitmask);
			list[i].phy_11ax      = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11ax_BIT, p_c_list[i]->bitmask);
			list[i].wps           = H_GET_BIT(WIFI_SCAN_AP_REC_wps_BIT, p_c_list[i]->bitmask);
			list[i].ftm_responder = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_responder_BIT, p_c_list[i]->bitmask);
			list[i].ftm_initiator = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_initiator_BIT, p_c_list[i]->bitmask);
			list[i].reserved      = WIFI_SCAN_AP_GET_RESERVED_VAL(p_c_list[i]->bitmask);

			RPC_RSP_COPY_BYTES(p_a_cntry->cc, p_c_cntry->cc);
			p_a_cntry->schan = p_c_cntry->schan;
			p_a_cntry->nchan = p_c_cntry->nchan;
			p_a_cntry->max_tx_power = p_c_cntry->max_tx_power;
			p_a_cntry->policy = p_c_cntry->policy;

			ESP_LOGD(TAG, "SSID: %s BSSid: " MACSTR, list[i].ssid, MAC2STR(list[i].bssid));
			ESP_LOGD(TAG, "Primary: %u Second: %u RSSI: %d Authmode: %u",
					list[i].primary, list[i].second,
					list[i].rssi, list[i].authmode
					);
			ESP_LOGD(TAG, "PairwiseCipher: %u Groupcipher: %u Ant: %u",
					list[i].pairwise_cipher, list[i].group_cipher,
					list[i].ant
					);
			ESP_LOGD(TAG, "Bitmask: 11b:%u g:%u n:%u ax: %u lr:%u wps:%u ftm_resp:%u ftm_ini:%u res: %u",
					list[i].phy_11b, list[i].phy_11g,
					list[i].phy_11n, list[i].phy_11ax, list[i].phy_lr,
					list[i].wps, list[i].ftm_responder,
					list[i].ftm_initiator, list[i].reserved
					);
			ESP_LOGD(TAG, "Country cc:%c%c schan: %u nchan: %u max_tx_pow: %d policy: %u",
					p_a_cntry->cc[0], p_a_cntry->cc[1], p_a_cntry->schan, p_a_cntry->nchan,
					p_a_cntry->max_tx_power,p_a_cntry->policy);

			WifiHeApInfo *p_c_he_ap = p_c_list[i]->he_ap;
			wifi_he_ap_info_t *p_a_he_ap = &list[i].he_ap;
			// six bits
			p_a_he_ap->bss_color = p_c_he_ap->bitmask & 0x3F;
			p_a_he_ap->partial_bss_color = H_GET_BIT(WIFI_HE_AP_INFO_partial_bss_color_BIT, p_c_he_ap->bitmask);
			p_a_he_ap->bss_color_disabled = H_GET_BIT(WIFI_HE_AP_INFO_bss_color_disabled_BIT, p_c_he_ap->bitmask);

			ESP_LOGD(TAG, "HE_AP: bss_color %d, partial_bss_color %d, bss_color_disabled %d",
					p_a_he_ap->bss_color, p_a_he_ap->bss_color_disabled, p_a_he_ap->bss_color_disabled);

			//p_a_sta->rm_enabled = H_GET_BIT(STA_RM_ENABLED_BIT, p_c_sta->bitmask);
		}
		break;
    } case RPC_ID__Resp_WifiStaGetApInfo: {
		WifiApRecord *p_c = NULL;
		wifi_ap_record_t *ap_info = NULL;
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);
		WifiCountry *p_c_cntry = NULL;
		wifi_country_t *p_a_cntry = NULL;

		RPC_FAIL_ON_NULL(resp_wifi_sta_get_ap_info);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_ap_info);
		p_c = rpc_msg->resp_wifi_sta_get_ap_info->ap_records;

		p_a->number = 1;

		RPC_FAIL_ON_NULL(resp_wifi_sta_get_ap_info->ap_records);

		ap_info = (wifi_ap_record_t*)g_h.funcs->_h_calloc(p_a->number,
				sizeof(wifi_ap_record_t));
		p_a->out_list = ap_info;

		RPC_FAIL_ON_NULL_PRINT(ap_info, "Malloc Failed");

		app_resp->app_free_buff_func = g_h.funcs->_h_free;
		app_resp->app_free_buff_hdl = ap_info;

		p_c_cntry = p_c->country;
		p_a_cntry = &ap_info->country;

		ESP_LOGD(TAG, "ap_info");
		ESP_LOGD(TAG,"ssid len: %u", p_c->ssid.len);
		RPC_RSP_COPY_BYTES(ap_info->ssid, p_c->ssid);
		RPC_RSP_COPY_BYTES(ap_info->bssid, p_c->bssid);
		ap_info->primary = p_c->primary;
		ap_info->second = p_c->second;
		ap_info->rssi = p_c->rssi;
		ap_info->authmode = p_c->authmode;
		ap_info->pairwise_cipher = p_c->pairwise_cipher;
		ap_info->group_cipher = p_c->group_cipher;
		ap_info->ant = p_c->ant;
		ap_info->phy_11b       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11b_BIT, p_c->bitmask);
		ap_info->phy_11g       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11g_BIT, p_c->bitmask);
		ap_info->phy_11n       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11n_BIT, p_c->bitmask);
		ap_info->phy_lr        = H_GET_BIT(WIFI_SCAN_AP_REC_phy_lr_BIT, p_c->bitmask);
		ap_info->phy_11ax      = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11ax_BIT, p_c->bitmask);
		ap_info->wps           = H_GET_BIT(WIFI_SCAN_AP_REC_wps_BIT, p_c->bitmask);
		ap_info->ftm_responder = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_responder_BIT, p_c->bitmask);
		ap_info->ftm_initiator = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_initiator_BIT, p_c->bitmask);
		ap_info->reserved      = WIFI_SCAN_AP_GET_RESERVED_VAL(p_c->bitmask);

		RPC_RSP_COPY_BYTES(p_a_cntry->cc, p_c_cntry->cc);
		p_a_cntry->schan = p_c_cntry->schan;
		p_a_cntry->nchan = p_c_cntry->nchan;
		p_a_cntry->max_tx_power = p_c_cntry->max_tx_power;
		p_a_cntry->policy = p_c_cntry->policy;

		WifiHeApInfo *p_c_he_ap = p_c->he_ap;
		wifi_he_ap_info_t *p_a_he_ap = &ap_info->he_ap;
		// six bits
		p_a_he_ap->bss_color = p_c_he_ap->bitmask & 0x3F;
		p_a_he_ap->partial_bss_color = H_GET_BIT(WIFI_HE_AP_INFO_partial_bss_color_BIT,
				p_c_he_ap->bitmask);
		p_a_he_ap->bss_color_disabled = H_GET_BIT(WIFI_HE_AP_INFO_bss_color_disabled_BIT,
				p_c_he_ap->bitmask);

		break;
    } case RPC_ID__Resp_WifiClearApList: {
		RPC_FAIL_ON_NULL(resp_wifi_clear_ap_list);
		RPC_ERR_IN_RESP(resp_wifi_clear_ap_list);
		break;
	} case RPC_ID__Resp_WifiRestore: {
		RPC_FAIL_ON_NULL(resp_wifi_restore);
		RPC_ERR_IN_RESP(resp_wifi_restore);
		break;
	} case RPC_ID__Resp_WifiClearFastConnect: {
		RPC_FAIL_ON_NULL(resp_wifi_clear_fast_connect);
		RPC_ERR_IN_RESP(resp_wifi_clear_fast_connect);
		break;
	} case RPC_ID__Resp_WifiDeauthSta: {
		RPC_FAIL_ON_NULL(resp_wifi_deauth_sta);
		RPC_ERR_IN_RESP(resp_wifi_deauth_sta);
		break;
	} case RPC_ID__Resp_WifiSetStorage: {
		RPC_FAIL_ON_NULL(resp_wifi_set_storage);
		RPC_ERR_IN_RESP(resp_wifi_set_storage);
		break;
	} case RPC_ID__Resp_WifiSetBandwidth: {
		RPC_FAIL_ON_NULL(resp_wifi_set_bandwidth);
		RPC_ERR_IN_RESP(resp_wifi_set_bandwidth);
		break;
	} case RPC_ID__Resp_WifiGetBandwidth: {
		RPC_FAIL_ON_NULL(resp_wifi_get_bandwidth);
		RPC_ERR_IN_RESP(resp_wifi_get_bandwidth);
		app_resp->u.wifi_bandwidth.bw =
			rpc_msg->resp_wifi_get_bandwidth->bw;
		break;
	} case RPC_ID__Resp_WifiSetChannel: {
		RPC_FAIL_ON_NULL(resp_wifi_set_channel);
		RPC_ERR_IN_RESP(resp_wifi_set_channel);
		break;
	} case RPC_ID__Resp_WifiGetChannel: {
		RPC_FAIL_ON_NULL(resp_wifi_get_channel);
		RPC_ERR_IN_RESP(resp_wifi_get_channel);
		app_resp->u.wifi_channel.primary =
			rpc_msg->resp_wifi_get_channel->primary;
		app_resp->u.wifi_channel.second =
			rpc_msg->resp_wifi_get_channel->second;
		break;
	} case RPC_ID__Resp_WifiSetCountryCode: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country_code);
		RPC_ERR_IN_RESP(resp_wifi_set_country_code);
		break;
	} case RPC_ID__Resp_WifiGetCountryCode: {
		RPC_FAIL_ON_NULL(resp_wifi_get_country_code);
		RPC_ERR_IN_RESP(resp_wifi_get_country_code);

		RPC_RSP_COPY_BYTES(&app_resp->u.wifi_country_code.cc[0],
				rpc_msg->resp_wifi_get_country_code->country);
		break;
	} case RPC_ID__Resp_WifiSetCountry: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country);
		RPC_ERR_IN_RESP(resp_wifi_set_country);
		break;
	} case RPC_ID__Resp_WifiGetCountry: {
		RPC_FAIL_ON_NULL(resp_wifi_get_country);
		RPC_ERR_IN_RESP(resp_wifi_get_country);

		RPC_RSP_COPY_BYTES(&app_resp->u.wifi_country.cc[0],
				rpc_msg->resp_wifi_get_country->country->cc);
		app_resp->u.wifi_country.schan        = rpc_msg->resp_wifi_get_country->country->schan;
		app_resp->u.wifi_country.nchan        = rpc_msg->resp_wifi_get_country->country->nchan;
		app_resp->u.wifi_country.max_tx_power = rpc_msg->resp_wifi_get_country->country->max_tx_power;
		app_resp->u.wifi_country.policy       = rpc_msg->resp_wifi_get_country->country->policy;
		break;
	} case RPC_ID__Resp_WifiApGetStaList: {
		RPC_FAIL_ON_NULL(resp_wifi_ap_get_sta_list);
		RPC_ERR_IN_RESP(resp_wifi_ap_get_sta_list);

		// handle case where slave's num is bigger than our ESP_WIFI_MAX_CONN_NUM
		uint32_t num_stations = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->num;
		if (num_stations > ESP_WIFI_MAX_CONN_NUM) {
			ESP_LOGW(TAG, "Slave returned %ld connected stations, but we can only accept %d items", num_stations, ESP_WIFI_MAX_CONN_NUM);
			num_stations = ESP_WIFI_MAX_CONN_NUM;
		}

		WifiStaInfo ** p_c_sta_list = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->sta;

		for (int i = 0; i < num_stations; i++) {
			wifi_sta_info_t * p_a_sta = &app_resp->u.wifi_ap_sta_list.sta[i];

			RPC_RSP_COPY_BYTES(p_a_sta->mac, p_c_sta_list[i]->mac);
			p_a_sta->rssi = p_c_sta_list[i]->rssi;

			p_a_sta->phy_11b = H_GET_BIT(WIFI_STA_INFO_phy_11b_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11g = H_GET_BIT(WIFI_STA_INFO_phy_11g_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11n = H_GET_BIT(WIFI_STA_INFO_phy_11n_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_lr = H_GET_BIT(WIFI_STA_INFO_phy_lr_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11ax = H_GET_BIT(WIFI_STA_INFO_phy_11ax_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->is_mesh_child = H_GET_BIT(WIFI_STA_INFO_is_mesh_child_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->reserved = WIFI_STA_INFO_GET_RESERVED_VAL(p_c_sta_list[i]->bitmask);
		}

		app_resp->u.wifi_ap_sta_list.num = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->num;
		break;
	} case RPC_ID__Resp_WifiApGetStaAid: {
		RPC_FAIL_ON_NULL(resp_wifi_ap_get_sta_aid);
		RPC_ERR_IN_RESP(resp_wifi_ap_get_sta_aid);

		app_resp->u.wifi_ap_get_sta_aid.aid = rpc_msg->resp_wifi_ap_get_sta_aid->aid;
		break;
	} case RPC_ID__Resp_WifiStaGetRssi: {
		RPC_FAIL_ON_NULL(resp_wifi_sta_get_rssi);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_rssi);

		app_resp->u.wifi_sta_get_rssi.rssi = rpc_msg->resp_wifi_sta_get_rssi->rssi;
		break;
	} case RPC_ID__Resp_WifiSetProtocol: {
		RPC_FAIL_ON_NULL(resp_wifi_set_protocol);
		RPC_ERR_IN_RESP(resp_wifi_set_protocol);
		break;
	} case RPC_ID__Resp_WifiGetProtocol: {
		RPC_FAIL_ON_NULL(resp_wifi_get_protocol);
		RPC_ERR_IN_RESP(resp_wifi_get_protocol);
		app_resp->u.wifi_protocol.protocol_bitmap =
			rpc_msg->resp_wifi_get_protocol->protocol_bitmap;
		break;
	} case RPC_ID__Resp_SetDhcpDnsStatus: {
		RPC_FAIL_ON_NULL(resp_set_dhcp_dns);
		RPC_ERR_IN_RESP(resp_set_dhcp_dns);
		break;
	} case RPC_ID__Resp_USR1: {
		RPC_FAIL_ON_NULL(resp_usr1);
		RPC_ERR_IN_RESP(resp_usr1);
		app_resp->u.rpc_usr.int_1 = rpc_msg->resp_usr1->int_1;
		app_resp->u.rpc_usr.int_2 = rpc_msg->resp_usr1->int_2;
		app_resp->u.rpc_usr.uint_1 = rpc_msg->resp_usr1->uint_1;
		app_resp->u.rpc_usr.uint_2 = rpc_msg->resp_usr1->uint_2;
		app_resp->u.rpc_usr.data_len = rpc_msg->resp_usr1->data.len;
		RPC_RSP_COPY_BYTES(&app_resp->u.rpc_usr.data[0], rpc_msg->resp_usr1->data);
		ESP_LOGV(TAG, "USR_REQ1 recvd: %"PRId32", %"PRId32", %"PRIu32", %"PRIu32", %u",
				app_resp->u.rpc_usr.int_1,
				app_resp->u.rpc_usr.int_2,
				app_resp->u.rpc_usr.uint_1,
				app_resp->u.rpc_usr.uint_2,
				app_resp->u.rpc_usr.data_len);
		break;
	} case RPC_ID__Resp_USR2: {
		RPC_FAIL_ON_NULL(resp_usr2);
		RPC_ERR_IN_RESP(resp_usr2);
		app_resp->u.rpc_usr.int_1 = rpc_msg->resp_usr2->int_1;
		app_resp->u.rpc_usr.int_2 = rpc_msg->resp_usr2->int_2;
		app_resp->u.rpc_usr.uint_1 = rpc_msg->resp_usr2->uint_1;
		app_resp->u.rpc_usr.uint_2 = rpc_msg->resp_usr2->uint_2;
		app_resp->u.rpc_usr.data_len = rpc_msg->resp_usr1->data.len;
		RPC_RSP_COPY_BYTES(&app_resp->u.rpc_usr.data[0], rpc_msg->resp_usr2->data);
		break;
	} case RPC_ID__Resp_USR3: {
		RPC_FAIL_ON_NULL(resp_usr3);
		RPC_ERR_IN_RESP(resp_usr3);
		app_resp->u.rpc_usr.int_1 = rpc_msg->resp_usr3->int_1;
		app_resp->u.rpc_usr.int_2 = rpc_msg->resp_usr3->int_2;
		app_resp->u.rpc_usr.uint_1 = rpc_msg->resp_usr3->uint_1;
		app_resp->u.rpc_usr.uint_2 = rpc_msg->resp_usr3->uint_2;
		app_resp->u.rpc_usr.data_len = rpc_msg->resp_usr1->data.len;
		RPC_RSP_COPY_BYTES(&app_resp->u.rpc_usr.data[0], rpc_msg->resp_usr3->data);
		break;
	} case RPC_ID__Resp_USR4: {
		RPC_FAIL_ON_NULL(resp_usr4);
		RPC_ERR_IN_RESP(resp_usr4);
		app_resp->u.rpc_usr.int_1 = rpc_msg->resp_usr4->int_1;
		app_resp->u.rpc_usr.int_2 = rpc_msg->resp_usr4->int_2;
		app_resp->u.rpc_usr.uint_1 = rpc_msg->resp_usr4->uint_1;
		app_resp->u.rpc_usr.uint_2 = rpc_msg->resp_usr4->uint_2;
		app_resp->u.rpc_usr.data_len = rpc_msg->resp_usr1->data.len;
		RPC_RSP_COPY_BYTES(&app_resp->u.rpc_usr.data[0], rpc_msg->resp_usr4->data);
		break;
	} case RPC_ID__Resp_USR5: {
		RPC_FAIL_ON_NULL(resp_usr5);
		RPC_ERR_IN_RESP(resp_usr5);
		app_resp->u.rpc_usr.int_1 = rpc_msg->resp_usr5->int_1;
		app_resp->u.rpc_usr.int_2 = rpc_msg->resp_usr5->int_2;
		app_resp->u.rpc_usr.uint_1 = rpc_msg->resp_usr5->uint_1;
		app_resp->u.rpc_usr.uint_2 = rpc_msg->resp_usr5->uint_2;
		app_resp->u.rpc_usr.data_len = rpc_msg->resp_usr1->data.len;
		RPC_RSP_COPY_BYTES(&app_resp->u.rpc_usr.data[0], rpc_msg->resp_usr5->data);
		break;
	} default: {
		ESP_LOGE(TAG, "Unsupported rpc Resp[%u]", rpc_msg->msg_id);
		goto fail_parse_rpc_msg;
		break;
	}

	}

	app_resp->resp_event_status = SUCCESS;
	return SUCCESS;

	/* 5. Free up buffers in failure cases */
fail_parse_rpc_msg:
	return SUCCESS;

fail_parse_rpc_msg2:
	return FAILURE;
}
