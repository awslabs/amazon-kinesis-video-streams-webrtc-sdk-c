#define LOG_CLASS "SRTP"
#include "../Include_i.h"

/*
 * This file builds against either libsrtp 2.x (the long-standing default) or
 * libsrtp 3.x (project renamed to libsrtp3 in cisco/libsrtp main, PSA Crypto +
 * mbedTLS 4 support via PR #782). Pick at CMake configure time with
 * `-DUSE_LIBSRTP3=ON`. The header included from Include_i.h follows the same
 * switch.
 *
 * The 3.x line introduces three relevant API breaks:
 *   1. srtp_policy_t is opaque — allocate via srtp_policy_create() and configure
 *      via srtp_policy_set_profile()/set_ssrc()/add_key() instead of writing
 *      directly into struct fields.
 *   2. srtp_protect()/srtp_unprotect() take separate src and dst buffers with
 *      explicit lengths (size_t), plus mki_index on the protect side. In-place
 *      operation is still supported by passing the same pointer for src and
 *      dst — KVS keeps using that in-place pattern.
 *   3. Master key + master salt are passed separately to srtp_policy_add_key(),
 *      not concatenated as in 2.x's policy.key. We split using
 *      srtp_profile_get_master_{key,salt}_length().
 */

#ifdef USE_LIBSRTP3

/*
 * Map a KVS_SRTP_PROFILE to the corresponding libsrtp 3.x profile enum.
 * In libsrtp 2.x the policy struct split rtp / rtcp crypto policies, so KVS
 * picked a 32-bit auth tag for RTP and the 80-bit default for RTCP. libsrtp
 * 3.x's profile enum already follows the DTLS-SRTP RFC semantics
 * (RFC 5764 §4.1.2) which fold RTP+RTCP into a single profile per session,
 * so the mapping below is a direct enum-to-enum translation.
 */
