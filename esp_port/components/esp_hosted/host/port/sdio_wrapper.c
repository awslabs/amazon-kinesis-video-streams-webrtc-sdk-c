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

#include "esp_log.h"

#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"

#include "os_wrapper.h"
#include "sdio_reg.h"
#include "sdio_wrapper.h"
#include "esp_hosted_config.h"

DEFINE_LOG_TAG(sdio_wrapper);

#define CIS_BUFFER_SIZE 256
#define FUNC1_EN_MASK   (BIT(1))
#define SDIO_INIT_MAX_RETRY 10 // max number of times we try to init SDIO FN 1

#define SDIO_FAIL_IF_NULL(x) do { \
		if (!x) return ESP_FAIL;  \
	} while (0);

#define SDIO_LOCK(x) do { \
	if (x) g_h.funcs->_h_lock_mutex(sdio_bus_lock, portMAX_DELAY); \
} while (0);

#define SDIO_UNLOCK(x) do { \
	if (x) g_h.funcs->_h_unlock_mutex(sdio_bus_lock); \
} while (0);

static sdmmc_card_t *card = NULL;
static void * sdio_bus_lock;

static esp_err_t hosted_sdio_print_cis_information(sdmmc_card_t* card)
{
	uint8_t cis_buffer[CIS_BUFFER_SIZE];
	size_t cis_data_len = 1024; //specify maximum searching range to avoid infinite loop
	esp_err_t ret = ESP_OK;

	SDIO_FAIL_IF_NULL(card);

	ret = sdmmc_io_get_cis_data(card, cis_buffer, CIS_BUFFER_SIZE, &cis_data_len);
	if (ret == ESP_ERR_INVALID_SIZE) {
		int temp_buf_size = cis_data_len;
		uint8_t* temp_buf = g_h.funcs->_h_malloc(temp_buf_size);
		assert(temp_buf);

		ESP_LOGW(TAG, "CIS data longer than expected, temporary buffer allocated.");
		ret = sdmmc_io_get_cis_data(card, temp_buf, temp_buf_size, &cis_data_len);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "failed to get CIS data.");
			HOSTED_FREE(temp_buf);
			return ret;
		}

		sdmmc_io_print_cis_info(temp_buf, cis_data_len, NULL);

		HOSTED_FREE(temp_buf);
	} else if (ret == ESP_OK) {
		sdmmc_io_print_cis_info(cis_buffer, cis_data_len, NULL);
	} else {
		ESP_LOGE(TAG, "failed to get CIS data.");
		return ret;
	}
	return ESP_OK;
}

static esp_err_t hosted_sdio_set_blocksize(uint8_t fn, uint16_t value)
{
	size_t offset = SD_IO_FBR_START * fn;
	const uint8_t *bs_u8 = (const uint8_t *) &value;
	uint16_t bs_read = 0;
	uint8_t *bs_read_u8 = (uint8_t *) &bs_read;

	// Set and read back block size
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, bs_u8[0], NULL));
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, bs_u8[1], NULL));
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, &bs_read_u8[0]));
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, &bs_read_u8[1]));
	ESP_LOGI(TAG, "Function %d Blocksize: %d", fn, (unsigned int) bs_read);

	if (bs_read == value)
		return ESP_OK;
	else
		return ESP_FAIL;
}

