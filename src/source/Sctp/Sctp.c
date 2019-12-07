#define LOG_CLASS "SCTP"
#include "../Include_i.h"

STATUS initSctpAddrConn(PSctpSession pSctpSession, struct sockaddr_conn *sconn)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    sconn->sconn_family = AF_CONN;
    putInt16((PINT16) &sconn->sconn_port, SCTP_ASSOCIATION_DEFAULT_PORT);
    sconn->sconn_addr = pSctpSession;

    LEAVES();
    return retStatus;
}

STATUS configureSctpSocket(struct socket *socket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    struct linger linger_opt;
    struct sctp_event event;
    UINT32 i;
    UINT32 valueOn = 1;
    UINT16 event_types[] = {SCTP_ASSOC_CHANGE, SCTP_PEER_ADDR_CHANGE, SCTP_REMOTE_ERROR, SCTP_SHUTDOWN_EVENT, SCTP_ADAPTATION_INDICATION, SCTP_PARTIAL_DELIVERY_EVENT};

    CHK(usrsctp_set_non_blocking(socket, 1) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);

    // onSctpOutboundPacket must not be called after close
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 0;
    CHK(usrsctp_setsockopt(socket, SOL_SOCKET, SO_LINGER, &linger_opt, SIZEOF(linger_opt)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);

    // packets are generally sent as soon as possible and no unnecessary
    // delays are introduced, at the cost of more packets in the network.
    CHK(usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_NODELAY, &valueOn, SIZEOF(valueOn)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);

    MEMSET(&event, 0, SIZEOF(event));
    event.se_assoc_id = SCTP_FUTURE_ASSOC;
    event.se_on = 1;
    for (i = 0; i < (UINT32)(SIZEOF(event_types)/SIZEOF(UINT16)); i++) {
        event.se_type = event_types[i];
        CHK(usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_EVENT, &event, SIZEOF(struct sctp_event)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);
    }

    struct sctp_initmsg initmsg;
    MEMSET(&initmsg, 0, SIZEOF(struct sctp_initmsg));
    initmsg.sinit_num_ostreams = 300;
    initmsg.sinit_max_instreams = 300;
    CHK(usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, SIZEOF(struct sctp_initmsg)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS initSctpSession()
{
    STATUS retStatus = STATUS_SUCCESS;

    usrsctp_init(0, &onSctpOutboundPacket, NULL);

    // Disable Explicit Congestion Notification
    usrsctp_sysctl_set_sctp_ecn_enable(0);

    return retStatus;
}

VOID deinitSctpSession()
{
    // need to block until usrsctp_finish or sctp thread could be calling free objects and cause segfault
    while (usrsctp_finish() != 0) {
        THREAD_SLEEP(DEFAULT_USRSCTP_TEARDOWN_POLLING_INTERVAL);
    }
}

STATUS createSctpSession(PSctpSessionCallbacks pSctpSessionCallbacks, PSctpSession* ppSctpSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSctpSession pSctpSession = NULL;
    struct sockaddr_conn localConn = {0}, remoteConn = {0};
    struct sctp_paddrparams params;
    INT32 connectStatus = 0;

    CHK(ppSctpSession != NULL && pSctpSessionCallbacks != NULL, STATUS_NULL_ARG);

    pSctpSession = (PSctpSession) MEMALLOC(SIZEOF(SctpSession));
    CHK(pSctpSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSctpSession->sctpSessionCallbacks = *pSctpSessionCallbacks;

    CHK_STATUS(initSctpAddrConn(pSctpSession, &localConn));
    CHK_STATUS(initSctpAddrConn(pSctpSession, &remoteConn));

    CHK((pSctpSession->socket = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, onSctpInboundPacket, NULL, 0, pSctpSession)) != NULL, STATUS_SCTP_SESSION_SETUP_FAILED);
    usrsctp_register_address(pSctpSession);
    CHK_STATUS(configureSctpSocket(pSctpSession->socket));

    CHK(usrsctp_bind(pSctpSession->socket, (struct sockaddr*) &localConn, SIZEOF(localConn)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);

    connectStatus = usrsctp_connect(pSctpSession->socket, (struct sockaddr*) &remoteConn, SIZEOF(remoteConn));
    CHK(connectStatus >= 0 || errno == EINPROGRESS, STATUS_SCTP_SESSION_SETUP_FAILED);

    MEMSET(&params, 0x00, SIZEOF(struct sctp_paddrparams));
    memcpy(&params.spp_address, &remoteConn, SIZEOF(remoteConn));
    params.spp_flags = SPP_PMTUD_DISABLE;
    params.spp_pathmtu = SCTP_MTU;
    CHK(usrsctp_setsockopt(pSctpSession->socket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &params, SIZEOF(params)) == 0, STATUS_SCTP_SESSION_SETUP_FAILED);


CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeSctpSession(&pSctpSession);
    }

    *ppSctpSession = pSctpSession;

    LEAVES();
    return retStatus;
}

STATUS freeSctpSession(PSctpSession* ppSctpSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSctpSession pSctpSession;

    CHK(ppSctpSession != NULL, STATUS_NULL_ARG);

    pSctpSession = *ppSctpSession;

    CHK(pSctpSession != NULL, retStatus);

    usrsctp_deregister_address(pSctpSession);
    if (pSctpSession->socket != NULL) {
        usrsctp_shutdown(pSctpSession->socket, SHUT_RDWR);
        usrsctp_close(pSctpSession->socket);
    }

    SAFE_MEMFREE(*ppSctpSession);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS sctpSessionWriteMessage(PSctpSession pSctpSession, UINT32 streamId, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    struct sctp_sendv_spa spa = {0};

    CHK(pSctpSession != NULL && pMessage != NULL, STATUS_NULL_ARG);
    spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
    spa.sendv_sndinfo.snd_sid = streamId;

    putInt32((PINT32) &spa.sendv_sndinfo.snd_ppid, isBinary ? SCTP_PPID_BINARY : SCTP_PPID_STRING);
    CHK(usrsctp_sendv(pSctpSession->socket, pMessage, pMessageLen, NULL, 0, &spa, SIZEOF(spa), SCTP_SENDV_SPA, 0) > 0, STATUS_INTERNAL_ERROR);

CleanUp:
    LEAVES();
    return retStatus;
}

INT32 onSctpOutboundPacket(PVOID addr, PVOID data, ULONG length, UINT8 tos, UINT8 set_df)
{
    UNUSED_PARAM(tos);
    UNUSED_PARAM(set_df);

    PSctpSession pSctpSession = (PSctpSession) addr;
    pSctpSession->sctpSessionCallbacks.outboundPacketFunc(pSctpSession->sctpSessionCallbacks.customData, data, length);

    return 0;
}

STATUS putSctpPacket(PSctpSession pSctpSession, PBYTE buf, UINT32 bufLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    usrsctp_conninput(pSctpSession, buf, bufLen, 0);

    LEAVES();
    return retStatus;
}

STATUS handleDcepPacket(PSctpSession pSctpSession, UINT32 streamId, PBYTE data, SIZE_T length)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 labelLength = 0;

    // Assert that is DCEP of type DataChannelOpen
    CHK(length > SCTP_DCEP_HEADER_LENGTH && data[0] == DCEP_DATA_CHANNEL_OPEN, STATUS_SUCCESS);

    MEMCPY(&labelLength, data + 8, SIZEOF(UINT16));
    putInt16((PINT16) &labelLength, labelLength);

    CHK((labelLength + SCTP_DCEP_HEADER_LENGTH) >= length, STATUS_SCTP_INVALID_DCEP_PACKET);

    pSctpSession->sctpSessionCallbacks.dataChannelOpenFunc(
            pSctpSession->sctpSessionCallbacks.customData,
            streamId,
            data + SCTP_DCEP_HEADER_LENGTH,
            labelLength
    );

CleanUp:
    LEAVES();
    return retStatus;
}

INT32 onSctpInboundPacket(struct socket* sock, union sctp_sockstore addr, PVOID data, ULONG length, struct sctp_rcvinfo rcv, INT32 flags, PVOID ulp_info)
{
    UNUSED_PARAM(sock);
    UNUSED_PARAM(addr);
    UNUSED_PARAM(flags);
    STATUS retStatus = STATUS_SUCCESS;
    PSctpSession pSctpSession = (PSctpSession) ulp_info;
    BOOL isBinary = FALSE;

    rcv.rcv_ppid = ntohl(rcv.rcv_ppid);
    switch (rcv.rcv_ppid) {
        case SCTP_PPID_DCEP:
            CHK_STATUS(handleDcepPacket(pSctpSession, rcv.rcv_sid, data, length));
            break;
        case SCTP_PPID_BINARY:
        case SCTP_PPID_BINARY_EMPTY:
            isBinary = TRUE;
            // fallthrough
        case SCTP_PPID_STRING:
        case SCTP_PPID_STRING_EMPTY:
            pSctpSession->sctpSessionCallbacks.dataChannelMessageFunc(
                    pSctpSession->sctpSessionCallbacks.customData,
                    rcv.rcv_sid,
                    isBinary,
                    data,
                    length
            );
            break;
        default:
            DLOGI("Unhandled PPID on incoming SCTP message %d", rcv.rcv_ppid);
            break;
    }

CleanUp:

    return 1;
}
