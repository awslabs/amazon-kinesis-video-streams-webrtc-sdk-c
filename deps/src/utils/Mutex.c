#include "Include_i.h"

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__

//
// Stub Mutex library functions
//
typedef struct {
    SRWLOCK srwLock;
    CRITICAL_SECTION criticalSection;
    BOOL reentrant;
} WinLock, *PWinLock;

MUTEX defaultCreateMutex(BOOL reentrant)
{
    PWinLock pLock = (PWinLock) MEMCALLOC(1, SIZEOF(WinLock));
    if (NULL == pLock) {
        return (MUTEX) NULL;
    }

    pLock->reentrant = reentrant;

    if (reentrant) {
        // Use critical sections
        InitializeCriticalSection(&pLock->criticalSection);
    } else {
        // Use SRW locks
        InitializeSRWLock(&pLock->srwLock);
    }

    return (MUTEX) pLock;
}

VOID defaultLockMutex(MUTEX mutex)
{
    PWinLock pLock = (PWinLock) mutex;

    CHECK_EXT(NULL != pLock, "Invalid lock value");

    if (pLock->reentrant) {
        EnterCriticalSection(&pLock->criticalSection);
    } else {
        AcquireSRWLockExclusive(&pLock->srwLock);
    }
}

VOID defaultUnlockMutex(MUTEX mutex)
{
    PWinLock pLock = (PWinLock) mutex;

    CHECK_EXT(NULL != pLock, "Invalid lock value");

    if (pLock->reentrant) {
        LeaveCriticalSection(&pLock->criticalSection);
    } else {
        ReleaseSRWLockExclusive(&pLock->srwLock);
    }
}

BOOL defaultTryLockMutex(MUTEX mutex)
{
    PWinLock pLock = (PWinLock) mutex;

    CHECK_EXT(NULL != pLock, "Invalid lock value");

    if (pLock->reentrant) {
        return TryEnterCriticalSection(&pLock->criticalSection);
    } else {
        return TryAcquireSRWLockExclusive(&pLock->srwLock);
    }
}

VOID defaultFreeMutex(MUTEX mutex)
{
    PWinLock pLock = (PWinLock) mutex;

    if (NULL == pLock) {
        // Early exit - idempotent
        return;
    }

    if (pLock->reentrant) {
        DeleteCriticalSection(&pLock->criticalSection);
    }

    MEMFREE(pLock);
}

CVAR defaultConditionVariableCreate()
{
    CVAR pVar = (CVAR) MEMCALLOC(1, SIZEOF(CONDITION_VARIABLE));
    if (NULL == pVar) {
        return (CVAR) NULL;
    }

    InitializeConditionVariable(pVar);

    return pVar;
}

VOID defaultConditionVariableFree(CVAR cvar)
{
    if (NULL == cvar) {
        // Early exit - idempotent
        return;
    }

    MEMFREE(cvar);
}

STATUS defaultConditionVariableSignal(CVAR cvar)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_ERR(NULL != cvar, STATUS_INVALID_ARG, "Invalid condition variable value");

    WakeConditionVariable(cvar);

CleanUp:
    return retStatus;
}

STATUS defaultConditionVariableBroadcast(CVAR cvar)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_ERR(NULL != cvar, STATUS_INVALID_ARG, "Invalid condition variable value");

    WakeAllConditionVariable(cvar);

CleanUp:
    return retStatus;
}

