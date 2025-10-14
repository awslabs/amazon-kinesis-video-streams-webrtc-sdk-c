// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#include "rpc_core.h"
#include "rpc_slave_if.h"
#include "rpc_common.h"
#include "adapter.h"
#include "esp_log.h"

DEFINE_LOG_TAG(rpc_req);

#define ADD_RPC_BUFF_TO_FREE_LATER(BuFf) {                                      \
	assert((app_req->n_rpc_free_buff_hdls+1)<=MAX_FREE_BUFF_HANDLES);           \
	app_req->rpc_free_buff_hdls[app_req->n_rpc_free_buff_hdls++] = BuFf;        \
}

#define RPC_ALLOC_ASSIGN(TyPe,MsG_StRuCt,InItFuNc)                            \
    TyPe *req_payload = (TyPe *)                                              \
        g_h.funcs->_h_calloc(1, sizeof(TyPe));                                \
    if (!req_payload) {                                                       \
        ESP_LOGE(TAG, "Failed to allocate memory for req->%s\n",#MsG_StRuCt);     \
        *failure_status = RPC_ERR_MEMORY_FAILURE;                              \
		return FAILURE;                                                       \
    }                                                                         \
    req->MsG_StRuCt = req_payload;                                             \
	InItFuNc(req_payload);                                                    \
    ADD_RPC_BUFF_TO_FREE_LATER((uint8_t*)req_payload);

//TODO: How this is different in slave_control.c
#define RPC_ALLOC_ELEMENT(TyPe,MsG_StRuCt,InIt_FuN) {                         \
    TyPe *NeW_AllocN = (TyPe *) g_h.funcs->_h_calloc(1, sizeof(TyPe));        \
    if (!NeW_AllocN) {                                                        \
        ESP_LOGE(TAG, "Failed to allocate memory for req->%s\n",#MsG_StRuCt);     \
        *failure_status = RPC_ERR_MEMORY_FAILURE;                              \
		return FAILURE;                                                       \
    }                                                                         \
    ADD_RPC_BUFF_TO_FREE_LATER((uint8_t*)NeW_AllocN);                         \
    MsG_StRuCt = NeW_AllocN;                                                  \
    InIt_FuN(MsG_StRuCt);                                                     \
}

/* RPC request is simple remote function invokation at slave from host
 *
 * For new RPC request, add up switch case for your message
 * If the RPC function to be invoked does not carry any arguments, just add
 * case in the top with intentional fall through
 * If any arguments are needed, you may have to add union for your message
 * in Ctrl_cmd_t in rpc_api.h and fill the request in new case
 *
 * For altogether new RPC function addition, please check
 * esp_hosted_fg/common/proto/esp_hosted_config.proto
 */
int compose_rpc_req(Rpc *req, ctrl_cmd_t *app_req, int32_t *failure_status)
{
	switch(req->msg_id) {

	case RPC_ID__Req_GetWifiMode:
	//case RPC_ID__Req_GetAPConfig:
	//case RPC_ID__Req_DisconnectAP:
	//case RPC_ID__Req_GetSoftAPConfig:
	//case RPC_ID__Req_GetSoftAPConnectedSTAList:
	//case RPC_ID__Req_StopSoftAP:
	case RPC_ID__Req_WifiGetPs:
	case RPC_ID__Req_OTABegin:
	case RPC_ID__Req_OTAEnd:
	case RPC_ID__Req_WifiDeinit:
	case RPC_ID__Req_WifiStart:
	case RPC_ID__Req_WifiStop:
	case RPC_ID__Req_WifiConnect:
	case RPC_ID__Req_WifiDisconnect:
	case RPC_ID__Req_WifiScanStop:
	case RPC_ID__Req_WifiScanGetApNum:
	case RPC_ID__Req_WifiClearApList:
	case RPC_ID__Req_WifiRestore:
	case RPC_ID__Req_WifiClearFastConnect:
	case RPC_ID__Req_WifiStaGetApInfo:
	case RPC_ID__Req_WifiGetMaxTxPower:
	case RPC_ID__Req_WifiGetChannel:
	case RPC_ID__Req_WifiGetCountryCode:
	case RPC_ID__Req_WifiGetCountry:
	case RPC_ID__Req_WifiApGetStaList:
	case RPC_ID__Req_WifiStaGetRssi: {
		/* Intentional fallthrough & empty */
		break;
	} case RPC_ID__Req_GetMACAddress: {
		RPC_ALLOC_ASSIGN(RpcReqGetMacAddress, req_get_mac_address,
				rpc__req__get_mac_address__init);

		req_payload->mode = app_req->u.wifi_mac.mode;

		break;
	} case RPC_ID__Req_SetMacAddress: {
		wifi_mac_t * p = &app_req->u.wifi_mac;
		RPC_ALLOC_ASSIGN(RpcReqSetMacAddress, req_set_mac_address,
				rpc__req__set_mac_address__init);

		req_payload->mode = p->mode;
		RPC_REQ_COPY_BYTES(req_payload->mac, p->mac, BSSID_BYTES_SIZE);

		break;
	} case RPC_ID__Req_SetWifiMode: {
		hosted_mode_t * p = &app_req->u.wifi_mode;
		RPC_ALLOC_ASSIGN(RpcReqSetMode, req_set_wifi_mode,
				rpc__req__set_mode__init);

		if ((p->mode < WIFI_MODE_NULL) || (p->mode >= WIFI_MODE_MAX)) {
			ESP_LOGE(TAG, "Invalid wifi mode\n");
			*failure_status = RPC_ERR_INCORRECT_ARG;
			return FAILURE;
		}
		req_payload->mode = p->mode;
		break;
	} case RPC_ID__Req_WifiSetPs: {
		wifi_power_save_t * p = &app_req->u.wifi_ps;
		RPC_ALLOC_ASSIGN(RpcReqSetPs, req_wifi_set_ps,
				rpc__req__set_ps__init);

		req_payload->type = p->ps_mode;
		break;
	} case RPC_ID__Req_OTAWrite: {
		ota_write_t *p = & app_req->u.ota_write;
		RPC_ALLOC_ASSIGN(RpcReqOTAWrite, req_ota_write,
				rpc__req__otawrite__init);

		if (!p->ota_data || (p->ota_data_len == 0)) {
			ESP_LOGE(TAG, "Invalid parameter\n");
			*failure_status = RPC_ERR_INCORRECT_ARG;
			return FAILURE;
		}

		req_payload->ota_data.data = p->ota_data;
		req_payload->ota_data.len = p->ota_data_len;
		break;
	} case RPC_ID__Req_WifiSetMaxTxPower: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetMaxTxPower,
				req_set_wifi_max_tx_power,
				rpc__req__wifi_set_max_tx_power__init);
		req_payload->power = app_req->u.wifi_tx_power.power;
		break;
	} case RPC_ID__Req_ConfigHeartbeat: {
		RPC_ALLOC_ASSIGN(RpcReqConfigHeartbeat, req_config_heartbeat,
				rpc__req__config_heartbeat__init);
		req_payload->enable = app_req->u.e_heartbeat.enable;
		req_payload->duration = app_req->u.e_heartbeat.duration;
		if (req_payload->enable) {
			ESP_LOGW(TAG, "Enable heartbeat with duration %ld\n", (long int)req_payload->duration);
			if (CALLBACK_AVAILABLE != is_event_callback_registered(RPC_ID__Event_Heartbeat))
				ESP_LOGW(TAG, "Note: ** Subscribe heartbeat event to get notification **\n");
		} else {
			ESP_LOGI(TAG, "Disable Heartbeat\n");
		}
		break;
	} case RPC_ID__Req_WifiInit: {
		wifi_init_config_t * p_a = &app_req->u.wifi_init_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiInit, req_wifi_init,
				rpc__req__wifi_init__init);
		RPC_ALLOC_ELEMENT(WifiInitConfig, req_payload->cfg, wifi_init_config__init);

		req_payload->cfg->static_rx_buf_num      = p_a->static_rx_buf_num       ;
		req_payload->cfg->dynamic_rx_buf_num     = p_a->dynamic_rx_buf_num      ;
		req_payload->cfg->tx_buf_type            = p_a->tx_buf_type             ;
		req_payload->cfg->static_tx_buf_num      = p_a->static_tx_buf_num       ;
		req_payload->cfg->dynamic_tx_buf_num     = p_a->dynamic_tx_buf_num      ;
		req_payload->cfg->cache_tx_buf_num       = p_a->cache_tx_buf_num        ;
		req_payload->cfg->csi_enable             = p_a->csi_enable              ;
		req_payload->cfg->ampdu_rx_enable        = p_a->ampdu_rx_enable         ;
		req_payload->cfg->ampdu_tx_enable        = p_a->ampdu_tx_enable         ;
		req_payload->cfg->amsdu_tx_enable        = p_a->amsdu_tx_enable         ;
		req_payload->cfg->nvs_enable             = p_a->nvs_enable              ;
		req_payload->cfg->nano_enable            = p_a->nano_enable             ;
		req_payload->cfg->rx_ba_win              = p_a->rx_ba_win               ;
		req_payload->cfg->wifi_task_core_id      = p_a->wifi_task_core_id       ;
		req_payload->cfg->beacon_max_len         = p_a->beacon_max_len          ;
		req_payload->cfg->mgmt_sbuf_num          = p_a->mgmt_sbuf_num           ;
		req_payload->cfg->sta_disconnected_pm    = p_a->sta_disconnected_pm     ;
		req_payload->cfg->espnow_max_encrypt_num = p_a->espnow_max_encrypt_num  ;
		req_payload->cfg->magic                  = p_a->magic                   ;

		/* uint64 - TODO: portable? */
		req_payload->cfg->feature_caps = p_a->feature_caps                      ;
		break;
    } case RPC_ID__Req_WifiGetConfig: {
		wifi_cfg_t * p_a = &app_req->u.wifi_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiGetConfig, req_wifi_get_config,
				rpc__req__wifi_get_config__init);

		req_payload->iface = p_a->iface;
		break;
    } case RPC_ID__Req_WifiSetConfig: {
		wifi_cfg_t * p_a = &app_req->u.wifi_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiSetConfig, req_wifi_set_config,
				rpc__req__wifi_set_config__init);

		req_payload->iface = p_a->iface;

		RPC_ALLOC_ELEMENT(WifiConfig, req_payload->cfg, wifi_config__init);

		switch(req_payload->iface) {

		case WIFI_IF_STA: {
			req_payload->cfg->u_case = WIFI_CONFIG__U_STA;

			wifi_sta_config_t *p_a_sta = &p_a->u.sta;
			RPC_ALLOC_ELEMENT(WifiStaConfig, req_payload->cfg->sta, wifi_sta_config__init);
			WifiStaConfig *p_c_sta = req_payload->cfg->sta;
			RPC_REQ_COPY_STR(p_c_sta->ssid, p_a_sta->ssid, SSID_LENGTH);

			RPC_REQ_COPY_STR(p_c_sta->password, p_a_sta->password, PASSWORD_LENGTH);

			p_c_sta->scan_method = p_a_sta->scan_method;
			p_c_sta->bssid_set = p_a_sta->bssid_set;

			if (p_a_sta->bssid_set)
				RPC_REQ_COPY_BYTES(p_c_sta->bssid, p_a_sta->bssid, BSSID_BYTES_SIZE);

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

			if (p_a_sta->he_dcm_set)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_dcm_set_BIT, p_c_sta->he_bitmask);

			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx is two bits wide
			if (p_a_sta->he_dcm_max_constellation_tx)
				p_c_sta->he_bitmask |= ((p_a_sta->he_dcm_max_constellation_tx & 0x03) << WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx_BITS);

			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx is two bits wide
			if (p_a_sta->he_dcm_max_constellation_rx)
				p_c_sta->he_bitmask |= ((p_a_sta->he_dcm_max_constellation_rx & 0x03) << WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx_BITS);

			if (p_a_sta->he_mcs9_enabled)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_mcs9_enabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_su_beamformee_disabled)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_su_beamformee_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_su_bmforming_feedback_disabled)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_trig_su_bmforming_feedback_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_mu_bmforming_partial_feedback_disabled)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_trig_mu_bmforming_partial_feedback_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_cqi_feedback_disabled)
				H_SET_BIT(WIFI_HE_STA_CONFIG_he_trig_cqi_feedback_disabled_BIT, p_c_sta->he_bitmask);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
			WIFI_HE_STA_SET_RESERVED_VAL(p_a_sta->he_reserved, p_c_sta->he_bitmask);