static esp_err_t hosted_sdio_card_fn_init(sdmmc_card_t *card)
{
	uint8_t ioe = 0;
	uint8_t ior = 0;
	uint8_t ie = 0;
	uint8_t bus_width = 0;
	uint16_t bs = 0;
	int i = 0;

	SDIO_FAIL_IF_NULL(card);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));
	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
	ESP_LOGD(TAG, "IOR: 0x%02x", ior);

	// enable function 1
	ioe |= FUNC1_EN_MASK;
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, ioe, &ioe));
	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));
	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	// wait for the card to become ready
	ior = 0;
	for (i = 0; i < SDIO_INIT_MAX_RETRY; i++) {
		ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
		ESP_LOGD(TAG, "IOR: 0x%02x", ior);
		if (ior & FUNC1_EN_MASK) {
			break;
		} else {
			usleep(10 * 1000);
		}
	}
	if (i >= SDIO_INIT_MAX_RETRY) {
		// card failed to become ready
		return ESP_FAIL;
	}

	// get interrupt status
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	ESP_LOGD(TAG, "IE: 0x%02x", ie);

	// enable interrupts for function 1 and master enable
	ie |= BIT(0) | FUNC1_EN_MASK;
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, ie, NULL));

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	ESP_LOGD(TAG, "IE: 0x%02x", ie);

	// get bus width register
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_BUS_WIDTH, &bus_width));
	ESP_LOGD(TAG, "BUS_WIDTH: 0x%02x", bus_width);

	// skip enable of continous SPI interrupts

	// set FN0 block size to 512
	bs = 512;
	ESP_ERROR_CHECK(hosted_sdio_set_blocksize(SDIO_FUNC_0, bs));

	// set FN1 block size to 512
	bs = 512;
	ESP_ERROR_CHECK(hosted_sdio_set_blocksize(SDIO_FUNC_1, bs));

	return ESP_OK;
}

