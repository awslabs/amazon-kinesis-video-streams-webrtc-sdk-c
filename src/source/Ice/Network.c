/**
 * Kinesis Video Producer Host Info
 */
#define LOG_CLASS "Network"
#include "../Include_i.h"

STATUS getLocalhostIpAddresses(PKvsIpAddress destIpList, PUINT32 pDestIpListLen, IceSetInterfaceFilterFunc filter, UINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 ipCount = 0, destIpListLen;
    BOOL filterSet = TRUE;

#ifdef _WIN32
    DWORD retWinStatus, sizeAAPointer;
    PIP_ADAPTER_ADDRESSES adapterAddresses, aa = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS ua;
#else
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
#endif
    struct sockaddr_in* pIpv4Addr = NULL;
    struct sockaddr_in6* pIpv6Addr = NULL;

    CHK(destIpList != NULL && pDestIpListLen != NULL, STATUS_NULL_ARG);
    CHK(*pDestIpListLen != 0, STATUS_INVALID_ARG);

    destIpListLen = *pDestIpListLen;
#ifdef _WIN32
    retWinStatus = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &sizeAAPointer);
    CHK(retWinStatus == ERROR_BUFFER_OVERFLOW, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);

    adapterAddresses = (PIP_ADAPTER_ADDRESSES) MEMALLOC(sizeAAPointer);

    retWinStatus = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses, &sizeAAPointer);
    CHK(retWinStatus == ERROR_SUCCESS, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);

    for (aa = adapterAddresses; aa != NULL && ipCount < destIpListLen; aa = aa->Next) {
        char ifa_name[BUFSIZ];
        memset(ifa_name, 0, BUFSIZ);
        WideCharToMultiByte(CP_ACP, 0, aa->FriendlyName, wcslen(aa->FriendlyName), ifa_name, BUFSIZ, NULL, NULL);

        for (ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            if (filter != NULL) {
                DLOGI("Callback set to allow network interface filtering");
                // The callback evaluates to a FALSE if the application is interested in black listing an interface
                if (filter(customData, ifa_name) == FALSE) {
                    filterSet = FALSE;
                } else {
                    filterSet = TRUE;
                }
            }

            // If filter is set, ensure the details are collected for the interface
            if (filterSet == TRUE) {
                int family = ua->Address.lpSockaddr->sa_family;

                if (family == AF_INET) {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV4;
                    destIpList[ipCount].port = 0;

                    pIpv4Addr = (struct sockaddr_in*) (ua->Address.lpSockaddr);
                    MEMCPY(destIpList[ipCount].address, &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
                } else {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV6;
                    destIpList[ipCount].port = 0;

                    pIpv6Addr = (struct sockaddr_in6*) (ua->Address.lpSockaddr);
                    // Ignore link local: not very useful and will add work unnecessarily
                    // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                    if (IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
                        continue;
                    }
                    MEMCPY(destIpList[ipCount].address, &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                }

                // in case of overfilling destIpList
                ipCount++;
            }
        }
    }
#else
    CHK(getifaddrs(&ifaddr) != -1, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);
    for (ifa = ifaddr; ifa != NULL && ipCount < destIpListLen; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && (ifa->ifa_flags & IFF_LOOPBACK) == 0 && // ignore loopback interface
            (ifa->ifa_flags & IFF_RUNNING) > 0 &&                            // interface has to be allocated
            (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
            // mark vpn interface
            destIpList[ipCount].isPointToPoint = ((ifa->ifa_flags & IFF_POINTOPOINT) != 0);

            if (filter != NULL) {
                // The callback evaluates to a FALSE if the application is interested in black listing an interface
                if (filter(customData, ifa->ifa_name) == FALSE) {
                    filterSet = FALSE;
                } else {
                    filterSet = TRUE;
                }
            }

            // If filter is set, ensure the details are collected for the interface
            if (filterSet == TRUE) {
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV4;
                    destIpList[ipCount].port = 0;
                    pIpv4Addr = (struct sockaddr_in*) ifa->ifa_addr;
                    MEMCPY(destIpList[ipCount].address, &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);

                } else {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV6;
                    destIpList[ipCount].port = 0;
                    pIpv6Addr = (struct sockaddr_in6*) ifa->ifa_addr;
                    // Ignore link local: not very useful and will add work unnecessarily
                    // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                    if (IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
                        continue;
                    }
                    MEMCPY(destIpList[ipCount].address, &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                }

                // in case of overfilling destIpList
                ipCount++;
            }
        }
    }
