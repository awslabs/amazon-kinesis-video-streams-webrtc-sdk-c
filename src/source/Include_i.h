/*******************************************
Main internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__

#pragma once

#ifdef  __cplusplus
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

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <srtp2/srtp.h>

// INET/INET6 MUST be defined before usrsctp
// If removed will cause corruption that is hard to determine at runtime
#define INET 1
#define INET6 1
#include <usrsctp.h>

#include <libwebsockets.h>

#if !defined __WINDOWS_BUILD__
#include <signal.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#endif

// Max uFrag and uPwd length as documented in https://tools.ietf.org/html/rfc5245#section-15.4
#define ICE_MAX_UFRAG_LEN               256
#define ICE_MAX_UPWD_LEN                256

// Max stun username attribute len: https://tools.ietf.org/html/rfc5389#section-15.3
#define STUN_MAX_USERNAME_LEN           (UINT16) 512

// https://tools.ietf.org/html/rfc5389#section-15.7
#define STUN_MAX_REALM_LEN              (UINT16) 128

// https://tools.ietf.org/html/rfc5389#section-15.8
#define STUN_MAX_NONCE_LEN              (UINT16) 128

// https://tools.ietf.org/html/rfc5389#section-15.6
#define STUN_MAX_ERROR_PHRASE_LEN       (UINT16) 128

// Byte sizes of the IP addresses
#define IPV6_ADDRESS_LENGTH             (UINT16) 16
#define IPV4_ADDRESS_LENGTH             (UINT16) 4

#define CERTIFICATE_FINGERPRINT_LENGTH 160

typedef enum {
    KVS_IP_FAMILY_TYPE_IPV4             = (UINT16) 0x0001,
    KVS_IP_FAMILY_TYPE_IPV6             = (UINT16) 0x0002,
} KVS_IP_FAMILY_TYPE;

typedef struct {
    UINT16 family;
    UINT16 port;                        // port is stored in network byte order
    BYTE address[IPV6_ADDRESS_LENGTH];  // address is stored in network byte order
    BOOL isPointToPoint;
} KvsIpAddress, *PKvsIpAddress;

#define IS_IPV4_ADDR(pAddress) ((pAddress)->family == KVS_IP_FAMILY_TYPE_IPV4)

// Used for ensuring alignment
#define ALIGN_UP_TO_MACHINE_WORD(x)             ROUND_UP((x), SIZEOF(SIZE_T))

////////////////////////////////////////////////////
// Project forward declarations
////////////////////////////////////////////////////
struct __TurnConnection;
struct __SocketConnection;
STATUS generateJSONSafeString(PCHAR, UINT32);

////////////////////////////////////////////////////
// Project internal includes
////////////////////////////////////////////////////
#include "Ice/Network.h"
#include "Ice/Tls.h"
#include "Ice/SocketConnection.h"
#include "Ice/ConnectionListener.h"
#include "Stun/Stun.h"
#include "Ice/IceUtils.h"
#include "Sdp/Sdp.h"
#include "Dtls/Dtls.h"
#include "Ice/IceAgent.h"
#include "Ice/TurnConnection.h"
#include "Ice/IceAgentStateMachine.h"
#include "Ice/NatBehaviorDiscovery.h"
#include "Srtp/SrtpSession.h"
#include "Sctp/Sctp.h"
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
#include "Rtp/Codecs/RtpOpusPayloader.h"
#include "Rtp/Codecs/RtpG711Payloader.h"
#include "Signaling/FileCache.h"
#include "Signaling/Signaling.h"
#include "Signaling/ChannelInfo.h"
#include "Signaling/StateMachine.h"
#include "Signaling/LwsApiCalls.h"

////////////////////////////////////////////////////
// Project internal defines
////////////////////////////////////////////////////

////////////////////////////////////////////////////
// Project internal functions
////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE_I__ */

