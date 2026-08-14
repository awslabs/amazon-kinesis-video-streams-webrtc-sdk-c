#include "WebRTCClientTestFixture.h"

#ifdef ENABLE_DATA_CHANNEL
namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DataChannelFunctionalityTest : public WebRtcClientTestBase {
};

// Macro so we don't have to deal with scope capture
#define TEST_DATA_CHANNEL_MESSAGE "This is my test message"

struct RemoteOpen {
    std::mutex lock{};
    std::map<std::string, uint64_t> channels{};
};

// Create two PeerConnections and ensure DataChannels that were declared
// before signaling go to connected
TEST_F(DataChannelFunctionalityTest, createDataChannel_Disconnected)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RemoteOpen remoteOpen{};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
        DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
        std::string name(pRtcDataChannel->name);
        {
            std::lock_guard<std::mutex> lock(remoteOpen->lock);
            if (remoteOpen->channels.count(name) == 0) {
                remoteOpen->channels.emplace(name, 1u);
            } else {
                auto count = remoteOpen->channels.at(name);
                remoteOpen->channels.erase(name);
                remoteOpen->channels.emplace(name, count + 1);
            }
        }
        dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", nullptr, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 2);
    ASSERT_EQ(ATOMIC_LOAD(&msgCount), 2);
    ASSERT_EQ(2, remoteOpen.channels.size());
    ASSERT_EQ(1, remoteOpen.channels.count("Offer PeerConnection"));
    ASSERT_EQ(1, remoteOpen.channels.count("Answer PeerConnection"));
    ASSERT_EQ(1u, remoteOpen.channels.at("Offer PeerConnection"));
    ASSERT_EQ(1u, remoteOpen.channels.at("Answer PeerConnection"));
}

// When a message drops from a data channel, the connection should retry it
TEST_F(DataChannelFunctionalityTest, createDataChannel_RecoverFromDroppedMessage)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    // "offer" creates a data channel, answer just listens on it
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelRemoteOpenCount = 0, msgCount = 0;
    struct OnOpenHandle {
        PSIZE_T datachannelRemoteOpenCount;
        PSIZE_T msgCount;
    };
    OnOpenHandle onOpenHandle{&datachannelRemoteOpenCount, &msgCount};
    // These variables need to be static so the lambda that uses them does not need to capture since lambdas with captures cannot
    // be converted to function pointers
    static bool dropNextMessage{false};
    static std::mutex dropMessageMutex{};
    static IceInboundPacketFunc iceInboundPacketCallback = nullptr;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Override the ice agent inbound packet callback to allow us to drop messages and simulate a flaky connection
    iceInboundPacketCallback = ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.inboundPacketFn;
    dropNextMessage = false; // do not drop any messages yet
    // Override the ice agent inbound packet callback to drop the first message, simulating
    // a peer that has stopped communicating without closing the connection
    const auto dropFirstInboundMessage = [](UINT64 customData, PBYTE pBuffer, UINT32 bufferLen) {
        std::lock_guard<std::mutex> lock{dropMessageMutex};
        if (dropNextMessage) {
            dropNextMessage = false;
            return;
        }

        iceInboundPacketCallback(customData, pBuffer, bufferLen);
    };
    ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.inboundPacketFn = dropFirstInboundMessage;

    // Needs to be static so onDataChannel can use it without capture
    static auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        DLOGD("onDataChannelMessage '%s'", pMsg);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto handle = reinterpret_cast<OnOpenHandle*>(customData);
        ATOMIC_INCREMENT(handle->datachannelRemoteOpenCount);
        DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
        // attach listener
        EXPECT_EQ(dataChannelOnMessage(pRtcDataChannel, (UINT64) handle->msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
        // send a message to ensure the channel is working
        dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    // Answer peer listens for data channels becoming available on which to attach the listener
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &onOpenHandle, onDataChannel), STATUS_SUCCESS);
    // Create a DataChannel from the offer to the answer peer
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    // Register listener on this side to ensure the channel is working before we simulate the failure
    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannel connects
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelRemoteOpenCount) + ATOMIC_LOAD(&msgCount)) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Assert that channels are open and messages are received before testing the dropped message edge case
    ASSERT_EQ(ATOMIC_LOAD(&datachannelRemoteOpenCount), 1);
    ASSERT_EQ(ATOMIC_LOAD(&msgCount), 1);

    // set up the ice agent to drop the next message, simulating a flaky connection
    {
        std::lock_guard<std::mutex> lock{dropMessageMutex};
        dropNextMessage = true;
    }

    // send a message that will be dropped
    dataChannelSend(pOfferDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));

    // busy wait for the message to be resent after being dropped
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&msgCount) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&answerPc);

    ASSERT_EQ(ATOMIC_LOAD(&msgCount), 2); // message should eventually make it through
}

