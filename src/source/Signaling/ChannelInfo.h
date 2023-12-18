/*******************************************
Signaling internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__
#define __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Max control plane URI char len
#define MAX_CONTROL_PLANE_URI_CHAR_LEN 256

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

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CHANNEL_INFO__ */
