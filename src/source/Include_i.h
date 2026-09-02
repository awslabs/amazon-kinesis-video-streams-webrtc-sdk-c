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

/* Randomness source. A build that supplies randomness through a PSA driver
 * (MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG, the default on ESP-IDF v6 targets) compiles the
 * entropy module out, and is permitted to drop CTR-DRBG with it, so
 * mbedtls_entropy_* / mbedtls_ctr_drbg_* do not exist at link time. Such builds take
 * their RNG from PSA instead. This is a build-configuration question rather than a
 * version one: stock mbedTLS 4 still ships both modules. */
#if defined(MBEDTLS_ENTROPY_C) && defined(MBEDTLS_CTR_DRBG_C)
#define MBEDTLS_HAS_ENTROPY (1)
#elif defined(MBEDTLS_PSA_CRYPTO_CLIENT) || defined(MBEDTLS_PSA_CRYPTO_C)
#define MBEDTLS_HAS_ENTROPY (0)
#include <mbedtls/psa_util.h>
#else
#error "mbedTLS must provide either the entropy and CTR-DRBG modules or PSA Crypto for randomness"
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

// Returns TRUE for IP addresses that are not globally reachable, per the IANA Special-Purpose
// Address Registries (RFC 6890 and the RFCs noted per range). A public TURN server (e.g. the
// KVS TURN service) cannot relay to these and rejects CreatePermission for them with
// "403 Forbidden IP". Address bytes are in network byte order. Both IPv4 and IPv6 are covered.
static inline BOOL IS_NON_ROUTABLE_ADDR(const PKvsIpAddress pAddress)
{
    BYTE b0, b1, b2;

    if (pAddress == NULL) {
        return FALSE;
    }

    // IPv6 non-globally-reachable ranges (address is 16 bytes, network byte order).
    if (IS_IPV6_ADDR(pAddress)) {
        PBYTE a = pAddress->address;
        UINT32 i;
        BOOL allZeroButLast = TRUE;

        if (a[0] == 0xfe && (a[1] & 0xc0) == 0x80) {
            return TRUE; // fe80::/10       link-local unicast (RFC 4291)
        }
        if ((a[0] & 0xfe) == 0xfc) {
            return TRUE; // fc00::/7        unique local address / ULA (RFC 4193)
        }
        if (a[0] == 0x20 && a[1] == 0x01 && a[2] == 0x0d && a[3] == 0xb8) {
            return TRUE; // 2001:db8::/32   documentation (RFC 3849)
        }
        for (i = 0; i < 15; ++i) {
            if (a[i] != 0) {
                allZeroButLast = FALSE;
                break;
            }
        }
        if (allZeroButLast && (a[15] == 0 || a[15] == 1)) {
            return TRUE; // ::/128 unspecified and ::1/128 loopback (RFC 4291)
        }

        return FALSE;
    }

    // Only IPv4 ranges remain (IS_IPV4_ADDR also handles the NULL check).
    if (!IS_IPV4_ADDR(pAddress)) {
        return FALSE;
    }

    b0 = pAddress->address[0];
    b1 = pAddress->address[1];
    b2 = pAddress->address[2];

    if (b0 == 0) {
        return TRUE; // 0.0.0.0/8        "this host on this network" (RFC 1122)
    }
    if (b0 == 10) {
        return TRUE; // 10.0.0.0/8       private-use (RFC 1918)
    }
    if (b0 == 100 && b1 >= 64 && b1 <= 127) {
        return TRUE; // 100.64.0.0/10    shared address space / CGNAT (RFC 6598)
    }
    if (b0 == 127) {
        return TRUE; // 127.0.0.0/8      loopback (RFC 1122)
    }
    if (b0 == 169 && b1 == 254) {
        return TRUE; // 169.254.0.0/16   link-local (RFC 3927)
    }
    if (b0 == 172 && b1 >= 16 && b1 <= 31) {
        return TRUE; // 172.16.0.0/12    private-use (RFC 1918)
    }
    if (b0 == 192 && b1 == 0 && b2 == 0) {
        return TRUE; // 192.0.0.0/24     IETF protocol assignments (RFC 6890)
    }
    if (b0 == 192 && b1 == 0 && b2 == 2) {
        return TRUE; // 192.0.2.0/24     documentation TEST-NET-1 (RFC 5737)
    }
    if (b0 == 192 && b1 == 88 && b2 == 99) {
        return TRUE; // 192.88.99.0/24   6to4 relay anycast, deprecated (RFC 7526)
    }
    if (b0 == 192 && b1 == 168) {
        return TRUE; // 192.168.0.0/16   private-use (RFC 1918)
    }
    if (b0 == 198 && (b1 == 18 || b1 == 19)) {
        return TRUE; // 198.18.0.0/15    benchmarking (RFC 2544)
    }
    if (b0 == 198 && b1 == 51 && b2 == 100) {
        return TRUE; // 198.51.100.0/24  documentation TEST-NET-2 (RFC 5737)
    }
    if (b0 == 203 && b1 == 0 && b2 == 113) {
        return TRUE; // 203.0.113.0/24   documentation TEST-NET-3 (RFC 5737)
    }
    if (b0 >= 240) {
        return TRUE; // 240.0.0.0/4      reserved / future use, incl. 255.255.255.255 (RFC 1112)
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