// Fill the SCTP send buffer (by dropping all inbound to the peer so nothing is SACKed), then
// verify the buffered data drains and is delivered once the "network" recovers. This reproduces
// the exact failure mode described in PR #2319: with the send buffer full and no inbound packets,
// only the periodic SCTP timer can advance retransmission and clear the stall.
TEST_F(DataChannelFunctionalityTest, createDataChannel_RecoverFromFullSendBuffer)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    // "offer" creates a data channel and sends on it, "answer" just listens
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelRemoteOpenCount = 0, answerMsgCount = 0;
    struct OnOpenHandle {
        PSIZE_T datachannelRemoteOpenCount;
        PSIZE_T answerMsgCount;
    };
    OnOpenHandle onOpenHandle{&datachannelRemoteOpenCount, &answerMsgCount};

    // Static so the capture-less lambdas (which are converted to function pointers) can reach them
    static bool dropAllInbound{false};
    static std::mutex dropMessageMutex{};
    static IceInboundPacketFunc iceInboundPacketCallback = nullptr;
    // Sub-MTU payload (fits in a single SCTP data chunk, no fragmentation). Sent repeatedly with
    // no draining, this fills the send buffer after a few hundred sends.
    static const UINT32 BIG_MSG_LEN = 1000;
    static BYTE bigMsg[BIG_MSG_LEN];
    MEMSET(bigMsg, 'A', BIG_MSG_LEN);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Override the answer's ICE inbound callback so we can drop ALL inbound packets on demand,
    // simulating a peer that has stopped acknowledging without tearing down the connection
    iceInboundPacketCallback = ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.inboundPacketFn;
    dropAllInbound = false;
    const auto maybeDropInbound = [](UINT64 customData, PBYTE pBuffer, UINT32 bufferLen) {
        {
            std::lock_guard<std::mutex> lock{dropMessageMutex};
            if (dropAllInbound) {
                return; // drop everything: the answer never SACKs, so the offer's send buffer fills
            }
        }
        iceInboundPacketCallback(customData, pBuffer, bufferLen);
    };
    ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.inboundPacketFn = maybeDropInbound;

    // Answer counts every message it receives
    static auto answerOnMessage = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        UNUSED_PARAM(pMsg);
        UNUSED_PARAM(pMsgLen);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto handle = reinterpret_cast<OnOpenHandle*>(customData);
        ATOMIC_INCREMENT(handle->datachannelRemoteOpenCount);
        DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
        // attach listener; the answer does not send anything back
        EXPECT_EQ(dataChannelOnMessage(pRtcDataChannel, (UINT64) handle->answerMsgCount, answerOnMessage), STATUS_SUCCESS);
    };

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &onOpenHandle, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", nullptr, &pOfferDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until the DataChannel is open on the answer side
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&datachannelRemoteOpenCount) != 1; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelRemoteOpenCount), 1);

    // Confirm the channel works end to end before injecting the fault
    EXPECT_EQ(dataChannelSend(pOfferDataChannel, TRUE, bigMsg, BIG_MSG_LEN), STATUS_SUCCESS);
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&answerMsgCount) != 1; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&answerMsgCount), 1);
    SIZE_T baselineMsgCount = ATOMIC_LOAD(&answerMsgCount);

    // Fault: drop ALL inbound to the answer so nothing the offer sends is ever acknowledged
    {
        std::lock_guard<std::mutex> lock{dropMessageMutex};
        dropAllInbound = true;
    }

    // Send until the SCTP send buffer is full. On a non-blocking socket usrsctp_sendv returns < 0
    // once the buffer fills, which surfaces as STATUS_SCTP_SENDV_FAILED. This is the concrete,
    // observable proof that the send buffer actually filled up.
    UINT32 enqueued = 0;
    BOOL sendBufferFull = FALSE;
    for (UINT32 i = 0; i < 5000; i++) {
        STATUS sendStatus = dataChannelSend(pOfferDataChannel, TRUE, bigMsg, BIG_MSG_LEN);
        if (sendStatus == STATUS_SUCCESS) {
            enqueued++;
        } else {
            DLOGI("Send buffer full after %u enqueued messages (status 0x%08x)", enqueued, sendStatus);
            sendBufferFull = TRUE;
            break;
        }
    }
    // VALIDATION: the send buffer must have actually filled up for this test to be meaningful
    ASSERT_TRUE(sendBufferFull) << "send buffer never filled; test precondition not met";
    ASSERT_GT(enqueued, 0u);

    // While the fault is active, none of the buffered data can reach the answer
    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    ASSERT_EQ(ATOMIC_LOAD(&answerMsgCount), baselineMsgCount) << "data should not flow while all inbound is dropped";

    // Network recovers: stop dropping. From here only the periodic SCTP timer can drive the
    // retransmission of the buffered data, since the answer sends nothing on its own.
    {
        std::lock_guard<std::mutex> lock{dropMessageMutex};
        dropAllInbound = false;
    }

    // Busy wait for all enqueued messages to drain through to the answer
    SIZE_T expected = baselineMsgCount + enqueued;
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&answerMsgCount) < expected; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&answerPc);

    // The stalled, buffered data should have fully recovered once the network came back
    ASSERT_EQ(ATOMIC_LOAD(&answerMsgCount), expected);
}

