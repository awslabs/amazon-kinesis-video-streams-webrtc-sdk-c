/**
 * Various utility functionality
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>
#include "endianness.h"
#include "fileio.h"
#include "tags.h"
#include "timer_queue.h"
#include "thread.h"
#include "filelogger.h"
#include "hash_table.h"
#include "ThreadSafeBlockingQueue.h"
#include "ThreadPool.h"

#define MAX_STRING_CONVERSION_BASE 36

// thread stack size to use when running on constrained device like raspberry pi
#define THREAD_STACK_SIZE_ON_CONSTRAINED_DEVICE (512 * 1024)

// Check for whitespace
#define IS_WHITE_SPACE(ch) (((ch) == ' ') || ((ch) == '\t') || ((ch) == '\r') || ((ch) == '\n') || ((ch) == '\v') || ((ch) == '\f'))

/**
 * EMA (Exponential Moving Average) alpha value and 1-alpha value - over appx 20 samples
 */
#define EMA_ALPHA_VALUE           ((DOUBLE) 0.05)
#define ONE_MINUS_EMA_ALPHA_VALUE ((DOUBLE) (1 - EMA_ALPHA_VALUE))

/**
 * Calculates the EMA (Exponential Moving Average) accumulator value
 *
 * a - Accumulator value
 * v - Next sample point
 *
 * @return the new Accumulator value
 */
#define EMA_ACCUMULATOR_GET_NEXT(a, v) (DOUBLE)(EMA_ALPHA_VALUE * (v) + ONE_MINUS_EMA_ALPHA_VALUE * (a))

/**
 * Base64 encode/decode functionality
 */
PUBLIC_API STATUS base64Encode(PVOID, UINT32, PCHAR, PUINT32);
PUBLIC_API STATUS base64Decode(PCHAR, UINT32, PBYTE, PUINT32);

/**
 * Hex encode/decode functionality
 */
PUBLIC_API STATUS hexEncode(PVOID, UINT32, PCHAR, PUINT32);
PUBLIC_API STATUS hexEncodeCase(PVOID, UINT32, PCHAR, PUINT32, BOOL);
PUBLIC_API STATUS hexDecode(PCHAR, UINT32, PBYTE, PUINT32);

/**
 * Integer to string conversion routines
 */
PUBLIC_API STATUS ulltostr(UINT64, PCHAR, UINT32, UINT32, PUINT32);
PUBLIC_API STATUS ultostr(UINT32, PCHAR, UINT32, UINT32, PUINT32);

/**
 * String to integer conversion routines. NOTE: The base is in [2-36]
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - Pointer to the end of the string. If NULL, the NULL terminator would be used
 * @param 3 - IN - Base of the number (10 - for decimal)
 * @param 4 - OUT - The resulting value
 */
PUBLIC_API STATUS strtoui32(PCHAR, PCHAR, UINT32, PUINT32);
PUBLIC_API STATUS strtoui64(PCHAR, PCHAR, UINT32, PUINT64);
PUBLIC_API STATUS strtoi32(PCHAR, PCHAR, UINT32, PINT32);
PUBLIC_API STATUS strtoi64(PCHAR, PCHAR, UINT32, PINT64);

/**
 * Safe variant of strchr
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - IN - The character to look for
 *
 * @return - Pointer to the first occurrence or NULL
 */
PUBLIC_API PCHAR strnchr(PCHAR, UINT32, CHAR);

/**
 * Safe variant of strstr. This is a default implementation for strnstr when not available.
 *
 * @param 1 - IN - Input string to process
 * @param 3 - IN - The string to look for
 * @param 2 - IN - String length.
 *
 * @return - Pointer to the first occurrence or NULL
 */
PUBLIC_API PCHAR defaultStrnstr(PCHAR, PCHAR, SIZE_T);

/**
 * Left and right trim of the whitespace
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 and 4 - OUT - The pointer to the trimmed start and/or end
 *
 * @return Status of the operation
 */
