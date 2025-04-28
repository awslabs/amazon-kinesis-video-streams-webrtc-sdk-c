/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#ifndef __AWS_KVS_WEBRTC_NETIO_INCLUDE__
#define __AWS_KVS_WEBRTC_NETIO_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include <stdbool.h>
#include "common_defs.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct NetIo* NetIoHandle;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * @brief Create a network I/O handle
 *
 * @return The network I/O handle
 */
NetIoHandle NetIo_create(void);

/**
 * @brief Terminate a network I/O handle
 *
 * @param[in] xNetIoHandle The network I/O handle
 */
void NetIo_terminate(NetIoHandle xNetIoHandle);

/**
 * @brief Connect to a host with port
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[in] pcHost The hostname
 * @param[in] pcPort The port
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_connect(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort);

/**
 * @brief Connect to a host with port and X509 certificates
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[in] pcHost The hostname
 * @param[in] pcPort The port
 * @param[in] pcRootCA The X509 root CA
 * @param[in] pcCert The X509 client certificate
 * @param[in] pcPrivKey The x509 client private key
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_connectWithX509(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort, const char* pcRootCA, const char* pcCert,
                          const char* pcPrivKey);

int NetIo_connectWithX509Path(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort, const char* pcRootCA, const char* pcCert,
                              const char* pcPrivKey);
/**
 * @breif Disconnect from a host
 *
 * @param[in] xNetIoHandle The network I/O handle
 */
void NetIo_disconnect(NetIoHandle xNetIoHandle);

/**
 * @brief Send data
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[in] pBuffer The data buffer
 * @param[in] uBytesToSend The length of data
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_send(NetIoHandle xNetIoHandle, const unsigned char* pBuffer, size_t uBytesToSend);

/**
 * @brief Receive data
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[in,out] pBuffer The data buffer
 * @param[in] uBufferSize The size of the data buffer
 * @param[out] puBytesReceived The actual bytes received
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_recv(NetIoHandle xNetIoHandle, unsigned char* pBuffer, size_t uBufferSize, size_t* puBytesReceived);

/**
 * @brief Check if any data available
 *
 * @param xNetIoHandle The network I/O handle
 * @return true if data available, false otherwise
 */
bool NetIo_isDataAvailable(NetIoHandle xNetIoHandle);

/**
 * @brief Configure receive timeout.
 *
 * @param xNetIoHandle The network I/O handle
 * @param uRecvTimeoutMs Receive timeout in milliseconds
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_setRecvTimeout(NetIoHandle xNetIoHandle, unsigned int uRecvTimeoutMs);

/**
 * @brief Configure send timeout.
 *
 * @param xNetIoHandle The network I/O handle
 * @param uSendTimeoutMs Send timeout in milliseconds
 * @return 0 on success, non-zero value otherwise
 */
int NetIo_setSendTimeout(NetIoHandle xNetIoHandle, unsigned int uSendTimeoutMs);
uint32_t NetIo_getSocket(NetIoHandle xNetIoHandle);

INT32 net_getErrorCode(VOID);
PCHAR net_getErrorString(INT32 error);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_NETIO_INCLUDE__ */
