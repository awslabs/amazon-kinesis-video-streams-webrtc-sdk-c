#define LOG_CLASS "RequestInfo"
#include "common_defs.h"
#include "request_info.h"

/**
 * @brief Service call result
 *  https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
 */
typedef enum {
    // Not defined
    HTTP_STATUS_NONE = 0,
    // Information responses
    //
    HTTP_STATUS_CONTINUE = 100,

    HTTP_STATUS_SWITCH_PROTOCOL = 101,

    // Successful responses
    // All OK
    HTTP_STATUS_OK = 200,

    // Client error responses
    // Bad request
    HTTP_STATUS_BAD_REQUEST = 400,
    // Security error
    HTTP_STATUS_UNAUTHORIZED = 401,
    // Forbidden
    HTTP_STATUS_FORBIDDEN = 403,
    // Resource not found exception
    HTTP_STATUS_NOT_FOUND = 404,
    // Invalid params error
    HTTP_STATUS_NOT_ACCEPTABLE = 406,
    // Request timeout
    HTTP_STATUS_REQUEST_TIMEOUT = 408,
    // Internal server error
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,

    // Server error responses
    // Not implemented
    HTTP_STATUS_NOT_IMPLEMENTED = 501,

    // Service unavailable
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503,

    // Gateway timeout
    HTTP_STATUS_GATEWAY_TIMEOUT = 504,

    // Network read timeout
    HTTP_STATUS_NETWORK_READ_TIMEOUT = 598,

    // Network connection timeout
    HTTP_STATUS_NETWORK_CONNECTION_TIMEOUT = 599,

    // Go Away result
    HTTP_STATUS_SIGNALING_GO_AWAY = 6000,

    // Reconnect ICE Server
    HTTP_STATUS_SIGNALING_RECONNECT_ICE = 6001,

    // Client limit exceeded error
    HTTP_STATUS_CLIENT_LIMIT = 10000,

    // Device limit exceeded error
    HTTP_STATUS_DEVICE_LIMIT = 10001,

    // Stream limit exception
    HTTP_STATUS_STREAM_LIMIT = 10002,

    // Resource in use exception
    HTTP_STATUS_RESOURCE_IN_USE = 10003,

    // Device not provisioned
    HTTP_STATUS_DEVICE_NOT_PROVISIONED = 10004,

    // Device not found
    HTTP_STATUS_DEVICE_NOT_FOUND = 10005,
    // Other errors
    HTTP_STATUS_UNKNOWN = 10006,
    // Resource deleted exception
    HTTP_STATUS_RESOURCE_DELETED = 10400,

} HTTP_STATUS_CODE;

PUBLIC_API STATUS createRequestInfo
        (PCHAR url, PCHAR body, PCHAR region, PCHAR certPath, PCHAR sslCertPath, PCHAR sslPrivateKeyPath,
        SSL_CERTIFICATE_TYPE certType, PCHAR userAgent, UINT64 connectionTimeout, UINT64 completionTimeout, UINT64 lowSpeedLimit,
        UINT64 lowSpeedTimeLimit, PAwsCredentials pAwsCredentials, PRequestInfo* ppRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRequestInfo pRequestInfo = NULL;
    UINT32 size = SIZEOF(RequestInfo), bodySize = 0;

    CHK(region != NULL && url != NULL && ppRequestInfo != NULL, STATUS_NULL_ARG);

    // Add body to the size excluding NULL terminator
    if (body != NULL) {
        bodySize = (UINT32)(STRLEN(body) * SIZEOF(CHAR));
        size += bodySize;
    }

    // Allocate the entire structure
    pRequestInfo = (PRequestInfo) MEMCALLOC(1, size);
    CHK(pRequestInfo != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pRequestInfo->pAwsCredentials = pAwsCredentials;
    pRequestInfo->verb = HTTP_REQUEST_VERB_POST;
    pRequestInfo->completionTimeout = completionTimeout;
    pRequestInfo->connectionTimeout = connectionTimeout;
    ATOMIC_STORE_BOOL(&pRequestInfo->terminating, FALSE);
    pRequestInfo->bodySize = bodySize;
    pRequestInfo->currentTime = GETTIME();
    pRequestInfo->callAfter = pRequestInfo->currentTime;
    STRNCPY(pRequestInfo->region, region, MAX_REGION_NAME_LEN);
#if USE_DYNAMIC_URL
    UINT32 urlLen = strlen(url);
    pRequestInfo->url = MEMALLOC(urlLen + 1);
    if (pRequestInfo->url) {
        // if allocation was not successful, user function of url will abort
        pRequestInfo->url[urlLen] = '\0';
        MEMCPY(pRequestInfo->url, url, urlLen);
    }
#else
    STRNCPY(pRequestInfo->url, url, MAX_URI_CHAR_LEN);
#endif
    if (certPath != NULL) {
        STRNCPY(pRequestInfo->certPath, certPath, MAX_PATH_LEN);
    }

    if (sslCertPath != NULL) {
        STRNCPY(pRequestInfo->sslCertPath, sslCertPath, MAX_PATH_LEN);
    }

    if (sslPrivateKeyPath != NULL) {
        STRNCPY(pRequestInfo->sslPrivateKeyPath, sslPrivateKeyPath, MAX_PATH_LEN);
    }

    pRequestInfo->certType = certType;
    pRequestInfo->lowSpeedLimit = lowSpeedLimit;
    pRequestInfo->lowSpeedTimeLimit = lowSpeedTimeLimit;

    // If the body is specified then it will be a request/response call
    // Otherwise we are streaming
    if (body != NULL) {
        pRequestInfo->body = (PCHAR)(pRequestInfo + 1);
        MEMCPY(pRequestInfo->body, body, bodySize);
    }

    // Create a list of headers
    CHK_STATUS(singleListCreate(&pRequestInfo->pRequestHeaders));

    // Set user agent header
    CHK_STATUS(setRequestHeader(pRequestInfo, (PCHAR) "user-agent", 0, userAgent, 0));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeRequestInfo(&pRequestInfo);
        pRequestInfo = NULL;
    }

    // Set the return value if it's not NULL
    if (ppRequestInfo != NULL) {
        *ppRequestInfo = pRequestInfo;
    }

    LEAVES();
    return retStatus;
}

