/**
 * AWS-LC post-quantum group configuration.
 *
 * Header-only on purpose. This helper is needed by BOTH SDK libraries:
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
static inline STATUS awsLcConfigureTlsGroups(SSL_CTX* pSslCtx)
{
    STATUS retStatus = STATUS_SUCCESS;
    /* Plain `int`, not INT32: SSL_CTX_set1_groups takes `const int *`. */
    const int pqcGroups[] = KVS_AWS_LC_PQC_GROUPS;

    CHK(pSslCtx != NULL, STATUS_NULL_ARG);

    /*
     * SSL_CTX_set1_groups takes NIDs and returns 1 on success. Chosen over
     * SSL_CTX_set1_group_ids (codepoints) because the latter does not exist on
     * the AWS-LC fips-* branches, and over SSL_CTX_set1_groups_list (strings)
     * because a typo in a name string would fail silently at runtime rather
     * than at compile time.
     */
    CHK_ERR(SSL_CTX_set1_groups(pSslCtx, pqcGroups, ARRAY_SIZE(pqcGroups)) == 1, STATUS_SSL_CTX_CREATION_FAILED,
            "Failed to set post-quantum key exchange groups on the TLS context");

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

#endif /* KVS_USE_AWS_LC */

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_AWSLC_PQC__
