/*
 * Header file for remote ICE configuration retrieval
 */

#ifndef __SIGNALING_REMOTE_H__
#define __SIGNALING_REMOTE_H__

#include "app_webrtc.h"

/**
 * Retrieves ICE server configuration for streaming-only mode
 *
 * @param pAppSignaling - IN - Application signaling configuration (unused in streaming-only mode)
 * @param pIceServers - OUT - Array of ICE server configurations to be populated
 * @param pServerNum - OUT - Number of ICE servers configured
 *
 * @return STATUS code of the execution
 */
STATUS app_signaling_queryServer_remote(PVOID pAppSignaling, PRtcIceServer pIceServers, PUINT32 pServerNum);

#endif /* __SIGNALING_REMOTE_H__ */
