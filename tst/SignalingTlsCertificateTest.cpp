#include "WebRTCClientTestFixture.h"

// These tests check that the signaling client rejects a bad server certificate.
// The signaling client makes every control plane and data plane HTTPS call, and
// the signaling WSS connection, through libwebsockets. libwebsockets verifies
// the server certificate with the crypto library the SDK was built against, so
// these tests run against OpenSSL and AWS-LC alike.
//
// Each test starts a local TLS server on 127.0.0.1:443 and [::1]:443 and points
// the control plane URL at https://localhost. The signaling client always
// connects to port 443, so the server cannot use a free high port. When the
// port cannot be opened, for example when the tests do not run as root on
// Linux, the tests are skipped.
//
// The certificates are made at run time:
//
//   valid      signed by the trusted test CA, DNS name localhost, valid now
//   untrusted  signed by a second CA that is not in the trust file
//   selfSigned self-signed, DNS name localhost
//   expired    signed by the trusted test CA, expired a day ago
//   wrongHost  signed by the trusted test CA, DNS name wrong.host.invalid
//
// The trust file holds only the trusted test CA. Each bad certificate differs
// from the valid one in one property, and the valid one connects. So when a
// bad certificate fails, it fails because of that property, not because the
// server or the network is broken.

#if defined(KVS_USE_OPENSSL) && !defined(_WIN32)

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

namespace {

const UINT16 kSignalingPort = 443;
const char kTrustedHost[] = "localhost";
const char kOtherHost[] = "wrong.host.invalid";
const long kOneDaySeconds = 24 * 60 * 60;

EVP_PKEY* makeP256Key()
{
    EVP_PKEY* pKey = NULL;
    EVP_PKEY_CTX* pCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (pCtx != NULL && EVP_PKEY_keygen_init(pCtx) == 1 && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pCtx, NID_X9_62_prime256v1) == 1) {
        EVP_PKEY_keygen(pCtx, &pKey);
    }
    EVP_PKEY_CTX_free(pCtx);
    return pKey;
}

BOOL addExtension(X509* pCert, X509* pIssuer, int nid, const char* value)
{
    X509V3_CTX ctx;
    X509_EXTENSION* pExt;
    BOOL added = FALSE;

    X509V3_set_ctx(&ctx, pIssuer, pCert, NULL, NULL, 0);
    pExt = X509V3_EXT_nconf_nid(NULL, &ctx, nid, value);
    if (pExt != NULL) {
        added = X509_add_ext(pCert, pExt, -1) == 1;
        X509_EXTENSION_free(pExt);
    }
    return added;
}

// Makes a certificate for pKey. A NULL pIssuer makes it self-signed. pDnsName
// is the subjectAltName DNS entry, or NULL for a CA.
X509* makeCert(EVP_PKEY* pKey, const char* pCommonName, const char* pDnsName, X509* pIssuer, EVP_PKEY* pIssuerKey, BOOL isCa,
               long notBeforeOffsetSeconds, long notAfterOffsetSeconds, long serial)
{
    X509* pCert = X509_new();
    X509_NAME* pName;
    BOOL ok = pCert != NULL;
    std::string san;

    ok = ok && X509_set_version(pCert, 2) == 1;
    ok = ok && ASN1_INTEGER_set(X509_get_serialNumber(pCert), serial) == 1;
    ok = ok && X509_gmtime_adj(X509_getm_notBefore(pCert), notBeforeOffsetSeconds) != NULL;
    ok = ok && X509_gmtime_adj(X509_getm_notAfter(pCert), notAfterOffsetSeconds) != NULL;
    ok = ok && X509_set_pubkey(pCert, pKey) == 1;
    if (ok) {
        pName = X509_get_subject_name(pCert);
        ok = X509_NAME_add_entry_by_txt(pName, "CN", MBSTRING_ASC, (const unsigned char*) pCommonName, -1, -1, 0) == 1;
        ok = ok && X509_set_issuer_name(pCert, pIssuer != NULL ? X509_get_subject_name(pIssuer) : pName) == 1;
    }

    X509* pSigner = pIssuer != NULL ? pIssuer : pCert;
    if (isCa) {
        ok = ok && addExtension(pCert, pSigner, NID_basic_constraints, "critical,CA:TRUE");
        ok = ok && addExtension(pCert, pSigner, NID_key_usage, "critical,keyCertSign,cRLSign");
    } else {
        ok = ok && addExtension(pCert, pSigner, NID_basic_constraints, "critical,CA:FALSE");
        ok = ok && addExtension(pCert, pSigner, NID_key_usage, "critical,digitalSignature");
        ok = ok && addExtension(pCert, pSigner, NID_ext_key_usage, "serverAuth");
        if (pDnsName != NULL) {
            san = std::string("DNS:") + pDnsName;
            ok = ok && addExtension(pCert, pSigner, NID_subject_alt_name, san.c_str());
        }
    }

    ok = ok && X509_sign(pCert, pIssuerKey != NULL ? pIssuerKey : pKey, EVP_sha256()) > 0;
    if (!ok) {
        X509_free(pCert);
        pCert = NULL;
    }
    return pCert;
}

