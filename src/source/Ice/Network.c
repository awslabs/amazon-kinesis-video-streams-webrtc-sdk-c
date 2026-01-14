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
        // Skip inactive interfaces and loop back interfaces
        if (aa->OperStatus == IfOperStatusUp && aa->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
            char ifa_name[BUFSIZ];
            MEMSET(ifa_name, 0, BUFSIZ);
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
                        // Ignore unspecified addres: the other peer can't use this address
                        // Ignore link local: not very useful and will add work unnecessarily
                        // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                        if (IN6_IS_ADDR_UNSPECIFIED(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) ||
                            IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
                            continue;
                        }
                        MEMCPY(destIpList[ipCount].address, &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                    }

                    // in case of overfilling destIpList
                    ipCount++;
                }
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
                // The callback evaluates to a FALSE if the application is interested in disallowing an interface
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
                    // Ignore unspecified address: the other peer can't use this address
                    // Ignore link local: not very useful and will add work unnecessarily
                    // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                    if (IN6_IS_ADDR_UNSPECIFIED(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) ||
                        IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
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

#if defined(HAVE_SOCKETPAIR)
STATUS createSocketPair(INT32 (*pSocketPair)[2])
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSocketPair != NULL, STATUS_NULL_ARG);
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, *pSocketPair) == -1) {
        DLOGE("socketpair() failed to create a pair of sockets with errno %s", getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_CREATE_SOCKET_PAIR_FAILED);
    }
CleanUp:
    return retStatus;
}
#endif

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
#ifdef NO_SIGNAL_SOCK_OPT
    optionValue = 1;
    if (setsockopt(sockfd, SOL_SOCKET, NO_SIGNAL_SOCK_OPT, &optionValue, SIZEOF(optionValue)) < 0) {
        DLOGD("setsockopt() NO_SIGNAL_SOCK_OPT failed with errno %s", getErrorString(getErrorCode()));
    }
#endif /* NO_SIGNAL_SOCK_OPT */
    if (sendBufSize > 0 && setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendBufSize, SIZEOF(sendBufSize)) < 0) {
        DLOGW("setsockopt() SO_SNDBUF failed with errno %s", getErrorString(getErrorCode()));
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
    CHK_ERR(retVal >= 0 || getErrorCode() == KVS_SOCKET_IN_PROGRESS, STATUS_SOCKET_CONNECT_FAILED, "connect() failed with errno %s",
            getErrorString(getErrorCode()));

CleanUp:
    return retStatus;
}

STATUS socketWrite(INT32 sockfd, const void* pBuffer, SIZE_T length)
{
    STATUS retStatus = STATUS_SUCCESS;
    ssize_t ret = (ssize_t) length;
#ifndef _WIN32
    if (ret != write(sockfd, pBuffer, length)) {
        DLOGW("write() failed to write over socket with errno %s", getErrorString(getErrorCode()));
        CHK(FALSE, STATUS_SOCKET_WRITE_FAILED);
    }
#endif
CleanUp:
    return retStatus;
}

BOOL isIpAddr(PCHAR hostname, UINT16 length)
{
    UINT32 offset = 0;
    UINT32 ip_1, ip_2, ip_3, ip_4, ip_5, ip_6, ip_7, ip_8;

    if (hostname == NULL) {
        DLOGW("Provided NULL hostname.");
        return FALSE;
    }
    if (length >= MAX_ICE_CONFIG_URI_LEN) {
        DLOGW("Provided invalid hostname length: %u.", length);
        return FALSE;
    }

    // Check if IPv4 address.
    if (SSCANF(hostname, "%u.%u.%u.%u%n", &ip_1, &ip_2, &ip_3, &ip_4, &offset) == 4 && ip_1 <= 255 && ip_2 <= 255 && ip_3 <= 255 && ip_4 <= 255 &&
        offset == STRLEN(hostname)) {
        return TRUE;
    }

    // Check if IPv6 address.
    // NOTE: This IPv6 address check assumes the full notation is used without any compression (e.g., no '::' usage).
    offset = 0;
    if (SSCANF(hostname, "%x:%x:%x:%x:%x:%x:%x:%x%n", &ip_1, &ip_2, &ip_3, &ip_4, &ip_5, &ip_6, &ip_7, &ip_8, &offset) == 8 &&
        // Verify that the entire input was consumed - do not allow extra characters.
        offset == STRLEN(hostname) &&

        // Check the validity of each hextet.
        ip_1 <= 0xFFFF && ip_2 <= 0xFFFF && ip_3 <= 0xFFFF && ip_4 <= 0xFFFF && ip_5 <= 0xFFFF && ip_6 <= 0xFFFF && ip_7 <= 0xFFFF && ip_8 <= 0xFFFF)

    {
        return TRUE;
    }

    return FALSE;
}

