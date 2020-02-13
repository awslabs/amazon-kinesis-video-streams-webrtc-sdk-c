/*******************************************
Socket Connection internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_SOCKET_CONNECTION__
#define __KINESIS_VIDEO_WEBRTC_SOCKET_CONNECTION__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define SSL_WRITE_RETRY_DELAY               10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND

typedef STATUS (*ConnectionDataAvailableFunc)(UINT64, struct __SocketConnection*, PBYTE, UINT32, PKvsIpAddress, PKvsIpAddress);

typedef struct __SocketConnection SocketConnection;
struct __SocketConnection {
    volatile ATOMIC_BOOL connectionClosed; // for tcp;
    INT32 localSocket;
    KVS_SOCKET_PROTOCOL protocol;
    KvsIpAddress peerIpAddr;

    BOOL secureConnection;
    SSL_CTX *pSslCtx;
    BIO *pReadBio;
    BIO *pWriteBio;
    SSL *pSsl;

    MUTEX lock;

    BOOL freeBios;

    ConnectionDataAvailableFunc dataAvailableCallbackFn;
    UINT64 dataAvailableCallbackCustomData;
};
typedef struct __SocketConnection* PSocketConnection;

/**
 * Create a SocketConnection object and store it in PSocketConnection. creates a socket based on KVS_SOCKET_PROTOCOL
 * specified, and bind it to the host ip address. If the protocol is tcp, then peer ip address is required and it will
 * try to establish the tcp connection.
 *
 * @param - PKvsIpAddress - IN - host ip address to bind to
 * @param - PKvsIpAddress - IN - peer ip address to connect in case of TCP
 * @param - KVS_SOCKET_PROTOCOL - IN - socket protocol. TCP or UDP
 * @param - UINT64 - IN - data available callback custom data
 * @param - ConnectionDataAvailableFunc - IN - data available callback (OPTIONAL)
 * @param - UINT32 - IN - send buffer size in bytes
 * @param - PSocketConnection* - OUT - the resulting SocketConnection struct
 *
 * @return - STATUS - status of execution
 */
STATUS createSocketConnection(PKvsIpAddress, PKvsIpAddress, KVS_SOCKET_PROTOCOL, UINT64, ConnectionDataAvailableFunc, UINT32, PSocketConnection*);

/**
 * Free the SocketConnection struct
 *
 * @param - PSocketConnection* - IN - SocketConnection to be freed
 *
 * @return - STATUS - status of execution
 */
STATUS freeSocketConnection(PSocketConnection*);

/**
 * Given a created SocketConnection, initialize TLS or DTLS handshake depending on the socket protocol
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 * @param - BOOL - IN - will SocketConnection act as server during the TLS or DTLS handshake
 *
 * @return - STATUS - status of execution
 */
STATUS socketConnectionInitSecureConnection(PSocketConnection, BOOL);

/**
 * Given a created SocketConnection, send data through the underlying socket. If socket type is UDP, then destination
 * address is required. If socket type is tcp, destination address is ignored and data is send to the peer address provided
 * at SocketConnection creation. If socketConnectionInitSecureConnection has been called then data will be encrypted,
 * otherwise data will be sent as is.
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 * @param - PBYTE - IN - buffer containing unencrypted data
 * @param - UINT32 - IN - length of buffer
 * @param - PKvsIpAddress - IN - destination address. Required only if socket type is UDP.
 *
 * @return - STATUS - status of execution
 */
STATUS socketConnectionSendData(PSocketConnection, PBYTE, UINT32, PKvsIpAddress);

/**
 * Check if SocketConnection is ready to send data. If connection is not secure then TRUE will be returned immediately.
 * Otherwise TRUE will be returned when TLS or DTLS handshake is done.
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 * @param - PBOOL - OUT - whether connection is ready to send data
 *
 * @return - STATUS - status of execution
 */
STATUS socketConnectionReadyToSend(PSocketConnection, PBOOL);

/**
 * If PSocketConnection is not secure then nothing happens, otherwise assuming the bytes passed in are encrypted, and
 * the encryted data will be replaced with unencrypted data at function return.
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 * @param - PBYTE - IN/OUT - buffer containing encrypted data. Will contain unencrypted on successful return
 * @param - UINT32 - IN - available length of buffer
 * @param - PUINT32 - IN/OUT - length of encrypted data. Will contain length of decrypted data on successful return
 *
 * @return - STATUS - status of execution
 */
STATUS socketConnectionReadData(PSocketConnection, PBYTE, UINT32, PUINT32);

/**
 * Mark PSocketConnection as closed
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 *
 * @return - STATUS - status of execution
 */
STATUS socketConnectionClosed(PSocketConnection);

/**
 * Return whether socket has been connected. Return TRUE for UDP sockets.
 * Return TRUE for TCP sockets once the connection has been established, otherwise return FALSE.
 *
 * @param - PSocketConnection - IN - the SocketConnection struct
 *
 * @return - STATUS - status of execution
 */
BOOL socketConnectionIsConnected(PSocketConnection);

// internal functions
STATUS createConnectionCertificateAndKey(X509 **, EVP_PKEY **);
INT32 certificateVerifyCallback(INT32 preverify_ok, X509_STORE_CTX *ctx);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_SOCKET_CONNECTION__ */