PUBLIC_API STATUS ltrimstr(PCHAR, UINT32, PCHAR*);
PUBLIC_API STATUS rtrimstr(PCHAR, UINT32, PCHAR*);
PUBLIC_API STATUS trimstrall(PCHAR, UINT32, PCHAR*, PCHAR*);

/**
 * To lower and to upper string conversion
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return Status of the operation
 */
PUBLIC_API STATUS tolowerstr(PCHAR, UINT32, PCHAR);
PUBLIC_API STATUS toupperstr(PCHAR, UINT32, PCHAR);

/**
 * To lower/upper string conversion internal function
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - INT - Whether to upper (TRUE) or to lower (FALSE)
 * @param 4 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return Status of the operation
 */
STATUS tolowerupperstr(PCHAR, UINT32, BOOL, PCHAR);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Bitfield functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the byte count
 */
#define GET_BYTE_COUNT_FOR_BITS(x) ((((x) + 7) & ~7) >> 3)

/**
 * Bit field declaration
 * NOTE: Variable size structure - the bit field follow directly after the main structure
 */
typedef struct {
    UINT32 itemCount;
    /*-- BYTE[byteCount] bits; --*/
} BitField, *PBitField;

/**
 * Create a new bit field with all bits set to 0
 */
PUBLIC_API STATUS bitFieldCreate(UINT32, PBitField*);

/**
 * Frees and de-allocates the bit field object
 */
PUBLIC_API STATUS bitFieldFree(PBitField);

/**
 * Sets or clears all the bits
 */
PUBLIC_API STATUS bitFieldReset(PBitField, BOOL);

/**
 * Gets the bit field size in items
 */
PUBLIC_API STATUS bitFieldGetCount(PBitField, PUINT32);

/**
 * Gets the value of the bit
 */
PUBLIC_API STATUS bitFieldGet(PBitField, UINT32, PBOOL);

/**
 * Sets the value of the bit
 */
PUBLIC_API STATUS bitFieldSet(PBitField, UINT32, BOOL);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BitBuffer reader functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Bit reader error values starting from 0x41000000
 */
#define STATUS_BIT_READER_BASE         STATUS_UTILS_BASE + 0x01000000
#define STATUS_BIT_READER_OUT_OF_RANGE STATUS_BIT_READER_BASE + 0x00000001
#define STATUS_BIT_READER_INVALID_SIZE STATUS_BIT_READER_BASE + 0x00000002

/**
 * Bit Buffer reader declaration
 */
typedef struct {
    // Bit buffer
    PBYTE buffer;

    // Size of the buffer in bits
    UINT32 bitBufferSize;

    // Current bit
    UINT32 currentBit;
} BitReader, *PBitReader;

/**
 * Resets the bit reader object
 */
PUBLIC_API STATUS bitReaderReset(PBitReader, PBYTE, UINT32);

/**
 * Set current pointer
 */
PUBLIC_API STATUS bitReaderSetCurrent(PBitReader, UINT32);

/**
 * Read a bit from the current pointer
 */
PUBLIC_API STATUS bitReaderReadBit(PBitReader, PUINT32);

/**
 * Read up-to 32 bits from the current pointer
 */
PUBLIC_API STATUS bitReaderReadBits(PBitReader, UINT32, PUINT32);

/**
 * Read the Exponential Golomb encoded number from the current position
 */
PUBLIC_API STATUS bitReaderReadExpGolomb(PBitReader, PUINT32);

/**
 * Read the Exponential Golomb encoded signed number from the current position
 */
PUBLIC_API STATUS bitReaderReadExpGolombSe(PBitReader, PINT32);


////////////////////////////////////////////////////
// Dumping memory functionality
////////////////////////////////////////////////////
VOID dumpMemoryHex(PVOID, UINT32);

