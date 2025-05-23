// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0-only OR Apache-2.0 */

#ifndef __ESP_NETWORK_ADAPTER__H
#define __ESP_NETWORK_ADAPTER__H

#define PRIO_Q_SERIAL                             0
#define PRIO_Q_BT                                 1
#define PRIO_Q_OTHERS                             2
#define MAX_PRIORITY_QUEUES                       3
#define MAC_SIZE_BYTES                            6

/* ESP Payload Header Flags */
#define MORE_FRAGMENT                             (1 << 0)
#define FLAG_WAKEUP_PKT                           (1 << 1)

/* Serial interface */
#define SERIAL_IF_FILE                            "/dev/esps0"

/* Protobuf related info */
/* Endpoints registered must have same string length */
#define RPC_EP_NAME_RSP                           "RPCRsp"
#define RPC_EP_NAME_EVT                           "RPCEvt"


#define H_SET_BIT(pos, val)                       (val|=(1<<pos))

#define H_GET_BIT(pos, val)                       (val&(1<<pos)? 1: 0)

/* Station config bitmasks */
enum {
	STA_RM_ENABLED_BIT         = 0,
	STA_BTM_ENABLED_BIT        = 1,
	STA_MBO_ENABLED_BIT        = 2,
	STA_FT_ENABLED_BIT         = 3,
	STA_OWE_ENABLED_BIT        = 4,
	STA_TRASITION_DISABLED_BIT = 5,
	STA_MAX_USED_BIT           = 6,
};

#define WIFI_CONFIG_STA_RESERVED_BITMASK          0xFFC0

#define WIFI_CONFIG_STA_GET_RESERVED_VAL(num)                                   \
    ((num&WIFI_CONFIG_STA_RESERVED_BITMASK)>>STA_MAX_USED_BIT)

#define WIFI_CONFIG_STA_SET_RESERVED_VAL(reserved_in,num_out)                   \
    (num_out|=(reserved_in <<  STA_MAX_USED_BIT));

enum {
	WIFI_SCAN_AP_REC_phy_11b_BIT       = 0,
	WIFI_SCAN_AP_REC_phy_11g_BIT       = 1,
	WIFI_SCAN_AP_REC_phy_11n_BIT       = 2,
	WIFI_SCAN_AP_REC_phy_lr_BIT        = 3,
	WIFI_SCAN_AP_REC_phy_11ax_BIT      = 4,
	WIFI_SCAN_AP_REC_wps_BIT           = 5,
	WIFI_SCAN_AP_REC_ftm_responder_BIT = 6,
	WIFI_SCAN_AP_REC_ftm_initiator_BIT = 7,
	WIFI_SCAN_AP_REC_MAX_USED_BIT      = 8,
};

#define WIFI_SCAN_AP_RESERVED_BITMASK             0xFF00

#define WIFI_SCAN_AP_GET_RESERVED_VAL(num)                                      \
    ((num&WIFI_SCAN_AP_RESERVED_BITMASK)>>WIFI_SCAN_AP_REC_MAX_USED_BIT)

#define WIFI_SCAN_AP_SET_RESERVED_VAL(reserved_in,num_out)                      \
    (num_out|=(reserved_in <<  WIFI_SCAN_AP_REC_MAX_USED_BIT));

enum {
	WIFI_STA_INFO_phy_11b_BIT       = 0,
	WIFI_STA_INFO_phy_11g_BIT       = 1,
	WIFI_STA_INFO_phy_11n_BIT       = 2,
	WIFI_STA_INFO_phy_lr_BIT        = 3,
	WIFI_STA_INFO_phy_11ax_BIT      = 4,
	WIFI_STA_INFO_is_mesh_child_BIT = 5,
	WIFI_STA_INFO_MAX_USED_BIT      = 6,
};

#define WIFI_STA_INFO_RESERVED_BITMASK             0xFFC0

#define WIFI_STA_INFO_GET_RESERVED_VAL(num)                                      \
    ((num&WIFI_STA_INFO_RESERVED_BITMASK)>>WIFI_STA_INFO_MAX_USED_BIT)

#define WIFI_STA_INFO_SET_RESERVED_VAL(reserved_in,num_out)                      \
    (num_out|=(reserved_in <<  WIFI_STA_INFO_MAX_USED_BIT));

