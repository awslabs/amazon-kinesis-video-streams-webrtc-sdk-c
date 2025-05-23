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

/* Wrapper interfaces for SDMMC to communicated with slave using SDIO */

#ifndef __SDIO_WRAPPER_H_
#define __SDIO_WRAPPER_H_

#include "esp_check.h"
#include "sdmmc_cmd.h"

#define MAX_TRANSPORT_BUFFER_SIZE        MAX_SDIO_BUFFER_SIZE

/* Hosted init function to init the SDIO host
 * returns a pointer to the sdio context */
void * hosted_sdio_init(void);

/* Hosted SDIO deinit function
 * expects a pointer to the sdio context */
/* Hosted SDIO to initialise the SDIO card */
int hosted_sdio_card_init(void);
int hosted_sdio_card_deinit(void);

/* Hosted SDIO functions to read / write to slave scratch registers
 * and to read / write block data
 * If lock_required is true, call will hold a mutex for the duration of the call */
int hosted_sdio_read_reg(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);
int hosted_sdio_write_reg(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);
int hosted_sdio_read_block(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);
int hosted_sdio_write_block(uint32_t reg, uint8_t *data, uint16_t size, bool lock_required);

/* Hosted SDIO function that will block waiting for a SDIO interrupt from the slave
 * returns when there is an interrupt or timeout */
int hosted_sdio_wait_slave_intr(uint32_t ticks_to_wait);

#endif
