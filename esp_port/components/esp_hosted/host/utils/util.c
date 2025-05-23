// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/** Includes **/
#include "util.h"
#include "ctype.h"
#include "string.h"

/** Constants/Macros **/

/** Exported variables **/

/** Function declaration **/

/** Exported Functions **/
/*
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
int ipv4_addr_aton(const char *cp, uint32_t *ip_uint32)
{
	u_long val, base, n;
	char c;
	u_long parts[4], *pp = parts;

	for (;;) {
		/*
		 * Collect number up to ``.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, other=decimal.
		 */
		val = 0; base = 10;
		if (*cp == '0') {
			if (*++cp == 'x' || *cp == 'X')
				base = 16, cp++;
			else
				base = 8;
		}
		while ((c = *cp) != '\0') {
			if (isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				cp++;
				continue;
			}
			if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) +
					(c + 10 - (islower(c) ? 'a' : 'A'));
				cp++;
				continue;
			}
			break;
		}
		if (*cp == '.') {
			/*
			 * Internet format:
			 *	a.b.c.d
			 *	a.b.c	(with c treated as 16-bits)
			 *	a.b	(with b treated as 24 bits)
			 */
			if (pp >= parts + 3 || val > 0xff)
				return (0);
			*pp++ = val, cp++;
		} else
			break;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && (!isascii((uint8_t)*cp) || !isspace((uint8_t)*cp)))
		return (0);
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;
	switch (n) {

	case 1:				/* a -- 32 bits */
		break;

	case 2:				/* a.b -- 8.24 bits */
		if (val > 0xffffff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		if (val > 0xffff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if(ip_uint32) {
		*ip_uint32 = hton_long(val);
	}
	return (1);
}

/**
 * @brief  convert ip in int to string
 *
 * @param addr ip address in network order to convert
 * @param buf target buffer where the string is stored
 * @param buflen length of buf
 * @return either pointer to buf which now holds the ASCII
 *         representation of addr or NULL if buf was too small
 */

char * ipv4_addr_ntoa(uint32_t addr, char *buf, int buflen)
{
	char inv[3];
	char *rp;
	uint8_t *ap;
	uint8_t rem;
	uint8_t n;
	uint8_t i;
	int len = 0;
	uint32_t addr_nw = ntoh_long(addr);

	rp = buf;
	ap = (uint8_t *)&addr_nw;
	for (n = 0; n < 4; n++) {
		i = 0;
		do {
			rem = *ap % (uint8_t)10;
			*ap /= (uint8_t)10;
			inv[i++] = (char)('0' + rem);
		} while (*ap);
		while (i--) {
			if (len++ >= buflen) {
				return NULL;
			}
			*rp++ = inv[i];
		}
		if (len++ >= buflen) {
			return NULL;
		}
		*rp++ = '.';
		ap++;
	}
	*--rp = 0;
	return buf;
}

/**
  * @brief  Convert mac string to byte stream
  * @param  out - output mac in bytes
  *         s - input mac string
  * @retval STM_OK/STM_FAIL
  */
stm_ret_t convert_mac_to_bytes(uint8_t *out, const char *s)
{
	int mac[MAC_LEN] = {0};
	int num_bytes = 0;

	if (!s || (strlen(s) < MIN_MAC_STRING_LEN))  {
		return STM_FAIL;
	}

	num_bytes =  sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x",
			&mac[0],&mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	if ((num_bytes < MAC_LEN)  ||
		(mac[0] > 0xFF) ||
		(mac[1] > 0xFF) ||
		(mac[2] > 0xFF) ||
		(mac[3] > 0xFF) ||
		(mac[4] > 0xFF) ||
		(mac[5] > 0xFF)) {
		return STM_FAIL;
	}

	out[0] = mac[0]&0xff;
	out[1] = mac[1]&0xff;
	out[2] = mac[2]&0xff;
	out[3] = mac[3]&0xff;
	out[4] = mac[4]&0xff;
	out[5] = mac[5]&0xff;

	return STM_OK;
}

/**
  * @brief  compare two buff in bytes
  * @param  buff1 - in bytes
  *         buff2 - in bytes
  * @retval 1 if same, else 0
  */
uint8_t is_same_buff(void *buff1, void *buff2, uint16_t len)
{
	uint16_t idx;
	uint8_t *b1 = (uint8_t*)buff1;
	uint8_t *b2 = (uint8_t*)buff2;

	if ((b1 == NULL) && (b2==NULL)) {
		if(len) {
			return 0;
		}
		return 1;
	}

	if(!b1 || !b2) {
		return 0;
	}

	/* Function assumes buff1 and buff2 are allocated for len */
	for (idx=0; idx < len; idx++) {
		if (*b1 != *b2) {
			return 0;
		}
		b1++;
		b2++;
	}
	return 1;
}

/**
  * @brief  Get ip in 32bit from dotted string notation
  * @param  ip_s - input ip address in string
  *         ip_x - output ip address in 32 bit
  * @retval STM_OK/STM_FAIL
  */
stm_ret_t get_ipaddr_from_str(const char *ip_s, uint32_t *ip_x)
{
	uint32_t ip_nw = 0;
	if (! ipv4_addr_aton(ip_s, &ip_nw))
	{
		return STM_FAIL;
	}
	/* ipv4_addr_aton does conversion in network order. reverse */
	*ip_x = ntoh_long(ip_nw);
	return STM_OK;
}
