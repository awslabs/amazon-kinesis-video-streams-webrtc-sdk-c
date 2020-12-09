#include "gtest/gtest.h"
#include "../src/source/Include_i.h"
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

#define TEST_DEFAULT_REGION             ((PCHAR) "us-west-2")
#define TEST_STREAMING_TOKEN_DURATION   (40 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define TEST_JITTER_BUFFER_CLOCK_RATE   (1000)
#define TEST_SIGNALING_MASTER_CLIENT_ID (PCHAR) "Test_Master_ClientId"
#define TEST_SIGNALING_VIEWER_CLIENT_ID (PCHAR) "Test_Viewer_ClientId"
#define TEST_SIGNALING_CHANNEL_NAME     (PCHAR) "ScaryTestChannel_"
#define SIGNAING_TEST_CORRELATION_ID    (PCHAR) "Test_correlation_id"
#define TEST_SIGNALING_MESSAGE_TTL      (120 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define TEST_VIDEO_FRAME_SIZE           (120 * 1024)
#define TEST_FILE_CREDENTIALS_FILE_PATH (PCHAR) "credsFile"
#define MAX_TEST_AWAIT_DURATION         (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define TEST_CACHE_FILE_PATH            (PCHAR) "./.TestSignalingCache_v0"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// This comes from Producer-C, but is not exported. We are copying it here instead of making it part of the public API.
// It *MAY* become de-synchronized. If you hit issues after updating Producer-C confirm these two structs are in sync
typedef struct {
    AwsCredentialProvider credentialProvider;
    PAwsCredentials pAwsCredentials;
} StaticCredentialProvider, *PStaticCredentialProvider;

STATUS createRtpPacketWithSeqNum(UINT16 seqNum, PRtpPacket* ppRtpPacket);

class WebRtcClientTestBase : public ::testing::Test {
  public:
    PUINT32 mExpectedFrameSizeArr;
    PBYTE* mPExpectedFrameArr;
    UINT32 mExpectedFrameCount;
    PUINT32 mExpectedDroppedFrameTimestampArr;
    UINT32 mExpectedDroppedFrameCount;
    PRtpPacket* mPRtpPackets;
    UINT32 mRtpPacketCount;
    SIGNALING_CLIENT_HANDLE mSignalingClientHandle;

    WebRtcClientTestBase();

    PCHAR getAccessKey()
    {
        return mAccessKey;
    }

    PCHAR getSecretKey()
    {
        return mSecretKey;
    }

    PCHAR getSessionToken()
    {
        return mSessionToken;
    }

