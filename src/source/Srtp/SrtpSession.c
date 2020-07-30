#define LOG_CLASS "SRTP"
#include "../Include_i.h"

STATUS initSrtpSession(PBYTE receiveKey, PBYTE transmitKey, KVS_SRTP_PROFILE profile, PSrtpSession* ppSrtpSession)
{
    ENTERS();
    UNUSED_PARAM(profile);

    STATUS retStatus = STATUS_SUCCESS;
    PSrtpSession pSrtpSession = NULL;
    srtp_policy_t transmitPolicy, receivePolicy;
    srtp_err_status_t errStatus;
    void (*srtp_policy_setter)(srtp_crypto_policy_t*) = NULL;
    void (*srtcp_policy_setter)(srtp_crypto_policy_t*) = NULL;

    CHK(receiveKey != NULL && transmitKey != NULL && ppSrtpSession != NULL, STATUS_NULL_ARG);

    pSrtpSession = (PSrtpSession) MEMCALLOC(1, SIZEOF(SrtpSession));

    MEMSET(&transmitPolicy, 0x00, SIZEOF(srtp_policy_t));
    MEMSET(&receivePolicy, 0x00, SIZEOF(srtp_policy_t));

    switch (profile) {
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_32:
            srtp_policy_setter = srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32;
            srtcp_policy_setter = srtp_crypto_policy_set_rtp_default;
            break;
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80:
            srtp_policy_setter = srtp_crypto_policy_set_rtp_default;
            srtcp_policy_setter = srtp_crypto_policy_set_rtp_default;
            break;
        default:
            CHK(FALSE, STATUS_SSL_UNKNOWN_SRTP_PROFILE);
    }

    srtp_policy_setter(&receivePolicy.rtp);
    srtcp_policy_setter(&receivePolicy.rtcp);

    receivePolicy.key = receiveKey;
    receivePolicy.ssrc.type = ssrc_any_inbound;
    receivePolicy.next = NULL;

    CHK_ERR((errStatus = srtp_create(&(pSrtpSession->srtp_receive_session), &receivePolicy)) == srtp_err_status_ok,
            STATUS_SRTP_RECEIVE_SESSION_CREATION_FAILED, "Create srtp session for the receiver failed with error code %u", errStatus);

    srtp_policy_setter(&transmitPolicy.rtp);
    srtcp_policy_setter(&transmitPolicy.rtcp);

    transmitPolicy.key = transmitKey;
    transmitPolicy.ssrc.type = ssrc_any_outbound;
    transmitPolicy.next = NULL;

    CHK_ERR((errStatus = srtp_create(&(pSrtpSession->srtp_transmit_session), &transmitPolicy)) == srtp_err_status_ok,
            STATUS_SRTP_TRANSMIT_SESSION_CREATION_FAILED, "Create srtp session for the transmitter failed with error code %u", errStatus);

    *ppSrtpSession = pSrtpSession;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeSrtpSession(&pSrtpSession);
    }

    LEAVES();
    return retStatus;
}

STATUS freeSrtpSession(PSrtpSession* ppSrtpSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;

    PSrtpSession pSrtpSession = NULL;

    CHK(ppSrtpSession != NULL, STATUS_NULL_ARG);
    CHK(*ppSrtpSession != NULL, retStatus);

    pSrtpSession = *ppSrtpSession;

    if ((pSrtpSession->srtp_transmit_session != NULL) && (errStatus = srtp_dealloc(pSrtpSession->srtp_transmit_session)) != srtp_err_status_ok) {
        DLOGW("Dealloc of transmit session failed with error code %d\n", errStatus);
    }
    if ((pSrtpSession->srtp_receive_session != NULL) && (errStatus = srtp_dealloc(pSrtpSession->srtp_receive_session)) != srtp_err_status_ok) {
        DLOGW("Dealloc of receive session failed with error code %d\n", errStatus);
    }

    SAFE_MEMFREE(pSrtpSession);
    *ppSrtpSession = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS decryptSrtpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;

    CHK_ERR((errStatus = srtp_unprotect(pSrtpSession->srtp_receive_session, encryptedMessage, len)) == srtp_err_status_ok, STATUS_SRTP_DECRYPT_FAILED,
            "Decrypting rtp packet failed with error code %u on srtp session %" PRIu64, errStatus, pSrtpSession->srtp_receive_session);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS decryptSrtcpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;

    CHK_ERR((errStatus = srtp_unprotect_rtcp(pSrtpSession->srtp_receive_session, encryptedMessage, len)) == srtp_err_status_ok,
            STATUS_SRTP_DECRYPT_FAILED, "Decrypting rtcp packet failed with error code %u on srtp session %" PRIu64, errStatus,
            pSrtpSession->srtp_receive_session);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS encryptRtpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t status;

    status = srtp_protect(pSrtpSession->srtp_transmit_session, message, len);

    CHK_ERR(status == srtp_err_status_ok, STATUS_SRTP_ENCRYPT_FAILED, "srtp_protect returned %lu on srtp session %" PRIu64, status,
            pSrtpSession->srtp_transmit_session);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS encryptRtcpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t status;

    status = srtp_protect_rtcp(pSrtpSession->srtp_transmit_session, message, len);

    CHK_ERR(status == srtp_err_status_ok, STATUS_SRTP_ENCRYPT_FAILED, "srtp_protect_rtcp returned %lu on srtp session %" PRIu64, status,
            pSrtpSession->srtp_transmit_session);

CleanUp:
    LEAVES();
    return retStatus;
}
