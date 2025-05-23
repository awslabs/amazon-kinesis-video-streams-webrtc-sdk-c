/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// This file is auto-generated
#include "esp_wifi.h"

void run_all_wifi_apis(void)
{
    {
        const wifi_init_config_t *config = NULL;
        esp_wifi_init(config);
    }

    {
        esp_wifi_deinit();
    }

    {
        wifi_mode_t mode = 0;
        esp_wifi_set_mode(mode);
    }

    {
        wifi_mode_t *mode = NULL;
        esp_wifi_get_mode(mode);
    }

    {
        esp_wifi_start();
    }

    {
        esp_wifi_stop();
    }

    {
        esp_wifi_restore();
    }

    {
        esp_wifi_connect();
    }

    {
        esp_wifi_disconnect();
    }

    {
        esp_wifi_clear_fast_connect();
    }

    {
        uint16_t aid = 0;
        esp_wifi_deauth_sta(aid);
    }

    {
        const wifi_scan_config_t *config = NULL;
        _Bool block = 0;
        esp_wifi_scan_start(config, block);
    }

    {
        esp_wifi_scan_stop();
    }

    {
        uint16_t *number = NULL;
        esp_wifi_scan_get_ap_num(number);
    }

    {
        uint16_t *number = NULL;
        wifi_ap_record_t *ap_records = NULL;
        esp_wifi_scan_get_ap_records(number, ap_records);
    }

    {
        wifi_ap_record_t *ap_record = NULL;
        esp_wifi_scan_get_ap_record(ap_record);
    }

    {
        esp_wifi_clear_ap_list();
    }

    {
        wifi_ap_record_t *ap_info = NULL;
        esp_wifi_sta_get_ap_info(ap_info);
    }

    {
        wifi_ps_type_t type = 0;
        esp_wifi_set_ps(type);
    }

    {
        wifi_ps_type_t *type = NULL;
        esp_wifi_get_ps(type);
    }

    {
        wifi_interface_t ifx = 0;
        uint8_t protocol_bitmap = 0;
        esp_wifi_set_protocol(ifx, protocol_bitmap);
    }

    {
        wifi_interface_t ifx = 0;
        uint8_t *protocol_bitmap = NULL;
        esp_wifi_get_protocol(ifx, protocol_bitmap);
    }

    {
        wifi_interface_t ifx = 0;
        wifi_bandwidth_t bw = 0;
        esp_wifi_set_bandwidth(ifx, bw);
    }

    {
        wifi_interface_t ifx = 0;
        wifi_bandwidth_t *bw = NULL;
        esp_wifi_get_bandwidth(ifx, bw);
    }

    {
        uint8_t primary = 0;
        wifi_second_chan_t second = 0;
        esp_wifi_set_channel(primary, second);
    }

    {
        uint8_t *primary = NULL;
        wifi_second_chan_t *second = NULL;
        esp_wifi_get_channel(primary, second);
    }

    {
        const wifi_country_t *country = NULL;
        esp_wifi_set_country(country);
    }

    {
        wifi_country_t *country = NULL;
        esp_wifi_get_country(country);
    }

    {
        wifi_interface_t ifx = 0;
        const uint8_t mac[6] = {};
        esp_wifi_set_mac(ifx, mac);
    }

    {
        wifi_interface_t ifx = 0;
        uint8_t mac[6] = {};
        esp_wifi_get_mac(ifx, mac);
    }

    {
        wifi_promiscuous_cb_t cb = 0;
        esp_wifi_set_promiscuous_rx_cb(cb);
    }

    {
        _Bool en = 0;
        esp_wifi_set_promiscuous(en);
    }

    {
        _Bool *en = NULL;
        esp_wifi_get_promiscuous(en);
    }

    {
        const wifi_promiscuous_filter_t *filter = NULL;
        esp_wifi_set_promiscuous_filter(filter);
    }

    {
        wifi_promiscuous_filter_t *filter = NULL;
        esp_wifi_get_promiscuous_filter(filter);
    }

    {
        const wifi_promiscuous_filter_t *filter = NULL;
        esp_wifi_set_promiscuous_ctrl_filter(filter);
    }

    {
        wifi_promiscuous_filter_t *filter = NULL;
        esp_wifi_get_promiscuous_ctrl_filter(filter);
    }

    {
        wifi_interface_t interface = 0;
        wifi_config_t *conf = NULL;
        esp_wifi_set_config(interface, conf);
    }

    {
        wifi_interface_t interface = 0;
        wifi_config_t *conf = NULL;
        esp_wifi_get_config(interface, conf);
    }

    {
        wifi_sta_list_t *sta = NULL;
        esp_wifi_ap_get_sta_list(sta);
    }

    {
        const uint8_t mac[6] = {};
        uint16_t *aid = NULL;
        esp_wifi_ap_get_sta_aid(mac, aid);
    }

    {
        wifi_storage_t storage = 0;
        esp_wifi_set_storage(storage);
    }

    {
        _Bool enable = 0;
        wifi_vendor_ie_type_t type = 0;
        wifi_vendor_ie_id_t idx = 0;
        const void *vnd_ie = NULL;
        esp_wifi_set_vendor_ie(enable, type, idx, vnd_ie);
    }

    {
        esp_vendor_ie_cb_t cb = 0;
        void *ctx = NULL;
        esp_wifi_set_vendor_ie_cb(cb, ctx);
    }

    {
        int8_t power = 0;
        esp_wifi_set_max_tx_power(power);
    }

    {
        int8_t *power = NULL;
        esp_wifi_get_max_tx_power(power);
    }

    {
        uint32_t mask = 0;
        esp_wifi_set_event_mask(mask);
    }

    {
        uint32_t *mask = NULL;
        esp_wifi_get_event_mask(mask);
    }

    {
        wifi_interface_t ifx = 0;
        const void *buffer = NULL;
        int len = 0;
        _Bool en_sys_seq = 0;
        esp_wifi_80211_tx(ifx, buffer, len, en_sys_seq);
    }

    {
        wifi_csi_cb_t cb = 0;
        void *ctx = NULL;
        esp_wifi_set_csi_rx_cb(cb, ctx);
    }

    {
        const wifi_csi_config_t *config = NULL;
        esp_wifi_set_csi_config(config);
    }

    {
        _Bool en = 0;
        esp_wifi_set_csi(en);
    }

    {
        wifi_interface_t interface = 0;
        esp_wifi_get_tsf_time(interface);
    }

    {
        wifi_interface_t ifx = 0;
        uint16_t sec = 0;
        esp_wifi_set_inactive_time(ifx, sec);
    }

    {
        wifi_interface_t ifx = 0;
        uint16_t *sec = NULL;
        esp_wifi_get_inactive_time(ifx, sec);
    }

    {
        uint32_t modules = 0;
        esp_wifi_statis_dump(modules);
    }

    {
        int32_t rssi = 0;
        esp_wifi_set_rssi_threshold(rssi);
    }

    {
        wifi_ftm_initiator_cfg_t *cfg = NULL;
        esp_wifi_ftm_initiate_session(cfg);
    }

    {
        esp_wifi_ftm_end_session();
    }

    {
        int16_t offset_cm = 0;
        esp_wifi_ftm_resp_set_offset(offset_cm);
    }

    {
        wifi_ftm_report_entry_t *report = NULL;
        uint8_t num_entries = 0;
        esp_wifi_ftm_get_report(report, num_entries);
    }

    {
        wifi_interface_t ifx = 0;
        _Bool disable = 0;
        esp_wifi_config_11b_rate(ifx, disable);
    }

    {
        uint16_t wake_interval = 0;
        esp_wifi_connectionless_module_set_wake_interval(wake_interval);
    }

    {
        esp_wifi_force_wakeup_acquire();
    }

    {
        esp_wifi_force_wakeup_release();
    }

    {
        const char *country = NULL;
        _Bool ieee80211d_enabled = 0;
        esp_wifi_set_country_code(country, ieee80211d_enabled);
    }

    {
        char *country = NULL;
        esp_wifi_get_country_code(country);
    }

    {
        wifi_interface_t ifx = 0;
        wifi_phy_rate_t rate = 0;
        esp_wifi_config_80211_tx_rate(ifx, rate);
    }

    {
        wifi_interface_t ifx = 0;
        esp_wifi_disable_pmf_config(ifx);
    }

    {
        uint16_t *aid = NULL;
        esp_wifi_sta_get_aid(aid);
    }

    {
        wifi_phy_mode_t *phymode = NULL;
        esp_wifi_sta_get_negotiated_phymode(phymode);
    }

    {
        _Bool enabled = 0;
        esp_wifi_set_dynamic_cs(enabled);
    }

    {
        int *rssi = NULL;
        esp_wifi_sta_get_rssi(rssi);
    }

}
