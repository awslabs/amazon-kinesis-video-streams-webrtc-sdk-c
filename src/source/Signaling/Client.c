#define LOG_CLASS "SignalingClient"
#include "../Include_i.h"

STATUS createSignalingClientSync(PSignalingClientInfo pClientInfo,
                                 PChannelInfo pChannelInfo,
                                 PSignalingClientCallbacks pCallbacks,
                                 PAwsCredentialProvider pCredentialProvider,
                                 PSIGNALING_CLIENT_HANDLE pSignalingHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    SignalingClientInfoInternal signalingClientInfoInternal;

    DLOGI("Creating Signaling Client Sync");
    CHK(pSignalingHandle != NULL && pClientInfo != NULL, STATUS_NULL_ARG);

    // Convert the client info to the internal structure with empty values
    MEMSET(&signalingClientInfoInternal, 0x00, SIZEOF(signalingClientInfoInternal));
    signalingClientInfoInternal.signalingClientInfo = *pClientInfo;

    CHK_STATUS(createSignalingSync(&signalingClientInfoInternal, pChannelInfo, pCallbacks, pCredentialProvider, &pSignalingClient));

    *pSignalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeSignaling(&pSignalingClient);
    }

    LEAVES();
    return retStatus;
}

STATUS freeSignalingClient(PSIGNALING_CLIENT_HANDLE pSignalingHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient;

    DLOGI("Freeing Signaling Client");
    CHK(pSignalingHandle != NULL, STATUS_NULL_ARG);

    // Get the client handle
    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(*pSignalingHandle);

    CHK_STATUS(freeSignaling(&pSignalingClient));

    // Set the signaling client handle pointer to invalid
    *pSignalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientSendMessageSync(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGI("Signaling Client Sending Message Sync");

    CHK_STATUS(signalingSendMessageSync(pSignalingClient, pSignalingMessage));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientConnectSync(SIGNALING_CLIENT_HANDLE signalingClientHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGI("Signaling Client Connect Sync");

    CHK_STATUS(signalingConnectSync(pSignalingClient));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientGetIceConfigInfoCout(SIGNALING_CLIENT_HANDLE signalingClientHandle, PUINT32 pIceConfigCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGI("Signaling Client Get ICE Config Info Count");

    CHK_STATUS(signalingGetIceConfigInfoCout(pSignalingClient, pIceConfigCount));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientGetIceConfigInfo(SIGNALING_CLIENT_HANDLE signalingClientHandle, UINT32 index, PIceConfigInfo* ppIceConfigInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGI("Signaling Client Get ICE Config Info");

    CHK_STATUS(signalingGetIceConfigInfo(pSignalingClient, index, ppIceConfigInfo));

CleanUp:

    LEAVES();
    return retStatus;
}
