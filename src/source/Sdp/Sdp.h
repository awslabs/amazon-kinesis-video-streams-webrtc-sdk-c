//
// Session Description Protocol Utils
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_SDP_SDP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_SDP_SDP__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SDP_ATTRIBUTE_MARKER              "a="
#define SDP_BANDWIDTH_MARKER              "b="
#define SDP_CONNECTION_INFORMATION_MARKER "c="
#define SDP_EMAIL_ADDRESS_MARKER          "e="
#define SDP_ENCRYPTION_KEY_MARKER         "k="

// Media title information or Session information
#define SDP_INFORMATION_MARKER "i="

#define SDP_MEDIA_NAME_MARKER       "m="
#define SDP_ORIGIN_MARKER           "o="
#define SDP_PHONE_NUMBER_MARKER     "p="
#define SDP_SESSION_NAME_MARKER     "s="
#define SDP_TIME_DESCRIPTION_MARKER "t="
#define SDP_TIMEZONE_MARKER         "z="
#define SDP_URI_MARKER              "u="
#define SDP_VERSION_MARKER          "v="

// The sequence CRLF (0x0d0a) is used to end a record, although parsers SHOULD be
// tolerant and also accept records terminated with a single newline
// character.
// Reference: https://tools.ietf.org/html/rfc4566#section-5
#define SDP_LINE_SEPARATOR "\r\n"

#define SDP_CANDIDATE_TYPE_HOST    "host"
#define SDP_CANDIDATE_TYPE_SERFLX  "srflx"
#define SDP_CANDIDATE_TYPE_PRFLX   "prflx"
#define SDP_CANDIDATE_TYPE_RELAY   "relay"
#define SDP_CANDIDATE_TYPE_UNKNOWN "unknown"

#define SDP_ATTRIBUTE_LENGTH 2

#define MAX_SDP_OFFSET_LENGTH                255
#define MAX_SDP_ENCRYPTION_KEY_METHOD_LENGTH 255
#define MAX_SDP_ENCRYPTION_KEY_LENGTH        255

/* https://datatracker.ietf.org/doc/html/rfc4566#section-5.7 -- the SDK hardcodes this to be IN as per spec.
 * Also, this SDK is to be used in the Internet realm. Allowing for some extra buffer
 */
#define MAX_SDP_NETWORK_TYPE_LENGTH 7

/* https://datatracker.ietf.org/doc/html/rfc4566#section-5.7 -- Given the SDK is to operate in IP based sessions,
 * the possible values are IP4/IP6 as registered with IANA. Allowing for some extra buffer
 */
#define MAX_SDP_ADDRESS_TYPE_LENGTH 7

/* https://datatracker.ietf.org/doc/html/rfc4566#section-5.7 -- Given the SDK is to operate in IP based sessions,
 * an IPv4 address can be a maximum of <IPv4(15)/TTL(3)/multicast number of addresses(3)> characters where TTL is
 * between 0 and 255. IPv6 can be a maximum of <IPv6 uncompressed(39)>. Setting to 63 for additional padding
 */
#define MAX_SDP_CONNECTION_ADDRESS_LENGTH 63

// https://datatracker.ietf.org/doc/html/rfc4566#section-5.2 -- the SDK sets it to "-" and the SDK does not parse incoming username either
#define MAX_SDP_SESSION_USERNAME_LENGTH 32

// https://datatracker.ietf.org/doc/html/rfc4566#section-6 -- name length restricted based on current supported attribute set
#define MAX_SDP_ATTRIBUTE_NAME_LENGTH 32

// One of the attributes is streamId + trackId which sums up to 512 maximum characters
#define MAX_SDP_ATTRIBUTE_VALUE_LENGTH 512

#define MAX_SDP_MEDIA_NAME_LENGTH 255

/* https://tools.ietf.org/html/rfc4566#section-5.4. Given these are free-form textual strings, that is, the length could be anything.
 * Although our SDK parses this information, the SDK does not use it. Leaving this attribute in if SDK uses it in
 * the future, but keeping it at smaller size to ensure structure memory efficiency
 */
#define MAX_SDP_MEDIA_TITLE_LENGTH 127

/* https://tools.ietf.org/html/rfc4566#section-5.4. Given these are free-form textual strings, that is, the length could be anything.
 * Although our SDK parses this information, the SDK does not use it. Leaving this attribute in if SDK uses it in
 * the future, but keeping it at smaller size to ensure structure memory efficiency
 */
#define MAX_SDP_SESSION_INFORMATION_LENGTH 127

#define MAX_SDP_SESSION_NAME_LENGTH          255
#define MAX_SDP_SESSION_URI_LENGTH           255
#define MAX_SDP_SESSION_EMAIL_ADDRESS_LENGTH 255
#define MAX_SDP_SESSION_PHONE_NUMBER_LENGTH  255

#define MAX_SDP_TOKEN_LENGTH 128
#define MAX_SDP_FMTP_VALUES  64

#define MAX_SDP_SESSION_TIME_DESCRIPTION_COUNT 2
#define MAX_SDP_SESSION_TIMEZONE_COUNT         2
/**
 * https://tools.ietf.org/html/rfc4566#section-5.14
 *
 * reserving enough for audio, video, text, application and message for now
 */
#define MAX_SDP_SESSION_MEDIA_COUNT   5
#define MAX_SDP_MEDIA_BANDWIDTH_COUNT 2

#define MAX_SDP_ATTRIBUTES_COUNT 256

/*
 * c=<nettype> <addrtype> <connection-address>
 * https://tools.ietf.org/html/rfc4566#section-5.7
 */
