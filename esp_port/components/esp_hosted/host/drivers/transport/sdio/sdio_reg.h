// SPDX-License-Identifier: Apache-2.0
/*
 * Espressif Systems Wireless LAN device driver
 *
 * Copyright (C) 2015-2023 Espressif Systems (Shanghai) PTE LTD
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

#ifndef __SDIO_REG_H
#define __SDIO_REG_H

/** Includes **/
#include "common.h"

/** constants/macros **/
#define SD_IO_CCCR_FN_ENABLE           0x02
#define SD_IO_CCCR_FN_READY            0x03
#define SD_IO_CCCR_INT_ENABLE          0x04
#define SD_IO_CCCR_BUS_WIDTH           0x07

#define CCCR_BUS_WIDTH_ECSI            (1<<5)

#define SD_IO_CCCR_BLKSIZEL            0x10
#define SD_IO_CCCR_BLKSIZEH            0x11

/* Interrupt Status */
#define ESP_SLAVE_BIT0_INT             BIT(0)
#define ESP_SLAVE_BIT1_INT             BIT(1)
#define ESP_SLAVE_BIT2_INT             BIT(2)
#define ESP_SLAVE_BIT3_INT             BIT(3)
#define ESP_SLAVE_BIT4_INT             BIT(4)
#define ESP_SLAVE_BIT5_INT             BIT(5)
#define ESP_SLAVE_BIT6_INT             BIT(6)
#define ESP_SLAVE_BIT7_INT             BIT(7)
#define ESP_SLAVE_RX_UNDERFLOW_INT     BIT(16)
#define ESP_SLAVE_TX_OVERFLOW_INT      BIT(17)
#define ESP_SLAVE_RX_NEW_PACKET_INT    BIT(23)

#define ESP_SLAVE_CMD53_END_ADDR       0x1F800
#define ESP_SLAVE_LEN_MASK             0xFFFFF
#define ESP_BLOCK_SIZE                 512
#define ESP_RX_BYTE_MAX                0x100000
#define ESP_RX_BUFFER_SIZE             1536

#define ESP_TX_BUFFER_MASK             0xFFF
#define ESP_TX_BUFFER_MAX              0x1000
#define ESP_MAX_BUF_CNT                10

#define ESP_SLAVE_SLCHOST_BASE         0x3FF55000

#define HOST_TO_SLAVE_INTR             ESP_SLAVE_SCRATCH_REG_7
/* SLAVE registers */
/* Interrupt Registers */
#define ESP_SLAVE_INT_RAW_REG          (ESP_SLAVE_SLCHOST_BASE + 0x50)
#define ESP_SLAVE_INT_ST_REG           (ESP_SLAVE_SLCHOST_BASE + 0x58)
#define ESP_SLAVE_INT_CLR_REG          (ESP_SLAVE_SLCHOST_BASE + 0xD4)
#define ESP_HOST_INT_ENA_REG           (ESP_SLAVE_SLCHOST_BASE + 0xDC)

/* Host side interrupts for ESP_HOST_INT_ENA_REG */
#if CONFIG_SLAVE_CHIPSET_ESP32 || CONFIG_SLAVE_CHIPSET_ESP32C6
  #define SDIO_S2H_INTR_NEW_PKT         (23)
  #define SDIO_S2H_INTR_FLOW_CTRL_ON    (7)
  #define SDIO_S2H_INTR_FLOW_CTRL_OFF   (6)
#else
  #error "SDIO New Packet Intr Bit not defined for Hosted Slave"
#endif

/* Data path registers*/
#define ESP_SLAVE_PACKET_LEN_REG       (ESP_SLAVE_SLCHOST_BASE + 0x60)
#define ESP_SLAVE_TOKEN_RDATA          (ESP_SLAVE_SLCHOST_BASE + 0x44)

/* Scratch registers*/
#define ESP_SLAVE_SCRATCH_REG_0        (ESP_SLAVE_SLCHOST_BASE + 0x6C)
#define ESP_SLAVE_SCRATCH_REG_1        (ESP_SLAVE_SLCHOST_BASE + 0x70)
#define ESP_SLAVE_SCRATCH_REG_2        (ESP_SLAVE_SLCHOST_BASE + 0x74)
#define ESP_SLAVE_SCRATCH_REG_3        (ESP_SLAVE_SLCHOST_BASE + 0x78)
#define ESP_SLAVE_SCRATCH_REG_4        (ESP_SLAVE_SLCHOST_BASE + 0x7C)
#define ESP_SLAVE_SCRATCH_REG_6        (ESP_SLAVE_SLCHOST_BASE + 0x88)
#define ESP_SLAVE_SCRATCH_REG_7        (ESP_SLAVE_SLCHOST_BASE + 0x8C)
#define ESP_SLAVE_SCRATCH_REG_8        (ESP_SLAVE_SLCHOST_BASE + 0x9C)
#define ESP_SLAVE_SCRATCH_REG_9        (ESP_SLAVE_SLCHOST_BASE + 0xA0)
#define ESP_SLAVE_SCRATCH_REG_10       (ESP_SLAVE_SLCHOST_BASE + 0xA4)
#define ESP_SLAVE_SCRATCH_REG_11       (ESP_SLAVE_SLCHOST_BASE + 0xA8)
#define ESP_SLAVE_SCRATCH_REG_12       (ESP_SLAVE_SLCHOST_BASE + 0xAC)
#define ESP_SLAVE_SCRATCH_REG_13       (ESP_SLAVE_SLCHOST_BASE + 0xB0)
#define ESP_SLAVE_SCRATCH_REG_14       (ESP_SLAVE_SLCHOST_BASE + 0xB4)
#define ESP_SLAVE_SCRATCH_REG_15       (ESP_SLAVE_SLCHOST_BASE + 0xB8)

#define ESP_ADDRESS_MASK               (0x3FF)

#define ESP_VENDOR_ID                  (0x6666)
#define ESP_DEVICE_ID_1                (0x2222)
#define ESP_DEVICE_ID_2                (0x3333)


#define SDIO_REG(x)                    ((x)&ESP_ADDRESS_MASK)

#define SDIO_FUNC_0                    (0)
#define SDIO_FUNC_1                    (1)

#define ESP_SDIO_CONF_OFFSET           (0)
#define ESP_SDIO_SEND_OFFSET           (16)

#endif /* __SDIO_REG_H */