////////////////////////////////////////////////////
// Check memory content
////////////////////////////////////////////////////
BOOL checkBufferValues(PVOID, BYTE, SIZE_T);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Time functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This function generates the time string based on the timestamp format requested and the timestamp provided
 * Example: generateTimestampStrInMilliseconds(1689093566, "%Y-%m-%d %H:%M:%S", timeString, (UINT32) ARRAY_SIZE(timeString), &timeStrLen);
 * @UINT64  - IN - timestamp in 100ns to be converted to string. Sample timestamp input: 1689093566
 * @PCHAR   - IN - timestamp format string. Sample time format input: %Y-%m-%d %H:%M:%S
 * @PCHAR   - IN - buffer to hold the resulting string. Sample timestring output: 2023-07-11 16:37:07
 * @UINT32  - IN - buffer size
 * @PUINT32 - OUT - actual number of characters in the result string not including null terminator
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS generateTimestampStr(UINT64, PCHAR, PCHAR, UINT32, PUINT32);

/**
 * This function generates the millisecond portion of the timestamp and appends to the timestamp format supplied.
 * The output is of the format: <Timestring-format>.ssssss where ssssss is the millisecond format
 * Example: generateTimestampStrInMilliseconds("%Y-%m-%d %H:%M:%S", timeString, (UINT32) ARRAY_SIZE(timeString), &timeStrLen);
 * Formats can be constructed using this: https://man7.org/linux/man-pages/man3/strftime.3.html
 * @PCHAR   - IN - timestamp format string without milliseconds. Sample time format input: %Y-%m-%d %H:%M:%S
 * @PCHAR   - IN - buffer to hold the resulting string. Sample timestring output: 2023-07-11 16:37:07.025527,
 * where 025527 is the appended millisecond value
 * @UINT32  - IN - buffer size
 * @PUINT32 - OUT - actual number of characters in the result string not including null terminator
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS generateTimestampStrInMilliseconds(PCHAR, PCHAR, UINT32, PUINT32);

// yyyy-mm-dd HH:MM:SS
// #define MAX_TIMESTAMP_FORMAT_STR_LEN 26

// Length = len (.) + len(sss) + len('\0')
#define MAX_MILLISECOND_PORTION_LENGTH 5

// Max timestamp string length including null terminator
#define MAX_TIMESTAMP_STR_LEN 17

// (thread-0x7000076b3000)
#define MAX_THREAD_ID_STR_LEN 23

// Max log message length
#define MAX_LOG_FORMAT_LENGTH 600

// Set the global log level
#define SET_LOGGER_LOG_LEVEL(l) loggerSetLogLevel((l))

// Get the global log level
#define GET_LOGGER_LOG_LEVEL() loggerGetLogLevel()

/*
 * Set log level
 * @UINT32 - IN - target log level
 */
PUBLIC_API VOID loggerSetLogLevel(UINT32);

/**
 * Get current log level
 * @return - UINT32 - current log level
 */
PUBLIC_API UINT32 loggerGetLogLevel();

/**
 * Prepend log message with timestamp and thread id.
 * @PCHAR - IN - buffer holding the log
 * @UINT32 - IN - buffer length
 * @PCHAR - IN - log format string
 * @UINT32 - IN - log level
 * @return - VOID
 */
PUBLIC_API VOID addLogMetadata(PCHAR, UINT32, PCHAR, UINT32);

/**
 * Updates a CRC32 checksum
 * @UINT32 - IN - initial checksum result from previous update; for the first call, it should be 0.
 * @PBYTE - IN - buffer used to compute checksum
 * @UINT32 - IN - number of bytes to use from buffer
 * @return - UINT32 crc32 checksum
 */
PUBLIC_API UINT32 updateCrc32(UINT32, PBYTE, UINT32);

/**
 * @PBYTE - IN - buffer used to compute checksum
 * @UINT32 - IN - number of bytes to use from buffer
 * @return - UINT32 crc32 checksum
 */
