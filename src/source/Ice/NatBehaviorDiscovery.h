#ifndef __KINESIS_VIDEO_WEBRTC_ICE_NAT_BEHAVIOR_DISCOVERY__
#define __KINESIS_VIDEO_WEBRTC_ICE_NAT_BEHAVIOR_DISCOVERY__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT 5
#define DEFAULT_TEST_NAT_TEST_RESPONSE_WAIT_TIME   500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define NAT_BEHAVIOR_DISCOVER_PROCESS_TEST_COUNT   3

#define NAT_BEHAVIOR_NONE_STR                 (PCHAR) "NONE"
#define NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT_STR   (PCHAR) "NOT_BEHIND_ANY_NAT"
#define NAT_BEHAVIOR_NO_UDP_CONNECTIVITY_STR  (PCHAR) "NO_UDP_CONNECTIVITY"
#define NAT_BEHAVIOR_ENDPOINT_INDEPENDENT_STR (PCHAR) "ENDPOINT_INDEPENDENT"
#define NAT_BEHAVIOR_ADDRESS_DEPENDENT_STR    (PCHAR) "ADDRESS_DEPENDENT"
#define NAT_BEHAVIOR_PORT_DEPENDENT_STR       (PCHAR) "PORT_DEPENDENT"

typedef struct {
    /* Should be able to contain max number of binding response we can get */
    PStunPacket bindingResponseList[DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT * (NAT_BEHAVIOR_DISCOVER_PROCESS_TEST_COUNT * 2)];
    UINT32 bindingResponseCount;
    CVAR cvar;
    MUTEX lock;
} NatTestData, *PNatTestData;

STATUS natTestIncomingDataHandler(UINT64, PSocketConnection, PBYTE, UINT32, PKvsIpAddress, PKvsIpAddress);

STATUS executeNatTest(PStunPacket, PKvsIpAddress, PSocketConnection, UINT32, PNatTestData, PStunPacket*);

STATUS getMappAddressAttribute(PStunPacket, PStunAttributeAddress*);

STATUS discoverNatMappingBehavior(PIceServer, PNatTestData, PSocketConnection, NAT_BEHAVIOR*);

STATUS discoverNatFilteringBehavior(PIceServer, PNatTestData, PSocketConnection, NAT_BEHAVIOR*);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_ICE_NAT_BEHAVIOR_DISCOVERY__ */
