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

    initRtcConfiguration(&configuration);

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

TEST_F(DataChannelFunctionalityTest, dataChannelSendRecvMessageAfterDtlsCompleted)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr, pAnswerDataChannel = nullptr;
    SIZE_T pOfferRemoteDataChannel = 0, pAnswerRemoteDataChannel = 0;
    SIZE_T datachannelLocalOpenCount = 0, msgCount = 0;
    BOOL dtlsCompleted = FALSE;

    initRtcConfiguration(&configuration);

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

    initRtcConfiguration(&configuration);

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
    for (auto i = 0; i <= 10 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_RELIABLE_UNORDERED, 0);
    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_TIMED, 0);
    ASSERT_EQ(getUnalignedInt32BigEndian((PINT32)(pSctpSession->packet + SIZEOF(UINT32))), rtcDataChannelInit.maxPacketLifeTime.value);

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

    initRtcConfiguration(&configuration);

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
    for (auto i = 0; i <= 10 && (ATOMIC_LOAD(&datachannelLocalOpenCount) + ATOMIC_LOAD(&msgCount)) != 4 ; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Close the connection to avoid data race while accessing SctpSession
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    pKvsDataChannel = (PKvsDataChannel) pOfferDataChannel;
    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_RELIABLE_UNORDERED, 0);
    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_REXMIT, 0);
    ASSERT_EQ(getUnalignedInt32BigEndian((PINT32)(pSctpSession->packet + SIZEOF(UINT32))), rtcDataChannelInit.maxRetransmits.value);

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

    initRtcConfiguration(&configuration);

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

    ASSERT_EQ(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_RELIABLE_UNORDERED, 0);
    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_TIMED, 0);
    ASSERT_EQ(getUnalignedInt32BigEndian((PINT32)(pSctpSession->packet + SIZEOF(UINT32))), rtcDataChannelInit.maxPacketLifeTime.value);

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

    initRtcConfiguration(&configuration);

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
    
    ASSERT_EQ(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_RELIABLE_UNORDERED, 0);
    ASSERT_NE(pSctpSession->packet[1] & DCEP_DATA_CHANNEL_REXMIT, 0);
    ASSERT_EQ(getUnalignedInt32BigEndian((PINT32)(pSctpSession->packet + SIZEOF(UINT32))), rtcDataChannelInit.maxRetransmits.value);

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

    initRtcConfiguration(&configuration);

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

// Context struct for registering message callback inside onDataChannel to avoid TSAN race.
// The onMessage handler must be set before any messages can arrive on the channel.
struct RemoteChannelCtx {
    SIZE_T remoteDataChannel;
    UINT64 msgCustomData;
    RtcOnMessage msgCallback;
};

