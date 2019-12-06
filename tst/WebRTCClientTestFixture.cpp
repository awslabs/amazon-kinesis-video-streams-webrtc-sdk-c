#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

//
// Global memory allocation counter
//
UINT64 gTotalWebRtcClientMemoryUsage = 0;

//
// Global memory counter lock
//
MUTEX gTotalWebRtcClientMemoryMutex;

WebRtcClientTestBase::WebRtcClientTestBase() :
        mAccessKeyIdSet(FALSE),
        mCaCertPath(NULL),
        mAccessKey(NULL),
        mSecretKey(NULL),
        mSessionToken(NULL),
        mRegion(NULL)
{
    // Initialize the endianness of the library
    initializeEndianness();

    // Store the function pointers
    gTotalWebRtcClientMemoryUsage = 0;
    mStoredMemAlloc = globalMemAlloc;
    mStoredMemAlignAlloc = globalMemAlignAlloc;
    mStoredMemCalloc = globalMemCalloc;
    mStoredMemFree = globalMemFree;

    // Create the mutex for the synchronization for the instrumentation
    gTotalWebRtcClientMemoryMutex = MUTEX_CREATE(FALSE);

    globalMemAlloc = instrumentedWebRtcClientMemAlloc;
    globalMemAlignAlloc = instrumentedWebRtcClientMemAlignAlloc;
    globalMemCalloc = instrumentedWebRtcClientMemCalloc;
    globalMemFree = instrumentedWebRtcClientMemFree;

    SRAND(12345);

    mStreamingRotationPeriod = TEST_STREAMING_TOKEN_DURATION;
}

void WebRtcClientTestBase::SetUp()
{
    DLOGI("\nSetting up test: %s\n", GetTestName());
    mReadyFrameIndex = 0;
    mDroppedFrameIndex = 0;
    mExpectedFrameCount = 0;
    mExpectedDroppedFrameCount = 0;


    if (NULL != (mAccessKey = getenv(ACCESS_KEY_ENV_VAR))) {
        mAccessKeyIdSet = TRUE;
    }

    mSecretKey = getenv(SECRET_KEY_ENV_VAR);
    mSessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if (NULL == (mRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        mRegion = TEST_DEFAULT_REGION;
    }

    if (NULL == (mCaCertPath = getenv(CACERT_PATH_ENV_VAR))) {
        mCaCertPath = EMPTY_STRING;
    }

    if (mAccessKey) {
        ASSERT_EQ(STATUS_SUCCESS, createStaticCredentialProvider(mAccessKey, 0, mSecretKey, 0,
                                                                 mSessionToken, 0, MAX_UINT64, &mTestCredentialProvider));
    } else {
        mTestCredentialProvider = nullptr;
    }

    // Prepare the test channel name by prefixing the host name with test channel name
    // replacing a potentially bad characters with '.'
    STRCPY(mChannelName, TEST_SIGNALING_CHANNEL_NAME);
    UINT32 testNameLen = STRLEN(TEST_SIGNALING_CHANNEL_NAME);
    gethostname(mChannelName + testNameLen, MAX_CHANNEL_NAME_LEN - testNameLen);

    // Replace any potentially "bad" characters
    PCHAR pCur = &mChannelName[testNameLen];
    while (*pCur != '\0') {
        BOOL found = FALSE;
        for (UINT32 i = 0; !found && i < ARRAY_SIZE(SIGNALING_VALID_NAME_CHARS) - 1; i++) {
            if (*pCur == SIGNALING_VALID_NAME_CHARS[i]) {
                found = TRUE;
            }
        }

        if (!found) {
            *pCur = '.';
        }

        pCur++;
    }
}

void WebRtcClientTestBase::TearDown()
{
    DLOGI("\nTearing down test: %s\n", GetTestName());

    freeStaticCredentialProvider(&mTestCredentialProvider);

    // Validate the allocations cleanup
    DLOGI("Final remaining allocation size is %llu", gTotalWebRtcClientMemoryUsage);
    EXPECT_EQ(0, gTotalWebRtcClientMemoryUsage);
    globalMemAlloc = mStoredMemAlloc;
    globalMemAlignAlloc = mStoredMemAlignAlloc;
    globalMemCalloc = mStoredMemCalloc;
    globalMemFree = mStoredMemFree;
    MUTEX_FREE(gTotalWebRtcClientMemoryMutex);
}

VOID WebRtcClientTestBase::initializeJitterBuffer(UINT32 expectedFrameCount, UINT32 expectedDroppedFrameCount, UINT32 rtpPacketCount)
{
    UINT32 i, timestamp;
    EXPECT_EQ(STATUS_SUCCESS, createJitterBuffer(testFrameReadyFunc, testFrameDroppedFunc, testDepayRtpFunc, DEFAULT_JITTER_BUFFER_MAX_LATENCY, TEST_JITTER_BUFFER_CLOCK_RATE, (UINT64) this, &mJitterBuffer));
    mExpectedFrameCount = expectedFrameCount;
    mFrame = NULL;
    if (expectedFrameCount > 0) {
        mPExpectedFrameArr = (PBYTE *) MEMALLOC(SIZEOF(PBYTE) * expectedFrameCount);
        mExpectedFrameSizeArr = (PUINT32) MEMALLOC(SIZEOF(UINT32) * expectedFrameCount);
    }
    mExpectedDroppedFrameCount = expectedDroppedFrameCount;
    if (expectedDroppedFrameCount > 0) {
        mExpectedDroppedFrameTimestampArr = (PUINT32) MEMALLOC(SIZEOF(UINT32) * expectedDroppedFrameCount);
    }

    mPRtpPackets = (PRtpPacket*) MEMALLOC(SIZEOF(PRtpPacket) * rtpPacketCount);
    mRtpPacketCount = rtpPacketCount;

    // Assume timestamp is on time unit ms for test
    for (i = 0, timestamp = 0; i < rtpPacketCount; i++, timestamp += 200) {
        EXPECT_EQ(STATUS_SUCCESS, createRtpPacket(2, FALSE, FALSE, 0, FALSE,
                                                  96, i, timestamp, 0x1234ABCD, NULL,
                                                  0, 0, NULL, NULL, 0, mPRtpPackets + i));
    }
}

VOID WebRtcClientTestBase::setPayloadToFree()
{
    UINT32 i;
    for (i = 0; i < mRtpPacketCount; i++) {
        mPRtpPackets[i]->pRawPacket = mPRtpPackets[i]->payload;
    }
}

VOID WebRtcClientTestBase::clearJitterBufferForTest()
{
    UINT32 i;
    EXPECT_EQ(STATUS_SUCCESS, freeJitterBuffer(&mJitterBuffer));
    if (mExpectedFrameCount > 0) {
        for (i = 0; i < mExpectedFrameCount; i++) {
            MEMFREE(mPExpectedFrameArr[i]);
        }
        MEMFREE(mPExpectedFrameArr);
        MEMFREE(mExpectedFrameSizeArr);
    }
    if (mExpectedDroppedFrameCount > 0) {
        MEMFREE(mExpectedDroppedFrameTimestampArr);
    }
    MEMFREE(mPRtpPackets);
    EXPECT_EQ(mExpectedFrameCount, mReadyFrameIndex);
    EXPECT_EQ(mExpectedDroppedFrameCount, mDroppedFrameIndex);
    if (mFrame != NULL) {
        MEMFREE(mFrame);
    }
}

PCHAR WebRtcClientTestBase::GetTestName()
{
    return (PCHAR) ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
}

}  // namespace webrtcclient
}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;
