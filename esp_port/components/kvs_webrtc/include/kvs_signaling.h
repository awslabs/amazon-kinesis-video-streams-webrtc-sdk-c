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

/**
 * @brief KVS Signaling configuration structure
 *
 * This structure contains all the configuration options needed for
 * AWS Kinesis Video Streams signaling, including both IoT Core
 * credentials and direct AWS credentials options.
 */
typedef struct {
    // Channel configuration
    char *pChannelName;                      // Name of the signaling channel

    // AWS credentials configuration
    bool useIotCredentials;                  // Whether to use IoT Core credentials
    char *iotCoreCredentialEndpoint;         // IoT Core credential endpoint
    char *iotCoreCert;                       // Path to IoT Core certificate
    char *iotCorePrivateKey;                 // Path to IoT Core private key
    char *iotCoreRoleAlias;                  // IoT Core role alias
    char *iotCoreThingName;                  // IoT Core thing name

    // Direct AWS credentials (if not using IoT credentials)
    char *awsAccessKey;                      // AWS access key
    char *awsSecretKey;                      // AWS secret key
    char *awsSessionToken;                   // AWS session token

    // Common AWS options
    char *awsRegion;                         // AWS region
    char *caCertPath;                        // Path to CA certificates
} KvsSignalingConfig, *PKvsSignalingConfig;

/**
 * @brief Get the KVS signaling client interface implementation
 *
 * This returns a pointer to the KVS signaling interface that implements
 * the portable WebRtcSignalingClientInterface. All interactions should
 * go through this interface to maintain portability.
 *
 * @return WebRtcSignalingClientInterface* Pointer to the KVS signaling interface
 */
WebRtcSignalingClientInterface* getKvsSignalingClientInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* _KVS_SIGNALING_H_ */