STATUS getIpAddrFromDnsHostname(PCHAR hostname, PCHAR address, UINT16 lengthSrc, UINT16 maxLenDst)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 i = 0, j = 0;
    UINT16 hostNameLen = lengthSrc;
    CHK(hostname != NULL && address != NULL, STATUS_NULL_ARG);
    CHK(hostNameLen > 0 && hostNameLen < MAX_ICE_CONFIG_URI_LEN, STATUS_INVALID_ARG);

    // TURN server URLs conform with the public IPv4 DNS hostname format defined here:
    // https://docs.aws.amazon.com/vpc/latest/userguide/vpc-dns.html#vpc-dns-hostnames
    // So, we first try to parse the IP from the hostname if it conforms to this format
    // For example: 35-90-63-38.t-ae7dd61a.kinesisvideo.us-west-2.amazonaws.com
    while (hostNameLen > 0 && hostname[i] != '.') {
        if (hostname[i] >= '0' && hostname[i] <= '9') {
            if (j > maxLenDst) {
                DLOGW("Generated address is past allowed size");
                retStatus = STATUS_INVALID_ADDRESS_LENGTH;
                break;
            }
            address[j] = hostname[i];
        } else if (hostname[i] == '-') {
            if (j > maxLenDst) {
                DLOGW("Generated address is past allowed size");
                retStatus = STATUS_INVALID_ADDRESS_LENGTH;
                break;
            }
            address[j] = '.';
        } else {
            DLOGW("Received unexpected hostname format: %s", hostname);
            break;
        }
        j++;
        i++;
        hostNameLen--;
    }

    address[j] = '\0';

CleanUp:
    return retStatus;
}

// NOTE: IPv6 address parsing assumes the full notation is used without any compression (e.g., no '::' usage).
STATUS getDualStackIpAddrFromDnsHostname(PCHAR hostname, PCHAR ipv4Address, PCHAR ipv6Address, UINT16 lengthSrc, UINT16 maxLenV4Dst,
                                         UINT16 maxLenV6Dst)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 i = 0, j = 0;
    UINT16 hostNameLen = lengthSrc;
    CHK(hostname != NULL && ipv4Address != NULL && ipv6Address != NULL, STATUS_NULL_ARG);
    CHK(hostNameLen > 0 && hostNameLen < MAX_ICE_CONFIG_URI_LEN, STATUS_INVALID_ARG);
    CHAR c;

    // Dual-stack TURN server URLs conform with the following IPv4 and IPv6 DNS hostname format:
    // 35-90-63-38_2001-0db8-85a3-0000-0000-8a2e-0370-7334.t-ae7dd61a.kinesisvideo.us-west-2.api.aws

    // Parse the IPv4 portion.
    while (hostNameLen > 0 && hostname[i] != '_') {
        CHK_WARN(j < maxLenV4Dst, STATUS_INVALID_ADDRESS_LENGTH, "Generated IPv4 address is past allowed size.");

        c = hostname[i];

        if (c >= '0' && c <= '9') {
            ipv4Address[j] = hostname[i];
        } else if (hostname[i] == '-') {
            ipv4Address[j] = '.';
        } else {
            CHK_WARN(FALSE, STATUS_INVALID_ARG, "Parsing IPv4 address failed, received unexpected hostname format: %s", hostname);
        }

        j++;
        i++;
        hostNameLen--;
    }
    ipv4Address[j] = '\0';

    j = 0;
    i++;
    hostNameLen--;

    // Parse the IPv6 portion.
    while (hostNameLen > 0 && hostname[i] != '.') {
        CHK_WARN(j < maxLenV6Dst, STATUS_INVALID_ADDRESS_LENGTH, "Generated IPv6 address is past allowed size.");

        c = hostname[i];

        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            ipv6Address[j] = hostname[i];
        } else if (hostname[i] == '-') {
            ipv6Address[j] = ':';
        } else {
            CHK_WARN(FALSE, STATUS_INVALID_ARG, "Parsing IPv6 address failed, received unexpected hostname format: %s", hostname);
        }

        j++;
        i++;
        hostNameLen--;
    }
    ipv6Address[j] = '\0';

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        if (ipv4Address != NULL) {
            ipv4Address[0] = '\0';
        }
        if (ipv6Address != NULL) {
            ipv6Address[0] = '\0';
        }
        DLOGW("Failed to parse dual-stack address with error 0x%08x from hostname: %s", retStatus, hostname);
    }

    return retStatus;
}

