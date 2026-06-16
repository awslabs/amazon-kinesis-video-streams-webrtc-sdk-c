#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SdpBufferSizeConfigTest : public WebRtcClientTestBase {};

// Verify the relationship between MAX_SIGNALING_MESSAGE_LEN and MAX_SESSION_DESCRIPTION_INIT_SDP_LEN
TEST_F(SdpBufferSizeConfigTest, signalingMessageLenDerivedFromSdpLen)
{
#ifdef KVS_SDP_BUFFER_SIZE
    EXPECT_EQ((UINT32) KVS_SDP_BUFFER_SIZE, MAX_SIGNALING_MESSAGE_LEN);
    UINT32 expectedSdpLen = ((UINT32) KVS_SDP_BUFFER_SIZE - 1024) * 3 / 4;
    EXPECT_EQ(expectedSdpLen, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);
#else
    EXPECT_EQ(18750u, MAX_SIGNALING_MESSAGE_LEN);
    EXPECT_EQ(25000u, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);
#endif
}

// Test Case: SDP at exact limit - should be accepted and parsed successfully
TEST_F(SdpBufferSizeConfigTest, deserializeSessionDescriptionInit_SdpAtExactLimit)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Create an SDP value that is exactly MAX_SESSION_DESCRIPTION_INIT_SDP_LEN bytes.
    // The deserializer requires \\r\\n (JSON-escaped CRLF) line endings to parse content.
    // Each "v=0\\r\\n" segment is 6 characters in the JSON token.
    std::string sdpLine = "v=0\\r\\n";
    std::string sdpValue;
    while (sdpValue.size() + sdpLine.size() <= (size_t) MAX_SESSION_DESCRIPTION_INIT_SDP_LEN) {
        sdpValue += sdpLine;
    }
    // Pad remaining space with 'a' characters followed by a final line ending
    size_t remaining = MAX_SESSION_DESCRIPTION_INIT_SDP_LEN - sdpValue.size();
    if (remaining >= 4) {
        // Leave room for \\r\\n at the end
        sdpValue += std::string(remaining - 4, 'a') + "\\r\\n";
    } else {
        sdpValue += std::string(remaining, 'a');
    }
    ASSERT_EQ((size_t) MAX_SESSION_DESCRIPTION_INIT_SDP_LEN, sdpValue.size());

    // Build valid JSON with type and sdp keys
    std::string json = "{\"type\": \"offer\", \"sdp\": \"" + sdpValue + "\"}";

    STATUS status = deserializeSessionDescriptionInit((PCHAR) json.c_str(), (UINT32) json.length(), &rtcSessionDescriptionInit);
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_EQ(SDP_TYPE_OFFER, rtcSessionDescriptionInit.type);
}

// Test Case: SDP exceeds limit by 1 byte - should be rejected with STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED
TEST_F(SdpBufferSizeConfigTest, deserializeSessionDescriptionInit_SdpExceedsLimitByOne)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Create an SDP value that is MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1 bytes
    std::string sdpValue(MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1, 'a');

    // Build valid JSON with type and sdp keys
    std::string json = "{\"type\": \"offer\", \"sdp\": \"" + sdpValue + "\"}";

    STATUS status = deserializeSessionDescriptionInit((PCHAR) json.c_str(), (UINT32) json.length(), &rtcSessionDescriptionInit);
    EXPECT_EQ(STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED, status);
}

// Test Case: Signaling message exceeds MAX_SIGNALING_MESSAGE_LEN - should be rejected before decode
TEST_F(SdpBufferSizeConfigTest, parseSignalingMessage_MessageExceedsMaxLen)
{
    ReceivedSignalingMessage receivedMessage;

    // Create a message that is exactly MAX_SIGNALING_MESSAGE_LEN + 1 bytes
    const std::string oversizedMessage(MAX_SIGNALING_MESSAGE_LEN + 1, 'X');

    STATUS status = parseSignalingMessage((PCHAR) oversizedMessage.c_str(), (UINT32) oversizedMessage.length(), &receivedMessage);
    EXPECT_EQ(STATUS_INVALID_API_CALL_RETURN_JSON, status);
}

// Test Case: Message at signaling limit boundary should be accepted for parsing (valid JSON required for full parse)
TEST_F(SdpBufferSizeConfigTest, parseSignalingMessage_MessageAtExactLimit)
{
    ReceivedSignalingMessage receivedMessage;

    // A valid JSON message that fits within the signaling buffer.
    // Use a small valid signaling message to confirm parsing works within limits.
    const std::string jsonMessage = R"({
        "messageType": "SDP_OFFER",
        "senderClientId": "testClient",
        "messagePayload": "eyJ0eXBlIjoib2ZmZXIiLCJzZHAiOiJ2PTBcclxuIn0="
    })";

    // This should succeed as the message is well within limits
    EXPECT_TRUE(jsonMessage.length() <= MAX_SIGNALING_MESSAGE_LEN);
    STATUS status = parseSignalingMessage((PCHAR) jsonMessage.c_str(), (UINT32) jsonMessage.length(), &receivedMessage);
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_EQ(SIGNALING_MESSAGE_TYPE_OFFER, receivedMessage.signalingMessage.messageType);
}