static STATUS kvsToLibsrtpProfile(KVS_SRTP_PROFILE kvsProfile, srtp_profile_t* pProfile)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pProfile != NULL, STATUS_NULL_ARG);

    switch (kvsProfile) {
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_32:
            *pProfile = srtp_profile_aes128_cm_sha1_32;
            break;
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80:
            *pProfile = srtp_profile_aes128_cm_sha1_80;
            break;
        default:
            DLOGE("Unknown KVS_SRTP_PROFILE %u", kvsProfile);
            CHK(FALSE, STATUS_SSL_UNKNOWN_SRTP_PROFILE);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/*
 * Build a libsrtp 3.x policy handle for the given key + profile + SSRC type.
 *
 * KVS passes the master key + master salt as a single concatenated buffer
 * (per the DTLS-SRTP key derivation in Dtls_mbedtls.c). libsrtp 3.x's
 * srtp_policy_add_key() wants them separately, so we use
 * srtp_profile_get_master_{key,salt}_length() to find the split point.
 */
static STATUS buildPolicy(srtp_profile_t profile, PBYTE keyAndSalt, srtp_ssrc_type_t ssrcType, srtp_policy_t* pPolicy)
{
    STATUS retStatus = STATUS_SUCCESS;
    srtp_policy_t policy = NULL;
    srtp_err_status_t errStatus;
    size_t keyLen, saltLen;
    srtp_ssrc_t ssrc;

    CHK(pPolicy != NULL && keyAndSalt != NULL, STATUS_NULL_ARG);

    keyLen = srtp_profile_get_master_key_length(profile);
    saltLen = srtp_profile_get_master_salt_length(profile);
    CHK_ERR(keyLen > 0 && saltLen > 0, STATUS_SSL_UNKNOWN_SRTP_PROFILE, "Profile %u has zero key/salt length", profile);

    CHK_ERR((errStatus = srtp_policy_create(&policy)) == srtp_err_status_ok, STATUS_SRTP_INIT_FAILED, "srtp_policy_create failed with error code %u",
            errStatus);

    CHK_ERR((errStatus = srtp_policy_set_profile(policy, profile)) == srtp_err_status_ok, STATUS_SRTP_INIT_FAILED,
            "srtp_policy_set_profile failed with error code %u", errStatus);

    ssrc.type = ssrcType;
    ssrc.value = 0;
    CHK_ERR((errStatus = srtp_policy_set_ssrc(policy, ssrc)) == srtp_err_status_ok, STATUS_SRTP_INIT_FAILED,
            "srtp_policy_set_ssrc failed with error code %u", errStatus);

    CHK_ERR((errStatus = srtp_policy_add_key(policy, keyAndSalt, keyLen, keyAndSalt + keyLen, saltLen, NULL, 0)) == srtp_err_status_ok,
            STATUS_SRTP_INIT_FAILED, "srtp_policy_add_key failed with error code %u", errStatus);

    *pPolicy = policy;
    policy = NULL;

CleanUp:
    if (policy != NULL) {
        srtp_policy_destroy(policy);
    }
    return retStatus;
}

STATUS initSrtpSession(PBYTE receiveKey, PBYTE transmitKey, KVS_SRTP_PROFILE profile, PSrtpSession* ppSrtpSession)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PSrtpSession pSrtpSession = NULL;
    srtp_policy_t transmitPolicy = NULL, receivePolicy = NULL;
    srtp_err_status_t errStatus;
    srtp_profile_t libsrtpProfile;

    CHK(receiveKey != NULL && transmitKey != NULL && ppSrtpSession != NULL, STATUS_NULL_ARG);
    CHK_STATUS(kvsToLibsrtpProfile(profile, &libsrtpProfile));

    pSrtpSession = (PSrtpSession) MEMCALLOC(1, SIZEOF(SrtpSession));
    CHK(pSrtpSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(buildPolicy(libsrtpProfile, receiveKey, ssrc_any_inbound, &receivePolicy));
    CHK_ERR((errStatus = srtp_create(&(pSrtpSession->srtp_receive_session), receivePolicy)) == srtp_err_status_ok,
            STATUS_SRTP_RECEIVE_SESSION_CREATION_FAILED, "Create srtp session for the receiver failed with error code %u", errStatus);

    CHK_STATUS(buildPolicy(libsrtpProfile, transmitKey, ssrc_any_outbound, &transmitPolicy));
    CHK_ERR((errStatus = srtp_create(&(pSrtpSession->srtp_transmit_session), transmitPolicy)) == srtp_err_status_ok,
            STATUS_SRTP_TRANSMIT_SESSION_CREATION_FAILED, "Create srtp session for the transmitter failed with error code %u", errStatus);

    *ppSrtpSession = pSrtpSession;
    pSrtpSession = NULL;

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (receivePolicy != NULL) {
        srtp_policy_destroy(receivePolicy);
    }
    if (transmitPolicy != NULL) {
        srtp_policy_destroy(transmitPolicy);
    }
    if (STATUS_FAILED(retStatus) && pSrtpSession != NULL) {
        freeSrtpSession(&pSrtpSession);
    }

    LEAVES();
    return retStatus;
}

STATUS decryptSrtpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;
    size_t srcLen, dstLen;

    CHK(pSrtpSession != NULL && encryptedMessage != NULL && len != NULL, STATUS_NULL_ARG);

    /* libsrtp 3.x: in-place is supported when src == dst. dst_len starts as
     * the buffer capacity and is overwritten with the decrypted packet length. */
    srcLen = (size_t) *len;
    dstLen = srcLen;
    errStatus = srtp_unprotect(pSrtpSession->srtp_receive_session, (const uint8_t*) encryptedMessage, srcLen, (uint8_t*) encryptedMessage, &dstLen);
    CHK_ERR(errStatus == srtp_err_status_ok, STATUS_SRTP_DECRYPT_FAILED, "Decrypting rtp packet failed with error code %u", errStatus);
    *len = (INT32) dstLen;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS decryptSrtcpPacket(PSrtpSession pSrtpSession, PVOID encryptedMessage, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;
    size_t srcLen, dstLen;

    CHK(pSrtpSession != NULL && encryptedMessage != NULL && len != NULL, STATUS_NULL_ARG);

    srcLen = (size_t) *len;
    dstLen = srcLen;
    errStatus =
        srtp_unprotect_rtcp(pSrtpSession->srtp_receive_session, (const uint8_t*) encryptedMessage, srcLen, (uint8_t*) encryptedMessage, &dstLen);
    CHK_ERR(errStatus == srtp_err_status_ok, STATUS_SRTP_DECRYPT_FAILED, "Decrypting rtcp packet failed with error code %u", errStatus);
    *len = (INT32) dstLen;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS encryptRtpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;
    size_t srcLen, dstLen;

    CHK(pSrtpSession != NULL && message != NULL && len != NULL, STATUS_NULL_ARG);

    /* Caller is required to allocate the buffer with at least SRTP_MAX_TRAILER_LEN
     * bytes of headroom (matches the libsrtp 2.x in-place contract that KVS
     * already follows). dst_len starts as the available capacity and is
     * overwritten with the protected packet length. */
    srcLen = (size_t) *len;
    dstLen = srcLen + SRTP_MAX_TRAILER_LEN;
    errStatus = srtp_protect(pSrtpSession->srtp_transmit_session, (const uint8_t*) message, srcLen, (uint8_t*) message, &dstLen, 0);
    CHK_ERR(errStatus == srtp_err_status_ok, STATUS_SRTP_ENCRYPT_FAILED, "srtp_protect returned %u", errStatus);
    *len = (INT32) dstLen;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS encryptRtcpPacket(PSrtpSession pSrtpSession, PVOID message, PINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    srtp_err_status_t errStatus;
    size_t srcLen, dstLen;

    CHK(pSrtpSession != NULL && message != NULL && len != NULL, STATUS_NULL_ARG);

    srcLen = (size_t) *len;
    dstLen = srcLen + SRTP_MAX_TRAILER_LEN;
    errStatus = srtp_protect_rtcp(pSrtpSession->srtp_transmit_session, (const uint8_t*) message, srcLen, (uint8_t*) message, &dstLen, 0);
    CHK_ERR(errStatus == srtp_err_status_ok, STATUS_SRTP_ENCRYPT_FAILED, "srtp_protect_rtcp returned %u", errStatus);
    *len = (INT32) dstLen;

CleanUp:
    LEAVES();
    return retStatus;
}

#else /* USE_LIBSRTP3 — legacy libsrtp 2.x path below */

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

#endif /* USE_LIBSRTP3 */

/* freeSrtpSession is identical for both versions — srtp_dealloc API matches. */
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
        DLOGW("Dealloc of transmit session failed with error code %d", errStatus);
    }
    if ((pSrtpSession->srtp_receive_session != NULL) && (errStatus = srtp_dealloc(pSrtpSession->srtp_receive_session)) != srtp_err_status_ok) {
        DLOGW("Dealloc of receive session failed with error code %d", errStatus);
    }

    SAFE_MEMFREE(pSrtpSession);
    *ppSrtpSession = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}
