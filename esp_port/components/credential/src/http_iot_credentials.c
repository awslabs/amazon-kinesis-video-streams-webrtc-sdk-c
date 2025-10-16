/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/******************************************************************************
 * HEADERS
 ******************************************************************************/
#define LOG_CLASS "HttpApi"
#include "http_api.h"
#include "auth.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include <inttypes.h>
#include "fileio.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define HTTP_API_ENTER() DLOGD("%s(%d) enter", __func__, __LINE__)
#define HTTP_API_EXIT()  DLOGD("%s(%d) exit", __func__, __LINE__)

#define HTTP_API_COMPLETION_TIMEOUT 15000    // 15 seconds
#define HTTP_API_RECV_BUFFER_MAX_SIZE 8192   // 8KB receive buffer
#define HTTP_CLIENT_RECV_BUF_SIZE 2048   // 2KB recv buffer
#define HTTP_CLIENT_SEND_BUF_SIZE 2048   // 2KB send buffer

// Max control plane URI char len
#define MAX_CONTROL_PLANE_URI_CHAR_LEN 256

#define HTTP_API_ROLE_ALIASES                   "/role-aliases"
#define HTTP_API_CREDENTIALS                    "/credentials"
#define HTTP_API_IOT_THING_NAME_HEADER          "x-amzn-iot-thingname"

// Custom response context structure for collecting response data
typedef struct {
    PCHAR responseData;
    UINT32 currentSize;
    UINT32 maxSize;
} EspHttpRespContext;

// HTTP response callback handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    EspHttpRespContext* context = (EspHttpRespContext*)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (context != NULL && context->responseData != NULL) {
                UINT32 remaining = (context->maxSize > context->currentSize) ? (context->maxSize - context->currentSize) : 0;
                UINT32 copy_len = 0;
                if (remaining > 1) {
                    copy_len = MIN(remaining - 1, (UINT32) evt->data_len);
                }

                if (copy_len > 0) {
                    MEMCPY(context->responseData + context->currentSize, evt->data, copy_len);
                    context->currentSize += copy_len;
                    context->responseData[context->currentSize] = '\0';
                } else {
                    DLOGE("HTTP response buffer overflow, data truncated");
                }
            }
            break;

        case HTTP_EVENT_ERROR:
            DLOGE("HTTP Event Error");
            break;

        case HTTP_EVENT_ON_FINISH:
            DLOGD("HTTP Event Finished");
            break;

        default:
            break;
    }
    return ESP_OK;
}

// Check if environment variable exists and return its value or default
static const char* getEnvOrDefault(const char* envName, const char* defaultValue) {
    const char* value = getenv(envName);
    return (value != NULL) ? value : defaultValue;
}

// Helper function to get the contents of a file
static char* readCertFile(const char* path, size_t* out_len)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    UINT64 fileSize = 0;
    STATUS status = getFileLength((PCHAR)path, &fileSize);

    if (STATUS_FAILED(status) || fileSize == 0) {
        DLOGE("Failed to get file length or file is empty: %s", path);
        return NULL;
    }

    char* buffer = (char*) MEMCALLOC(fileSize + 1, 1);
    if (!buffer) {
        DLOGE("Memory allocation failed for file: %s", path);
        return NULL;
    }

    status = readFile((PCHAR)path, TRUE, (PBYTE)buffer, &fileSize);

    if (STATUS_FAILED(status)) {
        DLOGE("Failed to read file: %s, error: 0x%08" PRIx32, path, status);
        SAFE_MEMFREE(buffer);
        return NULL;
    }

    buffer[fileSize] = '\0';

    if (out_len) {
        *out_len = (size_t)fileSize + 1; // include terminator
    }

    DLOGI("Successfully read file: %s (%" PRIu64 " bytes, %" PRIu64 " with null)",
             path, fileSize, fileSize + 1);
    return buffer;
}

