/*******************************************
Ice candidate internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_ICE_CANDIDATE_FILE_CACHE__
#define __KINESIS_VIDEO_WEBRTC_ICE_CANDIDATE_FILE_CACHE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ICE_CANDIDATE_CACHE_FILE_PATH                     (PCHAR) "./.IceCandidateCache_v0"
#define MAX_SERIALIZED_ICE_CANDIDATE_CACHE_ENTRY_LEN    5 * 3 + KVS_MAX_IPV4_ADDRESS_STRING_LEN
#define MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT           32

STATUS deserializeIceCandidateCacheEntries(PCHAR, UINT64, PKvsIpAddress, PCHAR);
STATUS iceCandidateCacheLoadFromFile(PKvsIpAddress, PBOOL, PCHAR);
STATUS iceCandidateCacheSaveToFile(PKvsIpAddress, UINT32, PCHAR);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_FILE_CACHE__ */
