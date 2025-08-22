/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Header file for ICE server bridge client (streaming-only side)
 *
 * This component handles ICE server requests via bridge communication
 * from the streaming_only device to the signaling_only device.
 */

#ifndef __ICE_BRIDGE_CLIENT_H__
#define __ICE_BRIDGE_CLIENT_H__

#include "app_webrtc_if.h"
#include "signaling_serializer.h"

/**
 * @brief Retrieves ICE server configuration via bridge communication
 *
 * This function implements the ICE server query for streaming-only mode by
 * sending index-based requests to the signaling device via bridge and
 * receiving individual server responses.
 *
 * @param pAppSignaling - IN - Application signaling configuration (unused in streaming-only mode)
 * @param pIceServers - OUT - Array of ICE server configurations to be populated (RtcIceServer format)
 * @param pServerNum - OUT - Number of ICE servers configured
 *
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS ice_bridge_client_get_servers(void* pAppSignaling, void* pIceServers, uint32_t* pServerNum);

/**
 * @brief Callback function for ICE server responses from bridge_signaling
 *
 * This function is called by bridge_signaling.c when an ICE server response
 * is received from the signaling device. It updates the internal state to
 * notify the waiting ice_bridge_client_get_servers function.
 *
 * @param ice_server_response - IN - ICE server response data
 */
void ice_bridge_client_set_ice_server_response(const ss_ice_server_response_t* ice_server_response);

#endif /* __ICE_BRIDGE_CLIENT_H__ */