// Test byte-exact transmission with varied payload sizes and binary/string modes
TEST_F(DataChannelFunctionalityTest, dataChannelSendRecvVariedPayloadSizes)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0;
    RtcStats rtcMetrics;
    MEMSET(&rtcMetrics, 0x00, SIZEOF(RtcStats));
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_DATA_CHANNEL;

    // Track received messages: store (length, isBinary, first-byte) tuples
    struct RecvState {
        std::mutex lock;
        std::vector<std::tuple<UINT32, BOOL, BYTE>> received;
    };
    RecvState recvState{};

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Register message callback inside onDataChannel to avoid race with connection listener thread
    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto ctx = reinterpret_cast<RemoteChannelCtx*>(customData);
        dataChannelOnMessage(pRtcDataChannel, ctx->msgCustomData, ctx->msgCallback);
        ATOMIC_STORE((PSIZE_T) &ctx->remoteDataChannel, reinterpret_cast<UINT64>(pRtcDataChannel));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RtcOnMessage recvMsgCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        auto state = reinterpret_cast<RecvState*>(customData);
        std::lock_guard<std::mutex> guard(state->lock);
        BYTE firstByte = (pMsgLen > 0) ? pMsg[0] : 0;
        state->received.emplace_back(pMsgLen, isBinary, firstByte);
    };

    RemoteChannelCtx remoteCtx{};
    remoteCtx.msgCustomData = (UINT64) &recvState;
    remoteCtx.msgCallback = recvMsgCallback;

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteCtx, onDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "TestChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Set metrics identifier AFTER connection — allocateSctp() re-keys the hash table
    // using SCTP stream IDs (channelId), which differ from the pre-connection id
    rtcMetrics.rtcStatsObject.rtcDataChannelStats.dataChannelIdentifier = ((PKvsDataChannel) pOfferDataChannel)->channelId;

    // Wait for DataChannel to open
    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&datachannelLocalOpenCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 1);

    // Wait for remote data channel to appear (message callback already registered in onDataChannel)
    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&remoteCtx.remoteDataChannel) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_NE(ATOMIC_LOAD(&remoteCtx.remoteDataChannel), 0);

    // Send payloads of varied sizes: 1, 64, 1024, 8192 bytes
    UINT32 sizes[] = {1, 64, 1024, 8192};
    UINT32 totalBytesSent = 0;
    UINT32 totalMessagesSent = 0;

    for (auto size : sizes) {
        std::vector<BYTE> payload(size);
        // Fill with a recognizable pattern: first byte = size & 0xFF
        MEMSET(payload.data(), (BYTE)(size & 0xFF), size);

        // Send as string (isBinary = FALSE)
        EXPECT_EQ(dataChannelSend(pOfferDataChannel, FALSE, payload.data(), size), STATUS_SUCCESS);
        totalBytesSent += size;
        totalMessagesSent++;

        // Send as binary (isBinary = TRUE)
        payload[0] = (BYTE)((size & 0xFF) + 1); // Distinct marker for binary
        EXPECT_EQ(dataChannelSend(pOfferDataChannel, TRUE, payload.data(), size), STATUS_SUCCESS);
        totalBytesSent += size;
        totalMessagesSent++;
    }

    // Wait for all messages to arrive (8 total: 4 sizes x 2 modes)
    for (auto i = 0; i <= 30; i++) {
        {
            std::lock_guard<std::mutex> guard(recvState.lock);
            if (recvState.received.size() >= 8) {
                break;
            }
        }
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Verify metrics
    EXPECT_EQ(rtcPeerConnectionGetMetrics(offerPc, NULL, &rtcMetrics), STATUS_SUCCESS);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.messagesSent, totalMessagesSent);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.bytesSent, totalBytesSent);
    EXPECT_EQ(rtcMetrics.rtcStatsObject.rtcDataChannelStats.state, RTC_DATA_CHANNEL_STATE_OPEN);

    {
        std::lock_guard<std::mutex> guard(recvState.lock);
        ASSERT_EQ(recvState.received.size(), 8);

        // Verify sizes arrived correctly (string then binary for each size)
        for (UINT32 idx = 0; idx < 4; idx++) {
            UINT32 expectedSize = sizes[idx];
            // String message
            EXPECT_EQ(std::get<0>(recvState.received[idx * 2]), expectedSize);
            // Binary message
            EXPECT_EQ(std::get<0>(recvState.received[idx * 2 + 1]), expectedSize);
        }
    }

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Flood the DataChannel with rapid sends to verify no crashes, deadlocks, or leaks
TEST_F(DataChannelFunctionalityTest, dataChannelFloodSend)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0;
    SIZE_T msgCount = 0;

    static constexpr UINT32 FLOOD_MESSAGE_COUNT = 1000;
    static constexpr UINT32 FLOOD_MESSAGE_SIZE = 32;
    BYTE floodPayload[FLOOD_MESSAGE_SIZE];
    MEMSET(floodPayload, 0xAB, FLOOD_MESSAGE_SIZE);

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Register message callback inside onDataChannel to avoid race with connection listener thread
    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto ctx = reinterpret_cast<RemoteChannelCtx*>(customData);
        dataChannelOnMessage(pRtcDataChannel, ctx->msgCustomData, ctx->msgCallback);
        ATOMIC_STORE((PSIZE_T) &ctx->remoteDataChannel, reinterpret_cast<UINT64>(pRtcDataChannel));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RtcOnMessage countMsgCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        UNUSED_PARAM(pMsg);
        UNUSED_PARAM(pMsgLen);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RemoteChannelCtx remoteCtx{};
    remoteCtx.msgCustomData = (UINT64) &msgCount;
    remoteCtx.msgCallback = countMsgCallback;

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteCtx, onDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "FloodChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Wait for channel to open
    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&datachannelLocalOpenCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 1);

    // Wait for remote data channel (message callback already registered in onDataChannel)
    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&remoteCtx.remoteDataChannel) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_NE(ATOMIC_LOAD(&remoteCtx.remoteDataChannel), 0);

    // Flood: send 1000 messages in a tight loop
    UINT32 sendFailures = 0;
    for (UINT32 i = 0; i < FLOOD_MESSAGE_COUNT; i++) {
        STATUS status = dataChannelSend(pOfferDataChannel, FALSE, floodPayload, FLOOD_MESSAGE_SIZE);
        if (status != STATUS_SUCCESS) {
            sendFailures++;
        }
    }

    // Wait for messages to be received (up to 30 seconds)
    for (auto i = 0; i <= 30 && ATOMIC_LOAD(&msgCount) < FLOOD_MESSAGE_COUNT; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // All sends should succeed
    EXPECT_EQ(sendFailures, 0);
    // All messages should arrive
    EXPECT_EQ(ATOMIC_LOAD(&msgCount), FLOOD_MESSAGE_COUNT);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Multiple threads sending concurrently on the same DataChannel
TEST_F(DataChannelFunctionalityTest, dataChannelConcurrentSends)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0;
    SIZE_T msgCount = 0;

    static constexpr UINT32 NUM_THREADS = 4;
    static constexpr UINT32 MESSAGES_PER_THREAD = 250;
    static constexpr UINT32 TOTAL_MESSAGES = NUM_THREADS * MESSAGES_PER_THREAD;
    static constexpr UINT32 MSG_SIZE = 32;

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Register message callback inside onDataChannel to avoid race with connection listener thread
    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto ctx = reinterpret_cast<RemoteChannelCtx*>(customData);
        dataChannelOnMessage(pRtcDataChannel, ctx->msgCustomData, ctx->msgCallback);
        ATOMIC_STORE((PSIZE_T) &ctx->remoteDataChannel, reinterpret_cast<UINT64>(pRtcDataChannel));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RtcOnMessage countMsgCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        UNUSED_PARAM(pMsg);
        UNUSED_PARAM(pMsgLen);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RemoteChannelCtx remoteCtx{};
    remoteCtx.msgCustomData = (UINT64) &msgCount;
    remoteCtx.msgCallback = countMsgCallback;

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteCtx, onDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "ConcurrentChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&datachannelLocalOpenCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 1);

    // Wait for remote data channel (message callback already registered in onDataChannel)
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&remoteCtx.remoteDataChannel) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_NE(ATOMIC_LOAD(&remoteCtx.remoteDataChannel), 0);

    // Spawn threads that all send on the same DataChannel
    std::atomic<UINT32> totalSendFailures{0};
    std::vector<std::thread> senderThreads;

    for (UINT32 t = 0; t < NUM_THREADS; t++) {
        senderThreads.emplace_back([&, t]() {
            BYTE payload[MSG_SIZE];
            // Each thread fills with a distinct marker byte
            MEMSET(payload, (BYTE)(t + 1), MSG_SIZE);
            for (UINT32 i = 0; i < MESSAGES_PER_THREAD; i++) {
                STATUS status = dataChannelSend(pOfferDataChannel, FALSE, payload, MSG_SIZE);
                if (status != STATUS_SUCCESS) {
                    totalSendFailures.fetch_add(1);
                }
            }
        });
    }

    // Wait for all sender threads to complete
    for (auto& th : senderThreads) {
        th.join();
    }

    // Wait for all messages to arrive (up to 30 seconds)
    for (auto i = 0; i <= 30 && ATOMIC_LOAD(&msgCount) < TOTAL_MESSAGES; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_EQ(totalSendFailures.load(), 0);
    EXPECT_EQ(ATOMIC_LOAD(&msgCount), TOTAL_MESSAGES);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Close the remote peer while a background thread is actively sending
TEST_F(DataChannelFunctionalityTest, dataChannelSendDuringDisconnect)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0;
    SIZE_T remoteDataChannel = 0;

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        ATOMIC_STORE((PSIZE_T) customData, reinterpret_cast<UINT64>(pRtcDataChannel));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteDataChannel, onDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "DisconnectChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&datachannelLocalOpenCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 1);

    // Spawn a thread that sends in a tight loop
    std::atomic<bool> stopSending{false};
    std::atomic<UINT32> sendCount{0};

    std::thread senderThread([&]() {
        BYTE payload[32];
        MEMSET(payload, 0xCC, 32);
        while (!stopSending.load()) {
            // Ignore return status — we expect failures after close
            dataChannelSend(pOfferDataChannel, FALSE, payload, 32);
            sendCount.fetch_add(1);
        }
    });

    // Let sender run briefly, then close the answer peer
    THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    closePeerConnection(answerPc);

    // Let sender continue briefly after close to exercise the error path
    THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    stopSending.store(true);

    senderThread.join();

    // The key assertion: we got here without crashing or deadlocking
    EXPECT_GT(sendCount.load(), 0u);

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Send on a DataChannel before the peer connection is established
TEST_F(DataChannelFunctionalityTest, dataChannelSendBeforeConnect)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "PreConnectChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);

    // pSctpSession is NULL before connection — send should fail cleanly
    BYTE payload[] = "test payload";
    STATUS sendStatus = dataChannelSend(pOfferDataChannel, FALSE, payload, SIZEOF(payload) - 1);
    EXPECT_NE(sendStatus, STATUS_SUCCESS);

    // Verify we didn't crash
    freePeerConnection(&offerPc);
}

