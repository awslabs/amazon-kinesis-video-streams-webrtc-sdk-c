/*
 * Implementation of remote ICE configuration retrieval for streaming-only mode
 * This implementation provides static STUN/TURN servers without requiring AWS credentials
 */

#include "common_defs.h"
#include "app_webrtc.h"
#include "signalling_remote.h"

#define LOG_CLASS "signalling_remote"
static const char *TAG = "signalling_remote";

/*
 * For streaming-only mode, we provide a static configuration of ICE servers
 * This avoids the need for AWS credentials since we're not connecting to a signaling channel
 */
STATUS app_signaling_queryServer_remote(PVOID pAppSignaling, PRtcIceServer pIceServers, PUINT32 pServerNum)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 uriCount = 0;
    UNUSED_PARAM(pAppSignaling);

    ESP_LOGI(TAG, "Setting up ICE servers for streaming-only mode");

    // Add a STUN server
    if (uriCount < MAX_ICE_SERVERS_COUNT) {
        SNPRINTF(pIceServers[uriCount].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.l.google.com:19302");
        pIceServers[uriCount].username[0] = '\0';  // No credentials for STUN
        pIceServers[uriCount].credential[0] = '\0';
        ESP_LOGI(TAG, "Added STUN server: %s", pIceServers[uriCount].urls);
        uriCount++;
    }

    // Add another STUN server for redundancy
    if (uriCount < MAX_ICE_SERVERS_COUNT) {
        SNPRINTF(pIceServers[uriCount].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun1.l.google.com:19302");
        pIceServers[uriCount].username[0] = '\0';
        pIceServers[uriCount].credential[0] = '\0';
        ESP_LOGI(TAG, "Added STUN server: %s", pIceServers[uriCount].urls);
        uriCount++;
    }

    // You can add TURN servers here if needed
    // Example:
    // if (uriCount < MAX_ICE_SERVERS_COUNT) {
    //     SNPRINTF(pIceServers[uriCount].urls, MAX_ICE_CONFIG_URI_LEN, "turn:your-turn-server.com:3478?transport=udp");
    //     STRNCPY(pIceServers[uriCount].username, "username", MAX_ICE_CONFIG_USER_NAME_LEN);
    //     STRNCPY(pIceServers[uriCount].credential, "password", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    //     ESP_LOGI(TAG, "Added TURN server: %s", pIceServers[uriCount].urls);
    //     uriCount++;
    // }

    *pServerNum = uriCount;
    ESP_LOGI(TAG, "Configured %d ICE servers for streaming-only mode", (int) uriCount);

    return retStatus;
}