// What the local server saw during one test.
struct ServerState {
    std::atomic<bool> stop{false};
    std::atomic<int> handshakesStarted{0};
    std::atomic<int> handshakesCompleted{0};
    std::atomic<int> alertsReceived{0};
    std::mutex lock;
    std::string lastAlert;
};

// The info callback has no user argument, and only one server runs at a time.
ServerState* gActiveServer = NULL;

void serverInfoCallback(const SSL* pSsl, int where, int ret)
{
    UNUSED_PARAM(pSsl);
    ServerState* pState = gActiveServer;
    if (pState == NULL) {
        return;
    }
    if ((where & SSL_CB_HANDSHAKE_START) != 0) {
        pState->handshakesStarted++;
    }
    if ((where & SSL_CB_READ_ALERT) != 0) {
        std::lock_guard<std::mutex> guard(pState->lock);
        pState->alertsReceived++;
        pState->lastAlert = SSL_alert_desc_string_long(ret);
    }
}

int openListener(int family, std::string& error)
{
    int fd = socket(family, SOCK_STREAM, 0);
    int on = 1;
    int bound;

    if (fd < 0) {
        error = strerror(errno);
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (family == AF_INET6) {
        struct sockaddr_in6 address;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        MEMSET(&address, 0x00, sizeof(address));
        address.sin6_family = AF_INET6;
        address.sin6_port = htons(kSignalingPort);
        address.sin6_addr = in6addr_loopback;
        bound = bind(fd, (struct sockaddr*) &address, sizeof(address));
    } else {
        struct sockaddr_in address;
        MEMSET(&address, 0x00, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(kSignalingPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bound = bind(fd, (struct sockaddr*) &address, sizeof(address));
    }
    if (bound != 0 || listen(fd, 8) != 0) {
        error = strerror(errno);
        close(fd);
        return -1;
    }
    return fd;
}

// Reads the request headers and the body so the client is not reset while it
// is still writing, then answers 404. The signaling client records the HTTP
// status, which shows the TLS handshake completed.
void answerRequest(SSL* pSsl)
{
    std::string request;
    char buffer[4096];
    size_t headerEnd = std::string::npos;
    size_t bodyLength = 0;
    int read;

    while (headerEnd == std::string::npos && request.size() < 64 * 1024) {
        if ((read = SSL_read(pSsl, buffer, sizeof(buffer))) <= 0) {
            return;
        }
        request.append(buffer, (size_t) read);
        headerEnd = request.find("\r\n\r\n");
    }
    if (headerEnd == std::string::npos) {
        return;
    }

    std::string headers = request.substr(0, headerEnd);
    for (char& c : headers) {
        c = (char) tolower((unsigned char) c);
    }
    size_t lengthPos = headers.find("content-length:");
    if (lengthPos != std::string::npos) {
        bodyLength = (size_t) strtoul(headers.c_str() + lengthPos + STRLEN("content-length:"), NULL, 10);
    }
    while (request.size() < headerEnd + 4 + bodyLength) {
        if ((read = SSL_read(pSsl, buffer, sizeof(buffer))) <= 0) {
            return;
        }
        request.append(buffer, (size_t) read);
    }

    const char response[] = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 2\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "{}";
    SSL_write(pSsl, response, (int) STRLEN(response));
}

void runServer(ServerState* pState, SSL_CTX* pCtx, int listeners[2])
{
    while (!pState->stop.load()) {
        struct pollfd pollFds[2];
        nfds_t count = 0;
        for (int i = 0; i < 2; i++) {
            if (listeners[i] >= 0) {
                pollFds[count].fd = listeners[i];
                pollFds[count].events = POLLIN;
                pollFds[count].revents = 0;
                count++;
            }
        }
        if (poll(pollFds, count, 100) <= 0) {
            continue;
        }
        for (nfds_t i = 0; i < count; i++) {
            if ((pollFds[i].revents & POLLIN) == 0) {
                continue;
            }
            int clientFd = accept(pollFds[i].fd, NULL, NULL);
            if (clientFd < 0) {
                continue;
            }
            struct timeval timeout = {5, 0};
            setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            SSL* pSsl = SSL_new(pCtx);
            if (pSsl != NULL) {
                SSL_set_fd(pSsl, clientFd);
                if (SSL_accept(pSsl) == 1) {
                    pState->handshakesCompleted++;
                    answerRequest(pSsl);
                    SSL_shutdown(pSsl);
                }
                SSL_free(pSsl);
            }
            ERR_clear_error();
            close(clientFd);
        }
    }
}

} // namespace

class SignalingTlsCertificateTest : public WebRtcClientTestBase {
  protected:
    void SetUp() override
    {
        WebRtcClientTestBase::SetUp();

        mTrustedCaKey = makeP256Key();
        mOtherCaKey = makeP256Key();
        mServerKey = makeP256Key();
        ASSERT_TRUE(mTrustedCaKey != NULL && mOtherCaKey != NULL && mServerKey != NULL);

        mTrustedCa = makeCert(mTrustedCaKey, "KVS test trusted CA", NULL, NULL, NULL, TRUE, -kOneDaySeconds, kOneDaySeconds, 1);
        mOtherCa = makeCert(mOtherCaKey, "KVS test untrusted CA", NULL, NULL, NULL, TRUE, -kOneDaySeconds, kOneDaySeconds, 2);
        ASSERT_TRUE(mTrustedCa != NULL && mOtherCa != NULL);

        // The trust file holds only the trusted test CA.
        STRCPY(mTrustFilePath, (::testing::TempDir() + "kvsSignalingTlsCa-XXXXXX").c_str());
        int fd = mkstemp(mTrustFilePath);
        ASSERT_GE(fd, 0);
        FILE* pFile = fdopen(fd, "w");
        ASSERT_TRUE(pFile != NULL);
        ASSERT_EQ(1, PEM_write_X509(pFile, mTrustedCa));
        fclose(pFile);
    }

    void TearDown() override
    {
        stopServer();
        X509_free(mTrustedCa);
        X509_free(mOtherCa);
        EVP_PKEY_free(mTrustedCaKey);
        EVP_PKEY_free(mOtherCaKey);
        EVP_PKEY_free(mServerKey);
        if (mTrustFilePath[0] != '\0') {
            unlink(mTrustFilePath);
        }
        WebRtcClientTestBase::TearDown();
    }

    // Starts the local server with pCert. Returns FALSE when port 443 cannot
    // be opened on either loopback address.
    BOOL startServer(X509* pCert, std::string& error)
    {
        std::string error4, error6;

        mServerCtx = SSL_CTX_new(TLS_method());
        if (mServerCtx == NULL || SSL_CTX_use_certificate(mServerCtx, pCert) != 1 || SSL_CTX_use_PrivateKey(mServerCtx, mServerKey) != 1) {
            error = "failed to set up the server TLS context";
            return FALSE;
        }
        SSL_CTX_set_info_callback(mServerCtx, serverInfoCallback);

        mListeners[0] = openListener(AF_INET, error4);
        mListeners[1] = openListener(AF_INET6, error6);
        if (mListeners[0] < 0 && mListeners[1] < 0) {
            error = "cannot listen on port 443 (IPv4: " + error4 + ", IPv6: " + error6 + ")";
            return FALSE;
        }

        gActiveServer = &mServerState;
        mServerThread = std::thread(runServer, &mServerState, mServerCtx, mListeners);
        return TRUE;
    }

    void stopServer()
    {
        mServerState.stop = true;
        if (mServerThread.joinable()) {
            mServerThread.join();
        }
        gActiveServer = NULL;
        for (int i = 0; i < 2; i++) {
            if (mListeners[i] >= 0) {
                close(mListeners[i]);
                mListeners[i] = -1;
            }
        }
        if (mServerCtx != NULL) {
            SSL_CTX_free(mServerCtx);
            mServerCtx = NULL;
        }
    }

    // Makes one DescribeSignalingChannel call to https://localhost through
    // libwebsockets, with the test CA as the only trusted root. Returns the
    // status of the call and the service call result the client recorded.
    void describeAgainstLocalServer(STATUS* pStatus, SERVICE_CALL_RESULT* pResult)
    {
        SignalingClientInfoInternal clientInfoInternal;
        SignalingClientCallbacks signalingClientCallbacks;
        ChannelInfo channelInfo;
        PAwsCredentialProvider pCredentialProvider = NULL;
        PSignalingClient pSignalingClient = NULL;

        MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));
        clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
        clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
        STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

        MEMSET(&signalingClientCallbacks, 0x00, SIZEOF(SignalingClientCallbacks));
        signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;

        MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
        channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
        channelInfo.pChannelName = mChannelName;
        channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
        channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
        channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
        channelInfo.retry = FALSE;
        channelInfo.reconnect = FALSE;
        channelInfo.pRegion = mRegion;
        channelInfo.pControlPlaneUrl = (PCHAR) "https://localhost";
        channelInfo.pCertPath = mTrustFilePath;
        channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

        *pStatus = STATUS_INTERNAL_ERROR;
        *pResult = SERVICE_CALL_RESULT_NOT_SET;

        ASSERT_EQ(STATUS_SUCCESS,
                  createStaticCredentialProvider((PCHAR) "accessKey", 0, (PCHAR) "secretKey", 0, NULL, 0, MAX_UINT64, &pCredentialProvider));
        // createSignalingSync() stops at GET_TOKEN, which the static provider
        // serves locally, so the only network call is the one below.
        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, pCredentialProvider, &pSignalingClient));
        if (pSignalingClient != NULL) {
            *pStatus = describeChannelLws(pSignalingClient, GETTIME());
            *pResult = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result);
        }

        EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
        EXPECT_EQ(STATUS_SUCCESS, freeStaticCredentialProvider(&pCredentialProvider));
    }

    // Serves pCert and checks that the signaling client refused it: the
    // handshake started, the client sent an alert, and no HTTP response
    // reached the client.
    void expectRejected(X509* pCert)
    {
        STATUS status;
        SERVICE_CALL_RESULT result;
        std::string error;

        ASSERT_TRUE(pCert != NULL);
        if (!startServer(pCert, error)) {
            GTEST_SKIP() << error;
        }
        describeAgainstLocalServer(&status, &result);
        stopServer();

        EXPECT_EQ(STATUS_SIGNALING_LWS_CALL_FAILED, status);
        EXPECT_EQ(SERVICE_CALL_RESULT_NOT_SET, result) << "an HTTP response arrived, so the client accepted the certificate";
        EXPECT_GE(mServerState.handshakesStarted.load(), 1) << "the client never reached the local server";
        EXPECT_EQ(0, mServerState.handshakesCompleted.load()) << "the TLS handshake completed with a bad certificate";
        EXPECT_GE(mServerState.alertsReceived.load(), 1) << "the client closed without sending a TLS alert";
        std::lock_guard<std::mutex> guard(mServerState.lock);
        DLOGI("Client rejected the certificate with alert: %s", mServerState.lastAlert.c_str());
    }

    EVP_PKEY* mTrustedCaKey = NULL;
    EVP_PKEY* mOtherCaKey = NULL;
    EVP_PKEY* mServerKey = NULL;
    X509* mTrustedCa = NULL;
    X509* mOtherCa = NULL;
    CHAR mTrustFilePath[MAX_PATH_LEN + 1] = {0};

    SSL_CTX* mServerCtx = NULL;
    int mListeners[2] = {-1, -1};
    ServerState mServerState;
    std::thread mServerThread;
};