STATUS getIpWithHostName(PCHAR hostname, PDualKvsIpAddresses destIps)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 errCode;
    UINT16 hostnameLen, addrLen;
    PCHAR errStr;
    struct addrinfo *res, *rp;
    BOOL ipv4Resolved = FALSE;
    BOOL ipv6Resolved = FALSE;
    struct sockaddr_in* ipv4SockAddr;
    struct sockaddr_in6* ipv6SockAddr;
    struct in_addr inaddr;
    struct in6_addr in6addr;

    CHAR addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {'\0'};
    CHAR ipv4Addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {'\0'};
    CHAR ipv6Addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {'\0'};
    BOOL isStunServer;
    BOOL wasAddressParseSuccessful = FALSE;
    BOOL useDualStackMode = FALSE;

    CHK(hostname != NULL, STATUS_NULL_ARG);
    DLOGI("ICE SERVER Hostname received: %s", hostname);

    hostnameLen = STRLEN(hostname);
    addrLen = SIZEOF(addr);
    isStunServer = STRNCMP(hostname, KINESIS_VIDEO_STUN_URL_PREFIX, KINESIS_VIDEO_STUN_URL_PREFIX_LENGTH) == 0;

    useDualStackMode = isEnvVarEnabled(USE_DUAL_STACK_ENDPOINTS_ENV_VAR);

    // Adding this check in case we directly get an IP address. With the current usage pattern,
    // there is no way this function would receive an address directly, but having this check
    // in place anyways
    if (isIpAddr(hostname, hostnameLen)) {
        MEMCPY(addr, hostname, hostnameLen);
        addr[hostnameLen] = '\0';
    } else if (!isStunServer) {
        // Try to parse the address from the TURN server hostname.

        if (useDualStackMode) {
            DLOGD("Attempting to parse dual-stack IP addresses from TURN server hostname: %s", hostname);

            retStatus = getDualStackIpAddrFromDnsHostname(hostname, ipv4Addr, ipv6Addr, hostnameLen, ARRAY_SIZE(ipv4Addr), ARRAY_SIZE(ipv6Addr));
            if (retStatus == STATUS_SUCCESS) {
                DLOGD("Parsed dual-stack IP addresses from TURN server hostname: IPv4 %s, IPv6 %s", ipv4Addr, ipv6Addr);
            }
        } else {
            DLOGD("Attempting to parse IP address from legacy TURN server hostname: %s", hostname);

            retStatus = getIpAddrFromDnsHostname(hostname, addr, hostnameLen, addrLen);
            if (retStatus == STATUS_SUCCESS) {
                DLOGD("Parsed IP address from legacy TURN server hostname: %s", addr);
            }
        }
    }

    wasAddressParseSuccessful = isIpAddr(addr, STRLEN(addr)) || (isIpAddr(ipv4Addr, STRLEN(ipv4Addr)) && isIpAddr(ipv6Addr, STRLEN(ipv6Addr)));

    // Fall back to getaddrinfo if parsing the address from the hostname was not possible or failed.
    if (!wasAddressParseSuccessful || retStatus != STATUS_SUCCESS) {
        // Only print the warning for TURN servers since STUN addresses don't get parsed from the hostname.
        if (!isStunServer) {
            DLOGW("Parsing for TURN address failed for %s, fallback to getaddrinfo", hostname);
        }

        // Skip the IPv6 resolution if dual stack env var is not set.
        if (!useDualStackMode) {
            ipv6Resolved = TRUE;
        }

        errCode = getaddrinfo(hostname, NULL, NULL, &res);
        if (errCode != 0) {
            errStr = errCode == EAI_SYSTEM ? (strerror(errno)) : ((PCHAR) gai_strerror(errCode));
            CHK_ERR(FALSE, STATUS_RESOLVE_HOSTNAME_FAILED, "getaddrinfo() with errno %s", errStr);
        }
        for (rp = res; rp != NULL && !(ipv4Resolved && ipv6Resolved); rp = rp->ai_next) {
            if (rp->ai_family == AF_INET && !ipv4Resolved) {
                ipv4SockAddr = (struct sockaddr_in*) rp->ai_addr;
                destIps->ipv4Address.family = KVS_IP_FAMILY_TYPE_IPV4;
                MEMCPY(destIps->ipv4Address.address, &ipv4SockAddr->sin_addr, IPV4_ADDRESS_LENGTH);

                CHK_STATUS(getIpAddrStr(&(destIps->ipv4Address), ipv4Addr, ARRAY_SIZE(ipv4Addr)));
                DLOGD("Found an IPv4 ICE server addresss: %s", ipv4Addr);

                ipv4Resolved = TRUE;

            } else if (rp->ai_family == AF_INET6 && !ipv6Resolved) {
                ipv6SockAddr = (struct sockaddr_in6*) rp->ai_addr;
                destIps->ipv6Address.family = KVS_IP_FAMILY_TYPE_IPV6;
                MEMCPY(destIps->ipv6Address.address, &ipv6SockAddr->sin6_addr, IPV6_ADDRESS_LENGTH);

                CHK_STATUS(getIpAddrStr(&(destIps->ipv6Address), ipv6Addr, ARRAY_SIZE(ipv6Addr)));
                DLOGD("Found an IPv6 ICE server addresss: %s", ipv6Addr);

                ipv6Resolved = TRUE;
            }
        }
        freeaddrinfo(res);
        CHK_ERR(ipv4Resolved || ipv6Resolved, STATUS_HOSTNAME_NOT_FOUND, "Could not find network address of %s", hostname);
    } else {
        // Address parsing was successful. Capture the address(es).

        // Both addresses should be present in TURN url for dual-stack case.
        if (STRLEN(ipv4Addr) > 0 && STRLEN(ipv6Addr) > 0) {
            // Dual-stack case.

            if (inet_pton(AF_INET, ipv4Addr, &inaddr) == 1) {
                destIps->ipv4Address.family = KVS_IP_FAMILY_TYPE_IPV4;
                MEMCPY(destIps->ipv4Address.address, &inaddr, IPV4_ADDRESS_LENGTH);
            } else {
                DLOGW("inet_pton failed on IPv4 ICE server address: %s", ipv4Addr);
                retStatus = STATUS_INVALID_ARG;
            }

            if (inet_pton(AF_INET6, ipv6Addr, &in6addr) == 1) {
                destIps->ipv6Address.family = KVS_IP_FAMILY_TYPE_IPV6;
                MEMCPY(destIps->ipv6Address.address, &in6addr, IPV6_ADDRESS_LENGTH);
            } else {
                DLOGW("inet_pton failed on IPv6 ICE server address: %s", ipv6Addr);
                retStatus = STATUS_INVALID_ARG;
            }

        } else {
            // Legacy case.

            if (inet_pton(AF_INET, addr, &inaddr) == 1) {
                destIps->ipv4Address.family = KVS_IP_FAMILY_TYPE_IPV4;
                MEMCPY(destIps->ipv4Address.address, &inaddr, IPV4_ADDRESS_LENGTH);
            } else if (inet_pton(AF_INET6, addr, &in6addr) == 1) {
                // This case will never happen with current TURN server URL format,
                // but adding in case a hardcoded direct IPv6 address gets used.
                destIps->ipv6Address.family = KVS_IP_FAMILY_TYPE_IPV6;
                MEMCPY(destIps->ipv6Address.address, &in6addr, IPV6_ADDRESS_LENGTH);
            } else {
                DLOGW("inet_pton failed on legacy ICE server address: %s", addr);
                retStatus = STATUS_INVALID_ARG;
            }
        }
    }

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