typedef struct {
    CHAR networkType[MAX_SDP_NETWORK_TYPE_LENGTH + 1];
    CHAR addressType[MAX_SDP_ADDRESS_TYPE_LENGTH + 1];
    CHAR connectionAddress[MAX_SDP_CONNECTION_ADDRESS_LENGTH + 1];
} SdpConnectionInformation, *PSdpConnectionInformation;

/*
 * o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
 * https://tools.ietf.org/html/rfc4566#section-5.2
 */
typedef struct {
    CHAR userName[MAX_SDP_SESSION_USERNAME_LENGTH + 1];
    UINT64 sessionId;
    UINT64 sessionVersion;
    SdpConnectionInformation sdpConnectionInformation;
} SdpOrigin, *PSdpOrigin;

/*
 * https://tools.ietf.org/html/rfc4566#section-5.9
 * https://tools.ietf.org/html/rfc4566#section-5.10
 */
typedef struct {
    UINT64 startTime;
    UINT64 stopTime;
} SdpTimeDescription, *PSdpTimeDescription;

/*
 * z=<adjustment time> <offset> <adjustment time> <offset> ...
 * https://tools.ietf.org/html/rfc4566#section-5.11
 */
typedef struct {
    UINT64 adjustmentTime;
    CHAR offset[MAX_SDP_OFFSET_LENGTH + 1];
} SdpTimeZone, *PSdpTimeZone;

typedef struct {
    CHAR method[MAX_SDP_ENCRYPTION_KEY_METHOD_LENGTH + 1];
    CHAR sdpEncryptionKey[MAX_SDP_ENCRYPTION_KEY_LENGTH + 1];
} SdpEncryptionKey, *PSdpEncryptionKey;

/*
 * a=<attribute>
 * a=<attribute>:<value>
 * https://tools.ietf.org/html/rfc4566#section-5.13
 */
typedef struct {
    CHAR attributeName[MAX_SDP_ATTRIBUTE_NAME_LENGTH + 1];
    CHAR attributeValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH + 1];
} SdpAttributes, *PSdpAttributes;

typedef struct {
    // m=<media> <port>/<number of ports> <proto> <fmt> ...
    // https://tools.ietf.org/html/rfc4566#section-5.14
    CHAR mediaName[MAX_SDP_MEDIA_NAME_LENGTH + 1];

    // i=<session description>
    // https://tools.ietf.org/html/rfc4566#section-5.4. Given these are free-form strings, the length could be anything.
    // Although our SDK parses this information, the SDK does not use it. Leaving this attribute in if SDK uses it in
    // the future
    CHAR mediaTitle[MAX_SDP_MEDIA_TITLE_LENGTH + 1];

    SdpConnectionInformation sdpConnectionInformation;

    SdpEncryptionKey sdpEncryptionKey;

    SdpAttributes sdpAttributes[MAX_SDP_ATTRIBUTES_COUNT];

    UINT8 mediaAttributesCount;

    UINT8 mediaBandwidthCount;
} SdpMediaDescription, *PSdpMediaDescription;

typedef struct {
    // https://tools.ietf.org/html/rfc4566#section-5.1
    UINT64 version;

    SdpOrigin sdpOrigin;

    // s=<session name>
    // https://tools.ietf.org/html/rfc4566#section-5.3
    CHAR sessionName[MAX_SDP_SESSION_NAME_LENGTH + 1];

    // i=<session description>
    // https://tools.ietf.org/html/rfc4566#section-5.4
    CHAR sessionInformation[MAX_SDP_SESSION_INFORMATION_LENGTH + 1];

    // u=<uri>
    // https://tools.ietf.org/html/rfc4566#section-5.5
    CHAR uri[MAX_SDP_SESSION_URI_LENGTH + 1];

    // e=<email-address>
    // https://tools.ietf.org/html/rfc4566#section-5.6
    CHAR emailAddress[MAX_SDP_SESSION_EMAIL_ADDRESS_LENGTH + 1];

    // p=<phone-number>
    // https://tools.ietf.org/html/rfc4566#section-5.6
    CHAR phoneNumber[MAX_SDP_SESSION_PHONE_NUMBER_LENGTH + 1];

    SdpConnectionInformation sdpConnectionInformation;

    SdpTimeDescription sdpTimeDescription[MAX_SDP_SESSION_TIME_DESCRIPTION_COUNT];

    SdpTimeZone sdpTimeZone[MAX_SDP_SESSION_TIMEZONE_COUNT];

    SdpEncryptionKey sdpEncryptionKey;

    SdpAttributes sdpAttributes[MAX_SDP_ATTRIBUTES_COUNT];

    SdpMediaDescription mediaDescriptions[MAX_SDP_SESSION_MEDIA_COUNT];

    UINT16 sessionAttributesCount;

    UINT16 mediaCount;

    UINT8 timezoneCount;

    UINT8 timeDescriptionCount;

    UINT8 bandwidthCount;
} SessionDescription, *PSessionDescription;

// Return code maps to an errno just for SDP parsing
STATUS deserializeSessionDescription(PSessionDescription, PCHAR);

// Return code maps to a code if we are trying to serialize an invalid session_description
STATUS serializeSessionDescription(PSessionDescription, PCHAR, PUINT32);

STATUS parseMediaName(PSessionDescription, PCHAR, UINT32);
STATUS parseSessionAttributes(PSessionDescription, PCHAR, UINT32);
STATUS parseMediaAttributes(PSessionDescription, PCHAR, UINT32);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_SDP_SDP__