PUBLIC_API STATUS freeRequestInfo(PRequestInfo* ppRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRequestInfo pRequestInfo = NULL;

    CHK(ppRequestInfo != NULL, STATUS_NULL_ARG);

    pRequestInfo = (PRequestInfo) *ppRequestInfo;

    // Call is idempotent
    CHK(pRequestInfo != NULL, retStatus);

    // Remove and free the headers
    removeRequestHeaders(pRequestInfo);

    // Free the header list itself
    singleListFree(pRequestInfo->pRequestHeaders);

#if USE_DYNAMIC_URL
    // Release the url
    SAFE_MEMFREE(pRequestInfo->url);
#endif

    // Release the object
    MEMFREE(pRequestInfo);

    // Set the pointer to NULL
    *ppRequestInfo = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS requestRequiresSecureConnection(PCHAR pUrl, PBOOL pSecure)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pUrl != NULL && pSecure != NULL, STATUS_NULL_ARG);
    *pSecure =
        (0 == STRNCMPI(pUrl, HTTPS_SCHEME_NAME, SIZEOF(HTTPS_SCHEME_NAME) - 1) || 0 == STRNCMPI(pUrl, WSS_SCHEME_NAME, SIZEOF(WSS_SCHEME_NAME) - 1));

CleanUp:

    return retStatus;
}

PUBLIC_API STATUS createRequestHeader(PCHAR headerName, UINT32 headerNameLen, PCHAR headerValue, UINT32 headerValueLen, PRequestHeader* ppHeader)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 nameLen, valueLen, size;
    PRequestHeader pRequestHeader = NULL;

    CHK(ppHeader != NULL && headerName != NULL && headerValue != NULL, STATUS_NULL_ARG);

    // Calculate the length if needed
    if (headerNameLen == 0) {
        nameLen = (UINT32) STRLEN(headerName);
    } else {
        nameLen = headerNameLen;
    }

    if (headerValueLen == 0) {
        valueLen = (UINT32) STRLEN(headerValue);
    } else {
        valueLen = headerValueLen;
    }

    CHK(nameLen > 0 && valueLen > 0, STATUS_INVALID_ARG);
    CHK(nameLen < MAX_REQUEST_HEADER_NAME_LEN, STATUS_MAX_REQUEST_HEADER_NAME_LEN);
    CHK(valueLen < MAX_REQUEST_HEADER_VALUE_LEN, STATUS_MAX_REQUEST_HEADER_VALUE_LEN);

    size = SIZEOF(RequestHeader) + (nameLen + 1 + valueLen + 1) * SIZEOF(CHAR);

    // Create the request header
    pRequestHeader = (PRequestHeader) MEMALLOC(size);
    CHK(pRequestHeader != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pRequestHeader->nameLen = nameLen;
    pRequestHeader->valueLen = valueLen;

    // Pointing after the structure
    pRequestHeader->pName = (PCHAR)(pRequestHeader + 1);
    pRequestHeader->pValue = pRequestHeader->pName + nameLen + 1;

    MEMCPY(pRequestHeader->pName, headerName, nameLen * SIZEOF(CHAR));
    pRequestHeader->pName[nameLen] = '\0';
    MEMCPY(pRequestHeader->pValue, headerValue, valueLen * SIZEOF(CHAR));
    pRequestHeader->pValue[valueLen] = '\0';

CleanUp:

    if (STATUS_FAILED(retStatus) && pRequestHeader != NULL) {
        MEMFREE(pRequestHeader);
        pRequestHeader = NULL;
    }

    if (ppHeader != NULL) {
        *ppHeader = pRequestHeader;
    }

    return retStatus;
}

