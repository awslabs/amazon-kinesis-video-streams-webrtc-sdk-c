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

/** prevent recursive inclusion **/
#ifndef __UTIL_H
#define __UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/** Includes **/
#include "common.h"

/** Constants/Macros **/

/** Exported Structures **/

/** Exported variables **/

/** Exported Functions **/

stm_ret_t get_ipaddr_from_str(const char *ip_s, uint32_t *ip_x);
int ipv4_addr_aton(const char *cp, uint32_t *ip_uint32);
char * ipv4_addr_ntoa(const uint32_t addr, char *buf, int buflen);
stm_ret_t convert_mac_to_bytes(uint8_t *out, const char *s);
uint8_t is_same_buff(void *buff1, void *buff2, uint16_t len);
stm_ret_t get_self_ip(int iface_type, uint32_t *self_ip);

#ifdef __cplusplus
}
#endif

#endif
