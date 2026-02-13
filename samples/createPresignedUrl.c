#include "Samples.h"

#define CUSTOM_USER_AGENT_STRING ((PCHAR) "AWS-WEBRTC-KVS-AGENT-PRESIGNED-C/1.16.0")

// Endpoint example: "wss://m-1234abcd.kinesisvideo.aws-region.amazonaws.com"
STATUS createPresignedUrl(PCHAR signalingChannelEndpoint, PCHAR channelArn, PCHAR region,
                          PCHAR accessKeyId, PCHAR secretKey, PCHAR sessionToken, UINT64 expiry,
                          PCHAR presignedURL) {
    STATUS retStatus = STATUS_SUCCESS;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    PRequestInfo pRequestInfo = NULL;
    PAwsCredentials pAwsCredentials = NULL;
    UINT16 urlLen;

    if (sessionToken == NULL) {
        CHK_STATUS(createAwsCredentials(accessKeyId, STRLEN(accessKeyId), secretKey, STRLEN(secretKey), NULL, 0, expiry,
                                        &pAwsCredentials));
    } else {
        CHK_STATUS(createAwsCredentials(accessKeyId, 0, secretKey, 0, sessionToken, 0, expiry,
                                        &pAwsCredentials));
    }

    SNPRINTF(url, MAX_URI_CHAR_LEN + 1, "%s/?%s=%s", signalingChannelEndpoint, "X-Amz-ChannelARN", channelArn);
    CHK_STATUS(createRequestInfo(url, NULL, region, NULL, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED,
                                 CUSTOM_USER_AGENT_STRING, 2 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pAwsCredentials, &pRequestInfo));
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    CHK_STATUS(removeRequestHeader(pRequestInfo, (PCHAR) "user-agent"));

    // Sign the request
    CHK_STATUS(signAwsRequestInfoQueryParam(pRequestInfo));
    // Remove the headers
    CHK_STATUS(removeRequestHeaders(pRequestInfo));

    urlLen = STRLEN(pRequestInfo->url);
    STRNCPY(presignedURL, pRequestInfo->url, urlLen);
    presignedURL[urlLen] = '\0';

CleanUp:
    CHK_LOG_ERR(retStatus);

    return retStatus;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR presignedURL[MAX_URI_CHAR_LEN + 1];
    PCHAR region = NULL;
    PCHAR signalingChannelEndpoint = NULL;
    PCHAR channelArn = NULL;
    PCHAR accessKeyId = NULL;
    PCHAR secretKey = NULL;
    PCHAR sessionToken = NULL;

    (void) setLogLevel();

    if (argc <= 2) {
        DLOGE("Usage: %s <channelArn> <wssEndpoint>", argv[0]);
        return 1;
    }

    channelArn = argv[1];
    signalingChannelEndpoint = argv[2];
    CHK_ERR(!IS_NULL_OR_EMPTY_STRING(channelArn), STATUS_NULL_ARG, "channelArn is required");
    CHK_ERR(!IS_NULL_OR_EMPTY_STRING(signalingChannelEndpoint), STATUS_NULL_ARG, "signalingChannelWssEndpoint is required");

    DLOGI("ChannelArn: %s", channelArn);
    DLOGI("WssEndpoint: %s", signalingChannelEndpoint);

    // Get parameters from environment variables
    CHK_ERR((region = GETENV("AWS_DEFAULT_REGION")) != NULL, STATUS_INVALID_ARG, "AWS_DEFAULT_REGION must be set");
    CHK_ERR((accessKeyId = GETENV("AWS_ACCESS_KEY_ID")) != NULL, STATUS_INVALID_ARG, "AWS_ACCESS_KEY_ID must be set");
    CHK_ERR((secretKey = GETENV("AWS_SECRET_ACCESS_KEY")) != NULL, STATUS_INVALID_ARG, "AWS_SECRET_ACCESS_KEY must be set");
    sessionToken = GETENV("AWS_SESSION_TOKEN");

    // Generate pre-signed URL
    CHK_STATUS(createPresignedUrl(signalingChannelEndpoint, channelArn, region, accessKeyId, secretKey, sessionToken,
                                  5 * HUNDREDS_OF_NANOS_IN_A_MINUTE, presignedURL));

    // Avoiding the logging prefix for easier copy/paste
    printf("%s\n", presignedURL);

CleanUp:
    CHK_LOG_ERR(retStatus);

    return (INT32) retStatus;
}