    VOID initializeSignalingClientStructs()
    {
        mTags[0].version = TAG_CURRENT_VERSION;
        mTags[0].name = (PCHAR) "Tag Name 0";
        mTags[0].value = (PCHAR) "Tag Value 0";
        mTags[1].version = TAG_CURRENT_VERSION;
        mTags[1].name = (PCHAR) "Tag Name 1";
        mTags[1].value = (PCHAR) "Tag Value 1";
        mTags[2].version = TAG_CURRENT_VERSION;
        mTags[2].name = (PCHAR) "Tag Name 2";
        mTags[2].value = (PCHAR) "Tag Value 2";

        mSignalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
        mSignalingClientCallbacks.customData = (UINT64) this;
        mSignalingClientCallbacks.messageReceivedFn = NULL;
        mSignalingClientCallbacks.errorReportFn = NULL;
        mSignalingClientCallbacks.stateChangeFn = NULL;

        mClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
        mClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
        mClientInfo.cacheFilePath = NULL; // Use the default path
        STRCPY(mClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

        MEMSET(&mChannelInfo, 0x00, SIZEOF(mChannelInfo));
        mChannelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
        mChannelInfo.pChannelName = mChannelName;
        mChannelInfo.pKmsKeyId = NULL;
        mChannelInfo.tagCount = 3;
        mChannelInfo.pTags = mTags;
        mChannelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
        mChannelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
        mChannelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
        mChannelInfo.cachingPeriod = 0;
        mChannelInfo.retry = TRUE;
        mChannelInfo.reconnect = TRUE;
        mChannelInfo.pCertPath = mCaCertPath;
        mChannelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
        if ((mChannelInfo.pRegion = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
            mChannelInfo.pRegion = (PCHAR) TEST_DEFAULT_REGION;
        }
    }

    STATUS initializeSignalingClient(PAwsCredentialProvider pCredentialProvider = NULL)
    {
        STATUS retStatus;

        initializeSignalingClientStructs();

        retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks,
                                              pCredentialProvider != NULL ? pCredentialProvider : mTestCredentialProvider, &mSignalingClientHandle);

        if (mAccessKeyIdSet) {
            EXPECT_EQ(STATUS_SUCCESS, retStatus);
        } else {
            mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
            EXPECT_NE(STATUS_SUCCESS, retStatus);
        }

        return retStatus;
    }

    STATUS deinitializeSignalingClient()
    {
        // Delete the created channel
        if (mAccessKeyIdSet) {
            deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle), 0);
        }

        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&mSignalingClientHandle));

        return STATUS_SUCCESS;
    }

    static STATUS testFrameReadyFunc(UINT64 customData, UINT16 startIndex, UINT16 endIndex, UINT32 frameSize)
    {
        WebRtcClientTestBase* base = (WebRtcClientTestBase*) customData;
        UINT32 filledSize;
        EXPECT_GT(base->mExpectedFrameCount, base->mReadyFrameIndex);
        EXPECT_EQ(base->mExpectedFrameSizeArr[base->mReadyFrameIndex], frameSize);
        if (base->mFrame != NULL) {
            MEMFREE(base->mFrame);
            base->mFrame = NULL;
        }
        base->mFrame = (PBYTE) MEMALLOC(frameSize);
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferFillFrameData(base->mJitterBuffer, base->mFrame, frameSize, &filledSize, startIndex, endIndex));
        EXPECT_EQ(frameSize, filledSize);
        EXPECT_EQ(0, MEMCMP(base->mPExpectedFrameArr[base->mReadyFrameIndex], base->mFrame, frameSize));
        base->mReadyFrameIndex++;
        return STATUS_SUCCESS;
    }

    static STATUS testFrameDroppedFunc(UINT64 customData, UINT16 startIndex, UINT16 endIndex, UINT32 timestamp)
    {
        UNUSED_PARAM(startIndex);
        UNUSED_PARAM(endIndex);
        auto* base = (WebRtcClientTestBase*) customData;
        EXPECT_GT(base->mExpectedDroppedFrameCount, base->mDroppedFrameIndex);
        EXPECT_EQ(base->mExpectedDroppedFrameTimestampArr[base->mDroppedFrameIndex], timestamp);
        base->mDroppedFrameIndex++;
        return STATUS_SUCCESS;
    }

    static STATUS testDepayRtpFunc(PBYTE payload, UINT32 payloadLength, PBYTE outBuffer, PUINT32 pBufferSize, PBOOL pIsStart)
    {
        ENTERS();
        STATUS retStatus = STATUS_SUCCESS;
        UINT32 bufferSize = 0;
        BOOL sizeCalculationOnly = (outBuffer == NULL);

        UNUSED_PARAM(pIsStart);
        CHK(payload != NULL && pBufferSize != NULL, STATUS_NULL_ARG);
        CHK(payloadLength > 0, retStatus);

        bufferSize = payloadLength;

        // Only return size if given buffer is NULL
        CHK(!sizeCalculationOnly, retStatus);
        CHK(payloadLength <= *pBufferSize, STATUS_BUFFER_TOO_SMALL);

        MEMCPY(outBuffer, payload, payloadLength);

    CleanUp:
        if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
            bufferSize = 0;
        }

        if (pBufferSize != NULL) {
            *pBufferSize = bufferSize;
        }

        if (pIsStart != NULL) {
            *pIsStart = (payload[payloadLength] != 0);
        }

        LEAVES();
        return retStatus;
    }

    STATUS readFrameData(PBYTE pFrame, PUINT32 pSize, UINT32 index, PCHAR frameFilePath)
    {
        STATUS retStatus = STATUS_SUCCESS;
        CHAR filePath[MAX_PATH_LEN + 1];
        UINT64 size = 0;

        CHK(pFrame != NULL && pSize != NULL, STATUS_NULL_ARG);

        SNPRINTF(filePath, MAX_PATH_LEN, "%s/frame-%04d.h264", frameFilePath, index);

        // Get the size and read into frame
        CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
        CHK_STATUS(readFile(filePath, TRUE, pFrame, &size));

        *pSize = (UINT32) size;

    CleanUp:

        return retStatus;
    }

    bool connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc, PCHAR pOfferCertFingerprint = NULL,
                         PCHAR pAnswerCertFingerprint = NULL);
    void addTrackToPeerConnection(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track, PRtcRtpTransceiver* transceiver, RTC_CODEC codec,
                                  MEDIA_STREAM_TRACK_KIND kind);
    void getIceServers(PRtcConfiguration pRtcConfiguration);

  protected:
    virtual void SetUp();
    virtual void TearDown();
    PCHAR GetTestName();
    VOID initializeJitterBuffer(UINT32, UINT32, UINT32);
    VOID clearJitterBufferForTest();
    VOID setPayloadToFree();

    PAwsCredentialProvider mTestCredentialProvider;

    PCHAR mAccessKey;
    PCHAR mSecretKey;
    PCHAR mSessionToken;
    PCHAR mRegion;
    PCHAR mCaCertPath;
    UINT64 mStreamingRotationPeriod;
    UINT32 mLogLevel;

    SIZE_T stateChangeCount[RTC_PEER_CONNECTION_TOTAL_STATE_COUNT] = {0};

    CHAR mDefaultRegion[MAX_REGION_NAME_LEN + 1];
    BOOL mAccessKeyIdSet;
    CHAR mChannelName[MAX_CHANNEL_NAME_LEN + 1];

    PJitterBuffer mJitterBuffer;
    PBYTE mFrame;
    UINT32 mReadyFrameIndex;
    UINT32 mDroppedFrameIndex;

    ChannelInfo mChannelInfo;
    SignalingClientCallbacks mSignalingClientCallbacks;
    SignalingClientInfo mClientInfo;
    Tag mTags[3];
};

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