// Control: a certificate from the trusted CA for localhost connects, and the
// local server's 404 reaches the client. Without this, the tests below could
// pass because the server or the network failed.
TEST_F(SignalingTlsCertificateTest, acceptsValidCertificate)
{
    STATUS status;
    SERVICE_CALL_RESULT result;
    std::string error;
    X509* pCert = makeCert(mServerKey, kTrustedHost, kTrustedHost, mTrustedCa, mTrustedCaKey, FALSE, -kOneDaySeconds, kOneDaySeconds, 10);

    ASSERT_TRUE(pCert != NULL);
    if (!startServer(pCert, error)) {
        X509_free(pCert);
        GTEST_SKIP() << error;
    }
    describeAgainstLocalServer(&status, &result);
    stopServer();
    X509_free(pCert);

    EXPECT_EQ(STATUS_SIGNALING_LWS_CALL_FAILED, status) << "the 404 should fail the call after a completed handshake";
    EXPECT_EQ(SERVICE_CALL_RESOURCE_NOT_FOUND, result);
    EXPECT_GE(mServerState.handshakesCompleted.load(), 1);
}

TEST_F(SignalingTlsCertificateTest, rejectsCertificateFromUntrustedCa)
{
    X509* pCert = makeCert(mServerKey, kTrustedHost, kTrustedHost, mOtherCa, mOtherCaKey, FALSE, -kOneDaySeconds, kOneDaySeconds, 11);
    expectRejected(pCert);
    X509_free(pCert);
}

TEST_F(SignalingTlsCertificateTest, rejectsSelfSignedCertificate)
{
    X509* pCert = makeCert(mServerKey, kTrustedHost, kTrustedHost, NULL, NULL, FALSE, -kOneDaySeconds, kOneDaySeconds, 12);
    expectRejected(pCert);
    X509_free(pCert);
}

TEST_F(SignalingTlsCertificateTest, rejectsExpiredCertificate)
{
    X509* pCert = makeCert(mServerKey, kTrustedHost, kTrustedHost, mTrustedCa, mTrustedCaKey, FALSE, -2 * kOneDaySeconds, -kOneDaySeconds, 13);
    expectRejected(pCert);
    X509_free(pCert);
}

TEST_F(SignalingTlsCertificateTest, rejectsCertificateForWrongHost)
{
    X509* pCert = makeCert(mServerKey, kOtherHost, kOtherHost, mTrustedCa, mTrustedCaKey, FALSE, -kOneDaySeconds, kOneDaySeconds, 14);
    expectRejected(pCert);
    X509_free(pCert);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