/* WIFI HE AP Info bitmasks */
enum {
	// WIFI_HE_AP_INFO_BSS_COLOR is six bits wide
	WIFI_HE_AP_INFO_partial_bss_color_BIT  = 6,
	WIFI_HE_AP_INFO_bss_color_disabled_BIT = 7,
	WIFI_HE_AP_INFO_MAX_USED_BIT           = 8,
};

#define WIFI_HE_AP_INFO_BSS_COLOR_BITS 0x3F

/* WIFI HE Station Config bitmasks */
enum {
	WIFI_HE_STA_CONFIG_he_dcm_set_BIT                                     = 0,
	// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx is two bits wide
	WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx_BITS                   = 1,
	// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx is two bits wide
	WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx_BITS                   = 3,
	WIFI_HE_STA_CONFIG_he_mcs9_enabled_BIT                                = 5,
	WIFI_HE_STA_CONFIG_he_su_beamformee_disabled_BIT                      = 6,
	WIFI_HE_STA_CONFIG_he_trig_su_bmforming_feedback_disabled_BIT         = 7,
	WIFI_HE_STA_CONFIG_he_trig_mu_bmforming_partial_feedback_disabled_BIT = 8,
	WIFI_HE_STA_CONFIG_he_trig_cqi_feedback_disabled_BIT                  = 9,
	WIFI_HE_STA_CONFIG_MAX_USED_BIT                                       = 10,
};

#define WIFI_HE_STA_CONFIG_BITS 0xFC00

#define WIFI_HE_STA_GET_RESERVED_VAL(num)                                      \
    ((num&WIFI_HE_STA_CONFIG_BITS)>>WIFI_HE_STA_CONFIG_MAX_USED_BIT)

#define WIFI_HE_STA_SET_RESERVED_VAL(reserved_in,num_out)                      \
    (num_out|=(reserved_in <<  WIFI_HE_STA_CONFIG_MAX_USED_BIT));

#define H_FLOW_CTL_NC  0
#define H_FLOW_CTL_ON  1
#define H_FLOW_CTL_OFF 2

struct esp_payload_header {
	uint8_t          if_type:4;
	uint8_t          if_num:4;
	uint8_t          flags;
	uint16_t         len;
	uint16_t         offset;
	uint16_t         checksum;
	uint16_t		 seq_num;
	uint8_t          throttle_cmd:2;
	uint8_t          reserved2:6;
#ifdef CONFIG_ESP_PKT_STATS
//#ifdef CONFIG_ESP_PKT_NUM_IN_HEADER
	uint16_t         pkt_num;
#endif
	/* Position of union field has to always be last,
	 * this is required for hci_pkt_type */
	union {
		uint8_t      reserved3;
		uint8_t      hci_pkt_type;		/* Packet type for HCI interface */
		uint8_t      priv_pkt_type;		/* Packet type for priv interface */
	};
	/* Do no add anything here */
} __attribute__((packed));

#define H_ESP_PAYLOAD_HEADER_OFFSET sizeof(struct esp_payload_header)


typedef enum {
	ESP_INVALID_IF,
	ESP_STA_IF,
	ESP_AP_IF,
	ESP_SERIAL_IF,
	ESP_HCI_IF,
	ESP_PRIV_IF,
	ESP_TEST_IF,
	ESP_ETH_IF,
	ESP_MAX_IF,
} esp_hosted_if_type_t;

typedef enum {
	ESP_OPEN_DATA_PATH,
	ESP_CLOSE_DATA_PATH,
	ESP_RESET,
	ESP_POWER_SAVE_ON,
	ESP_POWER_SAVE_OFF,
	ESP_MAX_HOST_INTERRUPT,
} ESP_HOST_INTERRUPT;


typedef enum {
	ESP_WLAN_SDIO_SUPPORT = (1 << 0),
	ESP_BT_UART_SUPPORT = (1 << 1),
	ESP_BT_SDIO_SUPPORT = (1 << 2),
	ESP_BLE_ONLY_SUPPORT = (1 << 3),
	ESP_BR_EDR_ONLY_SUPPORT = (1 << 4),
	ESP_WLAN_SPI_SUPPORT = (1 << 5),
	ESP_BT_SPI_SUPPORT = (1 << 6),
	ESP_CHECKSUM_ENABLED = (1 << 7),
} ESP_CAPABILITIES;