// https://docs.aws.amazon.com/iot/latest/developerguide/authorizing-direct-aws.html
STATUS http_api_getIotCredential(PIotCredentialProvider pIotCredentialProvider)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;

    /* Variables for HTTP request */
    PCHAR pUrl = NULL;
    UINT32 urlLen;
    PCHAR pHost = NULL;
    UINT32 httpStatusCode = 0;
    PCHAR pResponseData = NULL;
    EspHttpRespContext responseContext = {0};
    esp_http_client_handle_t client = NULL;
    const char *pCertPath = NULL;
    const char *pPrivateKeyPath = NULL;
    const char *pCaCertPath = NULL;

    char* clientCertData = NULL; size_t clientCertLen = 0;
    char* caCertData = NULL;     size_t caCertLen = 0;
    char* privateKeyData = NULL; size_t privateKeyLen = 0;

    // Allocate buffers
    CHK(NULL != (pHost = (PCHAR) MEMCALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN, 1)), STATUS_NOT_ENOUGH_MEMORY);
    urlLen = STRLEN(CONTROL_PLANE_URI_PREFIX) + STRLEN(pIotCredentialProvider->iotGetCredentialEndpoint) + STRLEN(HTTP_API_ROLE_ALIASES) +
        STRLEN("/") + STRLEN(pIotCredentialProvider->roleAlias) + STRLEN(HTTP_API_CREDENTIALS) + 1;
    CHK(NULL != (pUrl = (PCHAR) MEMCALLOC(urlLen, 1)), STATUS_NOT_ENOUGH_MEMORY);

    // Allocate response buffer
    CHK(NULL != (pResponseData = (PCHAR) MEMCALLOC(HTTP_API_RECV_BUFFER_MAX_SIZE, 1)), STATUS_NOT_ENOUGH_MEMORY);
    pResponseData[0] = '\0';

    // Set up response context
    responseContext.responseData = pResponseData;
    responseContext.currentSize = 0;
    responseContext.maxSize = HTTP_API_RECV_BUFFER_MAX_SIZE;

    // Create the API url
    CHK(SNPRINTF(pUrl, urlLen, "%s%s%s%c%s%s", CONTROL_PLANE_URI_PREFIX, pIotCredentialProvider->iotGetCredentialEndpoint, HTTP_API_ROLE_ALIASES, '/',
                 pIotCredentialProvider->roleAlias, HTTP_API_CREDENTIALS) > 0,
        STATUS_HTTP_IOT_FAILED);

    // Extract hostname from URL for TLS hostname verification
    UINT32 hostLen = 0;
    PCHAR pHostStart = STRSTR(pUrl, "://");
    if (pHostStart != NULL) {
        pHostStart += 3; // Skip "://"
        PCHAR pHostEnd = STRCHR(pHostStart, '/');
        if (pHostEnd != NULL) {
            hostLen = (UINT32)(pHostEnd - pHostStart);
            STRNCPY(pHost, pHostStart, MIN(hostLen, MAX_CONTROL_PLANE_URI_CHAR_LEN - 1));
            pHost[MIN(hostLen, MAX_CONTROL_PLANE_URI_CHAR_LEN - 1)] = '\0';
        }
    }

    // Resolve certificate paths: prefer provider paths, fallback to environment variables if not provided
    pCertPath = (STRLEN(pIotCredentialProvider->certPath) > 0) ? pIotCredentialProvider->certPath
                                                               : getEnvOrDefault("AWS_IOT_CORE_CERT", NULL);
    pPrivateKeyPath = (STRLEN(pIotCredentialProvider->privateKeyPath) > 0) ? pIotCredentialProvider->privateKeyPath
                                                                          : getEnvOrDefault("AWS_IOT_CORE_PRIVATE_KEY", NULL);
    pCaCertPath = (STRLEN(pIotCredentialProvider->caCertPath) > 0) ? pIotCredentialProvider->caCertPath
                                                                   : getEnvOrDefault("AWS_KVS_CACERT_PATH", NULL);

    // Configure HTTP client
    esp_http_client_config_t config = (esp_http_client_config_t){0};
    config.url = pUrl;
    config.method = HTTP_METHOD_GET;
    config.event_handler = http_event_handler;
    config.user_data = &responseContext;
    config.timeout_ms = HTTP_API_COMPLETION_TIMEOUT;
    config.buffer_size = HTTP_CLIENT_RECV_BUF_SIZE;
    config.buffer_size_tx = HTTP_CLIENT_SEND_BUF_SIZE;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach; // use IDF bundle by default

    // Client certificate and key (mutual TLS)
    if (pCertPath != NULL && STRLEN(pCertPath) > 0) {
        DLOGI("Loading client certificate from: %s", pCertPath);
        clientCertData = readCertFile(pCertPath, &clientCertLen);
        CHK(clientCertData != NULL, STATUS_INVALID_OPERATION);
        config.client_cert_pem = clientCertData;
        config.client_cert_len = clientCertLen;
    } else {
        DLOGE("Client certificate path not provided");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    if (pPrivateKeyPath != NULL && STRLEN(pPrivateKeyPath) > 0) {
        DLOGI("Loading private key from: %s", pPrivateKeyPath);
        privateKeyData = readCertFile(pPrivateKeyPath, &privateKeyLen);
        CHK(privateKeyData != NULL, STATUS_INVALID_OPERATION);
        config.client_key_pem = privateKeyData;
        config.client_key_len = privateKeyLen;
    } else {
        DLOGE("Private key path not provided");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    // Optional custom CA; otherwise rely on IDF bundle
    if (pCaCertPath != NULL && STRLEN(pCaCertPath) > 0) {
        DLOGI("Loading CA certificate from: %s", pCaCertPath);
        caCertData = readCertFile(pCaCertPath, &caCertLen);
        if (caCertData != NULL) {
            config.cert_pem = caCertData;
            config.cert_len = caCertLen;
        } else {
            DLOGW("Failed to load CA certificate, falling back to ESP-IDF cert bundle");
        }
    }

    DLOGI("IoT Credential Provider Settings:");
    DLOGI("  Endpoint: %s", pIotCredentialProvider->iotGetCredentialEndpoint);
    DLOGI("  Role Alias: %s", pIotCredentialProvider->roleAlias);
    DLOGI("  Thing Name: %s", pIotCredentialProvider->thingName);

    // Initialize HTTP client
    client = esp_http_client_init(&config);
    CHK(client != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Set headers
    CHK(esp_http_client_set_header(client, HTTP_API_IOT_THING_NAME_HEADER, pIotCredentialProvider->thingName) == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(esp_http_client_set_header(client, "accept", "*/*") == ESP_OK, STATUS_INTERNAL_ERROR);
    CHK(esp_http_client_set_header(client, "User-Agent", "ESP32-KVS-WebRTC/1.0") == ESP_OK, STATUS_INTERNAL_ERROR);

    // Perform the request
    DLOGI("Sending HTTP request to: %s", pUrl);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        DLOGE("HTTP request failed with error %d (%s)", err, esp_err_to_name(err));
        CHK(FALSE, STATUS_HTTP_IOT_FAILED);
    }

    // Get HTTP status code
    httpStatusCode = esp_http_client_get_status_code(client);
    DLOGI("HTTP response status code: %d", (int)httpStatusCode);
    if (httpStatusCode < 200 || httpStatusCode >= 300) {
        DLOGE("HTTP request failed with status code %d", (int)httpStatusCode);
        DLOGE("Response body: %s", pResponseData);
        CHK(FALSE, STATUS_HTTP_IOT_FAILED);
    }

    // Parse the response
    DLOGI("Received response of size %d bytes", (int)responseContext.currentSize);
    CHK_STATUS(http_api_rsp_getIoTCredential(pIotCredentialProvider, pResponseData, responseContext.currentSize));
    DLOGI("Successfully parsed IoT credential response");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("IoT credential request failed with status 0x%08" PRIx32, retStatus);
    }

    if (client != NULL) {
        esp_http_client_cleanup(client);
    }

    // Free allocated memory for certificates
    SAFE_MEMFREE(caCertData);
    SAFE_MEMFREE(clientCertData);
    SAFE_MEMFREE(privateKeyData);

    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    SAFE_MEMFREE(pResponseData);

    HTTP_API_EXIT();
    return retStatus;
}

/******************************************************************************
 * Response parser
 ******************************************************************************/
STATUS http_api_rsp_getIoTCredential(PIotCredentialProvider pIotCredentialProvider, const CHAR* pResponseStr, UINT32 resultLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    jsmn_parser parser;
    jsmntok_t* pTokens = NULL;

    UINT32 i, accessKeyIdLen = 0, secretKeyLen = 0, sessionTokenLen = 0, expirationTimestampLen = 0;
    INT32 tokenCount;

    PCHAR accessKeyId = NULL;
    PCHAR secretKey = NULL;
    PCHAR sessionToken = NULL;
    PCHAR expirationTimestamp = NULL;
    UINT64 expiration, currentTime;
    CHAR expirationTimestampStr[MAX_EXPIRATION_LEN + 1];

    CHK(NULL != (pTokens = (jsmntok_t*) MEMALLOC(MAX_JSON_TOKEN_COUNT * SIZEOF(jsmntok_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK(resultLen > 0, STATUS_HTTP_IOT_FAILED);

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, pTokens, MAX_JSON_TOKEN_COUNT);
    CHK(tokenCount > 1, STATUS_JSON_API_CALL_INVALID_RETURN);
    CHK(pTokens[0].type == JSMN_OBJECT, STATUS_JSON_API_CALL_INVALID_RETURN);

    for (i = 1; i < (UINT32) tokenCount; i++) {
        if (compareJsonString((PCHAR) pResponseStr, &pTokens[i], JSMN_STRING, "accessKeyId")) {
            accessKeyIdLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(accessKeyIdLen <= MAX_ACCESS_KEY_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            accessKeyId = (PCHAR)(pResponseStr + pTokens[i + 1].start);
            i++;
        } else if (compareJsonString((PCHAR) pResponseStr, &pTokens[i], JSMN_STRING, "secretAccessKey")) {
            secretKeyLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(secretKeyLen <= MAX_SECRET_KEY_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            secretKey = (PCHAR)(pResponseStr + pTokens[i + 1].start);
            i++;
        } else if (compareJsonString((PCHAR) pResponseStr, &pTokens[i], JSMN_STRING, "sessionToken")) {
            sessionTokenLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(sessionTokenLen <= MAX_SESSION_TOKEN_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            sessionToken = (PCHAR)(pResponseStr + pTokens[i + 1].start);
            i++;
        } else if (compareJsonString((PCHAR) pResponseStr, &pTokens[i], JSMN_STRING, "expiration")) {
            expirationTimestampLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(expirationTimestampLen <= MAX_EXPIRATION_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            expirationTimestamp = (PCHAR)(pResponseStr + pTokens[i + 1].start);
            MEMCPY(expirationTimestampStr, expirationTimestamp, expirationTimestampLen);
            expirationTimestampStr[expirationTimestampLen] = '\0';
            i++;
        }
    }

    CHK(accessKeyId != NULL && secretKey != NULL && sessionToken != NULL, STATUS_HTTP_IOT_FAILED);
    currentTime = pIotCredentialProvider->getCurrentTimeFn(pIotCredentialProvider->customData);
    CHK_STATUS(convertTimestampToEpoch(expirationTimestampStr, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration));
    DLOGD("Iot credential expiration time %" PRIu64, expiration / HUNDREDS_OF_NANOS_IN_A_SECOND);

    if (pIotCredentialProvider->pAwsCredentials != NULL) {
        aws_credential_free(&pIotCredentialProvider->pAwsCredentials);
        pIotCredentialProvider->pAwsCredentials = NULL;
    }

    // Fix-up the expiration to be no more than max enforced token rotation
    expiration = MIN(expiration, currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION);

    CHK_STATUS(aws_credential_create(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration,
                                     &pIotCredentialProvider->pAwsCredentials));

CleanUp:
    SAFE_MEMFREE(pTokens);
    return retStatus;
}
