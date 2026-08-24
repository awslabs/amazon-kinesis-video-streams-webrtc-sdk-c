/*******************************************
Main internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#endif

////////////////////////////////////////////////////
// Project include files
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#ifdef KVS_USE_OPENSSL
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#elif KVS_USE_MBEDTLS
/* mbedTLS 4 moved legacy entropy/CTR-DRBG/hash/RSA/ECP/bignum headers under
 * mbedtls/private/ and gates their prototypes behind
 * MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS. The symbols still ship in libmbedcrypto,
 * so we declare the macro before mbedtls/ssl.h to re-expose them across all
 * includes below. Note: the mbedtls/private/ headers are explicitly unsupported upstream
 * and may break across v4.x point releases. TODO: migrate to PSA Crypto APIs
 * (psa_crypto_init + psa_generate_random + psa_hash_compute + psa_mac_compute)
 * in a follow-up so we can drop this opt-out. */
/* Nested guard: a bare __has_include(...) in an #if is a syntax error on
 * pre-C23 / older compilers (e.g. GCC < 5) — the && does not stop the parse.
 * #ifdef __has_include is safe everywhere; only use the operator when present. */
#ifdef __has_include
#if __has_include(<mbedtls/build_info.h>)
#include <mbedtls/build_info.h>
#else
#include <mbedtls/version.h>
#endif
#else
#include <mbedtls/version.h>
#endif
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_MAJOR >= 4
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#endif
#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_MAJOR >= 4
#include <mbedtls/private/entropy.h>
#include <mbedtls/private/ctr_drbg.h>
#include <mbedtls/private/sha256.h>
#include <mbedtls/private/md5.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/private/rsa.h>
#include <mbedtls/private/ecp.h>
#include <mbedtls/private/bignum.h>
#else
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#if MBEDTLS_VERSION_NUMBER < 0x03000000
#include <mbedtls/certs.h>
#endif
#include <mbedtls/sha256.h>
#include <mbedtls/md5.h>
#endif
#endif

#ifdef USE_LIBSRTP3
#include <srtp3/srtp.h>
#else
#include <srtp2/srtp.h>
#endif

// INET/INET6 MUST be defined before usrsctp
// If removed will cause corruption that is hard to determine at runtime
#define INET  1
#define INET6 1
#include <usrsctp.h>

#if !defined __WINDOWS_BUILD__
#include <signal.h>
#include <sys/types.h>
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#endif

// Max uFrag and uPwd length as documented in https://tools.ietf.org/html/rfc5245#section-15.4
#define ICE_MAX_UFRAG_LEN 256
#define ICE_MAX_UPWD_LEN  256

// Max stun username attribute len: https://tools.ietf.org/html/rfc5389#section-15.3
#define STUN_MAX_USERNAME_LEN (UINT16) 512

// https://tools.ietf.org/html/rfc5389#section-15.7
#define STUN_MAX_REALM_LEN (UINT16) 128

// https://tools.ietf.org/html/rfc5389#section-15.8
#define STUN_MAX_NONCE_LEN (UINT16) 128

// https://tools.ietf.org/html/rfc5389#section-15.6
#define STUN_MAX_ERROR_PHRASE_LEN (UINT16) 128

// Byte sizes of the IP addresses
#define IPV6_ADDRESS_LENGTH (UINT16) 16
#define IPV4_ADDRESS_LENGTH (UINT16) 4

#define CERTIFICATE_FINGERPRINT_LENGTH 160

#define MAX_UDP_PACKET_SIZE 65507

typedef enum {
    KVS_IP_FAMILY_TYPE_NOT_SET = (UINT16) 0x0000, // Sentinel value for not yet set IP address.
    KVS_IP_FAMILY_TYPE_IPV4 = (UINT16) 0x0001,
    KVS_IP_FAMILY_TYPE_IPV6 = (UINT16) 0x0002,
} KVS_IP_FAMILY_TYPE;

typedef struct {
    UINT16 family;
    UINT16 port;                       // port is stored in network byte order
    BYTE address[IPV6_ADDRESS_LENGTH]; // address is stored in network byte order
    BOOL isPointToPoint;
} KvsIpAddress, *PKvsIpAddress;

// This structure stores both an IPv4 and IPv6 address (if applicable).
typedef struct {
    KvsIpAddress ipv4Address;
    KvsIpAddress ipv6Address;
} DualKvsIpAddresses, *PDualKvsIpAddresses;

static inline BOOL IS_IPV4_ADDR(const PKvsIpAddress pAddress)
{
    return pAddress != NULL && pAddress->family == KVS_IP_FAMILY_TYPE_IPV4;
}

static inline BOOL IS_IPV6_ADDR(const PKvsIpAddress pAddress)
{
    return pAddress != NULL && pAddress->family == KVS_IP_FAMILY_TYPE_IPV6;
}