PUBLIC_API STATUS setRequestHeader(PRequestInfo pRequestInfo, PCHAR headerName, UINT32 headerNameLen, PCHAR headerValue, UINT32 headerValueLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 count;
    PSingleListNode pCurNode, pPrevNode = NULL;
    PRequestHeader pRequestHeader = NULL, pCurrentHeader;
    UINT64 item;

    CHK(pRequestInfo != NULL && headerName != NULL && headerValue != NULL, STATUS_NULL_ARG);
    CHK_STATUS(singleListGetNodeCount(pRequestInfo->pRequestHeaders, &count));
    CHK(count < MAX_REQUEST_HEADER_COUNT, STATUS_MAX_REQUEST_HEADER_COUNT);

    CHK_STATUS(createRequestHeader(headerName, headerNameLen, headerValue, headerValueLen, &pRequestHeader));

    // Iterate through the list and insert in an alpha order
    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pCurrentHeader = (PRequestHeader) HANDLE_TO_POINTER(item);

        if (STRCMPI(pCurrentHeader->pName, pRequestHeader->pName) > 0) {
            if (pPrevNode == NULL) {
                // Insert at the head
                CHK_STATUS(singleListInsertItemHead(pRequestInfo->pRequestHeaders, POINTER_TO_HANDLE(pRequestHeader)));
            } else {
                CHK_STATUS(singleListInsertItemAfter(pRequestInfo->pRequestHeaders, pPrevNode, POINTER_TO_HANDLE(pRequestHeader)));
            }

            // Early return
            CHK(FALSE, retStatus);
        }

        pPrevNode = pCurNode;

        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

    // If not inserted then add to the tail
    CHK_STATUS(singleListInsertItemTail(pRequestInfo->pRequestHeaders, POINTER_TO_HANDLE(pRequestHeader)));

CleanUp:

    if (STATUS_FAILED(retStatus) && pRequestHeader != NULL) {
        MEMFREE(pRequestHeader);
    }

    return retStatus;
}

PUBLIC_API STATUS removeRequestHeader(PRequestInfo pRequestInfo, PCHAR headerName)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pCurNode;
    PRequestHeader pCurrentHeader = NULL;
    UINT64 item;

    CHK(pRequestInfo != NULL && headerName != NULL, STATUS_NULL_ARG);

    // Iterate through the list and insert in an alpha order
    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pCurrentHeader = (PRequestHeader) HANDLE_TO_POINTER(item);

        if (STRCMPI(pCurrentHeader->pName, headerName) == 0) {
            CHK_STATUS(singleListDeleteNode(pRequestInfo->pRequestHeaders, pCurNode));

            // Early return
            CHK(FALSE, retStatus);
        }

        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

CleanUp:

    SAFE_MEMFREE(pCurrentHeader);

    return retStatus;
}

PUBLIC_API STATUS removeRequestHeaders(PRequestInfo pRequestInfo)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pNode;
    UINT32 itemCount;
    PRequestHeader pRequestHeader;

    CHK(pRequestInfo != NULL, STATUS_NULL_ARG);

    singleListGetNodeCount(pRequestInfo->pRequestHeaders, &itemCount);
    while (itemCount-- != 0) {
        // Remove and delete the data
        singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pNode);
        pRequestHeader = (PRequestHeader) HANDLE_TO_POINTER(pNode->data);
        SAFE_MEMFREE(pRequestHeader);

        // Iterate
        singleListDeleteHead(pRequestInfo->pRequestHeaders);
    }

CleanUp:

    return retStatus;
}

SERVICE_CALL_RESULT getServiceCallResultFromHttpStatus(UINT32 httpStatus)
{
    switch (httpStatus) {
        case HTTP_STATUS_OK:
        case HTTP_STATUS_NOT_ACCEPTABLE:
        case HTTP_STATUS_NOT_FOUND:
        case HTTP_STATUS_FORBIDDEN:
        case HTTP_STATUS_RESOURCE_DELETED:
        case HTTP_STATUS_UNAUTHORIZED:
        case HTTP_STATUS_NOT_IMPLEMENTED:
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
        case HTTP_STATUS_REQUEST_TIMEOUT:
        case HTTP_STATUS_GATEWAY_TIMEOUT:
        case HTTP_STATUS_NETWORK_READ_TIMEOUT:
        case HTTP_STATUS_NETWORK_CONNECTION_TIMEOUT:
            return (SERVICE_CALL_RESULT) httpStatus;
        default:
            return HTTP_STATUS_UNKNOWN;
    }
}
