// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#include <stdint.h>

#include "adapter.h"
#include "os_wrapper.h"
#include "transport_drv.h"

#include "hci_drv.h"

#if H_BT_HOST_ESP_NIMBLE
#include "host/ble_hs_mbuf.h"
#include "os/os_mbuf.h"
#include "nimble/transport.h"
#include "nimble/transport/hci_h4.h"
#include "nimble/hci_common.h"
#endif

#include "esp_hosted_log.h"
static const char TAG[] = "vhci_drv";

/**
 * HCI_H4_xxx is the first byte of the received data
 */
int hci_rx_handler(interface_buffer_handle_t *buf_handle)
{
	uint8_t * data = buf_handle->payload;
	uint32_t len_total_read = buf_handle->payload_len;

	int rc;

	if (data[0] == HCI_H4_EVT) {
		uint8_t *evbuf;
		int totlen;

		totlen = BLE_HCI_EVENT_HDR_LEN + data[2];
		if (totlen > UINT8_MAX + BLE_HCI_EVENT_HDR_LEN) {
			ESP_LOGE(TAG, "Rx: len[%d] > max INT [%d], drop",
					totlen, UINT8_MAX + BLE_HCI_EVENT_HDR_LEN);
			return ESP_FAIL;
		}

		if (totlen > MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE)) {
			ESP_LOGE(TAG, "Rx: len[%d] > max BLE [%d], drop",
					totlen, MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE));
			return ESP_FAIL;
		}

		if (data[1] == BLE_HCI_EVCODE_HW_ERROR) {
			ESP_LOGE(TAG, "Rx: HW_ERROR");
			return ESP_FAIL;
		}

		/* Allocate LE Advertising Report Event from lo pool only */
		if ((data[1] == BLE_HCI_EVCODE_LE_META) &&
			(data[3] == BLE_HCI_LE_SUBEV_ADV_RPT || data[3] == BLE_HCI_LE_SUBEV_EXT_ADV_RPT)) {
			evbuf = ble_transport_alloc_evt(1);
			/* Skip advertising report if we're out of memory */
			if (!evbuf) {
				ESP_LOGE(TAG, "Rx: failed transport_alloc_evt(1)");
				return ESP_FAIL;
			}
		} else {
			evbuf = ble_transport_alloc_evt(0);
			if (!evbuf) {
				ESP_LOGE(TAG, "Rx: failed transport_alloc_evt(0)");
				return ESP_FAIL;
			}
		}

		memset(evbuf, 0, sizeof * evbuf);
		memcpy(evbuf, &data[1], totlen);

		rc = ble_transport_to_hs_evt(evbuf);
		if (rc) {
			ESP_LOGE(TAG, "Rx: transport_to_hs_evt failed");
			return ESP_FAIL;
		}
	} else if (data[0] == HCI_H4_ACL) {
		struct os_mbuf *m = NULL;

		m = ble_transport_alloc_acl_from_ll();
		if (!m) {
			ESP_LOGE(TAG, "Rx: alloc_acl_from_ll failed");
			return ESP_FAIL;
		}

		if ((rc = os_mbuf_append(m, &data[1], len_total_read - 1)) != 0) {
			ESP_LOGE(TAG, "Rx: failed os_mbuf_append; rc = %d", rc);
			os_mbuf_free_chain(m);
			return ESP_FAIL;
		}

		ble_transport_to_hs_acl(m);
	}
	return ESP_OK;
}

void hci_drv_init(void)
{
	// do nothing for VHCI: underlying transport should be ready
}

void hci_drv_show_configuration(void)
{
	ESP_LOGI(TAG, "Host BT Support: Enabled");
	ESP_LOGI(TAG, "\tBT Transport Type: VHCI");
}

#if H_BT_HOST_ESP_NIMBLE
/**
 * ESP NimBLE expects these interfaces for Tx
 *
 * For doing non-zero copy:
 * - transport expects the HCI_H4_xxx type to be the first byte of the
 *   data stream
 *
 * For doing zero copy:
 * - fill in esp_paylod_header and payload data
 * - HCI_H4_xxx type should be set in esp_payload_header.hci_pkt_type
 */

int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
	// TODO: zerocopy version

	// calculate data length from the incoming data
	int data_len = OS_MBUF_PKTLEN(om) + 1;

	uint8_t * data = NULL;
	int res;

	data = MEM_ALLOC(data_len);
	if (!data) {
		ESP_LOGE(TAG, "Tx %s: malloc failed", __func__);
		res = ESP_FAIL;
		goto exit;
	}

	data[0] = HCI_H4_ACL;
	res = ble_hs_mbuf_to_flat(om, &data[1], OS_MBUF_PKTLEN(om), NULL);
	if (res) {
		ESP_LOGE(TAG, "Tx: Error copying HCI_H4_ACL data %d", res);
		res = ESP_FAIL;
		goto exit;
	}

	res = esp_hosted_tx(ESP_HCI_IF, 0, data, data_len, H_BUFF_NO_ZEROCOPY, H_DEFLT_FREE_FUNC);

 exit:
	os_mbuf_free_chain(om);

	return res;
}

int ble_transport_to_ll_cmd_impl(void *buf)
{
	// TODO: zerocopy version

	// calculate data length from the incoming data
	int buf_len = 3 + ((uint8_t *)buf)[2] + 1;

	uint8_t * data = NULL;
	int res;

	data = MEM_ALLOC(buf_len);
	if (!data) {
		ESP_LOGE(TAG, "Tx %s: malloc failed", __func__);
		res =  ESP_FAIL;
		goto exit;
	}

	data[0] = HCI_H4_CMD;
	memcpy(&data[1], buf, buf_len - 1);

	res = esp_hosted_tx(ESP_HCI_IF, 0, data, buf_len, H_BUFF_NO_ZEROCOPY, H_DEFLT_FREE_FUNC);

 exit:
	ble_transport_free(buf);

	return res;
}
#endif // H_BT_HOST_ESP_NIMBLE