#define COMPUTE_CRC32(pBuffer, len) (updateCrc32(0, pBuffer, len))

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Semaphore functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Semaphore shutdown timeout value
 */
#define SEMAPHORE_SHUTDOWN_TIMEOUT (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Definition of the Semaphore handle
 */
typedef UINT32 SEMAPHORE_HANDLE;
typedef SEMAPHORE_HANDLE* PSEMAPHORE_HANDLE;

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_SEMAPHORE_HANDLE_VALUE
#define INVALID_SEMAPHORE_HANDLE_VALUE ((SEMAPHORE_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_SEMAPHORE_HANDLE
#define IS_VALID_SEMAPHORE_HANDLE(h) ((h) != INVALID_SEMAPHORE_HANDLE_VALUE)
#endif

/**
 * Create a semaphore object
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreCreate(UINT32, PSEMAPHORE_HANDLE);

/**
 * Create a semaphore object that starts with 0 count
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreEmptyCreate(UINT32, PSEMAPHORE_HANDLE);

/*
 * Frees the semaphore object releasing all the awaiting threads.
 *
 * NOTE: The call is idempotent.
 *
 * @param - PSEMAPHORE_HANDLE - IN/OUT/OPT - Semaphore handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreFree(PSEMAPHORE_HANDLE);

/*
 * Acquires a semaphore. Will block for the specified amount of time before failing the acquisition.
 *
 * IMPORTANT NOTE: On failed acquire it will not increment the acquired count.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Time to wait to acquire in 100ns
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreAcquire(SEMAPHORE_HANDLE, UINT64);

/*
 * Releases a semaphore. Blocked threads will be released to acquire the available slot
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreRelease(SEMAPHORE_HANDLE);

/*
 * Locks the semaphore for any acquisitions.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreLock(SEMAPHORE_HANDLE);

/*
 * Unlocks the semaphore.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreUnlock(SEMAPHORE_HANDLE);

/*
 * Await for the semaphore to drain.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Timeout value to wait for
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreWaitUntilClear(SEMAPHORE_HANDLE, UINT64);

/*
 * Get the current value of the semaphore count
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - PINT32 - OUT - Value of count
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreGetCount(SEMAPHORE_HANDLE, PINT32);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrumented memory allocators functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the global allocators to the instrumented ones.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setInstrumentedAllocators();

/**
 * No-op equivalent of the setInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(SET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setInstrumentedAllocatorsNoop();

/**
 * Resets the global allocators to the original ones.
 *
 * NOTE: Any attempt to free allocations which were allocated after set call
 * past this API call will result in memory corruption.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS resetInstrumentedAllocators();

/**
 * No-op equivalent of the resetInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(RESET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS resetInstrumentedAllocatorsNoop();

/**
 * Returns the current total allocation size.
 *
 * @return - Total allocation size
 */
PUBLIC_API SIZE_T getInstrumentedTotalAllocationSize();

#ifdef INSTRUMENTED_ALLOCATORS
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocators()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocators()
#else
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocatorsNoop()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocatorsNoop()
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////
// KVS retry strategies
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Retry configuration type
 */
typedef enum { KVS_RETRY_STRATEGY_DISABLED = 0x00, KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT = 0x01 } KVS_RETRY_STRATEGY_TYPE;

// Opaque pointers to hold retry strategy state and configuration
// depending on the underlying implementation
typedef PUINT64 PRetryStrategy;
typedef PUINT64 PRetryStrategyConfig;

/**
 * A generic retry strategy
 */
struct __KvsRetryStrategy {
    // Pointer to metadata/state/details for the retry strategy.
    // The actual data type is abstracted and will be inferred by
    // the RetryHandlerFn
    PRetryStrategy pRetryStrategy;
    // Optional configuration used to build the retry strategy. Once the retry strategy is created,
    // any changes to the config will be useless.
    PRetryStrategyConfig pRetryStrategyConfig;
    // Retry strategy type as defined in the above enum
    KVS_RETRY_STRATEGY_TYPE retryStrategyType;
};

