/**
 * AWS-LC post-quantum group configuration and per-handshake reporting.
 *
 * Header-only on purpose. These helpers are needed by BOTH SDK libraries:
 *
 *   kvsWebrtcClient          - Tls_openssl.c, for the TURN-over-TLS context
 *   kvsWebrtcSignalingClient - LwsApiCalls.c, for the libwebsockets contexts
 *
 * The two libraries are independent CMake targets and kvsWebrtcSignalingClient
 * does not link kvsWebrtcClient, so a definition compiled into one is not
 * resolvable from the other. Both do link AWS-LC, so `static inline` here gives
 * each translation unit its own copy without touching the link graph.
 */
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_AWSLC_PQC__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_AWSLC_PQC__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KVS_USE_AWS_LC

/**
 * Apply the post-quantum key exchange group list to a client SSL_CTX.
 *
 * Returns STATUS_SUCCESS on success. Callers should treat failure as
 * non-fatal: losing ML-KEM preference is a reduction in protection, not a loss
 * of connectivity, and failing the connection outright would be worse.
 */
static inline STATUS awsLcConfigureTlsGroups(SSL_CTX* pSslCtx, BOOL enablePqc)
{
    STATUS retStatus = STATUS_SUCCESS;
    /* Plain `int`, not INT32: SSL_CTX_set1_groups takes `const int *`. */
    const int pqcGroups[] = KVS_AWS_LC_PQC_GROUPS;
    const int classicalGroups[] = KVS_AWS_LC_CLASSICAL_GROUPS;
    const int* pGroups = NULL;
    size_t groupCount = 0;

    CHK(pSslCtx != NULL, STATUS_NULL_ARG);

    if (enablePqc) {
        pGroups = pqcGroups;
        groupCount = ARRAY_SIZE(pqcGroups);
    } else {
        pGroups = classicalGroups;
        groupCount = ARRAY_SIZE(classicalGroups);
    }

    /*
     * SSL_CTX_set1_groups takes NIDs and returns 1 on success. Chosen over
     * SSL_CTX_set1_group_ids (codepoints) because the latter does not exist on
     * the AWS-LC fips-* branches, and over SSL_CTX_set1_groups_list (strings)
     * because a typo in a name string would fail silently at runtime rather
     * than at compile time.
     */
    CHK_ERR(SSL_CTX_set1_groups(pSslCtx, pGroups, groupCount) == 1, STATUS_SSL_CTX_CREATION_FAILED,
            "Failed to set %s key exchange groups on the TLS context", enablePqc ? "post-quantum" : "classical");

    /*
     * ML-KEM is only reachable in TLS 1.3. AWS-LC negotiates the highest mutually
     * supported version by default, but pinning the floor makes the intent
     * explicit and stops a downgrade from silently costing PQC protection.
     */
    CHK_ERR(SSL_CTX_set_min_proto_version(pSslCtx, TLS1_2_VERSION) == 1, STATUS_SSL_CTX_CREATION_FAILED,
            "Failed to set the minimum TLS protocol version");

CleanUp:
    return retStatus;
}

/**
 * Log both sides of a completed handshake: what this SDK offered and what the
 * peer selected.
 *
 * Reporting both is the point. They diverge whenever the peer lacks ML-KEM, and
 * that divergence is exactly what an operator needs in order to tell a peer
 * limitation apart from an SDK misconfiguration. A line reading
 * "offered=X25519MLKEM768 negotiated=X25519 CLASSICAL" is the SDK behaving
 * correctly against a server that has not enabled ML-KEM yet.
 *
 * "offered" is stated from the configuration applied by
 * awsLcConfigureTlsGroups() rather than re-parsed off the wire; for byte-level
 * ClientHello evidence use scripts/verify-pqc.sh.
 */
/**
 * Records whether awsLcConfigureTlsGroups() successfully applied the PQC list to
 * the libwebsockets client SSL_CTX.
 *
 * libwebsockets owns that context and creates exactly one for the whole signaling
 * client, so a single flag is sufficient and accurate. Without it the log line
 * below would have to *assert* what was offered, which would be a claim rather
 * than an observation; when the list is AWS-LC's default instead of ours the
 * distinction matters, because the default is not a contract.
 */
static inline BOOL* awsLcPqcConfiguredSlot(VOID)
{
    static BOOL configured = FALSE;
    return &configured;
}

static inline VOID awsLcSetPqcConfigured(BOOL configured)
{
    *awsLcPqcConfiguredSlot() = configured;
}

static inline VOID awsLcRecordNegotiatedGroup(SSL* pSsl, PCHAR pDescription)
{
    UINT16 groupId;
    const CHAR* pGroupName = NULL;
    const CHAR* pVersion = NULL;
    PCHAR pLabel = (pDescription != NULL) ? pDescription : (PCHAR) "TLS";
    /* "configured" = this SDK explicitly set the group list. "default" = the list
     * came from AWS-LC's built-in preference, which leads with X25519MLKEM768
     * today but is not guaranteed to. Both offer ML-KEM; only the first is
     * something the SDK controls, so the log distinguishes them. */
    PCHAR pOffered = *awsLcPqcConfiguredSlot() ? (PCHAR) "X25519MLKEM768(configured)" : (PCHAR) "X25519MLKEM768(awslc-default)";

    if (pSsl == NULL) {
        return;
    }

    /* 0 when no handshake completed, or the connection used no group at all
     * (e.g. a pure-RSA TLS 1.2 suite). */
    groupId = SSL_get_group_id(pSsl);
    if (groupId == 0) {
        DLOGD("PQC[%s]: no key exchange group negotiated", pLabel);
        return;
    }

    pGroupName = SSL_get_group_name(groupId);
    pVersion = SSL_get_version(pSsl);

    if (groupId == SSL_GROUP_X25519_MLKEM768 || groupId == SSL_GROUP_SECP256R1_MLKEM768 || groupId == SSL_GROUP_SECP384R1_MLKEM1024) {
        DLOGI("PQC[%s]: %s offered=%s negotiated=%s (0x%04x) POST-QUANTUM", pLabel, pVersion != NULL ? pVersion : "unknown", pOffered,
              pGroupName != NULL ? pGroupName : "unknown", groupId);
    } else {
        DLOGW("PQC[%s]: %s offered=%s negotiated=%s (0x%04x) CLASSICAL - peer did not accept ML-KEM", pLabel, pVersion != NULL ? pVersion : "unknown",
              pOffered, pGroupName != NULL ? pGroupName : "unknown", groupId);
    }
}

#endif /* KVS_USE_AWS_LC */

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_AWSLC_PQC__
