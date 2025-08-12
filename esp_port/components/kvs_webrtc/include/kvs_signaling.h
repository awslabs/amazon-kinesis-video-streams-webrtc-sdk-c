/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _KVS_SIGNALING_H_
#define _KVS_SIGNALING_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "webrtc_signaling_if.h"

#ifdef __cplusplus
extern "C" {
#endif

// kvs fetch credentials callback
typedef int (*kvs_fetch_credentials_cb_t) (
                uint64_t customData,
                const char **pAccessKey,
                uint32_t *pAccessKeyLen,
                const char **pSecretKey,
                uint32_t *pSecretKeyLen,
                const char **pSessionToken,
                uint32_t *pSessionTokenLen, // session token length
                uint32_t *pExpiration /* Expiration in seconds from now */
            );

/**
 * @brief KVS Signaling configuration structure
 *
 * This structure contains all the configuration options needed for
 * AWS Kinesis Video Streams signaling, including both IoT Core
 * credentials and direct AWS credentials options.
 */
typedef struct {
    // Channel configuration
    char *pChannelName;                      //!< Name of the signaling channel (e.g., "my-esp32-channel")

    // AWS credentials configuration
    bool useIotCredentials;                  //!< Whether to use IoT Core credentials (TRUE) or direct AWS credentials (FALSE)
    char *iotCoreCredentialEndpoint;         //!< IoT Core credential endpoint URL (when useIotCredentials=TRUE)
    char *iotCoreCert;                       //!< Path to IoT Core device certificate file (e.g., "/spiffs/cert.pem")
    char *iotCorePrivateKey;                 //!< Path to IoT Core device private key file (e.g., "/spiffs/private.key")
    char *iotCoreRoleAlias;                  //!< IoT Core role alias for credential exchange
    char *iotCoreThingName;                  //!< IoT Core thing name (device identifier)

    // Direct AWS credentials (if not using IoT credentials)
    char *awsAccessKey;                      //!< AWS access key ID (when useIotCredentials=FALSE)
    char *awsSecretKey;                      //!< AWS secret access key (when useIotCredentials=FALSE)
    char *awsSessionToken;                   //!< AWS session token (optional, for temporary credentials)

    // Common AWS options
    char *awsRegion;                         //!< AWS region (e.g., "us-west-2", "eu-west-1")
    char *caCertPath;                        //!< Path to CA certificate bundle file (e.g., "/spiffs/ca.pem")

    // Optional: Callback-based credentials supplier. If set (non-NULL), this takes precedence
    // over IoT Core and static credentials. The callback should return 0 on success.
    // Define a public typedef so application can implement with exact signature.

    kvs_fetch_credentials_cb_t fetch_credentials_cb; //!< Application-provided callback
    uint64_t fetch_credentials_user_data;            //!< Opaque user data passed to fetch_credentials_cb
} kvs_signaling_config_t;

/**
 * @brief Get the KVS signaling client interface implementation
 *
 * This returns a pointer to the KVS signaling interface that implements
 * the portable webrtc_signaling_client_if_t. All interactions should
 * go through this interface to maintain portability.
 *
 * @return webrtc_signaling_client_if_t* Pointer to the KVS signaling interface
 */
webrtc_signaling_client_if_t* kvs_signaling_client_if_get(void);

#ifdef __cplusplus
}
#endif

#endif /* _KVS_SIGNALING_H_ */