typedef struct __KvsRetryStrategy KvsRetryStrategy;
typedef struct __KvsRetryStrategy* PKvsRetryStrategy;

/**
 * Handler to create retry strategy
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 1 PKvsRetryStrategy - OUT - pRetryStrategy field of KvsRetryStrategy struct will be populated.
 *
 * @return Status of the callback
 */
typedef STATUS (*CreateRetryStrategyFn)(PKvsRetryStrategy);

/**
 * Handler to get retry count
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 2 PUINT32 - OUT - retry count value
 *
 * @return Status of the callback
 */
typedef STATUS (*GetCurrentRetryAttemptNumberFn)(PKvsRetryStrategy, PUINT32);

/**
 * Handler to release resources associated with a retry strategy
 *
 * @param 1 PKvsRetryStrategy - KvsRetryStrategy passed by the caller.
 *
 * @return Status of the callback
 */
typedef STATUS (*FreeRetryStrategyFn)(PKvsRetryStrategy);

/**
 * Handler to execute retry strategy
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 2 PUINT64 - OUT - wait time value computed by ExecuteRetryStrategyFn
 *
 * @return Status of the callback
 */
typedef STATUS (*ExecuteRetryStrategyFn)(PKvsRetryStrategy, PUINT64);

struct __KvsRetryStrategyCallbacks {
    // Pointer to the function to create new retry strategy
    CreateRetryStrategyFn createRetryStrategyFn;
    // Get retry count
    GetCurrentRetryAttemptNumberFn getCurrentRetryAttemptNumberFn;
    // Pointer to the function to release allocated resources
    // associated with the retry strategy
    FreeRetryStrategyFn freeRetryStrategyFn;
    // Pointer to the actual handler for the given retry strategy
    ExecuteRetryStrategyFn executeRetryStrategyFn;
};

typedef struct __KvsRetryStrategyCallbacks KvsRetryStrategyCallbacks;
typedef struct __KvsRetryStrategyCallbacks* PKvsRetryStrategyCallbacks;
//////////////////////////////////////////////////////////////////////////////////////////////////////
// APIs for exponential backoff retry strategy
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Indicates infinite exponential retries
 */
#define KVS_INFINITE_EXPONENTIAL_RETRIES 0

/**
 * Factor for computing the exponential backoff wait time
 * The larger the value, the slower the retries will be.
 */
#define MIN_KVS_RETRY_TIME_FACTOR_MILLISECONDS     50
#define LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS   1000
#define DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS

/**
 * Factor determining the curve of exponential wait time
 */
#define DEFAULT_KVS_EXPONENTIAL_FACTOR 2

/**
 * Maximum exponential wait time. Once the exponential wait time
 * curve reaches this value, it stays at this value. This is
 * required to put a reasonable upper bound on wait time.
 * required to put a reasonable upper bound on wait time. If not provided
 * in the config, we'll use the default value
 */
#define MIN_KVS_MAX_WAIT_TIME_MILLISECONDS     10000
#define LIMIT_KVS_MAX_WAIT_TIME_MILLISECONDS   25000
#define DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS 16000

/**
 * Maximum time between two consecutive calls to exponentialBackoffBlockingWait
 * after which the retry count will be reset to initial state. This is needed
 * to restart the exponential wait time from base value if
 */
#define MIN_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS     90000
#define LIMIT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS   120000
#define DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS MIN_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS

/**
 * Factor to get a random jitter. Jitter values [0, 300).
 * Only applicable for Fixed jitter variant
 */
#define MIN_KVS_JITTER_FACTOR_MILLISECONDS     50
#define LIMIT_KVS_JITTER_FACTOR_MILLISECONDS   600
#define DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS 300