// Send on a DataChannel after the peer connection has been closed
TEST_F(DataChannelFunctionalityTest, dataChannelSendAfterClose)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PRtcDataChannel pOfferDataChannel = nullptr;
    SIZE_T datachannelLocalOpenCount = 0;
    SIZE_T msgCount = 0;

    initRtcConfiguration(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Register message callback inside onDataChannel to avoid race with connection listener thread
    auto onDataChannel = [](UINT64 customData, PRtcDataChannel pRtcDataChannel) {
        auto ctx = reinterpret_cast<RemoteChannelCtx*>(customData);
        dataChannelOnMessage(pRtcDataChannel, ctx->msgCustomData, ctx->msgCallback);
        ATOMIC_STORE((PSIZE_T) &ctx->remoteDataChannel, reinterpret_cast<UINT64>(pRtcDataChannel));
    };

    auto dataChannelOnOpenCallback = [](UINT64 customData, PRtcDataChannel pDataChannel) {
        UNUSED_PARAM(pDataChannel);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RtcOnMessage countMsgCallback = [](UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMsg, UINT32 pMsgLen) {
        UNUSED_PARAM(pDataChannel);
        UNUSED_PARAM(isBinary);
        UNUSED_PARAM(pMsg);
        UNUSED_PARAM(pMsgLen);
        ATOMIC_INCREMENT((PSIZE_T) customData);
    };

    RemoteChannelCtx remoteCtx{};
    remoteCtx.msgCustomData = (UINT64) &msgCount;
    remoteCtx.msgCallback = countMsgCallback;

    EXPECT_EQ(peerConnectionOnDataChannel(answerPc, (UINT64) &remoteCtx, onDataChannel), STATUS_SUCCESS);

    EXPECT_EQ(createDataChannel(offerPc, (PCHAR) "PostCloseChannel", nullptr, &pOfferDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(dataChannelOnOpen(pOfferDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Wait for channel to open
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&datachannelLocalOpenCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_EQ(ATOMIC_LOAD(&datachannelLocalOpenCount), 1);

    // Wait for remote channel (message callback already registered in onDataChannel)
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&remoteCtx.remoteDataChannel) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    ASSERT_NE(ATOMIC_LOAD(&remoteCtx.remoteDataChannel), 0);

    // Confirm a message works before close
    BYTE payload[] = "pre-close message";
    EXPECT_EQ(dataChannelSend(pOfferDataChannel, FALSE, payload, SIZEOF(payload) - 1), STATUS_SUCCESS);

    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&msgCount) == 0; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    EXPECT_EQ(ATOMIC_LOAD(&msgCount), 1);

    // Close the sender's peer connection
    closePeerConnection(offerPc);

    // Attempt to send after close — should not crash
    BYTE postClosePayload[] = "post-close message";
    dataChannelSend(pOfferDataChannel, FALSE, postClosePayload, SIZEOF(postClosePayload) - 1);

    // The key assertion: we got here without crashing
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
