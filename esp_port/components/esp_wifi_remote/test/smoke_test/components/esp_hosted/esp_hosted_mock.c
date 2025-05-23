/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// This file is auto-generated
#include "esp_wifi.h"
#include "esp_wifi_remote.h"

esp_err_t esp_wifi_remote_init(const wifi_init_config_t *config)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_deinit(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_mode(wifi_mode_t mode)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_mode(wifi_mode_t *mode)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_start(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_stop(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_restore(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_connect(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_disconnect(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_clear_fast_connect(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_deauth_sta(uint16_t aid)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_scan_start(const wifi_scan_config_t *config, _Bool block)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_scan_stop(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_scan_get_ap_num(uint16_t *number)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_scan_get_ap_record(wifi_ap_record_t *ap_record)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_clear_ap_list(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_ps(wifi_ps_type_t type)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_ps(wifi_ps_type_t *type)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_channel(uint8_t primary, wifi_second_chan_t second)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_country(const wifi_country_t *country)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_country(wifi_country_t *country)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_mac(wifi_interface_t ifx, const uint8_t mac[6])
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_promiscuous(_Bool en)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_promiscuous(_Bool *en)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_promiscuous_filter(const wifi_promiscuous_filter_t *filter)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_promiscuous_filter(wifi_promiscuous_filter_t *filter)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_promiscuous_ctrl_filter(const wifi_promiscuous_filter_t *filter)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_promiscuous_ctrl_filter(wifi_promiscuous_filter_t *filter)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ap_get_sta_list(wifi_sta_list_t *sta)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_storage(wifi_storage_t storage)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_vendor_ie(_Bool enable, wifi_vendor_ie_type_t type, wifi_vendor_ie_id_t idx, const void *vnd_ie)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_vendor_ie_cb(esp_vendor_ie_cb_t cb, void *ctx)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_max_tx_power(int8_t power)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_max_tx_power(int8_t *power)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_event_mask(uint32_t mask)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_event_mask(uint32_t *mask)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_80211_tx(wifi_interface_t ifx, const void *buffer, int len, _Bool en_sys_seq)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_csi_rx_cb(wifi_csi_cb_t cb, void *ctx)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_csi_config(const wifi_csi_config_t *config)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_csi(_Bool en)
{
    return ESP_OK;
}

int64_t esp_wifi_remote_get_tsf_time(wifi_interface_t interface)
{
    return 0;
}

esp_err_t esp_wifi_remote_set_inactive_time(wifi_interface_t ifx, uint16_t sec)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_inactive_time(wifi_interface_t ifx, uint16_t *sec)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_statis_dump(uint32_t modules)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_rssi_threshold(int32_t rssi)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ftm_initiate_session(wifi_ftm_initiator_cfg_t *cfg)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ftm_end_session(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ftm_resp_set_offset(int16_t offset_cm)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_ftm_get_report(wifi_ftm_report_entry_t *report, uint8_t num_entries)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_config_11b_rate(wifi_interface_t ifx, _Bool disable)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_connectionless_module_set_wake_interval(uint16_t wake_interval)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_force_wakeup_acquire(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_force_wakeup_release(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_country_code(const char *country, _Bool ieee80211d_enabled)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_get_country_code(char *country)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_config_80211_tx_rate(wifi_interface_t ifx, wifi_phy_rate_t rate)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_disable_pmf_config(wifi_interface_t ifx)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_sta_get_aid(uint16_t *aid)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_sta_get_negotiated_phymode(wifi_phy_mode_t *phymode)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_set_dynamic_cs(_Bool enabled)
{
    return ESP_OK;
}

esp_err_t esp_wifi_remote_sta_get_rssi(int *rssi)
{
    return ESP_OK;
}