typedef enum {
    BACKOFF_NOT_STARTED = (UINT16) 0x01,
    BACKOFF_IN_PROGRESS = (UINT16) 0x02,
    BACKOFF_TERMINATED = (UINT16) 0x03
} ExponentialBackoffStatus;

typedef enum {
    FULL_JITTER = (UINT16) 0x01,
    FIXED_JITTER = (UINT16) 0x02,
    NO_JITTER = (UINT16) 0x03,
} ExponentialBackoffJitterType;

typedef struct __ExponentialBackoffRetryStrategyConfig {
    // Max retries after which an error will be returned
    // to the application. For infinite retries, set this
    // to KVS_INFINITE_EXPONENTIAL_RETRIES.
    UINT32 maxRetryCount;
    // Maximum retry wait time. Once the retry wait time
    // reaches this value, subsequent retries will wait for
    // maxRetryWaitTime (plus jitter).
    UINT64 maxRetryWaitTime;
    // Factor for computing the exponential backoff wait time
    UINT64 retryFactorTime;
    // The minimum time between two consecutive retries
    // after which retry state will be reset i.e. retries
    // will start from initial retry state.
    UINT64 minTimeToResetRetryState;
    // Jitter type indicating how much jitter to be added
    // Default will be FULL_JITTER
    ExponentialBackoffJitterType jitterType;
    // Factor determining random jitter value.
    // Jitter will be between [0, jitterFactor)
    // This parameter is only valid for jitter type FIXED_JITTER
    UINT32 jitterFactor;
} ExponentialBackoffRetryStrategyConfig, *PExponentialBackoffRetryStrategyConfig;

#define TO_EXPONENTIAL_BACKOFF_STATE(ptr)  ((PExponentialBackoffRetryStrategyState) (ptr))
#define TO_EXPONENTIAL_BACKOFF_CONFIG(ptr) ((PExponentialBackoffRetryStrategyConfig) (ptr))

typedef struct {
    ExponentialBackoffRetryStrategyConfig exponentialBackoffRetryStrategyConfig;
    ExponentialBackoffStatus status;
    UINT32 currentRetryCount;
    // The system time at which last retry happened
    UINT64 lastRetrySystemTime;
    // The actual wait time for last retry
    UINT64 lastRetryWaitTime;
    // Lock to update operations
    MUTEX retryStrategyLock;
} ExponentialBackoffRetryStrategyState, *PExponentialBackoffRetryStrategyState;

/************************************************************************
 With default exponential values, the wait times will look like following -
    ************************************
    * Retry Count *      Wait time     *
    * **********************************
    *     1       *   1000ms + jitter  *
    *     2       *   2000ms + jitter  *
    *     3       *   4000ms + jitter  *
    *     4       *   8000ms + jitter  *
    *     5       *  16000ms + jitter  *
    *     6       *  16000ms + jitter  *
    *     7       *  16000ms + jitter  *
    *     8       *  16000ms + jitter  *
    ************************************
 for FULL_JITTER variant, jitter = random number between [0, wait time)
 for FIXED_JITTER variant, jitter = random number between [0, DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS)
************************************************************************/
static const ExponentialBackoffRetryStrategyConfig DEFAULT_EXPONENTIAL_BACKOFF_CONFIGURATION = {
    KVS_INFINITE_EXPONENTIAL_RETRIES,                       /* max retry count */
    DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS,                 /* max retry wait time */
    DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS,             /* factor determining exponential curve */
    DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, /* minimum time to reset retry state */
    FULL_JITTER,                                            /* use FULL_JITTER variant */
    0                                                       /* jitter value unused for full jitter variant */
};

