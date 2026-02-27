#include "Include_i.h"

getTime globalGetTime = defaultGetTime;
getTime globalGetRealTime = defaultGetTime;

#if !(defined _WIN32 || defined _WIN64 || defined __CYGWIN__)

getTmTime globalGetThreadSafeTmTime = defaultGetThreadSafeTmTime;
pthread_mutex_t globalGmTimeMutex = PTHREAD_MUTEX_INITIALIZER;

struct tm* defaultGetThreadSafeTmTime(const time_t* timer)
{
    struct tm* retVal;
    pthread_mutex_lock(&globalGmTimeMutex);
    retVal = GMTIME(timer);
    pthread_mutex_unlock(&globalGmTimeMutex);
    return retVal;
}
#endif

STATUS generateTimestampStrInMilliseconds(PCHAR formatStr, PCHAR pDestBuffer, UINT32 destBufferLen, PUINT32 pFormattedStrLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 formattedStrLen;
    UINT32 millisecondVal;
    struct tm* timeinfo;

    CHK(pDestBuffer != NULL && pFormattedStrLen != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(formatStr, MAX_TIMESTAMP_FORMAT_STR_LEN + 1) <= MAX_TIMESTAMP_FORMAT_STR_LEN, STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED);

#if defined _WIN32 || defined _WIN64
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    // Convert the file time to 100-nanosecond intervals (since January 1, 1601)
    ULARGE_INTEGER uli;
    uli.LowPart = fileTime.dwLowDateTime;
    uli.HighPart = fileTime.dwHighDateTime;
    ULONGLONG fileTime100ns = uli.QuadPart;
    fileTime100ns -= TIME_DIFF_UNIX_WINDOWS_TIME;

    // Convert to milliseconds
    ULONGLONG milliseconds100ns = fileTime100ns / 10000;

    // Convert milliseconds to time_t
    time_t seconds = milliseconds100ns / 1000;

    // Convert time_t to UTC tm struct
    timeinfo = GMTIME_THREAD_SAFE(&seconds);

    millisecondVal = milliseconds100ns % 1000;

#elif defined __MACH__ || defined __CYGWIN__
    struct timeval tv;
    gettimeofday(&tv, NULL); // Get current time

    timeinfo = GMTIME_THREAD_SAFE(&(tv.tv_sec)); // Convert to broken-down time

    millisecondVal = tv.tv_usec / 1000; // Convert microseconds to milliseconds

#else
    struct timespec nowTime;
    clock_gettime(CLOCK_REALTIME, &nowTime);

    timeinfo = GMTIME_THREAD_SAFE(&(nowTime.tv_sec));
    millisecondVal = nowTime.tv_nsec / (HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS); // Convert nanoseconds to milliseconds

#endif
    formattedStrLen = 0;
    *pFormattedStrLen = 0;

    formattedStrLen = (UINT32) STRFTIME(pDestBuffer, destBufferLen - MAX_MILLISECOND_PORTION_LENGTH, formatStr, timeinfo);
    CHK(formattedStrLen != 0, STATUS_STRFTIME_FALIED);
    // Total length is 8 plus terminating null character. Generated string would have utmost size - 1. Hence need to add 1 to max length
    SNPRINTF(pDestBuffer + formattedStrLen, MAX_MILLISECOND_PORTION_LENGTH + 1, ".%03d ", millisecondVal);
    formattedStrLen = (UINT32) STRLEN(pDestBuffer);
    pDestBuffer[formattedStrLen] = '\0';
    *pFormattedStrLen = formattedStrLen;

CleanUp:

    return retStatus;
}

STATUS generateTimestampStr(UINT64 timestamp, PCHAR formatStr, PCHAR pDestBuffer, UINT32 destBufferLen, PUINT32 pFormattedStrLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    time_t timestampSeconds;
    UINT32 formattedStrLen;
    CHK(pDestBuffer != NULL && pFormattedStrLen != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(formatStr, MAX_TIMESTAMP_FORMAT_STR_LEN + 1) <= MAX_TIMESTAMP_FORMAT_STR_LEN, STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED);

    timestampSeconds = timestamp / HUNDREDS_OF_NANOS_IN_A_SECOND;
    formattedStrLen = 0;
    *pFormattedStrLen = 0;

    formattedStrLen = (UINT32) STRFTIME(pDestBuffer, destBufferLen, formatStr, GMTIME_THREAD_SAFE(&timestampSeconds));
    CHK(formattedStrLen != 0, STATUS_STRFTIME_FALIED);

    pDestBuffer[formattedStrLen] = '\0';
    *pFormattedStrLen = formattedStrLen;

CleanUp:

    return retStatus;
}

UINT64 defaultGetTime()
{
#if defined _WIN32 || defined _WIN64
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    return ((((UINT64) fileTime.dwHighDateTime << 32) | fileTime.dwLowDateTime) - TIME_DIFF_UNIX_WINDOWS_TIME);
#elif defined __MACH__ || defined __CYGWIN__
    struct timeval nowTime;
    if (0 != gettimeofday(&nowTime, NULL)) {
        return 0;
    }

    return (UINT64) nowTime.tv_sec * HUNDREDS_OF_NANOS_IN_A_SECOND + (UINT64) nowTime.tv_usec * HUNDREDS_OF_NANOS_IN_A_MICROSECOND;
#else
    struct timespec nowTime;
    clock_gettime(CLOCK_REALTIME, &nowTime);

    // The precision needs to be on a 100th nanosecond resolution
    return (UINT64) nowTime.tv_sec * HUNDREDS_OF_NANOS_IN_A_SECOND + (UINT64) nowTime.tv_nsec / DEFAULT_TIME_UNIT_IN_NANOS;
#endif
}
