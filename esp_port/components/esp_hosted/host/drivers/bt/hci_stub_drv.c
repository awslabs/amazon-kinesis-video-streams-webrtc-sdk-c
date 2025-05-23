// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#include "hci_drv.h"

#include "esp_hosted_log.h"
static const char TAG[] = "hci_stub_drv";

#if H_BT_HOST_ESP_NIMBLE
#include "host/ble_hs_mbuf.h"
#include "nimble/transport.h"
#endif

#define WEAK __attribute__((weak))

int hci_rx_handler(interface_buffer_handle_t *buf_handle)
{
	/* Hosted transport received BT packets, but Hosted was not
	 * configured to handle BT packets. Drop them.
	 */
	return ESP_OK;
}

void hci_drv_init(void)
{
}

void hci_drv_show_configuration(void)
{
	ESP_LOGI(TAG, "Host BT Support: Disabled");
}

#if H_BT_HOST_ESP_NIMBLE
/**
 * ESP NimBLE expects these interfaces for Tx
 *
 * There are marked as weak references:
 *
 * - to allow ESP NimBLE BT Host code to override the functions if
 *   NimBLE BT Host is configured to act as the HCI transport
 *
 * - to allow the User to use their own ESP NimBLE HCI transport code
 *   without causing linker errors from Hosted
 *
 * - to allow Hosted code to build without linker errors if ESP NimBLE
 *   BT Host is enabled, but Hosted is not configured as HCI transport
 *   and there is no other ESP NimBLE HCI transport code being
 *   used. In this case, the stub functions are used and drops the
 *   incoming data.
 */

WEAK int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
	os_mbuf_free_chain(om);

	return ESP_FAIL;
}

WEAK int ble_transport_to_ll_cmd_impl(void *buf)
{
	ble_transport_free(buf);

	return ESP_FAIL;
}
#endif // H_BT_HOST_ESP_NIMBLE