// Returns TRUE for non-routable IPv4 addresses that a public TURN server (e.g. the KVS
// TURN service) refuses to create a permission for, responding with "403 Forbidden IP".
// Covered ranges (address bytes are in network byte order): 10.0.0.0/8, 172.16.0.0/12 and
// 192.168.0.0/16 (RFC1918 private), 127.0.0.0/8 (loopback), 169.254.0.0/16 (RFC3927
// link-local). IPv6 addresses return FALSE. NOTE: assumes the TURN server cannot relay to
// private/link-local peers, which holds for the KVS TURN service; an on-prem TURN server
// inside a private network may legitimately relay to these and would need this relaxed.
static inline BOOL IS_NON_ROUTABLE_ADDR(const PKvsIpAddress pAddress)
{
    // Only IPv4 ranges are classified here (IS_IPV4_ADDR also handles the NULL check).
    if (!IS_IPV4_ADDR(pAddress)) {
        return FALSE;
    }

    if (pAddress->address[0] == 127) {
        return TRUE; // 127.0.0.0/8 loopback
    }
    if (pAddress->address[0] == 10) {
        return TRUE; // 10.0.0.0/8 private
    }
    if (pAddress->address[0] == 172 && pAddress->address[1] >= 16 && pAddress->address[1] <= 31) {
        return TRUE; // 172.16.0.0/12 private
    }
    if (pAddress->address[0] == 192 && pAddress->address[1] == 168) {
        return TRUE; // 192.168.0.0/16 private
    }
    if (pAddress->address[0] == 169 && pAddress->address[1] == 254) {
        return TRUE; // 169.254.0.0/16 link-local
    }

    return FALSE;
}

// Used for ensuring alignment
#define ALIGN_UP_TO_MACHINE_WORD(x) ROUND_UP((x), SIZEOF(SIZE_T))

typedef STATUS (*IceServerSetIpFunc)(UINT64, PCHAR, PDualKvsIpAddresses);
STATUS getIpAddrStr(PKvsIpAddress pKvsIpAddress, PCHAR pBuffer, UINT32 bufferLen);

////////////////////////////////////////////////////
// Project forward declarations
////////////////////////////////////////////////////
struct __TurnConnection;
struct __SocketConnection;
STATUS generateJSONSafeString(PCHAR, UINT32);

////////////////////////////////////////////////////
// Project internal includes
////////////////////////////////////////////////////
#include "Threadpool/ThreadpoolContext.h"
#include "Crypto/IOBuffer.h"
#include "Crypto/Crypto.h"
#include "Crypto/Dtls.h"
#include "Crypto/Tls.h"
#include "Ice/Network.h"
#include "Ice/SocketConnection.h"
#include "Ice/ConnectionListener.h"
#include "Stun/Stun.h"
#include "Ice/IceUtils.h"
#include "Sdp/Sdp.h"
#include "Ice/IceAgent.h"
#include "Ice/TurnConnection.h"
#include "Ice/IceAgentStateMachine.h"
#include "Ice/TurnConnectionStateMachine.h"
#include "Ice/NatBehaviorDiscovery.h"
#include "Srtp/SrtpSession.h"
#include "Sctp/Sctp.h"
#include "Signaling/FileCache.h"
#include "Signaling/Signaling.h"
#include "Signaling/ChannelInfo.h"
#include "Signaling/StateMachine.h"
#include "Signaling/LwsApiCalls.h"
#include "Rtp/RtpPacket.h"
#include "Rtcp/RtcpPacket.h"
#include "Rtcp/RollingBuffer.h"
#include "Rtcp/RtpRollingBuffer.h"
#include "PeerConnection/JitterBuffer.h"
#include "PeerConnection/PeerConnection.h"
#include "PeerConnection/Retransmitter.h"
#include "PeerConnection/SessionDescription.h"
#include "PeerConnection/Rtp.h"
#include "PeerConnection/Rtcp.h"
#include "PeerConnection/DataChannel.h"
#include "Rtp/Codecs/RtpVP8Payloader.h"
#include "Rtp/Codecs/RtpH264Payloader.h"
#include "Rtp/Codecs/RtpH265Payloader.h"
#include "Rtp/Codecs/RtpOpusPayloader.h"
#include "Rtp/Codecs/RtpG711Payloader.h"
#include "Metrics/Metrics.h"

////////////////////////////////////////////////////
// Project internal defines
////////////////////////////////////////////////////

////////////////////////////////////////////////////
// Project internal functions
////////////////////////////////////////////////////

#define KVS_CONVERT_TIMESCALE(pts, from_timescale, to_timescale) (pts * to_timescale / from_timescale)

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__ */