typedef enum {
	// spi hd capabilities
	ESP_SPI_HD_INTERFACE_SUPPORT_2_DATA_LINES = (1 << 0),
	ESP_SPI_HD_INTERFACE_SUPPORT_4_DATA_LINES = (1 << 1),
	// leave a gap for future expansion

	// features supported
	ESP_WLAN_SUPPORT         = (1 << 4),
	ESP_BT_INTERFACE_SUPPORT = (1 << 5), // bt supported over current interface
} ESP_EXTENDED_CAPABILITIES;

typedef enum {
	ESP_TEST_RAW_TP_NONE = 0,
	ESP_TEST_RAW_TP = (1 << 0),
	ESP_TEST_RAW_TP__ESP_TO_HOST = (1 << 1),
	ESP_TEST_RAW_TP__HOST_TO_ESP = (1 << 2),
	ESP_TEST_RAW_TP__BIDIRECTIONAL = (1 << 3),
} ESP_RAW_TP_MEASUREMENT;

typedef enum {
	ESP_PACKET_TYPE_EVENT = 0x33,
	ESP_PACKET_TYPE_DATA,
} ESP_PRIV_PACKET_TYPE;

typedef enum {
	ESP_PRIV_EVENT_INIT = 0x22,
} ESP_PRIV_EVENT_TYPE;

enum {
	ESP_PKT_UPDATE_DHCP_STATUS,
};

typedef enum {
	ESP_PRIV_CAPABILITY=0x11,
	ESP_PRIV_FIRMWARE_CHIP_ID,
	ESP_PRIV_TEST_RAW_TP,
	ESP_PRIV_RX_Q_SIZE,
	ESP_PRIV_TX_Q_SIZE,
} ESP_PRIV_TAG_TYPE;

/* Host to slave config
 * generally after slave bootup msg */
typedef enum {
	HOST_CAPABILITIES=0x44,
	RCVD_ESP_FIRMWARE_CHIP_ID,
	SLV_CONFIG_TEST_RAW_TP,
	SLV_CFG_FLOW_CTL_START_THRESHOLD,
	SLV_CFG_FLOW_CTL_CLEAR_THRESHOLD,
} SLAVE_CONFIG_PRIV_TAG_TYPE;

struct esp_priv_event {
	uint8_t		event_type;
	uint8_t		event_len;
	uint8_t		event_data[0];
}__attribute__((packed));

#define SPI_HD_HOST_24_BIT_TX_INT  1

/* use upper 8 bits of tx buf len register as interrupt control bits
 * host sends CMD9 to clear the register */
#define SPI_HD_TX_BUF_LEN_MASK  (0x00FFFFFF)

#define SPI_HD_INT_MASK           (3 << 24)
#define SPI_HD_INT_START_THROTTLE (1 << 24)
#define SPI_HD_INT_STOP_THROTTLE  (1 << 25)

/** Slave Registers used for SPI Half-Duplex mode transfers */
typedef enum {
	SPI_HD_REG_SLAVE_READY     = 0x00,
	SPI_HD_REG_MAX_TX_BUF_LEN  = 0x04,
	SPI_HD_REG_MAX_RX_BUF_LEN  = 0x08,
	SPI_HD_REG_TX_BUF_LEN      = 0x0C, // updated when slave wants to tx data
	SPI_HD_REG_RX_BUF_LEN      = 0x10, // updated when slave can rx data
	SPI_HD_REG_SLAVE_CTRL      = 0x14, // to control the slave
} SLAVE_CONFIG_SPI_HD_REGISTERS;

typedef enum {
	SPI_HD_STATE_SLAVE_READY = 0xEE, // Slave SPI is ready
} SLAVE_CONFIG_SPI_HD_STATE;

// slave control bits
typedef enum {
	SPI_HD_CTRL_DATAPATH_ON  = (1 << 0),
} SLAVE_CTRL_MASK;

static inline uint16_t compute_checksum(uint8_t *buf, uint16_t len)
{
	uint16_t checksum = 0;
	uint16_t i = 0;

	while(i < len) {
		checksum += buf[i];
		i++;
	}

	return checksum;
}

#endif