STATUS defaultConditionVariableWait(CVAR cvar, MUTEX mutex, UINT64 timeout)
{
    STATUS retStatus = STATUS_SUCCESS;
    PWinLock pLock = (PWinLock) mutex;
    DWORD dwTimeout;

    CHK_ERR(NULL != cvar && NULL != pLock, STATUS_INVALID_ARG, "Invalid condition variable value");

    if (INFINITE_TIME_VALUE == timeout) {
        dwTimeout = INFINITE;
    } else {
        dwTimeout = (DWORD) (timeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    if (pLock->reentrant) {
        CHK(SleepConditionVariableCS(cvar, &pLock->criticalSection, dwTimeout), STATUS_WAIT_FAILED);
    } else {
        CHK(SleepConditionVariableSRW(cvar, &pLock->srwLock, dwTimeout, 0), STATUS_WAIT_FAILED);
    }

CleanUp:

    // Check for a timeout
    if (STATUS_FAILED(retStatus) && (ERROR_TIMEOUT == GetLastError())) {
        retStatus = STATUS_OPERATION_TIMED_OUT;
    }

    return retStatus;
}

#else

// Definition of the static global mutexes
pthread_mutex_t globalKvsReentrantMutex = GLOBAL_MUTEX_INIT_RECURSIVE;
pthread_mutex_t globalKvsNonReentrantMutex = GLOBAL_MUTEX_INIT;

MUTEX defaultCreateMutex(BOOL reentrant)
{
    pthread_mutex_t* pMutex;
    pthread_mutexattr_t mutexAttributes;

    // Allocate the mutex
    pMutex = (pthread_mutex_t*) MEMCALLOC(1, SIZEOF(pthread_mutex_t));
    if (NULL == pMutex) {
        return (MUTEX) (reentrant ? &globalKvsReentrantMutex : &globalKvsNonReentrantMutex);
    }

    if (0 != pthread_mutexattr_init(&mutexAttributes) ||
        0 != pthread_mutexattr_settype(&mutexAttributes, reentrant ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL) ||
        0 != pthread_mutex_init(pMutex, &mutexAttributes)) {
        // In case of an error return the global mutexes
        MEMFREE(pMutex);
        return (MUTEX) (reentrant ? &globalKvsReentrantMutex : &globalKvsNonReentrantMutex);
    }

    return (MUTEX) pMutex;
}

VOID defaultLockMutex(MUTEX mutex)
{
    pthread_mutex_lock((pthread_mutex_t*) mutex);
}

VOID defaultUnlockMutex(MUTEX mutex)
{
    pthread_mutex_unlock((pthread_mutex_t*) mutex);
}

BOOL defaultTryLockMutex(MUTEX mutex)
{
    return (0 == pthread_mutex_trylock((pthread_mutex_t*) mutex));
}

VOID defaultFreeMutex(MUTEX mutex)
{
    pthread_mutex_t* pMutex = (pthread_mutex_t*) mutex;
    pthread_mutex_destroy(pMutex);

    // De-allocate the memory if it's not a well-known mutex - aka if we had allocated it previously
    if (pMutex != &globalKvsReentrantMutex && pMutex != &globalKvsNonReentrantMutex) {
        MEMFREE(pMutex);
    }
}

pthread_cond_t globalKvsConditionVariable = PTHREAD_COND_INITIALIZER;

CVAR defaultConditionVariableCreate()
{
    CVAR pVar = (CVAR) MEMCALLOC(1, SIZEOF(pthread_cond_t));
    if (NULL == pVar) {
        return &globalKvsConditionVariable;
    }

    if (0 != pthread_cond_init(pVar, NULL)) {
        return &globalKvsConditionVariable;
    }

    return pVar;
}

VOID defaultConditionVariableFree(CVAR cvar)
{
    pthread_cond_t* pCondVar = (pthread_cond_t*) cvar;
    if (NULL == pCondVar) {
        // Early exit - idempotent
        return;
    }

    pthread_cond_destroy(pCondVar);

    if (pCondVar != &globalKvsConditionVariable) {
        MEMFREE(pCondVar);
    }
}

STATUS defaultConditionVariableSignal(CVAR cvar)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_ERR(NULL != cvar, STATUS_INVALID_ARG, "Invalid condition variable value");

    CHK(0 == pthread_cond_signal(cvar), STATUS_INVALID_OPERATION);

CleanUp:
    return retStatus;
}

STATUS defaultConditionVariableBroadcast(CVAR cvar)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_ERR(NULL != cvar, STATUS_INVALID_ARG, "Invalid condition variable value");

    CHK(0 == pthread_cond_broadcast(cvar), STATUS_INVALID_OPERATION);

CleanUp:
    return retStatus;
}

STATUS defaultConditionVariableWait(CVAR cvar, MUTEX mutex, UINT64 timeout)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 retVal = 0;
    struct timespec timeSpec;
    pthread_mutex_t* pMutex = (pthread_mutex_t*) mutex;
    UINT64 curTime = GETREALTIME();

    // Timeout is a duration so we need to construct an absolute time
    UINT64 time = timeout + curTime;

    // If we overflow or have specific infinite timeout then wait unconditionally
    if (time < timeout || INFINITE_TIME_VALUE == timeout) {
        CHK(0 == (retVal = pthread_cond_wait(cvar, pMutex)), STATUS_WAIT_FAILED);
    } else {
        timeSpec.tv_sec = time / HUNDREDS_OF_NANOS_IN_A_SECOND;
        timeSpec.tv_nsec = (time % HUNDREDS_OF_NANOS_IN_A_SECOND) * DEFAULT_TIME_UNIT_IN_NANOS;
        CHK(0 == (retVal = pthread_cond_timedwait(cvar, pMutex, &timeSpec)), STATUS_WAIT_FAILED);
    }

CleanUp:

    // Check for a timeout
    if (STATUS_FAILED(retStatus) && (ETIMEDOUT == retVal)) {
        retStatus = STATUS_OPERATION_TIMED_OUT;
    }

    return retStatus;
}

#endif

createMutex globalCreateMutex = defaultCreateMutex;
lockMutex globalLockMutex = defaultLockMutex;
unlockMutex globalUnlockMutex = defaultUnlockMutex;
tryLockMutex globalTryLockMutex = defaultTryLockMutex;
freeMutex globalFreeMutex = defaultFreeMutex;

createConditionVariable globalConditionVariableCreate = defaultConditionVariableCreate;
signalConditionVariable globalConditionVariableSignal = defaultConditionVariableSignal;
broadcastConditionVariable globalConditionVariableBroadcast = defaultConditionVariableBroadcast;
waitConditionVariable globalConditionVariableWait = defaultConditionVariableWait;
freeConditionVariable globalConditionVariableFree = defaultConditionVariableFree;
