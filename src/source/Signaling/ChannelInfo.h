/*******************************************
Signaling internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__
#define __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Max channel status string length in describe API call in chars
#define MAX_DESCRIBE_CHANNEL_STATUS_LEN 32

// Max channel type string length in describe API call in chars
#define MAX_DESCRIBE_CHANNEL_TYPE_LEN 128

// Signaling channel type string
#define SIGNALING_CHANNEL_TYPE_UNKNOWN_STR       (PCHAR) "UNKOWN"
#define SIGNALING_CHANNEL_TYPE_SINGLE_MASTER_STR (PCHAR) "SINGLE_MASTER"

// Signaling channel role type string
#define SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR (PCHAR) "UNKOWN"
#define SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR  (PCHAR) "MASTER"
#define SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR  (PCHAR) "VIEWER"

// Min and max for the message TTL value
#define MIN_SIGNALING_MESSAGE_TTL_VALUE (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define MAX_SIGNALING_MESSAGE_TTL_VALUE (120 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define SIGNALING_USER_AGENT_POSTFIX_NAME (PCHAR) "AWS-WEBRTC-KVS-AGENT"

#ifdef VERSION_STRING
#define SIGNALING_USER_AGENT_POSTFIX_VERSION (PCHAR) VERSION_STRING
#else
#define SIGNALING_USER_AGENT_POSTFIX_VERSION (PCHAR) "UNKNOWN"
#endif

// Structure to hold FIPS endpoint mapping
typedef struct {
    PCHAR pRegion;
    PCHAR pEndpoint;
} FipsEndpointMapping;

// Number of FIPS endpoint mappings
#define FIPS_ENDPOINT_MAPPING_COUNT 5

// FIPS endpoint mappings - region to endpoint URL (legacy/non-dual-stack)
// These endpoints are used when only USE_FIPS_ENDPOINT_ENV_VAR is enabled
static const FipsEndpointMapping FIPS_ENDPOINT_MAPPINGS[FIPS_ENDPOINT_MAPPING_COUNT] = {
    {"us-iso-east-1", "https://kinesisvideo-fips.us-iso-east-1.c2s.ic.gov"},
    {"us-iso-west-1", "https://kinesisvideo-fips.us-iso-west-1.c2s.ic.gov"},
    {"us-isob-east-1", "https://kinesisvideo-fips.us-isob-east-1.sc2s.sgov.gov"},
    {"us-gov-west-1", "https://kinesisvideo-fips.us-gov-west-1.amazonaws.com"},
    {"us-gov-east-1", "https://kinesisvideo-fips.us-gov-east-1.amazonaws.com"},
};

// FIPS dual-stack endpoint mappings - region to endpoint URL
// These endpoints are used when BOTH USE_FIPS_ENDPOINT_ENV_VAR and USE_DUAL_STACK_ENDPOINTS_ENV_VAR are enabled
static const FipsEndpointMapping FIPS_DUAL_STACK_ENDPOINT_MAPPINGS[FIPS_ENDPOINT_MAPPING_COUNT] = {
    {"us-iso-east-1", "https://kinesisvideo-fips.us-iso-east-1.api.aws.ic.gov"},
    {"us-iso-west-1", "https://kinesisvideo-fips.us-iso-west-1.api.aws.ic.gov"},
    {"us-isob-east-1", "https://kinesisvideo-fips.us-isob-east-1.api.aws.scloud"},
    {"us-gov-west-1", "https://kinesisvideo-fips.us-gov-west-1.api.aws"},
    {"us-gov-east-1", "https://kinesisvideo-fips.us-gov-east-1.api.aws"},
};

/**
 * Constructs the control plane endpoint URL based on region and environment settings.
 * 
 * Priority:
 * 1. FIPS + dual-stack enabled -> FIPS dual-stack endpoint
 * 2. FIPS only enabled -> FIPS legacy endpoint
 * 3. Dual-stack only enabled -> standard dual-stack endpoint
 * 4. Neither enabled -> standard legacy endpoint
 *
 * @param - PCHAR - IN - The AWS region string
 * @param - PCHAR - OUT - Buffer to store the constructed endpoint URL
 * @param - UINT32 - IN - Size of the endpoint buffer
 *
 * @return - STATUS_SUCCESS on success, error status otherwise
 */
STATUS constructControlPlaneEndpoint(PCHAR, PCHAR, UINT32);

/**
 * Takes in a pointer to a public version of ChannelInfo object.
 * Validates and creates an internal object
 *
 * @return - STATUS code of the execution
 */
STATUS createValidateChannelInfo(PChannelInfo, PChannelInfo*);

/**
 * Frees the channel info object.
 *
 * @param - PChannelInfo* - IN - Channel info object to free
 *
 * @return - STATUS code of the execution
 */
STATUS freeChannelInfo(PChannelInfo*);

/**
 * Returns the signaling channel status from a string
 *
 * @param - PCHAR - IN - String representation of the channel status
 * @param - UINT32 - IN - String length
 *
 * @return - Signaling channel status type
 */
SIGNALING_CHANNEL_STATUS getChannelStatusFromString(PCHAR, UINT32);

/**
 * Returns the signaling channel type from a string
 *
 * @param - PCHAR - IN - String representation of the channel type
 * @param - UINT32 - IN - String length
 *
 * @return - Signaling channel type
 */
SIGNALING_CHANNEL_TYPE getChannelTypeFromString(PCHAR, UINT32);

/**
 * Returns the signaling channel type string
 *
 * @param - SIGNALING_CHANNEL_TYPE - IN - Signaling channel type
 *
 * @return - Signaling channel type string
 */
PCHAR getStringFromChannelType(SIGNALING_CHANNEL_TYPE);

/**
 * Returns the signaling channel Role from a string
 *
 * @param - PCHAR - IN - String representation of the channel role
 * @param - UINT32 - IN - String length
 *
 * @return - Signaling channel type
 */
SIGNALING_CHANNEL_ROLE_TYPE getChannelRoleTypeFromString(PCHAR, UINT32);

/**
 * Returns the signaling channel role type string
 *
 * @param - SIGNALING_CHANNEL_TYPE - IN - Signaling channel type
 *
 * @return - Signaling channel type string
 */
PCHAR getStringFromChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE);

/**
 * Determines whether or not the channel arn is valid
 * If VALID it extracts the channel name
 * And Sets the pChannelName field in PChannelInfo
 *
 * @param - PChannelInfo - IN - channel info object
 * @param - PUINT16 - OUT - start index of the arn (if valid) where the channel name is
 * @param - PUINT16 - OUT - number of characters for the arn (if valid)
 *
 *@return - success if arn was valid otherwise failure
 */
STATUS validateKvsSignalingChannelArnAndExtractChannelName(PChannelInfo, PUINT16, PUINT16);

/**
 * Constructs the STUN server URL based on region and environment settings.
 * Handles standard, dual-stack, China, and FIPS endpoints.
 *
 * Note: FIPS STUN requires "stuns:" scheme (TLS), not "stun:"
 *
 * @param - PCHAR - IN - The AWS region string
 * @param - PCHAR - OUT - Buffer to store the constructed STUN URL
 * @param - UINT32 - IN - Size of the buffer
 *
 * @return - STATUS_SUCCESS on success, error status otherwise
 */
STATUS getStunUrl(PCHAR, PCHAR, UINT32);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__ */