#endif

CleanUp:

#ifdef _WIN32
    if (adapterAddresses != NULL) {
        SAFE_MEMFREE(adapterAddresses);
    }
#else
    if (ifaddr != NULL) {
        freeifaddrs(ifaddr);
    }
#endif

    if (pDestIpListLen != NULL) {
        *pDestIpListLen = ipCount;
    }

    LEAVES();
    return retStatus;
}

STATUS createSocket(KVS_IP_FAMILY_TYPE familyType, KVS_SOCKET_PROTOCOL protocol, UINT32 sendBufSize, PINT32 pOutSockFd)
{
    STATUS retStatus = STATUS_SUCCESS;

    INT32 sockfd, sockType, flags;
    INT32 optionValue;

    CHK(pOutSockFd != NULL, STATUS_NULL_ARG);

    sockType = protocol == KVS_SOCKET_PROTOCOL_UDP ? SOCK_DGRAM : SOCK_STREAM;

    sockfd = socket(familyType == KVS_IP_FAMILY_TYPE_IPV4 ? AF_INET : AF_INET6, sockType, 0);
    if (sockfd == -1) {
        DLOGW("socket() failed to create socket with errno %s", getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_CREATE_UDP_SOCKET_FAILED);
    }

    optionValue = 1;
    if (setsockopt(sockfd, SOL_SOCKET, NO_SIGNAL, &optionValue, SIZEOF(optionValue)) < 0) {
        DLOGD("setsockopt() failed with errno %s", getErrorString(getErrorCode()));
    }

    if (sendBufSize > 0 && setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendBufSize, SIZEOF(sendBufSize)) < 0) {
        DLOGW("setsockopt() failed with errno %s", getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_SOCKET_SET_SEND_BUFFER_SIZE_FAILED);
    }

    *pOutSockFd = (INT32) sockfd;

#ifdef _WIN32
    UINT32 nonblock = 1;
    ioctlsocket(sockfd, FIONBIO, &nonblock);
#else
    // Set the non-blocking mode for the socket
    flags = fcntl(sockfd, F_GETFL, 0);
    CHK_ERR(flags >= 0, STATUS_GET_SOCKET_FLAG_FAILED, "Failed to get the socket flags with system error %s", strerror(errno));
    CHK_ERR(0 <= fcntl(sockfd, F_SETFL, flags | O_NONBLOCK), STATUS_SET_SOCKET_FLAG_FAILED, "Failed to Set the socket flags with system error %s",
            strerror(errno));
#endif

    // done at this point for UDP
    CHK(protocol == KVS_SOCKET_PROTOCOL_TCP, retStatus);

    /* disable Nagle algorithm to not delay sending packets. We should have enough density to justify using it. */
    optionValue = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optionValue, SIZEOF(optionValue)) < 0) {
        DLOGW("setsockopt() TCP_NODELAY failed with errno %s", getErrorString(getErrorCode()));
    }

CleanUp:

    return retStatus;
}

STATUS closeSocket(INT32 sockfd)
{
    STATUS retStatus = STATUS_SUCCESS;

#ifdef _WIN32
    CHK_ERR(closesocket(sockfd) == 0, STATUS_CLOSE_SOCKET_FAILED, "Failed to close the socket %s", getErrorString(getErrorCode()));
#else
    CHK_ERR(close(sockfd) == 0, STATUS_CLOSE_SOCKET_FAILED, "Failed to close the socket %s", strerror(errno));
#endif

CleanUp:

    return retStatus;
}