/**************************************************************************************************
Direct API usage example:

 void sample_configureExponentialBackoffRetryStrategy() {

     KvsRetryStrategy kvsRetryStrategy = {NULL, NULL, 0};

     //
     // [Optional] Configure with some specific exponential backoff configuration?
     //     ExponentialBackoffRetryStrategyConfig someExponentialBackoffRetryStrategyConfig;
     //     kvsRetryStrategy.pRetryStrategyConfig = &someExponentialBackoffRetryStrategyConfig;
     // Note: This config will be deep copied while creating exponential backoff retry strategy. So its okay
     //        if you pass address of a local struct.
     //

     CHK_STATUS(exponentialBackoffRetryStrategyCreate(&kvsRetryStrategy));
     CHK_STATUS(kvsRetryStrategy.pRetryStrategy != NULL);
     CHK_STATUS(kvsRetryStrategy.retryStrategyType == KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT);

     while (...) {
         CHK_STATUS(exponentialBackoffBlockingWait(&kvsRetryStrategy));
        // some business logic which includes service API call(s)
     }

     CHK_STATUS(exponentialBackoffStateFree(&kvsRetryStrategy));
 }

**************************************************************************************************/

/**
 * @brief Initializes exponential backoff state with provided configuration
 * This should be called once before calling exponentialBackoffBlockingWait/getExponentialBackoffRetryStrategyWaitTime.
 * If unsure about the configuration parameters, it is recommended to initialize
 * the state using initializeExponentialBackoffStateWithDefaultConfig API
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - OUT - pRetryStrategy field of KvsRetryStrategy struct will be populated.
 *                                    If PKvsRetryStrategy->PRetryStrategyConfig not provided, default config will be used
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyCreate(PKvsRetryStrategy);

/**
 * @brief
 * Computes and returns the next exponential backoff wait time
 *
 * THREAD SAFE.
 *
 * Note: This API may return STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE error code if ExponentialBackoffState
 * is not configured correctly or is corrupted. In such case, the application should re-create
 * ExponentialBackoffState using the exponentialBackoffStateCreate OR exponentialBackoffStateWithDefaultConfigCreate
 * API and then call exponentialBackoffBlockingWait
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy
 * @return Status of the function call.
 */
PUBLIC_API STATUS getExponentialBackoffRetryStrategyWaitTime(PKvsRetryStrategy, PUINT64);

/**
 * @brief
 * Computes next exponential backoff wait time and blocks the current thread for that
 * much time. This is identical to getExponentialBackoffRetryStrategyWaitTime API
 * but it waits for the actual wait time before returning. To not block the current
 * thread, use getExponentialBackoffRetryStrategyWaitTime API.
 *
 * THREAD SAFE.
 *
 * Note: This API may return STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE error code if ExponentialBackoffState
 * is not configured correctly or is corrupted. In such case, the application should re-create
 * ExponentialBackoffState using the exponentialBackoffStateCreate OR exponentialBackoffStateWithDefaultConfigCreate
 * API and then call exponentialBackoffBlockingWait
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyBlockingWait(PKvsRetryStrategy);

/**
 * @brief Returns updated exponential backoff retry count when PRetryStrategy object is passed
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy for which retry state is maintained
 * @param 2 PUINT32 - OUT - Retry count
 * @return Status of the function call.
 */
PUBLIC_API STATUS getExponentialBackoffRetryCount(PKvsRetryStrategy, PUINT32);
/**
 * @brief Frees ExponentialBackoffState and its corresponding ExponentialBackoffConfig struct
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy.
 *                              pRetryStrategy field within PKvsRetryStrategy will be released.
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyFree(PKvsRetryStrategy);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Math Utility APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Computes power
 * Returns error when the base or exponent values are not within limits.
 * Acceptable values:
 *  base: [0, 5], exponent: [0, 27]
 *  base: [6, 100], exponent: [0, 9]
 *  base: [101, 1000], exponent: [0, 6]
 *
 * @param - UINT32 - IN - Base.
 * @param - UINT32 - IN - Exponent.
 * @param - PUINT64 - OUT - Result.
 *
 * @return - STATUS code of the execution.
 *
 */
PUBLIC_API STATUS computePower(UINT64, UINT64, PUINT64);

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H__
