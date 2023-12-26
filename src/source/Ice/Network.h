/*******************************************
HostInfo internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOCAL_NETWORK_INTERFACE_COUNT 128

// string buffer size for ipv4 and ipv6. Null terminator included.
// for ipv6: 0000:0000:0000:0000:0000:0000:0000:0000 = 39
// for ipv4 mapped ipv6: 0000:0000:0000:0000:0000:ffff:192.168.100.228 = 45
#define KVS_IP_ADDRESS_STRING_BUFFER_LEN 46

// 000.000.000.000
#define KVS_MAX_IPV4_ADDRESS_STRING_LEN 15

#define KVS_GET_IP_ADDRESS_PORT(a) ((UINT16) getInt16((a)->port))

#define IPV4_TEMPLATE "%d.%d.%d.%d"
#define IPV6_TEMPLATE "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"

#if defined(__MACH__)
#define NO_SIGNAL_SOCK_OPT SO_NOSIGPIPE
#define NO_SIGNAL_SEND     0
#else
#define NO_SIGNAL_SEND MSG_NOSIGNAL
#endif

// Some systems, such as Windows, do not have this value
#ifndef EAI_SYSTEM
#define EAI_SYSTEM -11
#endif

// Windows uses EWOULDBLOCK (WSAEWOULDBLOCK) to indicate connection attempt
// cannot be completed immediately, whereas POSIX uses EINPROGRESS.
#ifdef _WIN32
#define KVS_SOCKET_IN_PROGRESS EWOULDBLOCK
#else
#define KVS_SOCKET_IN_PROGRESS EINPROGRESS
#endif

typedef enum {
    KVS_SOCKET_PROTOCOL_NONE,
    KVS_SOCKET_PROTOCOL_TCP,
    KVS_SOCKET_PROTOCOL_UDP,
} KVS_SOCKET_PROTOCOL;

/**
 * @param - PKvsIpAddress - IN/OUT - array for getLocalhostIpAddresses to store any local ips it found. The ip address and port
 *                                   will be in network byte order.
 * @param - UINT32 - IN/OUT - length of the array, upon return it will be updated to the actual number of ips in the array
 *
 *@param - IceSetInterfaceFilterFunc - IN - set to custom interface filter callback
 *
 *@param - UINT64 - IN - Set to custom data that can be used in the callback later
 * @return - STATUS status of execution
 */
STATUS getLocalhostIpAddresses(PKvsIpAddress, PUINT32, IceSetInterfaceFilterFunc, UINT64);

// TODO add support for windows socketpair
#ifndef _WIN32
/**
 * @param - INT32 (*)[2] - OUT - Array for the socket pair fds
 *
 * @return - STATUS status of execution
 */
STATUS createSocketPair(INT32 (*)[2]);
#endif

/**
 * @param - KVS_IP_FAMILY_TYPE - IN - Family for the socket. Must be one of KVS_IP_FAMILY_TYPE
 * @param - KVS_SOCKET_PROTOCOL - IN - either tcp or udp
 * @param - UINT32 - IN - send buffer size in bytes
 * @param - PINT32 - OUT - PINT32 for the socketfd
 *
 * @return - STATUS status of execution
 */
STATUS createSocket(KVS_IP_FAMILY_TYPE, KVS_SOCKET_PROTOCOL, UINT32, PINT32);

/**
 * @param - INT32 - IN - INT32 for the socketfd
 *
 * @return - STATUS status of execution
 */
STATUS closeSocket(INT32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to bind. PKvsIpAddress->port will be changed to the actual port number
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
STATUS socketBind(PKvsIpAddress, INT32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to connect.
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
STATUS socketConnect(PKvsIpAddress, INT32);

/**
 * @param - INT32 - Socket to write to
 * @param - const void * - buffer of data to write
 * @param - SIZE_T - length of buffer
 *
 * @return - STATUS status of execution
 */
STATUS socketWrite(INT32, const void*, SIZE_T);

/**
 * @param - PCHAR - IN - hostname to resolve
 *
 * @param - PKvsIpAddress - OUT - resolved ip address
 *
 * @return - STATUS status of execution
 */
STATUS getIpWithHostName(PCHAR, PKvsIpAddress);

/**
 * @param - PCHAR - IN - IP address string to verify if it is IPv4 or IPv6 format
 *
 * @param - UINT16 - IN - Length of string
 *
 * @param - BOOL - OUT - Evaluates to TRUE if the provided string is IPv4/IPv6. False otherwise
 *
 * @return - STATUS status of execution
 */
BOOL isIpAddr(PCHAR, UINT16);

STATUS getIpAddrStr(PKvsIpAddress, PCHAR, UINT32);

BOOL isSameIpAddress(PKvsIpAddress, PKvsIpAddress, BOOL);

/**
 * @return - INT32 error code
 */
INT32 getErrorCode(VOID);

/**
 * @param - INT32 - IN - error code
 *
 * @return - PCHAR string associated with error code
 */
PCHAR getErrorString(INT32);

#ifdef _WIN32
#define POLL WSAPoll
#else
#define POLL poll
#endif

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__ */