STATUS socketBind(PKvsIpAddress pHostIpAddress, INT32 sockfd)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct sockaddr_in ipv4Addr;
    struct sockaddr_in6 ipv6Addr;
    struct sockaddr* sockAddr = NULL;
    socklen_t addrLen;

    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    CHK(pHostIpAddress != NULL, STATUS_NULL_ARG);

    if (pHostIpAddress->family == KVS_IP_FAMILY_TYPE_IPV4) {
        MEMSET(&ipv4Addr, 0x00, SIZEOF(ipv4Addr));
        ipv4Addr.sin_family = AF_INET;
        ipv4Addr.sin_port = 0; // use next available port
        MEMCPY(&ipv4Addr.sin_addr, pHostIpAddress->address, IPV4_ADDRESS_LENGTH);
        // TODO: Properly handle the non-portable sin_len field if needed per https://issues.amazon.com/KinesisVideo-4952
        // ipv4Addr.sin_len = SIZEOF(ipv4Addr);
        sockAddr = (struct sockaddr*) &ipv4Addr;
        addrLen = SIZEOF(struct sockaddr_in);

    } else {
        MEMSET(&ipv6Addr, 0x00, SIZEOF(ipv6Addr));
        ipv6Addr.sin6_family = AF_INET6;
        ipv6Addr.sin6_port = 0; // use next available port
        MEMCPY(&ipv6Addr.sin6_addr, pHostIpAddress->address, IPV6_ADDRESS_LENGTH);
        // TODO: Properly handle the non-portable sin6_len field if needed per https://issues.amazon.com/KinesisVideo-4952
        // ipv6Addr.sin6_len = SIZEOF(ipv6Addr);
        sockAddr = (struct sockaddr*) &ipv6Addr;
        addrLen = SIZEOF(struct sockaddr_in6);
    }

    if (bind(sockfd, sockAddr, addrLen) < 0) {
        CHK_STATUS(getIpAddrStr(pHostIpAddress, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
        DLOGW("bind() failed for ip%s address: %s, port %u with errno %s", IS_IPV4_ADDR(pHostIpAddress) ? EMPTY_STRING : "V6", ipAddrStr,
              (UINT16) getInt16(pHostIpAddress->port), getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_BINDING_SOCKET_FAILED);
    }

    if (getsockname(sockfd, sockAddr, &addrLen) < 0) {
        DLOGW("getsockname() failed with errno %s", getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_GET_PORT_NUMBER_FAILED);
    }

    pHostIpAddress->port = (UINT16) pHostIpAddress->family == KVS_IP_FAMILY_TYPE_IPV4 ? ipv4Addr.sin_port : ipv6Addr.sin6_port;

CleanUp:
    return retStatus;
}

STATUS socketConnect(PKvsIpAddress pPeerAddress, INT32 sockfd)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct sockaddr_in ipv4PeerAddr;
    struct sockaddr_in6 ipv6PeerAddr;
    struct sockaddr* peerSockAddr = NULL;
    socklen_t addrLen;
    INT32 retVal;

    CHK(pPeerAddress != NULL, STATUS_NULL_ARG);

    if (pPeerAddress->family == KVS_IP_FAMILY_TYPE_IPV4) {
        MEMSET(&ipv4PeerAddr, 0x00, SIZEOF(ipv4PeerAddr));
        ipv4PeerAddr.sin_family = AF_INET;
        ipv4PeerAddr.sin_port = pPeerAddress->port;
        MEMCPY(&ipv4PeerAddr.sin_addr, pPeerAddress->address, IPV4_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr*) &ipv4PeerAddr;
        addrLen = SIZEOF(struct sockaddr_in);
    } else {
        MEMSET(&ipv6PeerAddr, 0x00, SIZEOF(ipv6PeerAddr));
        ipv6PeerAddr.sin6_family = AF_INET6;
        ipv6PeerAddr.sin6_port = pPeerAddress->port;
        MEMCPY(&ipv6PeerAddr.sin6_addr, pPeerAddress->address, IPV6_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr*) &ipv6PeerAddr;
        addrLen = SIZEOF(struct sockaddr_in6);
    }

    retVal = connect(sockfd, peerSockAddr, addrLen);
    CHK_ERR(retVal >= 0 || getErrorCode() == EINPROGRESS, STATUS_SOCKET_CONNECT_FAILED, "connect() failed with errno %s",
            getErrorString(getErrorCode()));

CleanUp:
    return retStatus;
}