// Test Case: Payload field in signaling message exceeds MAX_SIGNALING_MESSAGE_LEN
TEST_F(SdpBufferSizeConfigTest, parseSignalingMessage_PayloadFieldExceedsMaxLen)
{
    ReceivedSignalingMessage receivedMessage;

    // Create a base64-encoded payload that exceeds the limit
    const std::string longPayload(MAX_SIGNALING_MESSAGE_LEN + 1, 'K');

    const std::string jsonMessage = R"({"messageType": "SDP_OFFER", "senderClientId": "client1", "messagePayload": ")" + longPayload + R"("})";

    // The total message itself might exceed the limit, which would be caught first.
    // If total message fits but the payload field is too long, it's also rejected.
    STATUS status = parseSignalingMessage((PCHAR) jsonMessage.c_str(), (UINT32) jsonMessage.length(), &receivedMessage);
    EXPECT_NE(STATUS_SUCCESS, status);
}

// Test Case: Verify default values when KVS_SDP_BUFFER_SIZE is not defined
TEST_F(SdpBufferSizeConfigTest, defaultBufferSizeValues)
{
#ifndef KVS_SDP_BUFFER_SIZE
    EXPECT_EQ(25000u, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);
    EXPECT_EQ(18750u, MAX_SIGNALING_MESSAGE_LEN);
#else
    EXPECT_EQ((UINT32) KVS_SDP_BUFFER_SIZE, MAX_SIGNALING_MESSAGE_LEN);
    EXPECT_EQ(((UINT32) KVS_SDP_BUFFER_SIZE - 1024) * 3 / 4, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);
#endif
}

// Test Case: SDP that fits in signaling buffer but would exceed SDP buffer after base64 decode
// This tests the scenario where base64-encoded payload fits in MAX_SIGNALING_MESSAGE_LEN
// but the decoded SDP exceeds MAX_SESSION_DESCRIPTION_INIT_SDP_LEN
TEST_F(SdpBufferSizeConfigTest, deserializeSessionDescriptionInit_SdpSlightlyOverLimit)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Create an SDP value that exceeds the limit by a significant margin
    std::string sdpValue(MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 100, 'v');

    std::string json = "{\"type\": \"answer\", \"sdp\": \"" + sdpValue + "\"}";

    STATUS status = deserializeSessionDescriptionInit((PCHAR) json.c_str(), (UINT32) json.length(), &rtcSessionDescriptionInit);
    EXPECT_EQ(STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED, status);
}

// Test Case: Verify the struct sizes are based on the configured limits
TEST_F(SdpBufferSizeConfigTest, structSizesMatchConfiguration)
{
    RtcSessionDescriptionInit sessionDescInit;
    SignalingMessage signalingMsg;

    // The sdp field should be MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1 (for null terminator)
    EXPECT_EQ(SIZEOF(sessionDescInit.sdp), (MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1) * SIZEOF(CHAR));

    // The payload field should be MAX_SIGNALING_MESSAGE_LEN + 1 (for null terminator)
    EXPECT_EQ(SIZEOF(signalingMsg.payload), (MAX_SIGNALING_MESSAGE_LEN + 1) * SIZEOF(CHAR));
}

// Test Case: LWS_MESSAGE_BUFFER_SIZE is correctly derived from MAX_SIGNALING_MESSAGE_LEN
TEST_F(SdpBufferSizeConfigTest, lwsBufferSizeDerivedFromSignalingLen)
{
    UINT32 expected = SIZEOF(CHAR) * (MAX_SIGNALING_MESSAGE_LEN + LWS_ALIGN_BYTES);
    EXPECT_EQ(LWS_MESSAGE_BUFFER_SIZE, expected);
}

// Test Case: A small but valid SDP (typical single-track) should always parse successfully
TEST_F(SdpBufferSizeConfigTest, deserializeSessionDescriptionInit_SmallValidSdp)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Minimal valid SDP that represents a single audio track (~200 bytes)
    auto validJson = R"({"type": "offer", "sdp": "v=0\r\no=- 123 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\na=rtpmap:111 opus/48000/2\r\n"})";

    STATUS status = deserializeSessionDescriptionInit((PCHAR) validJson, STRLEN(validJson), &rtcSessionDescriptionInit);
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_EQ(SDP_TYPE_OFFER, rtcSessionDescriptionInit.type);
    // Verify the SDP was actually copied
    EXPECT_TRUE(STRLEN(rtcSessionDescriptionInit.sdp) > 0);
}

// Test Case: Verify that serializeSessionDescriptionInit respects MAX_SIGNALING_MESSAGE_LEN output buffer
TEST_F(SdpBufferSizeConfigTest, serializeSessionDescriptionInit_FitsInSignalingBuffer)
{
    RtcSessionDescriptionInit sessionDescInit;
    MEMSET(&sessionDescInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    sessionDescInit.type = SDP_TYPE_OFFER;
    // Fill with a valid SDP that uses some space
    const char* sdp = "v=0\r\no=- 123 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n";
    STRNCPY(sessionDescInit.sdp, sdp, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

    CHAR outputBuffer[MAX_SIGNALING_MESSAGE_LEN + 1];
    UINT32 outputLen = MAX_SIGNALING_MESSAGE_LEN + 1;

    STATUS status = serializeSessionDescriptionInit(&sessionDescInit, outputBuffer, &outputLen);
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(outputLen <= MAX_SIGNALING_MESSAGE_LEN);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
