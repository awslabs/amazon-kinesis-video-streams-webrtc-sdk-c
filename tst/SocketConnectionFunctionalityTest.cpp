#include <atomic>

#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SocketConnectionFunctionalityTest : public WebRtcClientTestBase {};

static VOID setLocalhost(PKvsIpAddress pAddress)
{
    MEMSET(pAddress, 0x00, SIZEOF(KvsIpAddress));
    pAddress->family = KVS_IP_FAMILY_TYPE_IPV4;
    pAddress->isPointToPoint = FALSE;
    // 127.0.0.1
    pAddress->address[0] = 0x7f;
    pAddress->address[3] = 0x01;
    pAddress->port = 0;
}

// A socket the connection listener holds past the free timeout is freed by the listener when it releases it.
TEST_F(SocketConnectionFunctionalityTest, freeHandsHeldSocketToListener)
{
    KvsIpAddress localhost;
    PSocketConnection pSocketConnection = NULL, pHeld = NULL;

    setLocalhost(&localhost);
    ASSERT_EQ(STATUS_SUCCESS,
              createSocketConnection(KVS_IP_FAMILY_TYPE_IPV4, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, 0, NULL, 0, &pSocketConnection));
    pHeld = pSocketConnection;

    // What the listener does when it gathers the socket for a poll
    ATOMIC_STORE(&pHeld->inUse, SOCKET_CONNECTION_IN_USE);

    // The free waits for the listener, times out, and leaves the socket to the listener
    EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSocketConnection));
    EXPECT_EQ(NULL, pSocketConnection);
    EXPECT_EQ(SOCKET_CONNECTION_ORPHANED, ATOMIC_LOAD(&pHeld->inUse));
    EXPECT_TRUE(socketConnectionIsClosed(pHeld));

    // The listener releases the socket and frees it. A sanitizer build reports a leak if it does not.
    socketConnectionRelease(pHeld);
}

// A socket the listener releases in time goes back to idle and the free closes it as before.
TEST_F(SocketConnectionFunctionalityTest, releaseBeforeFreeKeepsSocket)
{
    KvsIpAddress localhost;
    PSocketConnection pSocketConnection = NULL;

    setLocalhost(&localhost);
    ASSERT_EQ(STATUS_SUCCESS,
              createSocketConnection(KVS_IP_FAMILY_TYPE_IPV4, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, 0, NULL, 0, &pSocketConnection));

    ATOMIC_STORE(&pSocketConnection->inUse, SOCKET_CONNECTION_IN_USE);
    socketConnectionRelease(pSocketConnection);
    EXPECT_EQ(SOCKET_CONNECTION_IDLE, ATOMIC_LOAD(&pSocketConnection->inUse));

    EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSocketConnection));
    EXPECT_EQ(NULL, pSocketConnection);
}

typedef struct {
    std::atomic<bool> entered;
    std::atomic<bool> release;
    std::atomic<bool> returned;
    INT32 socketAfterBlock;
} BlockingReceiver;

static STATUS blockingOnData(UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                             PKvsIpAddress pDest)
{
    BlockingReceiver* pReceiver = (BlockingReceiver*) customData;
    UNUSED_PARAM(pBuffer);
    UNUSED_PARAM(bufferLen);
    UNUSED_PARAM(pSrc);
    UNUSED_PARAM(pDest);

    pReceiver->entered = true;
    while (!pReceiver->release) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    // The socket must still be valid while the listener runs its callback. A free that does not wait
    // for the listener makes this a heap-use-after-free, which AddressSanitizer reports.
    pReceiver->socketAfterBlock = pSocketConnection->localSocket;
    pReceiver->returned = true;
    return STATUS_SUCCESS;
}

// The listener is inside a receive callback for longer than the free timeout while the owner frees the socket.
// Before the hand-off, the free released the socket under the listener, which then used freed memory.
TEST_F(SocketConnectionFunctionalityTest, freeWhileListenerInReceiveCallback)
{
    KvsIpAddress localhost;
    PConnectionListener pConnectionListener = NULL;
    PSocketConnection pReceiverSocket = NULL, pSenderSocket = NULL;
    BlockingReceiver receiver;
    BYTE packet[16] = {0};
    UINT64 deadline;

    receiver.entered = false;
    receiver.release = false;
    receiver.returned = false;
    receiver.socketAfterBlock = -1;

    setLocalhost(&localhost);
    EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
    EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
    ASSERT_EQ(STATUS_SUCCESS,
              createSocketConnection(KVS_IP_FAMILY_TYPE_IPV4, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, (UINT64) &receiver, blockingOnData, 0,
                                     &pReceiverSocket));
    ATOMIC_STORE_BOOL(&pReceiverSocket->receiveData, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pReceiverSocket));
    ASSERT_EQ(STATUS_SUCCESS, createSocketConnection(KVS_IP_FAMILY_TYPE_IPV4, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, 0, NULL, 0, &pSenderSocket));

    // Send until the listener picks a datagram up and blocks in the callback
    deadline = GETTIME() + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!receiver.entered && GETTIME() < deadline) {
        socketConnectionSendData(pSenderSocket, packet, SIZEOF(packet), &pReceiverSocket->hostIpAddr);
        THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    ASSERT_TRUE(receiver.entered);

    // The owner removes and frees the socket. The free times out after one second and hands the socket to the listener.
    EXPECT_EQ(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, pReceiverSocket));
    EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pReceiverSocket));
    EXPECT_EQ(NULL, pReceiverSocket);

    // The callback returns and the listener releases, then frees, the socket
    receiver.release = true;
    deadline = GETTIME() + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!receiver.returned && GETTIME() < deadline) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    EXPECT_TRUE(receiver.returned);

    // freeConnectionListener joins the listener thread, so its release has run before the test ends
    EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
    EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSenderSocket));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