TEST_F(DataChannelFunctionalityTest, dataChannelSendRecvMessageAfterDtlsCompleted)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T pOfferRemoteDataChannel = 0, pAnswerRemoteDataChannel = 0;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    BOOL dtlsCompleted = FALSE;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) { ATOMIC_STORE((PSIZE_T)customData, reinterpret_cast<UINT64>(pRtcDataChannel)); };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &pOfferRemoteDataChannel, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &pAnswerRemoteDataChannel, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", nullptr, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until remote channel open and dtls completed
    for (auto i = 0; i <= 100 &&
         (dtlsSessionIsInitFinished(((PKvsPeerConnection) offerPc)->pDtlsSession, &dtlsCompleted) || ATOMIC_LOAD(&pOfferRemoteDataChannel) == 0);
         i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_EQ(dtlsCompleted, TRUE);
    EXPECT_TRUE(ATOMIC_LOAD(&pOfferRemoteDataChannel) != 0);

    EXPECT_EQ(dataChannelSend((PRtcDataChannel) ATOMIC_LOAD(&pOfferRemoteDataChannel), FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE,
                              STRLEN(TEST_DATA_CHANNEL_MESSAGE)),
              STATUS_SUCCESS);

    /* wait until the channel message is received */
    for (auto i = 0; i <= 5 && ATOMIC_LOAD(&msgCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    EXPECT_EQ(ATOMIC_LOAD(&msgCount), 1);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, createDataChannel_PartialReliabilityUnorderedMaxPacketLifeTimeParameterSet)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RtcDataChannelInit rtcDataChannelInit;
    PSctpSession pSctpSession = NULL;
    PKvsDataChannel pKvsDataChannel = NULL;
    RemoteOpen remoteOpen{};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Set partial reliability parameters
    NULLABLE_SET_VALUE(rtcDataChannelInit.maxPacketLifeTime, 1234);
    NULLABLE_SET_EMPTY(rtcDataChannelInit.maxRetransmits);
    rtcDataChannelInit.ordered = FALSE;

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
      auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
      DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
      std::string name(pRtcDataChannel->name);
      {
          std::lock_guard<std::mutex> lock(remoteOpen->lock);
          if (remoteOpen->channels.count(name) == 0) {
              remoteOpen->channels.emplace(name, 1u);
          } else {
              auto count = remoteOpen->channels.at(name);
              remoteOpen->channels.erase(name);
              remoteOpen->channels.emplace(name, count + 1);
          }
      }
      dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
          ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", &rtcDataChannelInit, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", NULL, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    ASSERT_EQ(pSctpSession->spa.sendv_sndinfo.snd_flags, SCTP_UNORDERED);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_policy, SCTP_PR_SCTP_TTL);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_value, rtcDataChannelInit.maxPacketLifeTime.value);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, createDataChannel_PartialReliabilityUnOrderedMaxRetransmitsParameterSet)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RtcDataChannelInit rtcDataChannelInit;
    PSctpSession pSctpSession = NULL;
    PKvsDataChannel pKvsDataChannel = NULL;
    RemoteOpen remoteOpen{};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Set partial reliability parameters
    NULLABLE_SET_VALUE(rtcDataChannelInit.maxRetransmits, 5);
    NULLABLE_SET_EMPTY(rtcDataChannelInit.maxPacketLifeTime);
    rtcDataChannelInit.ordered = FALSE;

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
      auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
      DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
      std::string name(pRtcDataChannel->name);
      {
          std::lock_guard<std::mutex> lock(remoteOpen->lock);
          if (remoteOpen->channels.count(name) == 0) {
              remoteOpen->channels.emplace(name, 1u);
          } else {
              auto count = remoteOpen->channels.at(name);
              remoteOpen->channels.erase(name);
              remoteOpen->channels.emplace(name, count + 1);
          }
      }
      dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
          ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", &rtcDataChannelInit, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", NULL, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    ASSERT_EQ(pSctpSession->spa.sendv_sndinfo.snd_flags, SCTP_UNORDERED);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_policy, SCTP_PR_SCTP_RTX);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_value, rtcDataChannelInit.maxRetransmits.value);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, createDataChannel_PartialReliabilityOrderedMaxPacketLifeTimeParameterSet)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RtcDataChannelInit rtcDataChannelInit;
    PSctpSession pSctpSession = NULL;
    PKvsDataChannel pKvsDataChannel = NULL;
    RemoteOpen remoteOpen{};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Set partial reliability parameters
    NULLABLE_SET_VALUE(rtcDataChannelInit.maxPacketLifeTime, 1234);
    NULLABLE_SET_EMPTY(rtcDataChannelInit.maxRetransmits);
    rtcDataChannelInit.ordered = TRUE;

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
      auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
      DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
      std::string name(pRtcDataChannel->name);
      {
          std::lock_guard<std::mutex> lock(remoteOpen->lock);
          if (remoteOpen->channels.count(name) == 0) {
              remoteOpen->channels.emplace(name, 1u);
          } else {
              auto count = remoteOpen->channels.at(name);
              remoteOpen->channels.erase(name);
              remoteOpen->channels.emplace(name, count + 1);
          }
      }
      dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
          ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", &rtcDataChannelInit, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", NULL, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    ASSERT_NE(pSctpSession->spa.sendv_sndinfo.snd_flags, SCTP_UNORDERED);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_policy, SCTP_PR_SCTP_TTL);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_value, rtcDataChannelInit.maxPacketLifeTime.value);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, createDataChannel_PartialReliabilityOrderedMaxRetransmitsParameterSet)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RtcDataChannelInit rtcDataChannelInit;
    PSctpSession pSctpSession = NULL;
    PKvsDataChannel pKvsDataChannel = NULL;
    RemoteOpen remoteOpen{};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Set partial reliability parameters
    NULLABLE_SET_VALUE(rtcDataChannelInit.maxRetransmits, 5);
    NULLABLE_SET_EMPTY(rtcDataChannelInit.maxPacketLifeTime);
    rtcDataChannelInit.ordered = TRUE;

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
      auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
      DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
      std::string name(pRtcDataChannel->name);
      {
          std::lock_guard<std::mutex> lock(remoteOpen->lock);
          if (remoteOpen->channels.count(name) == 0) {
              remoteOpen->channels.emplace(name, 1u);
          } else {
              auto count = remoteOpen->channels.at(name);
              remoteOpen->channels.erase(name);
              remoteOpen->channels.emplace(name, count + 1);
          }
      }
      dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
          ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", &rtcDataChannelInit, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", NULL, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;
    
    ASSERT_NE(pSctpSession->spa.sendv_sndinfo.snd_flags, SCTP_UNORDERED);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_policy, SCTP_PR_SCTP_RTX);
    ASSERT_EQ(pSctpSession->spa.sendv_prinfo.pr_value, rtcDataChannelInit.maxRetransmits.value);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, createDataChannel_DataChannelMetricsTest)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    volatile SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    RemoteOpen remoteOpen{};
    RtcStats rtcMetrics;
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_DATA_CHANNEL;

    EXPECT_EQ(rtcPeerConnectionGetMetrics(NULL, NULL, NULL), STATUS_NULL_ARG);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto remoteOpen = reinterpret_cast<RemoteOpen*>(customData);
        DLOGD("onDataChannel '%s'", pRtcDataChannel->name);
        std::string name(pRtcDataChannel->name);
        {
            std::lock_guard<std::mutex> lock(remoteOpen->lock);
            if (remoteOpen->channels.count(name) == 0) {
                remoteOpen->channels.emplace(name, 1u);
            } else {
                auto count = remoteOpen->channels.at(name);
                remoteOpen->channels.erase(name);
                remoteOpen->channels.emplace(name, count + 1);
            }
        }
        dataChannelSend(pRtcDataChannel, FALSE, (PBYTE) TEST_DATA_CHANNEL_MESSAGE, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    auto dataChannelOnMessageCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        if (STRNCMP((PCHAR) pMsg, TEST_DATA_CHANNEL_MESSAGE, pMsgLen) == 0) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(peerConnectionOnDataChannel(offerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteOpen, onDataChannel), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "Offer PeerConnection", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    rtcMetrics.rtcStatsObject.rtcDataChannelStats.dataChannelIdentifier = pOfferDataChannel->id;
    EXPECT_EQ(rtcPeerConnectionGetMetrics(offerPc, NULL, &rtcMetrics), STATUS_SUCCESS);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.state, RTC_DATA_CHANNEL_STATE_CONNECTING);
    EXPECT_EQ(createDataChannel(answerPc, (PCHAR) "Answer PeerConnection", nullptr, &pAnswerDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pAnswerDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(dataChannelOnMessage(pOfferDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnMessage(pAnswerDataChannel, (UINT64) &msgCount, dataChannelOnMessageCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Busy wait until DataChannels connect and send a message
    for (auto i = 0; i <= 100 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    EXPECT_EQ(rtcPeerConnectionGetMetrics(offerPc, NULL, &rtcMetrics), STATUS_SUCCESS);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.bytesReceived, 0);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.messagesReceived, 0);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.bytesSent, STRLEN(TEST_DATA_CHANNEL_MESSAGE));
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.messagesSent, 1);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.state, RTC_DATA_CHANNEL_STATE_OPEN);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(DataChannelFunctionalityTest, dataChannelOpen_OversizedNameIsRejected)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    UINT32 oversizedLen = MAX_DATA_CHANNEL_NAME_LEN + 100;
    BYTE oversizedName[MAX_DATA_CHANNEL_NAME_LEN + 100];
    volatile SIZE_T onDataChannelCount = 0;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(oversizedName, 'A', oversizedLen);

    EXPECT_EQ(createPeerConnection(&configuration, &pPeerConnection), STATUS_SUCCESS);
    pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    auto onDataChannelCb = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        UNUSED_PARAM(pRtcDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    EXPECT_EQ(peerConnectionOnDataChannel(pPeerConnection, (UINT64) &onDataChannelCount, onDataChannelCb), STATUS_SUCCESS);

    onSctpSessionDataChannelOpen((UINT64) pKvsPeerConnection, 0, oversizedName, oversizedLen);

    EXPECT_EQ(ATOMIC_LOAD(&onDataChannelCount), 0);

    UINT64 hashValue = 0;
    EXPECT_NE(hashTableGet(pKvsPeerConnection->pDataChannels, 0, &hashValue), STATUS_SUCCESS);

    freePeerConnection(&pPeerConnection);
}

TEST_F(DataChannelFunctionalityTest, dataChannelOpen_NormalNameIsPreserved)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    const CHAR normalName[] = "my-data-channel";
    UINT32 nameLen = STRLEN(normalName);
    volatile SIZE_T onDataChannelCount = 0;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &pPeerConnection), STATUS_SUCCESS);
    pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    auto onDataChannelCb = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        UNUSED_PARAM(pRtcDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    EXPECT_EQ(peerConnectionOnDataChannel(pPeerConnection, (UINT64) &onDataChannelCount, onDataChannelCb), STATUS_SUCCESS);

    onSctpSessionDataChannelOpen((UINT64) pKvsPeerConnection, 0, (PBYTE) normalName, nameLen);

    EXPECT_EQ(ATOMIC_LOAD(&onDataChannelCount), 1);

    UINT64 hashValue = 0;
    EXPECT_EQ(hashTableGet(pKvsPeerConnection->pDataChannels, 0, &hashValue), STATUS_SUCCESS);
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) hashValue;

    EXPECT_EQ(STRLEN(pKvsDataChannel->dataChannel.name), nameLen);
    EXPECT_EQ(STRNCMP(pKvsDataChannel->dataChannel.name, normalName, nameLen), 0);
    EXPECT_EQ(STRNCMP(pKvsDataChannel->rtcDataChannelDiagnostics.label, normalName, nameLen), 0);

    freePeerConnection(&pPeerConnection);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