STATUS getIpWithHostName(PCHAR hostname, PKvsIpAddress destIp)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 errCode;
    PCHAR errStr;
    struct addrinfo *res, *rp;
    BOOL resolved = FALSE;
    struct sockaddr_in* ipv4Addr;
    struct sockaddr_in6* ipv6Addr;

    CHK(hostname != NULL, STATUS_NULL_ARG);

    errCode = getaddrinfo(hostname, NULL, NULL, &res);
    if (errCode != 0) {
        errStr = errCode == EAI_SYSTEM ? strerror(errno) : (PCHAR) gai_strerror(errCode);
        CHK_ERR(FALSE, STATUS_RESOLVE_HOSTNAME_FAILED, "getaddrinfo() with errno %s", errStr);
    }

    for (rp = res; rp != NULL && !resolved; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            ipv4Addr = (struct sockaddr_in*) rp->ai_addr;
            destIp->family = KVS_IP_FAMILY_TYPE_IPV4;
            MEMCPY(destIp->address, &ipv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
            resolved = TRUE;
        } else if (rp->ai_family == AF_INET6) {
            ipv6Addr = (struct sockaddr_in6*) rp->ai_addr;
            destIp->family = KVS_IP_FAMILY_TYPE_IPV6;
            MEMCPY(destIp->address, &ipv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
            resolved = TRUE;
        }
    }

    freeaddrinfo(res);
    CHK_ERR(resolved, STATUS_HOSTNAME_NOT_FOUND, "could not find network address of %s", hostname);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS getIpAddrStr(PKvsIpAddress pKvsIpAddress, PCHAR pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 generatedStrLen = 0; // number of characters written if buffer is large enough not counting the null terminator

    CHK(pKvsIpAddress != NULL, STATUS_NULL_ARG);
    CHK(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);

    if (IS_IPV4_ADDR(pKvsIpAddress)) {
        generatedStrLen = SNPRINTF(pBuffer, bufferLen, "%u.%u.%u.%u", pKvsIpAddress->address[0], pKvsIpAddress->address[1], pKvsIpAddress->address[2],
                                   pKvsIpAddress->address[3]);
    } else {
        generatedStrLen = SNPRINTF(pBuffer, bufferLen, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                                   pKvsIpAddress->address[0], pKvsIpAddress->address[1], pKvsIpAddress->address[2], pKvsIpAddress->address[3],
                                   pKvsIpAddress->address[4], pKvsIpAddress->address[5], pKvsIpAddress->address[6], pKvsIpAddress->address[7],
                                   pKvsIpAddress->address[8], pKvsIpAddress->address[9], pKvsIpAddress->address[10], pKvsIpAddress->address[11],
                                   pKvsIpAddress->address[12], pKvsIpAddress->address[13], pKvsIpAddress->address[14], pKvsIpAddress->address[15]);
    }

    // bufferLen should be strictly larger than generatedStrLen because bufferLen includes null terminator
    CHK(generatedStrLen < bufferLen, STATUS_BUFFER_TOO_SMALL);

CleanUp:

    return retStatus;
}

BOOL isSameIpAddress(PKvsIpAddress pAddr1, PKvsIpAddress pAddr2, BOOL checkPort)
{
    BOOL ret;
    UINT32 addrLen;

    if (pAddr1 == NULL || pAddr2 == NULL) {
        return FALSE;
    }

    addrLen = IS_IPV4_ADDR(pAddr1) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;

    ret =
        (pAddr1->family == pAddr2->family && MEMCMP(pAddr1->address, pAddr2->address, addrLen) == 0 && (!checkPort || pAddr1->port == pAddr2->port));

    return ret;
}

#ifdef _WIN32
INT32 getErrorCode(VOID)
{
    INT32 error = WSAGetLastError();
    switch (error) {
        case WSAEWOULDBLOCK:
            error = EWOULDBLOCK;
            break;
        case WSAEINPROGRESS:
            error = EINPROGRESS;
            break;
        case WSAEISCONN:
            error = EISCONN;
            break;
        case WSAEINTR:
            error = EINTR;
            break;
        default:
            /* leave unchanged */
            break;
    }
    return error;
}
#else
INT32 getErrorCode(VOID)
{
    return errno;
}
#endif

#ifdef _WIN32
PCHAR getErrorString(INT32 error)
{
    static CHAR buffer[1024];
    switch (error) {
        case EWOULDBLOCK:
            error = WSAEWOULDBLOCK;
            break;
        case EINPROGRESS:
            error = WSAEINPROGRESS;
            break;
        case EISCONN:
            error = WSAEISCONN;
            break;
        case EINTR:
            error = WSAEINTR;
            break;
        default:
            /* leave unchanged */
            break;
    }
    if (FormatMessage((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer,
                      SIZEOF(buffer), NULL) == 0) {
        SNPRINTF(buffer, SIZEOF(buffer), "error code %d", error);
    }

    return buffer;
}
#else
PCHAR getErrorString(INT32 error)
{
    return strerror(error);
}
#endif
