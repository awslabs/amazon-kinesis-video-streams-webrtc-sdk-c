/*
 * Espressif Systems Wireless LAN device driver
 *
 * Copyright (C) 2015-2022 Espressif Systems (Shanghai) PTE LTD
 * SPDX-License-Identifier: GPL-2.0 OR Apache-2.0
 */

#ifndef __RPC_CORE_H
#define __RPC_CORE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "rpc_slave_if.h"
#include "os_wrapper.h"

#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

#define MAX_SSID_LENGTH              32
#define MIN_PWD_LENGTH               8
#define MAX_PWD_LENGTH               64
#define MIN_CHNL_NO                  1
#define MAX_CHNL_NO                  11
#define MIN_CONN_NO                  1
#define MAX_CONN_NO                  10

#define CLEANUP_APP_MSG(app_msg) do {                                         \
  if (app_msg) {                                                              \
    if (app_msg->app_free_buff_hdl) {                                         \
      if (app_msg->app_free_buff_func) {                                      \
        app_msg->app_free_buff_func(app_msg->app_free_buff_hdl);              \
        app_msg->app_free_buff_hdl = NULL;                                    \
      }                                                                       \
    }                                                                         \
    HOSTED_FREE(app_msg);                                                     \
  }                                                                           \
} while(0);

#define RPC_FAIL_ON_NULL_PRINT(msGparaM, prinTmsG)                            \
    if (!msGparaM) {                                                          \
        ESP_LOGE(TAG, prinTmsG"\n");                                          \
        goto fail_parse_rpc_msg;                                              \
    }

#define RPC_FAIL_ON_NULL(msGparaM)                                            \
    if (!rpc_msg->msGparaM) {                                                 \
        ESP_LOGE(TAG, "Failed to process rx data\n");                         \
        goto fail_parse_rpc_msg;                                              \
    }


#define RPC_FREE_BUFFS() {                                                    \
  uint8_t idx = 0;                                                            \
  for (idx=0;idx<app_req->n_rpc_free_buff_hdls; idx++)                        \
    HOSTED_FREE(app_req->rpc_free_buff_hdls[idx]);                            \
}

typedef struct q_element {
    void *buf;
    int buf_len;
} esp_queue_elem_t;

//g_h.funcs->_h_memcpy(DsT.data, SrC, len_to_cp);

#if 0
#define RPC_REQ_COPY_BYTES(DsT,SrC,SizE) {                                   \
  if (SizE && SrC) {                                                          \
    DsT.data = (uint8_t *) g_h.funcs->_h_calloc(1, SizE);                     \
    if (!DsT.data) {                                                          \
      hosted_log("Failed to allocate memory for req.%s\n",#DsT);              \
      failure_status = RPC_ERR_MEMORY_FAILURE;                               \
      goto fail_req;                                                          \
    }                                                                         \
    buff_to_free[num_buff_to_free++] = (uint8_t*)DsT.data;                    \
    g_h.funcs->_h_memcpy(DsT.data, SrC, SizE);                                \
    DsT.len = SizE;                                                           \
  }                                                                           \
}
#endif
#define RPC_REQ_COPY_BYTES(DsT,SrC,SizE) {                                    \
  if (SizE && SrC) {                                                          \
	DsT.data = SrC;                                                           \
	DsT.len = SizE;                                                           \
  }                                                                           \
}

#define RPC_REQ_COPY_STR(DsT,SrC,MaxSizE) {                                   \
  if (SrC) {                                                                  \
    RPC_REQ_COPY_BYTES(DsT, SrC, min(strlen((char*)SrC)+1,MaxSizE));          \
  }                                                                           \
}

int rpc_core_init(void);
int rpc_core_deinit(void);
/*
 * Allows user app to create low level protobuf request
 * returns SUCCESS(0) or FAILURE(-1)
 */
int rpc_send_req(ctrl_cmd_t *app_req);

/* When request is sent without an async callback, this function will be called
 * It will wait for control response or timeout for control response
 * This is only used in synchrounous control path
 *
 * Input:
 * > req - control request from user
 *
 * Returns: control response or NULL in case of timeout
 *
 **/
ctrl_cmd_t * rpc_wait_and_parse_sync_resp(ctrl_cmd_t *req);


/* Checks if async control response callback is available
 * in argument passed of type control request
 *
 * Input:
 * > req - control request from user
 *
 * Returns:
 * > CALLBACK_AVAILABLE - if a non NULL asynchrounous control response
 *                      callback is available
 * In case of failures -
 * > MSG_ID_OUT_OF_ORDER - if request msg id is unsupported
 * > CALLBACK_NOT_REGISTERED - if aync callback is not available
 **/
int compose_rpc_req(Rpc *req, ctrl_cmd_t *app_req, int32_t *failure_status);

int is_event_callback_registered(int event);

int rpc_parse_evt(Rpc *rpc_msg, ctrl_cmd_t *app_ntfy);

int rpc_parse_rsp(Rpc *rpc_msg, ctrl_cmd_t *app_resp);
#endif /* __RPC_CORE_H */