static esp_err_t sdio_read_fromio(sdmmc_card_t *card, uint32_t function, uint32_t addr,
							uint8_t *data, uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	esp_err_t res;
	uint8_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_RX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdmmc_io_read_blocks(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdmmc_io_read_bytes(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return ESP_OK;
}

static esp_err_t sdio_write_toio(sdmmc_card_t *card, uint32_t function, uint32_t addr,
									uint8_t *data, uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	esp_err_t res;
	uint8_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_TX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdmmc_io_write_blocks(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdmmc_io_write_bytes(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return ESP_OK;
}

void * hosted_sdio_init(void)
{
	esp_err_t res;

	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	// initialise SDMMC host
	res = sdmmc_host_init();
	if (res != ESP_OK)
		return NULL;

	// configure SDIO interface and slot
	slot_config.width = H_SDIO_BUS_WIDTH;
#if defined(H_SDIO_SOC_USE_GPIO_MATRIX)
	slot_config.clk = H_SDIO_PIN_CLK;
	slot_config.cmd = H_SDIO_PIN_CMD;
	slot_config.d0  = H_SDIO_PIN_D0;
	slot_config.d1  = H_SDIO_PIN_D1;
#if (H_SDIO_BUS_WIDTH == 4)
	slot_config.d2  = H_SDIO_PIN_D2;
	slot_config.d3  = H_SDIO_PIN_D3;
#endif
#endif
	res = sdmmc_host_init_slot(H_SDMMC_HOST_SLOT, &slot_config);
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "init SDMMC Host slot %d failed", H_SDMMC_HOST_SLOT);
		return NULL;
	}
	// initialise connected SDIO card/slave
	card = (sdmmc_card_t *)g_h.funcs->_h_malloc(sizeof(sdmmc_card_t));
	if (!card)
		return NULL;

	// initialise mutex for bus locking
	sdio_bus_lock = g_h.funcs->_h_create_mutex();
	assert(sdio_bus_lock);

	return (void *)card;
}

int hosted_sdio_card_init(void)
{
	sdmmc_host_t config = SDMMC_HOST_DEFAULT();

	if (H_SDIO_BUS_WIDTH == 4)
		config.flags = SDMMC_HOST_FLAG_4BIT;
	else
		config.flags = SDMMC_HOST_FLAG_1BIT;
	config.max_freq_khz = H_SDIO_CLOCK_FREQ_KHZ;
	ESP_LOGI(TAG, "SDIO master: Data-Lines: %d-bit Freq(KHz)[%u KHz]", H_SDIO_BUS_WIDTH==4? 4:1,
			config.max_freq_khz);
	ESP_LOGI(TAG, "GPIOs: Slave_Reset[%u] CLK[%u] CMD[%u] D0[%u] D1[%u]",
			H_GPIO_PIN_RESET_Pin, H_SDIO_PIN_CLK, H_SDIO_PIN_CMD,
			H_SDIO_PIN_D0, H_SDIO_PIN_D1);
#if (H_SDIO_BUS_WIDTH == 4)
	ESP_LOGI(TAG, "GPIOs: D2[%u] D3[%u]", H_SDIO_PIN_D2, H_SDIO_PIN_D3);
#endif
	ESP_LOGI(TAG, "Queues: Tx[%u] Rx[%u] SDIO-Rx-Mode[%u]",
			CONFIG_ESP_SDIO_TX_Q_SIZE, CONFIG_ESP_SDIO_RX_Q_SIZE,
			H_SDIO_HOST_RX_MODE);

	ESP_LOGI(TAG, "SDIO: Mode: %d-bit Freq(KHz):%u", H_SDIO_BUS_WIDTH==4? 4:1,
			config.max_freq_khz);

#ifdef CONFIG_IDF_TARGET_ESP32P4
	// Set this flag to allocate aligned buffer of 512 bytes to meet
	// DMA's requirements for CMD53 byte mode. Mandatory when any
	// buffer is behind the cache, or not aligned to 4 byte boundary.
	config.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
#endif

	if (sdmmc_card_init(&config, card) != ESP_OK) {
		ESP_LOGE(TAG, "sdmmc_card_init failed");
		goto fail;
	}

	// output CIS info from the slave
	sdmmc_card_print_info(stdout, card);

	if (hosted_sdio_print_cis_information(card) != ESP_OK) {
		ESP_LOGW(TAG, "failed to print card info");
	}

	// initialise the card functions
	if (hosted_sdio_card_fn_init(card) != ESP_OK) {
		ESP_LOGE(TAG, "sdio_cared_fn_init failed");
		goto fail;
	}
	return ESP_OK;

fail:
	sdmmc_host_deinit();
	if (card) {
		HOSTED_FREE(card);
	}
	return ESP_FAIL;
}

esp_err_t hosted_sdio_card_deinit(void)
{
	if (card) {
		sdmmc_host_deinit();
		HOSTED_FREE(card);
		return ESP_OK;
	}
	return ESP_FAIL;
}

int hosted_sdio_read_reg(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(card);

	/* Need to apply address mask when reading/writing slave registers */
	reg &= ESP_ADDRESS_MASK;
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_read_byte(card, SDIO_FUNC_1, reg, data);
	} else {
		res = sdmmc_io_read_bytes(card, SDIO_FUNC_1, reg, data, size);
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_write_reg(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(card);

	/* Need to apply address mask when reading/writing slave registers */
	reg &= ESP_ADDRESS_MASK;
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_write_byte(card, SDIO_FUNC_1, reg, *data, NULL);
	} else {
		res = sdmmc_io_write_bytes(card, SDIO_FUNC_1, reg, data, size);
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_read_block(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(card);
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_read_byte(card, SDIO_FUNC_1, reg, data);
	} else {
		res = sdio_read_fromio(card, SDIO_FUNC_1, reg, data, H_SDIO_RX_LEN_TO_TRANSFER(size));
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_write_block(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(card);
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_write_byte(card, SDIO_FUNC_1, reg, *data, NULL);
	} else {
		res = sdio_write_toio(card, SDIO_FUNC_1, reg, data, H_SDIO_TX_LEN_TO_TRANSFER(size));
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

/* Blocking fn call. Returns when SDIO slave device generates a SDIO interupt */
int hosted_sdio_wait_slave_intr(uint32_t ticks_to_wait)
{
	SDIO_FAIL_IF_NULL(card);

	return sdmmc_io_wait_int(card, ticks_to_wait);
}
