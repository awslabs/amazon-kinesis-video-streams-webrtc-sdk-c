/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "common.h"
#include "version.h"

#ifdef KVSPIC_HAVE_UTSNAME_H
#include <sys/utsname.h>
#endif

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define VERSION_OS_NAME       "freertos/freertos"
#define VERSION_PLATFORM_NAME "esp32"

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__

STATUS defaultGetPlatformName(PCHAR pResult, UINT32 len)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 requiredLen;
    SYSTEM_INFO sysInfo;
    PCHAR platform;

    CHK(pResult != NULL, STATUS_NULL_ARG);

    ZeroMemory(&sysInfo, SIZEOF(SYSTEM_INFO));
    GetSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case 0x09:
            platform = (PCHAR) "AMD64";
            break;
        case 0x06:
            platform = (PCHAR) "IA64";
            break;
        case 0x00:
            platform = (PCHAR) "x86";
            break;
        default:
            platform = (PCHAR) "unknownArch";
            break;
    }

    requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", platform);

    CHK(requiredLen > 0 && (UINT32) requiredLen < len, STATUS_NOT_ENOUGH_MEMORY);

CleanUp:
    return retStatus;
}

#pragma warning(push)
#pragma warning(disable : 4996)
STATUS defaultGetOsVersion(PCHAR pResult, UINT32 len)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 requiredLen;
    OSVERSIONINFO versionInfo;

    CHK(pResult != NULL, STATUS_NULL_ARG);

    // With the release of Windows 8.1, the behavior of the GetVersionEx API has changed in the value it will
    // return for the operating system version can be overriden by the application manifest.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724451%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396
    //
    // However, we are going to use the API to get the version.
    //

    ZeroMemory(&versionInfo, SIZEOF(OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = SIZEOF(OSVERSIONINFO);

    if (!GetVersionEx(&versionInfo)) {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", (PCHAR) "Windows/UnknownVersion");
    } else {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s%u.%u.%u", (PCHAR) "Windows/", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion,
                               versionInfo.dwBuildNumber);
    }

    CHK(requiredLen > 0 && (UINT32) requiredLen < len, STATUS_NOT_ENOUGH_MEMORY);

CleanUp:
    return retStatus;
}
#pragma warning(pop)

#else

STATUS defaultGetOsVersion(PCHAR pResult, UINT32 len)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 requiredLen;
#ifdef KVSPIC_HAVE_UTSNAME_H
    struct utsname name;
#endif

    CHK(pResult != NULL, STATUS_NULL_ARG);

#ifdef KVSPIC_HAVE_UTSNAME_H
    if (uname(&name) >= 0) {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s/%s", name.sysname, name.release);
    } else
#endif
    {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", (PCHAR) VERSION_OS_NAME);
    }

    CHK(requiredLen > 0 && requiredLen < len, STATUS_NOT_ENOUGH_MEMORY);

CleanUp:
    return retStatus;
}

STATUS defaultGetPlatformName(PCHAR pResult, UINT32 len)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 requiredLen;
#ifdef KVSPIC_HAVE_UTSNAME_H
    struct utsname name;
#endif
    CHK(pResult != NULL, STATUS_NULL_ARG);

#ifdef KVSPIC_HAVE_UTSNAME_H
    if (uname(&name) >= 0) {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", name.machine);
    } else
#endif
    {
        requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", (PCHAR) VERSION_PLATFORM_NAME);
    }

    CHK(requiredLen > 0 && requiredLen < len, STATUS_NOT_ENOUGH_MEMORY);

CleanUp:
    return retStatus;
}

#endif

STATUS defaultGetCompilerInfo(PCHAR pResult, UINT32 len)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 requiredLen;

    CHK(pResult != NULL, STATUS_NULL_ARG);

#define __xstr__(s) __str__(s)
#define __str__(s)  #s
#if defined(_MSC_VER)
    requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s/%s", (PCHAR) "MSVC", (PCHAR) __xstr__(_MSC_VER));
#elif defined(__clang__)
    requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s/%s.%s.%s", (PCHAR) "Clang", (PCHAR) __xstr__(__clang_major__), (PCHAR) __xstr__(__clang_minor__),
                           (PCHAR) __xstr__(__clang_patchlevel__));
#elif defined(__GNUC__)
    requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s/%s.%s.%s", (PCHAR) "GCC", (PCHAR) __xstr__(__GNUC__), (PCHAR) __xstr__(__GNUC_MINOR__),
                           (PCHAR) __xstr__(__GNUC_PATCHLEVEL__));
#else
    requiredLen = SNPRINTF(pResult, len, (PCHAR) "%s", (PCHAR) "UnknownCompiler");
#endif
#undef __str__
#undef __xstr__

    CHK(requiredLen > 0 && (UINT32) requiredLen < len, STATUS_NOT_ENOUGH_MEMORY);

CleanUp:
    return retStatus;
}

STATUS getUserAgentString(PCHAR userAgentName, PCHAR customUserAgent, UINT32 len, PCHAR pUserAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pUserAgentName;
    INT32 requiredLen;
    CHAR osVer[MAX_OS_VERSION_STRING_LEN + 1];
    CHAR platformName[MAX_PLATFORM_NAME_STRING_LEN + 1];
    CHAR compilerInfo[MAX_COMPILER_INFO_STRING_LEN + 1];

    CHK(pUserAgent != NULL, STATUS_NULL_ARG);
    CHK(len <= MAX_USER_AGENT_LEN, STATUS_INVALID_USER_AGENT_LENGTH);

    CHK_STATUS(GET_OS_VERSION(osVer, ARRAY_SIZE(osVer)));
    CHK_STATUS(GET_PLATFORM_NAME(platformName, ARRAY_SIZE(platformName)));
    CHK_STATUS(GET_COMPILER_INFO(compilerInfo, ARRAY_SIZE(compilerInfo)));

    // Both the custom agent and the postfix are optional
    if (userAgentName == NULL || userAgentName[0] == '\0') {
        pUserAgentName = USER_AGENT_NAME;
    } else {
        pUserAgentName = userAgentName;
    }

    if (customUserAgent == NULL) {
        requiredLen = SNPRINTF(pUserAgent,
                               len + 1, // account for null terminator
                               (PCHAR) "%s/%s %s %s %s", pUserAgentName, AWS_SDK_KVS_PRODUCER_VERSION_STRING, compilerInfo, osVer, platformName);
    } else {
        requiredLen = SNPRINTF(pUserAgent,
                               len + 1, // account for null terminator
                               (PCHAR) "%s/%s %s %s %s %s", pUserAgentName, AWS_SDK_KVS_PRODUCER_VERSION_STRING, compilerInfo, osVer, platformName,
                               customUserAgent);
    }

    CHK(requiredLen > 0 && (UINT32) requiredLen <= len, STATUS_BUFFER_TOO_SMALL);

CleanUp:

    return retStatus;
}

getPlatformName globalGetPlatformName = defaultGetPlatformName;
getOsVersion globalGetOsVersion = defaultGetOsVersion;
getCompilerInfo globalGetCompilerInfo = defaultGetCompilerInfo;