#endif

			RPC_REQ_COPY_BYTES(p_c_sta->sae_h2e_identifier, p_a_sta->sae_h2e_identifier, SAE_H2E_IDENTIFIER_LEN);
			break;
		} case WIFI_IF_AP: {
			req_payload->cfg->u_case = WIFI_CONFIG__U_AP;

			wifi_ap_config_t * p_a_ap = &p_a->u.ap;
			RPC_ALLOC_ELEMENT(WifiApConfig, req_payload->cfg->ap, wifi_ap_config__init);
			WifiApConfig * p_c_ap = req_payload->cfg->ap;

			RPC_REQ_COPY_STR(p_c_ap->ssid, p_a_ap->ssid, SSID_LENGTH);
			RPC_REQ_COPY_STR(p_c_ap->password, p_a_ap->password, PASSWORD_LENGTH);
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
			break;
        } default: {
            ESP_LOGE(TAG, "unexpected wifi iface [%u]\n", p_a->iface);
			break;
        }

        } /* switch */
		break;

    } case RPC_ID__Req_WifiScanStart: {
		wifi_scan_config_t * p_a = &app_req->u.wifi_scan_config.cfg;

		RPC_ALLOC_ASSIGN(RpcReqWifiScanStart, req_wifi_scan_start,
				rpc__req__wifi_scan_start__init);

		req_payload->block = app_req->u.wifi_scan_config.block;
		if (app_req->u.wifi_scan_config.cfg_set) {

			RPC_ALLOC_ELEMENT(WifiScanConfig, req_payload->config, wifi_scan_config__init);

			RPC_ALLOC_ELEMENT(WifiScanTime , req_payload->config->scan_time, wifi_scan_time__init);
			RPC_ALLOC_ELEMENT(WifiActiveScanTime, req_payload->config->scan_time->active, wifi_active_scan_time__init);
			ESP_LOGD(TAG, "scan start4\n");

			WifiScanConfig *p_c = req_payload->config;
			WifiScanTime *p_c_st = NULL;
			wifi_scan_time_t *p_a_st = &p_a->scan_time;

			RPC_REQ_COPY_STR(p_c->ssid, p_a->ssid, SSID_LENGTH);
			RPC_REQ_COPY_STR(p_c->bssid, p_a->bssid, MAC_SIZE_BYTES);
			p_c->channel = p_a->channel;
			p_c->show_hidden = p_a->show_hidden;
			p_c->scan_type = p_a->scan_type;

			p_c_st = p_c->scan_time;

			p_c_st->passive = p_a_st->passive;
			p_c_st->active->min = p_a_st->active.min ;
			p_c_st->active->max = p_a_st->active.max ;

			p_c->home_chan_dwell_time = p_a->home_chan_dwell_time;

			req_payload->config_set = 1;
		}
		ESP_LOGI(TAG, "Scan start Req\n");

		break;

	} case RPC_ID__Req_WifiScanGetApRecords: {
		RPC_ALLOC_ASSIGN(RpcReqWifiScanGetApRecords, req_wifi_scan_get_ap_records,
				rpc__req__wifi_scan_get_ap_records__init);
		req_payload->number = app_req->u.wifi_scan_ap_list.number;
		break;
	} case RPC_ID__Req_WifiDeauthSta: {
		RPC_ALLOC_ASSIGN(RpcReqWifiDeauthSta, req_wifi_deauth_sta,
				rpc__req__wifi_deauth_sta__init);
		req_payload->aid = app_req->u.wifi_deauth_sta.aid;
		break;
	} case RPC_ID__Req_WifiSetStorage: {
		wifi_storage_t * p = &app_req->u.wifi_storage;
		RPC_ALLOC_ASSIGN(RpcReqWifiSetStorage, req_wifi_set_storage,
				rpc__req__wifi_set_storage__init);
		req_payload->storage = *p;
		break;
	} case RPC_ID__Req_WifiSetBandwidth: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetBandwidth, req_wifi_set_bandwidth,
				rpc__req__wifi_set_bandwidth__init);
		req_payload->ifx = app_req->u.wifi_bandwidth.ifx;
		req_payload->bw = app_req->u.wifi_bandwidth.bw;
		break;
	} case RPC_ID__Req_WifiGetBandwidth: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetBandwidth, req_wifi_get_bandwidth,
				rpc__req__wifi_get_bandwidth__init);
		req_payload->ifx = app_req->u.wifi_bandwidth.ifx;
		break;
	} case RPC_ID__Req_WifiSetChannel: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetChannel, req_wifi_set_channel,
				rpc__req__wifi_set_channel__init);
		req_payload->primary = app_req->u.wifi_channel.primary;
		req_payload->second = app_req->u.wifi_channel.second;
		break;
	} case RPC_ID__Req_WifiSetCountryCode: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetCountryCode, req_wifi_set_country_code,
				rpc__req__wifi_set_country_code__init);
		RPC_REQ_COPY_BYTES(req_payload->country, (uint8_t *)&app_req->u.wifi_country_code.cc[0], sizeof(app_req->u.wifi_country_code.cc));
		req_payload->ieee80211d_enabled = app_req->u.wifi_country_code.ieee80211d_enabled;
		break;
	} case RPC_ID__Req_WifiSetCountry: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetCountry, req_wifi_set_country,
				rpc__req__wifi_set_country__init);

		RPC_ALLOC_ELEMENT(WifiCountry, req_payload->country, wifi_country__init);
		RPC_REQ_COPY_BYTES(req_payload->country->cc, (uint8_t *)&app_req->u.wifi_country.cc[0], sizeof(app_req->u.wifi_country.cc));
		req_payload->country->schan        = app_req->u.wifi_country.schan;
		req_payload->country->nchan        = app_req->u.wifi_country.nchan;
		req_payload->country->max_tx_power = app_req->u.wifi_country.max_tx_power;
		req_payload->country->policy       = app_req->u.wifi_country.policy;
		break;
	} case RPC_ID__Req_WifiApGetStaAid: {
		RPC_ALLOC_ASSIGN(RpcReqWifiApGetStaAid, req_wifi_ap_get_sta_aid,
				rpc__req__wifi_ap_get_sta_aid__init);

		uint8_t * p = &app_req->u.wifi_ap_get_sta_aid.mac[0];
		RPC_REQ_COPY_BYTES(req_payload->mac, p, MAC_SIZE_BYTES);
		break;
	} case RPC_ID__Req_WifiSetProtocol: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetProtocol, req_wifi_set_protocol,
				rpc__req__wifi_set_protocol__init);
		req_payload->ifx = app_req->u.wifi_protocol.ifx;
		req_payload->protocol_bitmap = app_req->u.wifi_protocol.protocol_bitmap;
		break;
	} case RPC_ID__Req_WifiGetProtocol: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetProtocol, req_wifi_get_protocol,
				rpc__req__wifi_get_protocol__init);
		req_payload->ifx = app_req->u.wifi_protocol.ifx;
		break;
	} case RPC_ID__Req_SetDhcpDnsStatus: {
		RPC_ALLOC_ASSIGN(RpcReqSetDhcpDnsStatus, req_set_dhcp_dns,
				rpc__req__set_dhcp_dns_status__init);
		RpcReqSetDhcpDnsStatus *p_c = req_payload;
		rpc_set_dhcp_dns_status_t* p_a = &app_req->u.slave_dhcp_dns_status;

		p_c->iface = p_a->iface;
		p_c->dhcp_up = p_a->dhcp_up;
		p_c->dns_up = p_a->dns_up;
		p_c->dns_type = p_a->dns_type;
		p_c->net_link_up = p_a->net_link_up;

		RPC_REQ_COPY_STR(p_c->dhcp_ip, p_a->dhcp_ip, 64);
		RPC_REQ_COPY_STR(p_c->dhcp_nm, p_a->dhcp_nm, 64);
		RPC_REQ_COPY_STR(p_c->dhcp_gw, p_a->dhcp_gw, 64);
		RPC_REQ_COPY_STR(p_c->dns_ip, p_a->dns_ip, 64);
		break;
	} case RPC_ID__Req_USR1: {
		RPC_ALLOC_ASSIGN(RpcReqUSR, req_usr1,
				rpc__req__usr__init);
		RpcReqUSR *p_c = req_payload;
		rpc_usr_t * p_a = &app_req->u.rpc_usr;

		p_c->int_1 = p_a->int_1;
		p_c->int_2 = p_a->int_2;
		p_c->uint_1 = p_a->uint_1;
		p_c->uint_2 = p_a->uint_2;

		RPC_REQ_COPY_BYTES(p_c->data, p_a->data, p_a->data_len);
		/*ESP_LOGI(TAG, "Req Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			p_c->int_1, p_c->int_2, p_c->uint_1, p_c->uint_2, p_c->data.len, p_c->data.data);*/
		break;
	} case RPC_ID__Req_USR2: {
		RPC_ALLOC_ASSIGN(RpcReqUSR, req_usr2,
				rpc__req__usr__init);
		RpcReqUSR *p_c = req_payload;
		rpc_usr_t * p_a = &app_req->u.rpc_usr;

		p_c->int_1 = p_a->int_1;
		p_c->int_2 = p_a->int_2;
		p_c->uint_1 = p_a->uint_1;
		p_c->uint_2 = p_a->uint_2;
		RPC_REQ_COPY_BYTES(p_c->data, p_a->data, p_a->data_len);
		/*ESP_LOGI(TAG, "Req Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			p_c->int_1, p_c->int_2, p_c->uint_1, p_c->uint_2, p_c->data.len, p_c->data.data);*/
		break;
	} case RPC_ID__Req_USR3: {
		RPC_ALLOC_ASSIGN(RpcReqUSR, req_usr3,
				rpc__req__usr__init);
		RpcReqUSR *p_c = req_payload;
		rpc_usr_t * p_a = &app_req->u.rpc_usr;

		p_c->int_1 = p_a->int_1;
		p_c->int_2 = p_a->int_2;
		p_c->uint_1 = p_a->uint_1;
		p_c->uint_2 = p_a->uint_2;
		RPC_REQ_COPY_BYTES(p_c->data, p_a->data, p_a->data_len);
		/*ESP_LOGI(TAG, "Req Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			p_c->int_1, p_c->int_2, p_c->uint_1, p_c->uint_2, p_c->data.len, p_c->data.data);*/
		break;
	} case RPC_ID__Req_USR4: {
		RPC_ALLOC_ASSIGN(RpcReqUSR, req_usr4,
				rpc__req__usr__init);
		RpcReqUSR *p_c = req_payload;
		rpc_usr_t * p_a = &app_req->u.rpc_usr;

		p_c->int_1 = p_a->int_1;
		p_c->int_2 = p_a->int_2;
		p_c->uint_1 = p_a->uint_1;
		p_c->uint_2 = p_a->uint_2;
		RPC_REQ_COPY_BYTES(p_c->data, p_a->data, p_a->data_len);
		/*ESP_LOGI(TAG, "Req Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			p_c->int_1, p_c->int_2, p_c->uint_1, p_c->uint_2, p_c->data.len, p_c->data.data);*/
		break;
	} case RPC_ID__Req_USR5: {
		RPC_ALLOC_ASSIGN(RpcReqUSR, req_usr5,
				rpc__req__usr__init);
		RpcReqUSR *p_c = req_payload;
		rpc_usr_t * p_a = &app_req->u.rpc_usr;

		p_c->int_1 = p_a->int_1;
		p_c->int_2 = p_a->int_2;
		p_c->uint_1 = p_a->uint_1;
		p_c->uint_2 = p_a->uint_2;
		RPC_REQ_COPY_BYTES(p_c->data, p_a->data, p_a->data_len);
		/*ESP_LOGI(TAG, "Req Arg[%"PRId32",%"PRId32",%"PRIu32",%"PRIu32",data_len[%u],data[%s]",
			p_c->int_1, p_c->int_2, p_c->uint_1, p_c->uint_2, p_c->data.len, p_c->data.data);*/
		break;
	} default: {
		*failure_status = RPC_ERR_UNSUPPORTED_MSG;
		ESP_LOGE(TAG, "Unsupported RPC Req[%u]",req->msg_id);
		return FAILURE;
		break;
	}

	} /* switch */
	return SUCCESS;
}
