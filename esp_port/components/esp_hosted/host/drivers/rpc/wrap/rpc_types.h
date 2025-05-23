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
#ifndef __RPC_TYPES_H__
#define __RPC_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t int_1;
  int32_t int_2;
  uint32_t uint_1;
  uint32_t uint_2;
  uint16_t data_len;
  uint8_t data[1024];
} rpc_usr_t;

#ifdef __cplusplus
}
#endif

#endif
