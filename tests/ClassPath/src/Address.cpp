// We need our declaration
#include "../include/Network/Address.hpp"
// We also need allocation manager
#include "Platform/Platform.hpp"

#ifdef _WIN32
#if WantPingCode == 1
    #include <Icmpapi.h>
    #pragma comment(lib, "Iphlpapi.lib")
#endif
    #pragma comment(lib, "Ws2_32.lib")
#endif
#ifdef _LINUX
    #include <sys/ioctl.h>
    #include <net/if.h> // This is conflicting with the line below
    #include <net/if_arp.h>
    #include <linux/rtnetlink.h>
    #include <netinet/ip.h>
    #include <netinet/ip_icmp.h>
    #include <netinet/ether.h>
    #include <netinet/in.h>
    extern int getEthernetRate(int sock, struct ifreq * item);
    extern int getWIFIRate(int sock, const char * name);
#endif

#if defined(_MAC) // Should work on posix too
    #include <ifaddrs.h>
    #include <netinet/ip.h>
    #include <netinet/ip_icmp.h>
    #include <net/if.h>
#endif



namespace Network
{
    namespace Address
    {
        // Get the address as a UTF-8 textual value
        String IPV4::asText() const
        {
#if defined(_WIN32) || defined(NEXIO)
            return inet_ntoa(address.sin_addr);
#else   // Could've used inet_ntoa but it's deprecated.
            char buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), buffer, INET_ADDRSTRLEN);
            return buffer;
#endif
        }
        // Set the address as a UTF-8 textual value
        bool IPV4::asText(const String & textualValue)
        {
#if defined(_WIN32) || defined(NEXIO)
            struct in_addr addr;
            addr.s_addr = inet_addr((const char*)textualValue);
            if (addr.s_addr != INADDR_NONE) { address.sin_addr = addr; return true; }
            return false;
#else
            return inet_pton(AF_INET, (const char*)textualValue, &(address.sin_addr)) == 1;
#endif
        }

        // Get the port if specified in the address (0 otherwise)
        uint16 IPV4::getPort() const
        {
            return ntohs(address.sin_port);
        }
        // Set the port if possible in this address
        bool IPV4::setPort(const uint16 port)
        {
            address.sin_port = ntohs(port);
            return true;
        }

        static BaseAddress::Protocol getProtocolFromPortNumber(const uint16 port)
        {
            switch (port)
            {
            case (uint16)BaseAddress::Echo:          return BaseAddress::Echo;
            case (uint16)BaseAddress::Discard:       return BaseAddress::Discard;
            case (uint16)BaseAddress::DayTime:       return BaseAddress::DayTime;
            case (uint16)BaseAddress::FTPData:       return BaseAddress::FTPData;
            case (uint16)BaseAddress::FTP:           return BaseAddress::FTP;
            case (uint16)BaseAddress::SSH:           return BaseAddress::SSH;
            case (uint16)BaseAddress::Telnet:        return BaseAddress::Telnet;
            case (uint16)BaseAddress::SMTP:          return BaseAddress::SMTP;
            case (uint16)BaseAddress::Time:          return BaseAddress::Time;
            case (uint16)BaseAddress::WINS:          return BaseAddress::WINS;
            case (uint16)BaseAddress::WhoIs:         return BaseAddress::WhoIs;
            case (uint16)BaseAddress::DNS:           return BaseAddress::DNS;
            case (uint16)BaseAddress::DHCP:          return BaseAddress::DHCP;
            case (uint16)BaseAddress::BootPClient:   return BaseAddress::BootPClient;
            case (uint16)BaseAddress::TFTP:          return BaseAddress::TFTP;
            case (uint16)BaseAddress::HTTP:          return BaseAddress::HTTP;
            case (uint16)BaseAddress::POP3:          return BaseAddress::POP3;
            case (uint16)BaseAddress::NNTP:          return BaseAddress::NNTP;
            case (uint16)BaseAddress::NTP:           return BaseAddress::NTP;
            case (uint16)BaseAddress::IMAP:          return BaseAddress::IMAP;
            case (uint16)BaseAddress::SNMP:          return BaseAddress::SNMP;
            case (uint16)BaseAddress::IRC:           return BaseAddress::IRC;
            case (uint16)BaseAddress::LDAP:          return BaseAddress::LDAP;
            case (uint16)BaseAddress::HTTPS:         return BaseAddress::HTTPS;
            case (uint16)BaseAddress::Samba:         return BaseAddress::Samba;
            case (uint16)BaseAddress::SMTPS:         return BaseAddress::SMTPS;
            case (uint16)BaseAddress::DHCP6:         return BaseAddress::DHCP6;
            case (uint16)BaseAddress::RTSP:          return BaseAddress::RTSP;
            case (uint16)BaseAddress::NNTPS:         return BaseAddress::NNTPS;
            default:
                return BaseAddress::Unknown;
            }
        }


        // Get the protocol (if specified in this address)
        IPV4::Protocol IPV4::getProtocol() const
        {
            return getProtocolFromPortNumber(ntohs(address.sin_port));
        }
        // Set the protocol (if specified in this address)
        bool IPV4::setProtocol(const IPV4::Protocol protocol)
        {
            return setPort((uint16)protocol);
        }

        // Check if the address is valid
        bool IPV4::isValid() const
        {
            return (address.sin_addr.s_addr != 0xFFFFFFFF);
        }

        // Get a bindable address.
        const BaseAddress & IPV4::getBindableAddress() const
        {   // IPv4 address are bindable
            return *this;
        }
        // Get the low level object for this address
        const void * IPV4::getLowLevelObject() const
        {
            return &address;
        }
        // Get the low level object for this address
        const int IPV4::getLowLevelObjectSize() const
        {
            return sizeof(address);
        }



        // Query the name server from an textual address
        IPV4 IPV4::queryNameServer(const String & host, const Time::TimeOut & timeout)
        {
            if (timeout <= 0) return IPV4();
#ifndef NEXIO
            if (timeout == DefaultTimeOut)
            {
                struct addrinfo hints = {0}, *res = 0;
                hints.ai_family = AF_INET;
                hints.ai_flags = AI_NUMERICHOST;

                // First try without resolving the domain name
                if (getaddrinfo((const char*)host.upToFirst(":"), NULL, &hints, &res) != 0)
                {
                    // Ok, then resolve
                    hints.ai_flags = 0;
                    if (getaddrinfo((const char*)host.upToFirst(":"), NULL, &hints, &res) != 0)
                        return IPV4();
                }
                String port = host.fromFirst(":");
                if (!((struct sockaddr_in *)res[0].ai_addr)->sin_port && port.getLength())
                    ((struct sockaddr_in *)res[0].ai_addr)->sin_port = htons((unsigned int)port);


                IPV4 result((uint32)ntohl(((struct sockaddr_in *)res[0].ai_addr)->sin_addr.s_addr), (uint16)ntohs(((struct sockaddr_in *)res[0].ai_addr)->sin_port));
                freeaddrinfo(res);
                return result;
            }
#endif
            // Not implemented yet with a timeout
            return IPV4();
        }
        // Reverse query the name server for a textual address
        String IPV4::reverseQueryNameServer(const Time::TimeOut & timeout) const
        {
            if (timeout <= 0) return String();
#ifndef NEXIO
            if (timeout == DefaultTimeOut)
            {
                char hostname[NI_MAXHOST] = {0};
                if (getnameinfo((struct sockaddr*)&address, sizeof(address), hostname, NI_MAXHOST, NULL, 0, /*servInfo, NI_MAXSERV, NI_NUMERICSERV | */NI_NAMEREQD ) != 0)
                    return String();

                return hostname;
            }
#endif
            // Not implemented yet with a timeout
            return String();
        }
#if defined(_POSIX)
        // Check if the given address is an IPv4 one
        bool filterSockAddrIPV4(struct ifaddrs * addr)
        {
            return (addr && addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET);
        }
        // Check if the given address is an IPv6 one
        bool filterSockAddrIPV6(struct ifaddrs * addr)
        {
            return (addr && addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET6);
        }
#if defined(_MAC)
        // Check if the given address is a MAC address
        bool filterSockAddrEther(struct ifaddrs * addr)
        {
            return (addr && addr->ifa_addr && addr->ifa_addr->sa_family == AF_LINK);
        }
#endif
#endif
        // Get the number of local available interfaces.
        int IPV4::getLocalInterfacesCount()
        {
#ifdef MinSockCode
            return 0;
#else
    #ifdef _WIN32
            HMODULE hMod = LoadLibraryA("Iphlpapi.dll");
            if (hMod == NULL) return 0;
            DWORD (WINAPI *GetAdaptersInfo)( PIP_ADAPTER_INFO, PULONG) = (DWORD (WINAPI *)( PIP_ADAPTER_INFO, PULONG))GetProcAddress(hMod, "GetAdaptersInfo");
            if (!GetAdaptersInfo) { FreeLibrary(hMod); return 0; }

            // Make an initial call to GetAdaptersInfo to get
            // the necessary size into the ulOutBufLen variable
            ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
            GetAdaptersInfo(NULL, &ulOutBufLen);
            PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *) Platform::malloc(ulOutBufLen);
            if (pAdapterInfo == NULL) { FreeLibrary(hMod); return 0; }

            DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
            FreeLibrary(hMod);

            int count = 0;
            if (dwRetVal == NO_ERROR)
            {
                PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
                if(pAdapter)
                {
                    while(pAdapter)
                    { count ++; pAdapter = pAdapter->Next; }
                }
            }
            Platform::free(pAdapterInfo);
    #elif defined(_POSIX)
    /*
    #elif defined(_LINUX)
            char          buf[1024] = {0};
            struct ifconf ifc = {0};
            struct ifreq *ifr = NULL;

            // We need a socket to start with
            int sck = socket(AF_INET, SOCK_DGRAM, 0);
            if (sck < 0) return 0;

            // Query available interfaces.
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) { close(sck); fprintf(stderr, "Socket: Query interface count error [%s]\n", strerror(errno)); return 0; }

            // Iterate through the list of interfaces.
            ifr = ifc.ifc_req;
            int count = ifc.ifc_len / sizeof(struct ifreq);

            close(sck);
    #elif defined(_MAC)
    */
            struct ifaddrs * addresses = NULL;
            if (getifaddrs(&addresses) != 0) return 0;
            // Then count the interfaces
            int count = 0;
            struct ifaddrs * addr = addresses;
            while (addr)
            {
                if (filterSockAddrIPV4(addr))
                    count++;
                addr = addr->ifa_next;
            }
            freeifaddrs(addresses);
    #endif
            return count;
#endif
        }
        // Get the local interfaces address
        IPV4 IPV4::getLocalInterfaceAddress(const int index)
        {
            IPV4 result;
#ifndef MinSockCode
#ifdef _WIN32
            HMODULE hMod = LoadLibraryA("Iphlpapi.dll");
            if (hMod == NULL) return result;
            DWORD (WINAPI *GetAdaptersInfo)( PIP_ADAPTER_INFO, PULONG) = (DWORD (WINAPI *)( PIP_ADAPTER_INFO, PULONG))GetProcAddress(hMod, "GetAdaptersInfo");
            if (!GetAdaptersInfo) { FreeLibrary(hMod); return result; }

            // Make an initial call to GetAdaptersInfo to get
            // the necessary size into the ulOutBufLen variable
            ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
            GetAdaptersInfo(NULL, &ulOutBufLen);
            PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *) Platform::malloc(ulOutBufLen);
            if (pAdapterInfo == NULL) { FreeLibrary(hMod); return result; }


            DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
            FreeLibrary(hMod);

            if (dwRetVal == NO_ERROR)
            {
                PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
                if(pAdapter)
                {
                    int count = 0;
                    while(count < index && pAdapter)
                    {
                        count ++;
                        pAdapter = pAdapter->Next;
                    }
                    if (pAdapter)
                        result.asText(pAdapter->IpAddressList.IpAddress.String);
                }
            }
            Platform::free(pAdapterInfo);
//#elif defined(_POSIX)
// Even if the code for Mac below can be shared between Mac and Linux, we want to keep the same enumeration order for all the IPV4 methods
#elif defined(_LINUX)
            char          buf[1024] = {0};
            struct ifconf ifc = {0};
            struct ifreq *ifr = NULL;

            // We need a socket to start with
            int sck = socket(AF_INET, SOCK_DGRAM, 0);
            if (sck < 0) return result;

            // Query available interfaces.
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) { close(sck); return result; }

            // Iterate through the list of interfaces.
            ifr = ifc.ifc_req;
            int count = ifc.ifc_len / sizeof(struct ifreq);
            if (index < count)
            {
                struct ifreq *item = &ifr[index];

                // Show the device name and IP address
                struct sockaddr *addr = &(item->ifr_addr);
                if (addr && addr->sa_family == AF_INET)
                    result = IPV4((uint32)ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr));
            }

            close(sck);
#elif defined(_MAC)
            struct ifaddrs * addresses = NULL;
            if (getifaddrs(&addresses) != 0) return result;
            // Then count the interfaces
            struct ifaddrs * addr = addresses;
            int current = 0;
            while (addr)
            {
                if (filterSockAddrIPV4(addr))
                {
                    if (current == index) break;
                    current++;
                }

                addr = addr->ifa_next;
            }
            if (addr)
                result = IPV4((uint32)ntohl(((struct sockaddr_in *)addr->ifa_addr)->sin_addr.s_addr));
            freeifaddrs(addresses);
#endif
#endif
            return result;
        }
        // Check if an address is routed on the given interface.
        bool IPV4::isUsingInterface(const int index) const
        {
            IPV4 ifaddr, mask, gw;
            Type type;
            String macAddr;
            if (getLocalInterfaceDetails(index, ifaddr, mask, gw, type, macAddr, 0))
            {
                return (ifaddr.address.sin_addr.s_addr & mask.address.sin_addr.s_addr) == (address.sin_addr.s_addr & mask.address.sin_addr.s_addr);
            }
            return false;
        }
        // Get the MAC address of a remote device, using ARP protocol
        bool IPV4::getRemoteMACAddress(String & macAddrResult) const
        {
#ifdef MinSockCode
            return false;
#else
    #ifdef _WIN32
            HMODULE hMod = LoadLibraryA("Iphlpapi.dll");
            if (hMod == NULL) return false;
            DWORD (WINAPI *SendARP)( IPAddr, IPAddr, PULONG, PULONG ) = (DWORD (WINAPI *) ( IPAddr, IPAddr, PULONG, PULONG))GetProcAddress(hMod, "SendARP");
            if (!SendARP) { FreeLibrary(hMod); return false; }

            DWORD dwRetVal;
            IPAddr DestIp = 0;
            IPAddr SrcIp = 0;       // default for source ip
            ULONG MacAddr[2];       // for 6-byte hardware addresses
            ULONG PhysAddrLen = 6;  // default to length of six bytes
            char * DestIpString = NULL;
            char * SrcIpString = NULL;
            BYTE * bPhysAddr;
            memset(&MacAddr, 0xff, sizeof (MacAddr));
            dwRetVal = SendARP(address.sin_addr.s_addr, SrcIp, (PULONG)(&MacAddr), &PhysAddrLen);
            if (dwRetVal != NO_ERROR) return false;
            bPhysAddr = (BYTE *)& MacAddr;
            if (!PhysAddrLen) return false;
            macAddrResult.Format("%02X:%02X:%02X:%02X:%02X:%02X", (unsigned int)bPhysAddr[0], (unsigned int)bPhysAddr[1], (unsigned int)bPhysAddr[2], (unsigned int)bPhysAddr[3], (unsigned int)bPhysAddr[4], (unsigned int)bPhysAddr[5]);
            return true;
    #elif defined(_LINUX)
            int i;
            Platform::FileIndexWrapper s(socket(AF_INET, SOCK_STREAM, 0));

            // Get an internet domain socket.
            if (s == -1) return false;
            // Try to connect (non blocking) to force an (kernel based) ARP request on the address
            int socketFlags = fcntl(s, F_GETFL, 0);
            if (socketFlags == -1) return false;
            if (fcntl(s, F_SETFL, (socketFlags & ~O_NONBLOCK) | (O_NONBLOCK)) != 0) return false;
            uint16 oldPort = address.sin_port;
            const_cast<uint16&>(address.sin_port) = htons(80);
            connect(s, (const struct sockaddr*)&address, sizeof(address));
            const_cast<uint16&>(address.sin_port) = oldPort;

            struct timeval tv; tv.tv_usec = 0; tv.tv_sec = 3; // 3 sec timeout
            fd_set set; FD_ZERO(&set); FD_SET(s, &set);
            // Then select
            if (::select(FD_SETSIZE, NULL, &set, NULL, &tv) != 1) return false;
            s.Mutate(socket(AF_INET, SOCK_DGRAM, 0));
            if (s == -1) return false;


            // Now make an ARP request.
            struct arpreq       areq;
            struct sockaddr_in *sin;
            memset(&areq, 0, sizeof(areq));
            sin = (struct sockaddr_in *) &areq.arp_pa;
            sin->sin_family = AF_INET;
            sin->sin_addr = address.sin_addr;
            sin = (struct sockaddr_in *) &areq.arp_ha;
            sin->sin_family = ARPHRD_ETHER;

            i = 0;
            IPV4 ifAddr, mask, gw; IPV4::Type type; String macAddr, ifname;
            while (IPV4::getLocalInterfaceDetails(i, ifAddr, mask, gw, type, macAddr, &ifname))
            {
                if (ifname == "lo") { i++; continue; }
                strncpy(areq.arp_dev, (const char *)ifname, 15);
                if (ioctl(s, SIOCGARP, (caddr_t) &areq) != -1)
                {
                    macAddrResult.Format("%02X:%02X:%02X:%02X:%02X:%02X",   (uint8)areq.arp_ha.sa_data[0], (uint8)areq.arp_ha.sa_data[1], (uint8)areq.arp_ha.sa_data[2],
                                                                            (uint8)areq.arp_ha.sa_data[3], (uint8)areq.arp_ha.sa_data[4], (uint8)areq.arp_ha.sa_data[5]);
                    return true;
                }
                i++;
            }
            return false;
    #else
            //!< @todo: Use the route sysctls on Mac for this.
            return false;
    #endif
#endif
        }

#if defined(_POSIX)
        unsigned short in_cksum(unsigned short *addr, int len)
        {
            int sum = 0;
            u_short         answer = 0;
            u_short *  w = addr;
            int    nleft = len;
            /*
             * Our algorithm is simple, using a 32 bit accumulator (sum), we add
             * sequential 16 bit words to it, and at the end, fold back all the
             * carry bits from the top 16 bits into the lower 16 bits.
             */
            while (nleft > 1)
            {
                sum += *w++;
                nleft -= 2;
            }

            /* mop up an odd byte, if necessary */
            if (nleft == 1)
            {
                *(u_char *) (&answer) = *(u_char *) w;
                sum += answer;
            }

            /* add back carry outs from top 16 bits to low 16 bits */
            sum = (sum >> 16) + (sum & 0xffff);   /* add hi 16 to low 16 */
            sum += (sum >> 16);                   /* add carry */
            answer = ~sum;                        /* truncate to 16 bits */
            return (answer);
        }
#endif

#if WantPingCode == 1
        // Ping this address.
        int IPV4::pingDevice(const Time::TimeOut & timeout) const
        {
            if (timeout <= 0) return -1;
#ifdef _WIN32
            HANDLE hIcmpFile = IcmpCreateFile();
            if (hIcmpFile == INVALID_HANDLE_VALUE)
                return -1;

            ICMP_ECHO_REPLY reply;
            if (!IcmpSendEcho(hIcmpFile, *(IPAddr *)(&address.sin_addr), 0, 0, NULL, &reply, sizeof(reply), timeout))
            {
                IcmpCloseHandle(hIcmpFile);
                timeout.timedOut();
                return -1;
            }
            timeout.success();
            IcmpCloseHandle(hIcmpFile);

            in_addr ipreplied;
            ipreplied.S_un.S_addr = reply.Address;
            // Check if the same host replied
            if (memcmp(&reply.Address, &address.sin_addr, sizeof(reply.Address)))
                return -(int)reply.RoundTripTime;

            return reply.RoundTripTime;
#elif defined (_POSIX)
            // Check user right (if not running root, then we'll start 'ping' tool from the system)
            if (geteuid() != 0)
            {
                FILE * pingOutput = popen(String::Print("ping -c 1 -t %d %s | grep 'time='", timeout / 1000, (const char*)asText()), "r");
                if (pingOutput == NULL) return -1;
                char buffer[1024] = {0};
                if (fgets(buffer, ArrSz(buffer), pingOutput) == NULL)
                {
                    pclose(pingOutput);
                    return -1;
                }
                pclose(pingOutput);
                timeout.success();
                return String(buffer).fromFirst("time=").parseInt(10);
            }


            uint8 packet[sizeof(struct ip) + sizeof(struct icmp)];
            memset(packet, 0, ArrSz(packet));

            struct ip * ip = (struct ip*)packet;
            struct icmp * icmp = (struct icmp*)(packet + sizeof(*ip));

            // Need to find out on which interface the ping probe would succeed
            int localInterfaceIndex = -1;
            // First try, let's check for direct subnet pinging
            for (int i = 0; i < getLocalInterfacesCount(); i++)
            {
                if (isUsingInterface(i))
                {
                    localInterfaceIndex = i;
                    break;
                }
            }
            // Second try, we have to be routed to a gateway
            if (localInterfaceIndex == -1)
            {
                for (int i = 0; i < getLocalInterfacesCount(); i++)
                {
                    Network::Address::IPV4 localIP, mask, gw;
                    Network::Address::IPV4::Type type;
                    Network::String mac;
                    Network::Address::IPV4::getLocalInterfaceDetails(i, localIP, mask, gw, type, mac);

                    if (gw.isRoutable() && gw.isUsingInterface(i))
                    {
                        localInterfaceIndex = i;
                        break;
                    }
                }
            }
            // Ok, this ping would fail anyway, so exit right now
            if (localInterfaceIndex == -1) return -1;

            IPV4 srcAddr = getLocalInterfaceAddress(localInterfaceIndex);

            // Set up the IP header
            ip->ip_hl          = 5;
            ip->ip_v           = 4;
            ip->ip_tos         = 0;
            ip->ip_len         = sizeof(struct ip) + sizeof(struct icmp);
            ip->ip_id          = htons(0);
            ip->ip_off         = 0;
            ip->ip_ttl         = 64;
            ip->ip_p           = IPPROTO_ICMP;
            ip->ip_src         = srcAddr.address.sin_addr;
            ip->ip_dst         = address.sin_addr;
            ip->ip_sum         = in_cksum((unsigned short *)ip, sizeof(struct ip));

            Platform::FileIndexWrapper sockfd(socket(AF_INET, SOCK_RAW, IPPROTO_ICMP));
            if (sockfd == -1) return -1;

            int one = 1;
            // Don't add a IP header
            if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(int)) != 0)
                return -1;
            // Set the specified timeout
            struct timeval timeOut;
            timeOut.tv_sec = timeout / 1000;
            timeOut.tv_usec = (timeout % 1000) * 1000;
            // Even if this fails, we continue
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut));

            // ICMP packet
            icmp->icmp_type             = ICMP_ECHO;
            icmp->icmp_code             = 0;
            icmp->icmp_id               = clock();
            icmp->icmp_seq              = 0;
            icmp->icmp_cksum           = in_cksum((unsigned short *)icmp, sizeof(struct icmp));

            // Send the ICMP probe
            uint32 start = ::Time::getTimeWithBase(1000);
            if (sendto(sockfd, packet, ip->ip_len, 0, (struct sockaddr *)&address, sizeof(struct sockaddr)) != ip->ip_len)
            {
                timeout.filterError(-1);
                return -1;
            }

            // Wait for reply now
            struct sockaddr_in reply = {0};
            socklen_t replyLen = sizeof(reply);
            int size = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&reply, &replyLen);
            if (size == -1)
            {
                if (!timeout.timedOut()) timeout.filterError(-1);
                return -1;
            }
            timeout.success();
            uint32 stop = ::Time::getTimeWithBase(1000);
            int rtt = (stop - start);

            // Check the source for this packet
            if (memcmp(&reply.sin_addr, &address.sin_addr, sizeof(reply.sin_addr)))
                return -rtt;

            return rtt;
#endif
            return -1;
        }
#endif


#if defined(_LINUX)
#ifndef IF_NAMESIZE
    #ifdef IFNAMSIZ
        #define IF_NAMESIZE IFNAMSIZ
    #else
        #define IF_NAMESIZE 16
    #endif
#endif


        struct route_info
        {
            struct in_addr dstAddr;
            struct in_addr srcAddr;
            struct in_addr gateWay;
            char ifName[IF_NAMESIZE];
        };


        int readNlSock(int sockFd, char *bufPtr, int seqNum, int pId)
        {
            struct nlmsghdr *nlHdr;
            int readLen = 0, msgLen = 0;

            do
            {
                /* Recieve response from the kernel */
                if ((readLen = recv(sockFd, bufPtr, 8192 - msgLen, 0)) < 0)
                    return -1;

                nlHdr = (struct nlmsghdr *)bufPtr;

                /* Check if the header is valid */
                if ((NLMSG_OK(nlHdr, readLen) == 0) || (nlHdr->nlmsg_type == NLMSG_ERROR))
                    return -1;

                /* Check if the its the last message */
                if (nlHdr->nlmsg_type == NLMSG_DONE)
                {
                    break;
                }
                else
                {
                    /* Else move the pointer to buffer appropriately */
                    bufPtr += readLen;
                    msgLen += readLen;
                }

                /* Check if its a multi part message */
                if ((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0)
                {
                    /* return if its not */
                    break;
                }
            } while ((nlHdr->nlmsg_seq != (unsigned)seqNum) || (nlHdr->nlmsg_pid != (unsigned)pId));

            return msgLen;
        }

        /* parse the route info returned */
        void parseRoutes(struct nlmsghdr *nlHdr, struct route_info *rtInfo)
        {
            struct rtmsg *rtMsg;
            struct rtattr *rtAttr;
            int rtLen;

            rtMsg = (struct rtmsg *)NLMSG_DATA(nlHdr);

            /* If the route is not for AF_INET or does not belong to main or local routing table then we don't care, so return. */
            if ((rtMsg->rtm_family != AF_INET) || (rtMsg->rtm_table != RT_TABLE_MAIN && rtMsg->rtm_table != RT_TABLE_LOCAL))
                return;

            /* get the rtattr field */
            rtAttr = (struct rtattr *)RTM_RTA(rtMsg);
            rtLen = RTM_PAYLOAD(nlHdr);

            for (; RTA_OK(rtAttr, rtLen); rtAttr = RTA_NEXT(rtAttr, rtLen))
            {
                switch (rtAttr->rta_type)
                {
                    case RTA_OIF:
                        if_indextoname(* (int *)RTA_DATA(rtAttr), rtInfo->ifName);
                        break;

                    case RTA_GATEWAY:
                        memcpy(&rtInfo->gateWay, RTA_DATA(rtAttr), sizeof(rtInfo->gateWay));
                        break;

                    case RTA_PREFSRC:
                        memcpy(&rtInfo->srcAddr, RTA_DATA(rtAttr), sizeof(rtInfo->srcAddr));
                        break;

                    case RTA_DST:
                        memcpy(&rtInfo->dstAddr, RTA_DATA(rtAttr), sizeof(rtInfo->dstAddr));
                        break;
                }
            }

            return;
        }

        // meat
        int get_gatewayip(char *gatewayip, socklen_t size, const char * adapterName = "")
        {
            int found_gatewayip = 0;

            struct route_info *rtInfo;
            char msgBuf[8192]; // pretty large buffer

            int len, msgSeq = 0;
            Platform::FileIndexWrapper sock(socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE));

            // Check Socket
            if (sock < 0)
                return (-1);

            memset(msgBuf, 0, 8192);

            // point the header and the msg structure pointers into the buffer
            struct nlmsghdr *nlMsg = (struct nlmsghdr *)msgBuf;

            // Fill in the nlmsg header
            nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)); // Length of message.
            nlMsg->nlmsg_type = RTM_GETROUTE; // Get the routes from kernel routing table .

            nlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST; // The message is a request for dump.
            nlMsg->nlmsg_seq = msgSeq++; // Sequence of the message packet.
            nlMsg->nlmsg_pid = getpid(); // PID of process sending the request.

            // Send the request
            if (send(sock, nlMsg, nlMsg->nlmsg_len, 0) < 0)
                return -1;

            // Read the response
            if ((len = readNlSock(sock, msgBuf, msgSeq, getpid())) < 0)
                return -1;

            // Parse and print the response
            rtInfo = (struct route_info *)Platform::malloc(sizeof(struct route_info));

            for (; NLMSG_OK(nlMsg, len); nlMsg = NLMSG_NEXT(nlMsg, len))
            {
                memset(rtInfo, 0, sizeof(struct route_info));
                parseRoutes(nlMsg, rtInfo);

                // Check if default gateway
                if (rtInfo->dstAddr.s_addr == 0 && (adapterName[0] == 0 || strcmp(adapterName, rtInfo->ifName) == 0))
                {
                    // copy it over
                    inet_ntop(AF_INET, &rtInfo->gateWay, gatewayip, size);
                    found_gatewayip = 1;
                    break;
                }
            }

            Platform::free(rtInfo);
            return found_gatewayip;
        }
#endif

#if defined(_LINUX)
        static void fillInterfaceDetails(struct ifreq * item, int sck, IPV4 & address, IPV4 & mask, IPV4 & gateway, IPV4::Type & type, String & macAddress, String * adapterName)
        {
            // Device name and IP address
            struct sockaddr *addr = &(item->ifr_addr);
            if (addr && addr->sa_family == AF_INET)
                address = IPV4((uint32)ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr));

            // Network mask
            if(ioctl(sck, SIOCGIFNETMASK, item) >= 0)
            {
                struct sockaddr *addr = &(item->ifr_addr);
                if (addr && addr->sa_family == AF_INET)
                    mask = IPV4((uint32)ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr));
            }

            // Gateway is more complex, as it invoke routing tables
            char buffer[NI_MAXSERV];
            if (get_gatewayip(buffer, NI_MAXSERV, adapterName ? item->ifr_name : "") == 1)
            {
                gateway.asText(buffer);
            }

            // MAC address
            if(ioctl(sck, SIOCGIFHWADDR, item) >= 0)
            {
                macAddress.Format("%02X:%02X:%02X:%02X:%02X:%02X",
                                    (uint8)item->ifr_hwaddr.sa_data[0], (uint8)item->ifr_hwaddr.sa_data[1], (uint8)item->ifr_hwaddr.sa_data[2],
                                    (uint8)item->ifr_hwaddr.sa_data[3], (uint8)item->ifr_hwaddr.sa_data[4], (uint8)item->ifr_hwaddr.sa_data[5]);
            }

            // Adapter name
            if (adapterName)
                *adapterName = item->ifr_name;

            // The type
            if(ioctl(sck, SIOCGIFFLAGS, item) >= 0)
            {
                if (item->ifr_flags & 0x10)
                    type = IPV4::DialUp;
                else
                {
                    int speed = getEthernetRate(sck, item);
                    if (speed)
                    {
                        switch(speed)
                        {
                        case 10:   type = IPV4::Ethernet10M; break;
                        case 100:  type = IPV4::Ethernet100M; break;
                        case 1000: type = IPV4::Ethernet1G; break;
                        default:  // Can't read it, so 100Mbits ethernet is the most common
                            type = IPV4::Ethernet100M; break;
                        }
                    }
                    else
                    {
                        int speed = getWIFIRate(sck, item->ifr_name);
                        if (speed == 0)              type = IPV4::Ethernet10M;
                        else if (speed <= 11000000)  type = IPV4::WIFI11M;
                        else if (speed <= 54000000)  type = IPV4::WIFI54M;
                        else if (speed <= 108000000) type = IPV4::WIFI108M;
                        else if (speed <= 300000000) type = IPV4::WIFI300M;
                        else                         type = IPV4::WIFI11M;
                    }
                }
            }
        }
#endif

        // Get the local interface details.
        bool IPV4::getLocalInterfaceDetails(const int index, IPV4 & address, IPV4 & mask, IPV4 & gateway, Type & type, String & macAddress, String * adapterName)
        {
#if defined(_WIN32) || defined(NEXIO)
#if !defined(MinSockCode) && !defined(NEXIO)
            HMODULE hMod = LoadLibraryA("Iphlpapi.dll");
            if (hMod == NULL) return false;
            DWORD (WINAPI *GetAdaptersInfo)( PIP_ADAPTER_INFO, PULONG) = (DWORD (WINAPI *)( PIP_ADAPTER_INFO, PULONG))GetProcAddress(hMod, "GetAdaptersInfo");
            DWORD (WINAPI* GetIfEntry) (PMIB_IFROW) = (DWORD  (WINAPI*) (PMIB_IFROW))GetProcAddress(hMod, "GetIfEntry");
            DWORD (WINAPI* GetIfTable) (PMIB_IFTABLE, PULONG, BOOL) = (DWORD  (WINAPI*) (PMIB_IFTABLE, PULONG, BOOL))GetProcAddress(hMod, "GetIfTable");
            if (!GetAdaptersInfo || !GetIfEntry || !GetIfTable) { FreeLibrary(hMod); return false; }

            // Make an initial call to GetAdaptersInfo to get
            // the necessary size into the ulOutBufLen variable
            ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
            GetAdaptersInfo(NULL, &ulOutBufLen);
            PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *) Platform::malloc(ulOutBufLen);
            if (pAdapterInfo == NULL) { FreeLibrary(hMod); return false; }


            DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);

            int count = 0;
            bool result = false;
            if (dwRetVal == NO_ERROR)
            {
                PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
                if(pAdapter)
                {
                    while(count < index && pAdapter)
                    {
                        count ++;
                        pAdapter = pAdapter->Next;
                    }

                    if (pAdapter)
                    {
                        result = true;
                        address.asText(pAdapter->IpAddressList.IpAddress.String);
                        mask.asText(pAdapter->IpAddressList.IpMask.String);
                        gateway.asText(pAdapter->GatewayList.IpAddress.String);
                        macAddress.Format("%02X:%02X:%02X:%02X:%02X:%02X",
                            pAdapter->Address[0], pAdapter->Address[1], pAdapter->Address[2],
                            pAdapter->Address[3], pAdapter->Address[4], pAdapter->Address[5]);
                        if (adapterName)
                            *adapterName = pAdapter->AdapterName;
                        if (pAdapter->Type == MIB_IF_TYPE_PPP)
                            type = DialUp;
                        else if (pAdapter->Type == MIB_IF_TYPE_ETHERNET)
                        {
                            // Init structure PMIB_IFTABLE which contains mac address and network speed information
                            PMIB_IFTABLE pIfTable = 0;

                            DWORD           dwSize = 0;
                            unsigned long   speed = 0;

                            GetIfTable(pIfTable, &dwSize, false);
                            pIfTable = (PMIB_IFTABLE)Platform::malloc(dwSize);
                            int result = 0;

                            if ((result = GetIfTable(pIfTable, &dwSize, false)) == NO_ERROR)
                            {
                                // For each mac address
                                for (int i = 0; i < (int) pIfTable->dwNumEntries - 1; i++)
                                {
                                    MIB_IFROW pIfRow = pIfTable->table[i];
                                    if (memcmp(pIfRow.bPhysAddr, pAdapter->Address, 6) == 0)
                                    {
                                        // Read the speed
                                        if (pIfRow.dwSpeed < 1000000)
                                        {
                                            type = Other;
                                        } else
                                        {
                                            int mbits = pIfRow.dwSpeed / 1000000;
                                            switch(mbits)
                                            {
                                            case 10:
                                                type = Ethernet10M;
                                                break;
                                            case 100:
                                                type = Ethernet100M;
                                                break;
                                            case 1000:
                                                type = Ethernet1G;
                                                break;
                                            case 11:
                                                type = WIFI11M;
                                                break;
                                            case 54:
                                                type = WIFI54M;
                                                break;
                                            case 108:
                                                type = WIFI108M;
                                                break;
                                            case 300:
                                                type = WIFI300M;
                                                break;
                                            case 400:
                                                type = Firewire400M;
                                                break;
                                            case 800:
                                                type = Firewire800M;
                                                break;
                                            default:
                                                type = Other;
                                                {
                                                    HANDLE hDevice = INVALID_HANDLE_VALUE;

                                                    // Build the path to the device
                                                    TCHAR szDevPath[MAX_ADAPTER_NAME_LENGTH + 8];
                                                    wsprintf( szDevPath, _T("\\\\.\\%s"), pAdapter->AdapterName );

                                                    // Open the device for reading
                                                    hDevice = CreateFile(szDevPath, 0, FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES) NULL, OPEN_EXISTING, 0, (HANDLE)INVALID_HANDLE_VALUE);
                                                    if (hDevice != INVALID_HANDLE_VALUE)
                                                    {
                                                        // Query the device for its type
                                                        unsigned int    OidCode = 0x00010202; // OID_GEN_PHYSICAL_MEDIUM;
                                                        ULONG           ulOidPhysicalMedium=0;
                                                        ULONG           ulBytesReturned=0;
// We don't include NDIS here to avoid header conflicts with Platform SDK
#ifndef IOCTL_NDIS_QUERY_GLOBAL_STATS
                                                        #define _NDIS_CONTROL_CODE(request,method) \
                                                            CTL_CODE(FILE_DEVICE_PHYSICAL_NETCARD, request, method, FILE_ANY_ACCESS)
                                                        #define IOCTL_NDIS_QUERY_GLOBAL_STATS   _NDIS_CONTROL_CODE( 0, METHOD_OUT_DIRECT )
                                                        enum NDIS_PHYSICAL_MEDIUM
                                                        {
                                                            NdisPhysicalMediumUnspecified, NdisPhysicalMediumWirelessLan, NdisPhysicalMediumCableModem, NdisPhysicalMediumPhoneLine,
                                                            NdisPhysicalMediumPowerLine, NdisPhysicalMediumDSL, NdisPhysicalMediumFibreChannel, NdisPhysicalMedium1394,
                                                            NdisPhysicalMediumWirelessWan, NdisPhysicalMediumNative802_11, NdisPhysicalMediumBluetooth, NdisPhysicalMediumInfiniband,
                                                            NdisPhysicalMediumWiMax, NdisPhysicalMediumUWB, NdisPhysicalMedium802_3, NdisPhysicalMedium802_5,
                                                            NdisPhysicalMediumIrda, NdisPhysicalMediumWiredWAN, NdisPhysicalMediumWiredCoWan, NdisPhysicalMediumOther,
                                                            NdisPhysicalMediumMax
                                                        };
#endif
                                                        bool bResult = DeviceIoControl( hDevice, IOCTL_NDIS_QUERY_GLOBAL_STATS, &OidCode, sizeof(OidCode), &ulOidPhysicalMedium, sizeof(ULONG), &ulBytesReturned, NULL ) > 0;

                                                        if (ulOidPhysicalMedium == NdisPhysicalMediumWirelessLan)
                                                            type = WIFI11M;
                                                        else if (ulOidPhysicalMedium == NdisPhysicalMedium802_3)
                                                            type = Ethernet10M;
                                                        else if (ulOidPhysicalMedium == NdisPhysicalMediumPhoneLine)
                                                            type = DialUp;
                                                        else if (ulOidPhysicalMedium == NdisPhysicalMedium1394)
                                                            type = Firewire400M;
                                                        // Close the device
                                                        CloseHandle(hDevice);
                                                    }

                                                }

                                                break;
                                            }
                                        }

                                    }
                                }
                            }
                            Platform::free(pIfTable);
                        }
                    }
                }
            }
            Platform::free(pAdapterInfo);
            FreeLibrary(hMod);
            return result;
#else
            return false;
#endif
#elif defined(_LINUX)
            char    buf[1024] = {0};
            struct ifconf ifc = {0};
            struct ifreq *ifr = NULL;

            // We need a socket to start with
            Platform::FileIndexWrapper sck(socket(AF_INET, SOCK_DGRAM, 0));
            if (sck < 0) return false;

            // Query interfaces configuration
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) return false;


            // Iterate through the interfaces.
            ifr = ifc.ifc_req;
            int count = ifc.ifc_len / sizeof(struct ifreq);
            if (index < count)
            {
                struct ifreq *item = &ifr[index];
                fillInterfaceDetails(item, sck, address, mask, gateway, type, macAddress, adapterName);
            } else
            {
                // IFCONF doesn't report the interface with no IP assigned to it. So let's now iterate the other interfaces.
                struct ifreq req;
                int remIndex = index - (count - 1);

                memset(&req, 0, sizeof(req));
                req.ifr_ifindex = 1;

                while (remIndex && ioctl(sck, SIOCGIFNAME, &req) >= 0)
                {
                    // First make sure the interface is not already listed in the one from IFCONF call.
                    // We don't check the IFFLAGS because the interface could have be bring up between the previous call and this one.
                    int i = 0;
                    for (; i < count; i++)
                    {   // O(N*N) search, but since the number of interface is low, it's OK.
                        if (strcmp(ifr[i].ifr_name, req.ifr_name) == 0) break;
                    }
                    if (i == count)
                    {
                        // Not reported yet
                        struct ifreq local; memcpy(&local, &req, sizeof(req));
                        if (ioctl(sck, SIOCGIFADDR, &local) < 0)
                            // With no address, let's zero it
                            memset(&local.ifr_addr, 0, sizeof(local.ifr_addr));

                        fillInterfaceDetails(&local, sck, address, mask, gateway, type, macAddress, adapterName);
                        remIndex --;
                    }
                    req.ifr_ifindex++;
                }

                if (index) return false;
            }

            return true;
#elif defined(_MAC)
            struct ifaddrs * addresses = NULL;
            if (getifaddrs(&addresses) != 0) return false;

            // Then count the interfaces
            int current = 0, if_cur = 1;
            struct ifaddrs * addr = addresses;
            while (addr)
            {
                if (filterSockAddrIPV4(addr))
                {
                    if (current == index) break;
                    current++;
                }
                if_cur++;
                addr = addr->ifa_next;
            }

            String currentAdapterName;
            if (addr)
            {
                currentAdapterName = addr->ifa_name;
                if (adapterName) *adapterName = currentAdapterName;
                address = IPV4((uint32)ntohl(((struct sockaddr_in *)addr->ifa_addr)->sin_addr.s_addr));
                mask = IPV4((uint32)ntohl(((struct sockaddr_in *)addr->ifa_netmask)->sin_addr.s_addr));

                // Get the default gateway
                int mib[6];
                mib[0] = CTL_NET;
                mib[1] = PF_ROUTE;
                mib[2] = 0;
                mib[3] = AF_INET;
                mib[4] = NET_RT_FLAGS; //NET_RT_DUMP;
                mib[5] = RTF_GATEWAY;

                char * buf = NULL;
                size_t len = 0;
                if (sysctl(mib, 6, NULL, &len, NULL, 0) >= 0)
                {
                    buf = (char*)malloc(len);
                    if (sysctl(mib, 6, buf, &len, NULL, 0) >= 0)
                    {
                        char* end = buf + len;
                        char* next = buf;
                        while (next < end)
                        {
                            struct rt_msghdr* rtm = (struct rt_msghdr*)next;
                            struct sockaddr * rtinfo[RTAX_MAX] = {0};

                            struct sockaddr* startAddress = (struct sockaddr*)(rtm + 1);
                            for (int i = 0; i < RTAX_MAX; i++)
                            {
                                if (rtm->rtm_addrs & (1 << i))
                                {
                                    rtinfo[i] = startAddress;
                                    #define RoundTo4(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint32_t) - 1))) : sizeof(uint32_t))
                                    startAddress = (struct sockaddr *)(RoundTo4(startAddress->sa_len) + (char *)startAddress);
                                }
                            }

                            if ( ((rtm->rtm_addrs & (RTA_DST|RTA_GATEWAY)) == (RTA_DST|RTA_GATEWAY))
                                 && rtinfo[RTAX_DST]->sa_family == AF_INET && rtinfo[RTAX_GATEWAY]->sa_family == AF_INET
                                 && ((struct sockaddr_in *)rtinfo[RTAX_DST])->sin_addr.s_addr == 0)
                            {
                                char ifName[128];
                                if_indextoname(rtm->rtm_index, ifName);
                                if (currentAdapterName == ifName)
                                {
                                    gateway = IPV4((uint32)ntohl(((struct sockaddr_in *)rtinfo[RTAX_GATEWAY])->sin_addr.s_addr));
                                    break;
                                }
                            }
                            next += rtm->rtm_msglen;
                        }
                    }
                }

                free(buf);
                // Don't know how to find out the link type.
                // We probably can deduce the type from the adapterName, but it can be troublesome
                type = Other;

                // Then extract the MAC address
                addr = addresses;
                while (addr)
                {
                    if (currentAdapterName == addr->ifa_name)
                    {
                        sockaddr_storage* sto = (sockaddr_storage*) addr->ifa_addr;
                        if (sto->ss_family == AF_LINK)
                        {
                            const sockaddr_dl* const sadd = (const sockaddr_dl*) addr->ifa_addr;
#ifndef IFT_ETHER
#define IFT_ETHER 6
#endif
                            if (sadd->sdl_type == IFT_ETHER)
                            {
                                const uint8 * data = ((const uint8*) sadd->sdl_data) + sadd->sdl_nlen;
                                macAddress.Format("%02X:%02X:%02X:%02X:%02X:%02X",   (uint8)data[0], (uint8)data[1], (uint8)data[2],
                                                     (uint8)data[3], (uint8)data[4], (uint8)data[5]);

                                break;
                            }
                        }
                    }
                    addr = addr->ifa_next;
                }
            }
            freeifaddrs(addresses);

            return address.isValid();
#endif
        }

        // Get the class type for this address.
        IPV4::Class IPV4::getClassType() const
        {
            if ((ntohl(address.sin_addr.s_addr) & 0x80000000) == 0) return ClassA;
            if ((ntohl(address.sin_addr.s_addr) & 0xC0000000) == 0x80000000) return ClassB;
            if ((ntohl(address.sin_addr.s_addr) & 0xE0000000) == 0xC0000000) return ClassC;
            if ((ntohl(address.sin_addr.s_addr) & 0xF0000000) == 0xE0000000) return ClassD;
            return ClassE;
        }

        // Get the special address type for this address.
        IPV4::SpecialAddress IPV4::getSpecialAddressType() const
        {
            if (address.sin_addr.s_addr == 0) return Zero;
            if ((ntohl(address.sin_addr.s_addr) & 0xFF000000) >> 24 == 10) return Private10;
            if ((ntohl(address.sin_addr.s_addr) & 0xFF000000) >> 24 == 127) return Loopback;

            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 16 == 0xA9FE) return ZeroConf;
            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 16 == 0xAC10) return Private172;

            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 16 == 0xC0A8) return Private192;
            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 16 == 0xC012) return NetworkBenchmark;
            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 16 == 0xC013) return NetworkBenchmark;
            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 8 == 0xC00002) return Documentation;
            if ((ntohl(address.sin_addr.s_addr) & 0xFFFF0000) >> 8 == 0xC05863) return IPv6ToIPv4;

            if ((ntohl(address.sin_addr.s_addr) & 0xF0000000) >> 28 == 0xE) return Multicast;
            if ((ntohl(address.sin_addr.s_addr) & 0xF0000000) >> 28 == 0xF) return Reserved;

            return Routable;
        }

        // Check if a connection to the given address will require NAT passthrough system.
        bool IPV4::isNATPunchRequiredToConnect(const IPV4 & address) const
        {
            // Any one of our address is private, we are behind a NAT
            if (isValid() && (getSpecialAddressType() & PrivateMask)) // We are behind a NAT
                return (address.isValid() && (address.getSpecialAddressType() & PrivateMask)); // Only need to dig if the other side isn't public
            if (isValid() && address.isValid() && (address.getSpecialAddressType() & PrivateMask))
                return true; // If we are public, we still need to dig if the other size isn't public
            return false; // Should work directly here
        }

        // Get broadcast address
        IPV4 IPV4::getBroadcastAddress() const
        {
            int i = getLocalInterfacesCount();
            for (int j = 0; j < i; j++)
            {
                IPV4 local, mask, gw;
                Type type;
                String mac;
                if (getLocalInterfaceDetails(j, local, mask, gw, type, mac))
                {
                    if (local.address.sin_addr.s_addr == address.sin_addr.s_addr && local.getSpecialAddressType() & UsableMask)
                    {
                        uint32 addr = (ntohl(local.address.sin_addr.s_addr) & ntohl(mask.address.sin_addr.s_addr)) | ~ ntohl(mask.address.sin_addr.s_addr);
                        return IPV4(addr);
                    }
                }
            }
            return IPV4();
        }

#ifdef _WIN32
#ifndef MinSockCode
    const TCHAR serviceName[] = _T("WifiUnCrypt");
    const TCHAR displayName[] = _T("WEP/WPA key uncypher");
    bool shouldStillRun = true;
    int  serviceExitCode = 0;

#define CLOSE_MUTEX_NAME    _T("Global\\UncryptKeyMutex")
#define SHARED_MEMORY_NAME    _T("Global\\UncryptKeyMemory")

    BYTE cypheredKey[1024] = {0};
    int  cypheredKeyLength = 0;
    BYTE uncypheredKey[1024] = {0};
    int  uncypheredKeyLength = 0;

            // Use Win32 BLOB structure
#ifndef CRYPT_SUCCEED
    typedef struct
    {
        DWORD cbData;
        BYTE* pbData;
    } DATA_BLOB;
#endif

    void WINAPI serviceHandler( DWORD fdwControl )
    {
        if( fdwControl == SERVICE_CONTROL_SHUTDOWN )
            shouldStillRun = false; // This isn't atomic, but who care ?
    }

    void addToLog(const char * msg)
    {
/*
        FILE * file = fopen("C:\\started.log", "ab");
        fprintf(file, "%s\n", msg);
        fclose(file);
        */
    }

    void WINAPI ServiceMain( DWORD dwArgc, LPTSTR *lpszArgv )
    {
        SERVICE_STATUS_HANDLE hServiceStatus = RegisterServiceCtrlHandler( serviceName, serviceHandler );
        SERVICE_STATUS status = {0};
        status.dwServiceType        = SERVICE_WIN32_OWN_PROCESS;
        status.dwCurrentState       = SERVICE_RUNNING;
        status.dwControlsAccepted   = SERVICE_ACCEPT_SHUTDOWN;
        status.dwWin32ExitCode      = NO_ERROR;

        SetServiceStatus( hServiceStatus, &status );
        HANDLE hMutex = CreateEvent(NULL, FALSE, FALSE, CLOSE_MUTEX_NAME);
        if (hMutex == NULL) { serviceExitCode = -4; addToLog("Can't create event"); exit(-4); }
        HANDLE hMemory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 2056, SHARED_MEMORY_NAME);
        if (hMemory == NULL) { serviceExitCode = -5; addToLog("Can't create file mapping"); SetEvent(hMutex); CloseHandle(hMutex); exit(-5); }
        // Map the shared file as input
        LPBYTE buffer = (LPBYTE)MapViewOfFile(hMemory, FILE_MAP_ALL_ACCESS, 0, 0, 2056);
        if (buffer == NULL) { serviceExitCode = -6; addToLog("Can't map file"); SetEvent(hMutex); CloseHandle(hMutex); CloseHandle(hMemory); exit(-6); }

        HMODULE hCryptMod = LoadLibrary(_T("Crypt32.dll"));
        if (!hCryptMod)
        {
            serviceExitCode = -1;
            SetEvent(hMutex);
            CloseHandle(hMutex);
            UnmapViewOfFile(buffer);
            CloseHandle(hMemory);
            addToLog("Can't load crypt32.dll");
            exit(-1);
        }

        typedef BOOL (WINAPI *BlobDump) (DATA_BLOB *,void *,void *,void *, void *,void *,DATA_BLOB *);
        BlobDump CryptUnprotectData = (BlobDump)GetProcAddress(hCryptMod, "CryptUnprotectData");
        if (!CryptUnprotectData)
        {
            serviceExitCode = -2;
            FreeLibrary(hCryptMod);
            SetEvent(hMutex);
            CloseHandle(hMutex);
            UnmapViewOfFile(buffer);
            CloseHandle(hMemory);
            addToLog("Can't load get CryptUnprotectData");
            exit(-2);
        }


        DATA_BLOB pIn = {0}, pOut = {0};
        pIn.cbData = *(int*)buffer;
        pIn.pbData = buffer+sizeof(int);

        pOut.cbData = 0;
        pOut.pbData = 0;

        if (CryptUnprotectData( &pIn, NULL, NULL, NULL, NULL, 0, &pOut ) != TRUE )
        {
            serviceExitCode = -3;
            FreeLibrary(hCryptMod);
            SetEvent(hMutex);
            CloseHandle(hMutex);
            UnmapViewOfFile(buffer);
            CloseHandle(hMemory);
            addToLog("Failure to call CryptUnprotectData");
            exit(-3);
        }

        // Copy the data back to the shared file
        memcpy(buffer+1028, &pOut.cbData, sizeof(pOut.cbData));
        memcpy(buffer+1032, pOut.pbData, pOut.cbData);
        UnmapViewOfFile(buffer);

        FreeLibrary(hCryptMod);
        serviceExitCode = 0;
        SetEvent(hMutex);
        CloseHandle(hMutex);
        CloseHandle(hMemory);
        addToLog("Worked");
        exit(0);
    }

    typedef BOOL( __stdcall *CTM ) ( HANDLE, PSID, PBOOL );
    CTM CheckTokenMembership;

    int IsUserAnAdmin()
    {
        int bToken = 0;
        PSID pAdmin;
        SID_IDENTIFIER_AUTHORITY sAuthority = SECURITY_NT_AUTHORITY;

        HMODULE hAdvapi32 = LoadLibrary(_T("advapi32"));
        if( hAdvapi32 != NULL )
        {
            CheckTokenMembership = (CTM) GetProcAddress( hAdvapi32, "CheckTokenMembership" );
            if( CheckTokenMembership != NULL )
            {
                bToken = AllocateAndInitializeSid( &sAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdmin );
                if( bToken == 1 )
                {
                    int bCheck = CheckTokenMembership( NULL, pAdmin, &bToken );
                    if( bCheck == 0 ) bToken = 1;
                }
            }
        }
        FreeLibrary(hAdvapi32);
        return( bToken );
    }
#endif
#endif

        // If the interface type is Wifi, this method will return the connected SSID (if any) and the key (WEP / WPA/PSK)
        bool IPV4::getWIFIDetails(const int index, String & SSID, String & key)
        {
            IPV4 local, mask, gw;
            Type type; String mac; String name;
            if (!getLocalInterfaceDetails(index, local, mask, gw, type, mac, &name)) return false;
            if ((type & WIFIMask) == 0) return false;
            SSID = "";
            // Get current SSID for this interface
#ifdef _WIN32
#ifndef MinSockCode
            // We don't include kernel stuff here, but use it nonetheless
        #ifndef IOCTL_NDIS_QUERY_GLOBAL_STATS
            #define _NDIS_CONTROL_CODE(request, method) \
                        CTL_CODE(FILE_DEVICE_PHYSICAL_NETCARD, request, method, FILE_ANY_ACCESS)
            #define IOCTL_NDIS_QUERY_GLOBAL_STATS _NDIS_CONTROL_CODE(0, METHOD_OUT_DIRECT)
        #endif
        #ifndef OID_802_11_SSID
            #define OID_802_11_BSSID 0x0D010101
            #define OID_802_11_SSID 0x0D010102
            #define OID_802_11_WEP_STATUS 0x0d01011b
            typedef enum _NDIS_802_11_NETWORK_TYPE
            {
                Ndis802_11FH,
                Ndis802_11DS,
                Ndis802_11NetworkTypeMax
            } NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;

            typedef LONG NDIS_802_11_RSSI;

            typedef struct _NDIS_802_11_CONFIGURATION_FH
            {
                ULONG Length;
                ULONG HopPattern;
                ULONG HopSet;
                ULONG DwellTime;
            } NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

            typedef struct _NDIS_802_11_CONFIGURATION
            {
                ULONG Length;
                ULONG BeaconPeriod;
                ULONG ATIMWindow;
                ULONG DSConfig;
                NDIS_802_11_CONFIGURATION_FH    FHConfig;
            } NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

            typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE
            {
                Ndis802_11IBSS,
                Ndis802_11Infrastructure,
                Ndis802_11AutoUnknown,
                Ndis802_11InfrastructureMax
            } NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;

            typedef enum _NDIS_802_11_AUTHENTICATION_MODE
            {
                Ndis802_11AuthModeOpen,
                Ndis802_11AuthModeShared,
                Ndis802_11AuthModeAutoSwitch,
                Ndis802_11AuthModeMax
            } NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;

            typedef UCHAR NDIS_802_11_RATES[8];

            typedef UCHAR NDIS_802_11_MAC_ADDRESS[6];

            typedef struct _NDIS_802_11_SSID
            {
                ULONG SsidLength;
                UCHAR Ssid[32];
            } NDIS_802_11_SSID, *PNDIS_802_11_SSID;
            typedef struct _NDIS_WLAN_BSSID
            {
                ULONG Length;
                NDIS_802_11_MAC_ADDRESS MacAddress;
                UCHAR Reserved[2];
                NDIS_802_11_SSID Ssid;
                ULONG Privacy;
                NDIS_802_11_RSSI Rssi;
                NDIS_802_11_NETWORK_TYPE NetworkTypeInUse;
                NDIS_802_11_CONFIGURATION Configuration;
                NDIS_802_11_NETWORK_INFRASTRUCTURE InfrastructureMode;
                NDIS_802_11_RATES SupportedRates;
            } NDIS_WLAN_BSSID, *PNDIS_WLAN_BSSID;

            typedef enum _NDIS_802_11_WEP_STATUS
            {
                Ndis802_11WEPEnabled,
                Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
                Ndis802_11WEPDisabled,
                Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
                Ndis802_11WEPKeyAbsent,
                Ndis802_11WEPNotSupported,
                Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPNotSupported,
                Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
                Ndis802_11Encryption2Enabled,
                Ndis802_11Encryption2KeyAbsent,
                Ndis802_11Encryption3Enabled,
                Ndis802_11Encryption3KeyAbsent
            } NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS, NDIS_802_11_ENCRYPTION_STATUS, *PNDIS_802_11_ENCRYPTION_STATUS;
        #endif
            HANDLE hAdapter;
            TCHAR szAdapter[MAX_PATH];
#ifdef _MBCS
            wsprintf(szAdapter, "\\\\.\\%s", (const char*)name);
#else
            wsprintf(szAdapter, L"\\\\.\\%S", (const char*)name);
#endif
            hAdapter = CreateFile(szAdapter, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hAdapter == INVALID_HANDLE_VALUE) return false;
            DWORD dwBytes, dwOIDCode = OID_802_11_SSID;
            NDIS_802_11_SSID ssid={0};
            NDIS_802_11_WEP_STATUS wepStatus;
            if (!DeviceIoControl(hAdapter, IOCTL_NDIS_QUERY_GLOBAL_STATS, &dwOIDCode, sizeof(dwOIDCode), &ssid, sizeof(ssid), &dwBytes, NULL))
            { CloseHandle(hAdapter); return false; }
            dwOIDCode = OID_802_11_WEP_STATUS;
            if (DeviceIoControl(hAdapter, IOCTL_NDIS_QUERY_GLOBAL_STATS, &dwOIDCode, sizeof(dwOIDCode), &wepStatus, sizeof(wepStatus), &dwBytes, NULL))
            {
                // We have the encryption type here
                switch(wepStatus)
                {
                case Ndis802_11WEPKeyAbsent:
                case Ndis802_11WEPDisabled:
                case Ndis802_11WEPNotSupported:
                    key = "open:";
                    break;
                case Ndis802_11WEPEnabled:
                    key = "wep:";
                    break;
                case Ndis802_11Encryption2Enabled:
                    key = "wpa/psk:";
                    break;
                case Ndis802_11Encryption3Enabled:
                    key = "wpa2:";
                    break;
                default:
                    break;
                }

            }
            CloseHandle(hAdapter);

            SSID = (const char*)ssid.Ssid;

            int keyZero[32] =
            {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };

            int basicXor[32] =
            {
                0x56, 0x66, 0x09, 0x42, 0x08, 0x03, 0x98, 0x01,
                0x4D, 0x67, 0x08, 0x66, 0x11, 0x56, 0x66, 0x09,
                0x42, 0x08, 0x03, 0x98, 0x01, 0x4D, 0x67, 0x08,
                0x66, 0x11, 0x56, 0x66, 0x09, 0x42, 0x08, 0x03
            };



            // The next part depends on windows version
            OSVERSIONINFOEX osInfo = {0};
            osInfo.dwOSVersionInfoSize = sizeof(osInfo);
            if (GetVersionEx((LPOSVERSIONINFO)&osInfo) == FALSE) return false;
            if (osInfo.dwMajorVersion >= 6)
            {
                // Vista or later
                LPCTSTR KeyCode = _T("C:\\ProgramData\\Microsoft\\Wlansvc\\Profiles\\Interfaces\\");
//                WlanGetAvailableNetworkList
//                    WlanGetNetworkBssList

            }
            else if (osInfo.dwMajorVersion == 5 && osInfo.dwMinorVersion == 1)
            {
                // XP
                HKEY hWirelessConf;
                DATA_BLOB pIn, pOut;
                TCHAR goodKey[1024] = {0};
                LPCTSTR RegistryAddress = _T("Software\\Microsoft\\WZCSVC\\Parameters\\Interfaces\\");
#ifdef _MBCS
                wsprintf(goodKey, "%s%s", RegistryAddress, (const char*)name);
#else
                wsprintf(goodKey, L"%s%S", RegistryAddress, (const char*)name);
#endif

                if (RegOpenKey(HKEY_LOCAL_MACHINE, goodKey, &hWirelessConf ) != ERROR_SUCCESS) return false;

                int wkeyIndex = 0;
                while(1)
                {
                    TCHAR keyString[1024] = {0};
                    BYTE  buffer[1024] = {0};
                    wsprintf(keyString, _T("Static#%04d"), wkeyIndex);
                    int length = sizeof( buffer ), keyType;

                    if (RegQueryValueEx( hWirelessConf, keyString, NULL, (DWORD*)&keyType, buffer, (DWORD*)&length) != ERROR_SUCCESS)
                        break;

//                    printf("Found key at: %s\n", keyString);

                    if (SSID != (const char*)(buffer + 0x14))
                    {
//                        printf("Keyname: %s\n", buffer+0x14);
                        wkeyIndex++;
                        continue;
                    }

                    if (*(buffer+112) == 0x20)
                        key = "wpa/psk:";
                    else if (*(buffer+112) == 0x05)
                        key = "wep:";
                    else if (*(buffer+112) == 0x00)
                        key = "open:";
                    // Else fill previously by the NIC itself

                    pIn.cbData = length - *(int *)(buffer);
                    pIn.pbData = buffer + *(int *)(buffer);

                    SERVICE_TABLE_ENTRY serviceTableEntry[2] =
                    {
                        { (TCHAR*)serviceName, ServiceMain },
                        { NULL, NULL }
                    };

                    TCHAR userName[512];
                    DWORD userNameLen = sizeof(userName);
                    GetUserName(userName, &userNameLen);
                    if (lstrcmp(userName, _T("SYSTEM")) == 0)
                    {   // If we are the system account
                        StartServiceCtrlDispatcher(serviceTableEntry);
                        return false;
                    }

                    SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
                    if (hManager == NULL) return false;

                    HANDLE hMutex = CreateEvent(NULL, FALSE, FALSE, CLOSE_MUTEX_NAME);
                    if (hMutex == NULL) { CloseServiceHandle(hManager); return false; }
                    HANDLE hMemory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 2056, SHARED_MEMORY_NAME);
                    if (hMemory == NULL) { CloseServiceHandle(hManager); CloseHandle(hMutex); return false; }
                    // Copy our data to decrypt
                    LPBYTE map = (LPBYTE)MapViewOfFile(hMemory, FILE_MAP_ALL_ACCESS, 0, 0, 2056);
                    if (map == NULL) { CloseServiceHandle(hManager); CloseHandle(hMutex); CloseHandle(hMemory); return false; }
                    memset(map, 0, 2056);
                    memcpy(map, &pIn.cbData, sizeof(pIn.cbData));
                    memcpy(map+4, pIn.pbData, pIn.cbData);


                    // Then open our service
                    // Find the full path to our executable
                    TCHAR FilePath[MAX_PATH];
                    GetModuleFileName(NULL, FilePath, MAX_PATH);
                    SC_HANDLE hService = CreateService(hManager, serviceName, displayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, FilePath, NULL, NULL, NULL, NULL, NULL);
                    // Then wait until it's finished
                    if (hService == NULL) { CloseServiceHandle(hManager); UnmapViewOfFile(map); CloseHandle(hMutex); CloseHandle(hMemory); return false; }

                    if (StartService(hService, 0, NULL) == FALSE)
                    {
//                        printf("Start service failed: %08X [%s]\n", GetLastError(), FilePath);
                        DeleteService(hService); CloseServiceHandle(hService); CloseServiceHandle(hManager); UnmapViewOfFile(map); CloseHandle(hMutex); CloseHandle(hMemory); return false;
                    }
                    // Should wait on a mutex here
                    if (WaitForSingleObject(hMutex, 2000) == WAIT_OBJECT_0)
                    {
                        memcpy(&pOut.cbData, map+1028, sizeof(pOut.cbData));
                        pOut.pbData = map+1032;
                    } else { DeleteService(hService); CloseServiceHandle(hService); CloseServiceHandle(hManager); UnmapViewOfFile(map); CloseHandle(hMutex); CloseHandle(hMemory); return false; }
                    DeleteService(hService);
                    CloseServiceHandle(hService);
                    CloseServiceHandle(hManager);
                    CloseHandle(hMutex);
//                    printf("Uncrypted: %p\n", pOut.pbData);

                    if (!memcmp( pOut.pbData, keyZero, 32 ))
                    {
                        wkeyIndex++;
                        UnmapViewOfFile(map); CloseHandle(hMemory);
                        continue;
                    }
                    /*
                    if (SSID != (buffer + 0x14))
                    {
                        wkeyIndex++;
                        UnmapViewOfFile(map); CloseHandle(hMemory);
                        continue;
                    }
                    */
                    // Ok, dump the key now
//                      wsprintf(keyString, _T("  %-32s  "), buffer + 0x14 );
//                      printf("  %-32s  ", buffer + 0x14 );

//                    for( int i = 0; i < (int) pOut.cbData; i++ )
//                      printf("%02X", pOut.pbData[i] ^ basicXor[i % 32] );
                    keyString[0] = 0;
                    for( int i = 0; i < (int) pOut.cbData; i++ )
                        wsprintf(&keyString[lstrlen(keyString)],  _T("%02X"), pOut.pbData[i] ^ basicXor[i % 32] );
#ifdef _MBCS
                    sprintf((char*)buffer, "%s", keyString);
#else
                    sprintf((char*)buffer, "%S", keyString);
#endif
                    key += (const char*)buffer;
                    key += "\n";

                    wkeyIndex++;
                    UnmapViewOfFile(map); CloseHandle(hMemory);
                    continue;
                }

                RegCloseKey(hWirelessConf);
                return true;

            }
#endif
            return false;
#else
            /** @todo Should read WPA supplicant conf file to find out the informations */
            return false;
#endif
        }

        // Get the next IP address on our subnet.
        IPV4 & IPV4::operator ++()
        {
            address.sin_addr.s_addr = htonl(ntohl(address.sin_addr.s_addr) + 1);
            return *this;
        }
        // Get the previous IP address on our subnet.
        IPV4 & IPV4::operator --()
        {
            address.sin_addr.s_addr = htonl(ntohl(address.sin_addr.s_addr) - 1);
            return *this;
        }


        // Construct an IPV4 address
        IPV4::IPV4(const uint8 a, const uint8 b, const uint8 c, const uint8 d, const uint16 port)
        {
            memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl((uint32)(a<<24) | (b<<16) | (c<<8) | d);
        }
        // Construct an IPV4 address
        IPV4::IPV4(const uint32 addr, const uint16 port)
        {
            memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl(addr);
        }
        // Construct an IPV4 address from another address
        IPV4::IPV4(const IPV4 & other) : address(other.address) {}



        // Get the address as a UTF-8 textual value
        String IPV6::asText() const
        {
            char tmp[46] = {0}; // "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"
            int error = getnameinfo((struct sockaddr*)&address, sizeof(address), tmp, sizeof(tmp), NULL, 0, NI_NUMERICHOST);
            if (!error) return "";
            return String::Print("[%s:%d]", tmp, getPort());
        }
        // Set the address as a UTF-8 textual value
        bool IPV6::asText(const String & textualValue)
        {
            struct addrinfo hints;
            memset(&hints, '\0', sizeof(hints));
            hints.ai_family = AF_INET6;
            struct addrinfo * res = 0;
            int error = getaddrinfo((const char *)textualValue, NULL, &hints, &res);
            if (error == 0 || res->ai_addrlen != sizeof(address)) return false;
            struct sockaddr_in6 * addr = (struct sockaddr_in6 *)res->ai_addr;
            memcpy(&address.sin6_addr, &addr->sin6_addr, sizeof(addr->sin6_addr));
            return true;
        }

        // Get the port if specified in the address (0 otherwise)
        uint16 IPV6::getPort() const
        {
            return ntohs(address.sin6_port);
        }
        // Set the port if possible in this address
        bool IPV6::setPort(const uint16 port)
        {
            address.sin6_port = ntohs(port);
            return true;
        }

        // Get the protocol (if specified in this address)
        IPV6::Protocol IPV6::getProtocol() const
        {
            return getProtocolFromPortNumber(ntohs(address.sin6_port));
        }
        // Set the protocol (if specified in this address)
        bool IPV6::setProtocol(const IPV4::Protocol protocol)
        {
            return setPort((uint16)protocol);
        }

        // Check if the address is valid
        bool IPV6::isValid() const
        {
            uint8 tmp[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            return memcmp(&address.sin6_addr, tmp, sizeof(address.sin6_addr)) != 0;
        }

        // Get a bindable address.
        const BaseAddress & IPV6::getBindableAddress() const
        {   // IPv6 address are bindable
            return *this;
        }
        // Get the low level object for this address
        const void * IPV6::getLowLevelObject() const
        {
            return &address;
        }
        // Get the low level object for this address
        const int IPV6::getLowLevelObjectSize() const
        {
            return sizeof(address);
        }

        IPV6::IPV6(const uint16 a, const uint16 b, const uint16 c, const uint16 d,
                   const uint16 e, const uint16 f, const uint16 g, const uint16 h,
                   const uint16 port)
        {
            memset(&address, 0, sizeof(address));
            address.sin6_family = AF_INET6;
            address.sin6_port = htons(port);
#define SetAddr(X, Y) ((uint8*)&address.sin6_addr)[X] = Y >> 8; ((uint8*)&address.sin6_addr)[X+1] = Y & 0xFF
            SetAddr(0, a); SetAddr(2, b); SetAddr(4, c); SetAddr(6, d); SetAddr(8, e); SetAddr(10, f); SetAddr(12, g); SetAddr(14, h);
#undef SetAddr
        }
        IPV6::IPV6(const uint8 (&addressString)[16], const uint16 port)
        {
            memset(&address, 0, sizeof(address));
            memcpy(address.sin6_addr.s6_addr, addressString, sizeof(*addressString) * 16);
            address.sin6_family = AF_INET6;
            address.sin6_port = htons(port);
        }
        IPV6::IPV6(const uint8 * addressString, const uint16 port)
        {
            memset(&address, 0, sizeof(address));
            memcpy(address.sin6_addr.s6_addr, addressString, sizeof(*addressString) * 16);
            address.sin6_family = AF_INET6;
            address.sin6_port = htons(port);
        }

        IPV6::IPV6(const IPV6 & other) : address(other.address) {}

        // Query the name server from an textual address
        IPV6 IPV6::queryNameServer(const String & host, const Time::TimeOut & timeout)
        {
            if (timeout <= 0) return IPV6();
#ifndef NEXIO
            if (timeout == DefaultTimeOut)
            {
                struct addrinfo hints = {0}, *res = 0;
                hints.ai_family = AF_INET6;
                hints.ai_flags = AI_NUMERICHOST;

                // First try without resolving the domain name
                if (getaddrinfo((const char*)host.upToFirst("]", true), NULL, &hints, &res) != 0)
                {
                    // Ok, then resolve
                    hints.ai_flags = 0;
                    if (getaddrinfo((const char*)host.upToFirst("]", true), NULL, &hints, &res) != 0)
                        return IPV6();
                }
                String port = host.fromFirst("]");
                if (res[0].ai_family == AF_INET6 && !((struct sockaddr_in6 *)res[0].ai_addr)->sin6_port && port.getLength())
                    ((struct sockaddr_in6 *)res[0].ai_addr)->sin6_port = htons((unsigned int)port);


                IPV6 result((const uint8*)((struct sockaddr_in6 *)res[0].ai_addr)->sin6_addr.s6_addr, (uint16)ntohs(((struct sockaddr_in6 *)res[0].ai_addr)->sin6_port));
                freeaddrinfo(res);
                return result;
            }
#endif
            // Not implemented yet with a timeout
            return IPV6();
        }

        // URL Constructors
        URL::URL() {}
        URL::URL(const String & inputURL, const String & defaultScheme)
        {
            splitURI(inputURL);
            if (!scheme.getLength()) scheme = defaultScheme;
        }
        URL::URL(const String & _scheme, const String & _authority, const String & _path, const String & _query, const String & _fragment)
            : scheme(_scheme), authority(_authority), path(_path), query(_query), fragment(_fragment) { }

        #define isIn(X, Y) _isIn(X, sizeof(X) / sizeof(X[0]), Y)
        inline bool _isIn(const char * array, const unsigned int len, char ch)
        {
            for (unsigned int i = 0; i < len; ++i)
                if (array[i] == ch) return true;
            return false;
        }
        inline bool isHex(const char a)  {  return (a >= '0' && a <= '9') || (a >= 'a' && a <= 'f') || (a >= 'A' && a <= 'F'); }

        static const char unreserved[]     = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                                                'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                                                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                                '-', '_', '.', '!', '~', '*', '\'', '(', ')' };
        static const char less_reserved[]  = { ';', ':', '@', '&', '=', '+', '$', ',' };
        static const char ipv6_specific[]  = { '[', ']' };
        static const char reserved[]       = { ';', '/', '?', ':', '@', '&', '=', '+', '$', ',' };


        // Escape an URL to only allowed chars
        String URL::escapedURI(const String & inputURL, URL::EscapingRules rules, const bool trim)
        {
            String tmp = trim ? inputURL.Trimmed() : inputURL;

            String ret;
            if (rules == QueryOnly)
            {
                for (int i = 0; i < tmp.getLength(); i++)
                    if (isIn(unreserved, tmp[i]))  ret += tmp[i];
                    else                           ret += String().Format("%%%02x", (unsigned char)tmp[i]);
            }
            else for (int i = 0; i < tmp.getLength(); i++)
                    if (isIn(unreserved, tmp[i])  || isIn(less_reserved, tmp[i]) || isIn(reserved, tmp[i]))
                        ret += tmp[i];
                    else ret += String().Format("%%%02x", (unsigned char)tmp[i]);

            return ret;
        }

        static int fromHex(const Strings::VerySimpleReadOnlyString & s, int & consumed)
        {
            const char * t = (const char*)s.getData();
            if (s.getLength() < 2) return consumed = 0;
            char a = *t, b = *(t+1);
            if (a < '0' || a > 'f' || (a > '9' && a < 'A') || (a > 'F' && a < 'a')) return consumed = 0;
            if (b < '0' || b > 'f' || (b > '9' && b < 'A') || (b > 'F' && b < 'a')) return consumed = 0;
            if (a > 'F') a -= 'a' - 'A';
            if (b > 'F') b -= 'a' - 'A';
            consumed = 2;
            return (int)( (a > '9' ? (unsigned)(a - 'A' + 10) : (unsigned)(a - '0')) << 4) | (b > '9' ? (unsigned)(b - 'A' + 10) : (unsigned)(b - '0'));
        }

        // Unescape an URI
        String URL::unescapedURI(const String & _inputURL)
        {
            String ret;
            static String urlEscape = "%+";
            Strings::VerySimpleReadOnlyString inputURL(_inputURL);
            bool utf8Encoded = true;
            int pos = inputURL.findAnyChar(urlEscape, 0);
            while (pos != inputURL.getLength())
            {
                ret += Strings::convert(inputURL.splitAt(pos));
                if (inputURL[0] == '+') { ret += ' '; inputURL.splitAt(1); }
                else
                {
                    // There are two possibilities here. On old URI implementation, direct unicode encoding was allowed
                    // However, on new URI's RFC implementation, the RFC mandates that unicode characters to be UTF-8 encoded.
                    // Because this method can not assume any of these, the following algorithm is followed.
                    // If the sequence that follow this encoding is a valid UTF-8 text, then UTF-8 is being used without
                    // any conversion (UTF-8 is prioritary)
                    // Else, unicode is being used instead.
                    String enc; Strings::VerySimpleReadOnlyString backup(inputURL);
                    int val = 0, consumed = 0;
                    while (utf8Encoded  && inputURL[0] == '%')
                    {
                        inputURL.splitAt(1);
                        val = fromHex(inputURL.splitAt(2), consumed);
                        if (!consumed) ret += '%';
                        enc += (uint8)val;
                    }
                    if (utf8Encoded && Strings::checkUTF8(enc) == 0) ret += enc;
                    else
                    {   // Back to plain old unicode encoding of value by value
                        inputURL = backup;
                        utf8Encoded = false;
                        inputURL.splitAt(1);
                        val = fromHex(inputURL.splitAt(2), consumed);
                        if (!consumed) ret += '%';

                        wchar_t unicode[2] = { (wchar_t)val, 0};
                        Strings::ReadOnlyUnicodeString str(unicode, 1);
                        ret += Strings::convert(str); // We should support Unicode too here
                    }
                }
                pos = inputURL.findAnyChar(urlEscape, 0);
            }
            ret += Strings::convert(inputURL);
            return ret;
        }

        // Split a text based URI
        bool URL::splitURI(const String & inputURL)
        {   // Based on http://www.ietf.org/rfc/rfc2396.txt
            const char * input = (const char*)inputURL;
            const int32  length = (int32)inputURL.getLength();
           // static const char escaped[]        = { '%', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'a', 'b', 'c', 'd', 'e', 'f' };
            static const char excluded[]       = { '{', '}', '|', '\\', '^', '[', ']', '`' };
            static const char breakScheme[]    = { ':', '/', '?', '#' };
            static const char ipAddr[]         = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'a', 'b', 'c', 'd', 'e', 'f', '.', ':' };

            int i = 0;
            int scheme_e = 0, auth_s = 0, auth_e = 0, path_s = 0, path_e = 0, query_s = 0, query_e = 0, frag_s = 0, frag_e = 0;
            int absoluteURI = 0, hier_part = 0, opaque_part = 0, relativeURI = 0, ipv6ASCount = 0, ipv6AECount = 0;
            int onlyIpAddr = 1;
            // First loop on data, validate char and determine if URI is absolute or relative
            while (i < length)
            {   // Disallowed characters
                if (input[i] < 0x21 || (unsigned char)input[i] > 0x7F) return false;
                if (input[i] == '<' || input[i] == '>' /*|| input[i] == '#'*/ || input[i] == '"') return false;

                if (input[i] == '%')
                {
                    onlyIpAddr = 0;
                    if ((i + 2 > length || !isHex(input[i+1]) || !isHex(input[i+2]))) return false;
                    i+= 2; continue;
                }
                if (isIn(excluded, input[i]))
                {
                    if (input[i] == '[' && ipv6ASCount == 0) { ipv6ASCount++; ++i; continue; }
                    if (input[i] == ']' && ipv6AECount == 0 && ipv6ASCount == 1) { ipv6AECount++; ++i; continue; }
                    return false;
                }
                if (!isIn(ipAddr, input[i]))
                {
                    onlyIpAddr = 0;
                }


                if (!absoluteURI && isIn(breakScheme, input[i]))
                {
                    if (input[i] == ':')
                    {
                        scheme_e = i; absoluteURI = 1;
                        if (i + 1 < length && input[i+1] == '/') { hier_part = 1; ++i; }
                        else if (i + 1 < length && input[i+1] != '/') { opaque_part = 1; ++i; }
                    }
                    else { absoluteURI = 0; relativeURI = 1; }
                    ++i; continue;
                }

                ++i;
            }

            i = 0;
            if (onlyIpAddr)
            {
                // If there is only ip address digit, it's likely a IP address
                // While it's incorrect URL (a scheme is required), it's too common to refuse parsing it
                auth_s = 0;
                auth_e = length;
            }
            else if (absoluteURI)
            {
                if (scheme_e) i = scheme_e + 1;
                if (i+1 < length)
                {
                    if (hier_part)
                    {
                        if (input[i+1] == '/')
                        {
                            i += 2;
                            auth_s = i;
                            // Read the authority now
                            while (i < length && (isIn(unreserved, input[i]) || input[i] == '%' || isIn(less_reserved, input[i]) || isIn(ipv6_specific, input[i]))) ++i;
                            auth_e = i;
                        }

                        // Read the path now if any existing
                        if (i < length && input[i] == '/')
                        {
                            // Path starting
                            path_s = i;
                            while (i < length && input[i] != '?' && input[i] != '#') ++i;
                            path_e = i;

                            // If there is a query read it
                            if (input[i] == '?')
                            {
                                query_s = i+1;
                                ++i;
                                while (i < length && input[i] != '#') ++i;
                                query_e = i;
                            }
                        }
                    } else if (opaque_part)
                    {
                        // Read the path now
                        path_s = i;
                        while (i < length && (isIn(unreserved, input[i]) || input[i] == '%' || isIn(reserved, input[i]))) ++i;
                        path_e = i;

                        // No query to read
                    }
                    // Go to fragment parsing
                }
            } else if (relativeURI)
            {
                if (i+1 < length && input[i] == '/' && input[i+1] == '/')
                {
                    ++i;
                    auth_s = i;
                    // Read the authority now
                    while (i < length && (isIn(unreserved, input[i]) || input[i] == '%' || isIn(less_reserved, input[i]))) ++i;
                    auth_e = i;

                    // Read the path now if any existing
                    if (i < length && input[i] == '/')
                    {
                        // Path starting
                        path_s = i;
                        while (i < length && input[i] != '?' && input[i] != '#') ++i;
                        path_e = i;

                        // If there is a query read it
                        if (i < length && input[i] == '?')
                        {
                            query_s = i+1;
                            ++i;
                            while (i < length && input[i] != '#') ++i;
                            query_e = i;
                        }
                    }
                } else if (i < length)
                {
                    // Read the path now
                    path_s = i;
                    while (i < length && (isIn(unreserved, input[i]) || input[i] == '%' || isIn(reserved, input[i])) && input[i] != '?') ++i;
                    path_e = i;

                    if (i < length && input[i] == '?')
                    {
                        query_s = i+1;
                        ++i;
                        while (i < length && input[i] != '#') ++i;
                        query_e = i;
                    }
                    // Go to fragment parsing
                }
            }

            // Parse the fragment
            if (i < length && input[i] == '#')
            {
                frag_s = i+1;
                frag_e = length;
            }

            scheme      = scheme_e ? String((const void*)input, scheme_e) : "";
            authority   = auth_e ? String((const void*)&input[auth_s], auth_e - auth_s) : "";
            path        = path_e ? String((const void*)&input[path_s], path_e - path_s) : "";
            query       = query_e ? String((const void*)&input[query_s], query_e - query_s) : "";
            fragment    = frag_e ? String((const void*)&input[frag_s], frag_e - frag_s) : "";
            if (!scheme_e && !auth_e && !path_e && !query_e && !frag_e) path = input;
            // Make sure ";" separated elements could be parsed correctly
            query.replaceAllTokens(';', '&');
            return true;
        }
#undef isIn
        // Normalize a given path
        void URL::normalizePath(String & pathToNormalize) const
        {
            String outputStack;

            while (pathToNormalize.getLength())
            {
                if (pathToNormalize.midString(0, 3) == "../") pathToNormalize = pathToNormalize.midString(3, pathToNormalize.getLength());
                else if (pathToNormalize.midString(0, 2) == "./") pathToNormalize = pathToNormalize.midString(2, pathToNormalize.getLength());
                else if (pathToNormalize.midString(0, 3) == "/./") pathToNormalize = "/" + pathToNormalize.midString(3, pathToNormalize.getLength());
                else if (pathToNormalize == "/.") pathToNormalize = "/" + pathToNormalize.midString(2, pathToNormalize.getLength());
                else if (pathToNormalize.midString(0, 3) == "/.." || pathToNormalize == "/../")
                {
                    pathToNormalize = "/" + pathToNormalize.midString(4, pathToNormalize.getLength());
                    int lastSegmentPos = outputStack.reverseFind('/', outputStack.getLength());
                    if (lastSegmentPos != -1) outputStack = outputStack.midString(0, lastSegmentPos);
                }
                else if (pathToNormalize.invFindAnyChar(".", 0) == -1) pathToNormalize = "";
                else
                {
                    int firstSlash = pathToNormalize.Find('/', 0);
                    if (firstSlash == 0) firstSlash = pathToNormalize.Find('/', 1);
                    if (firstSlash == -1) { outputStack += pathToNormalize; pathToNormalize = ""; }
                    else
                    {
                        outputStack += pathToNormalize.midString(0, firstSlash);
                        pathToNormalize = pathToNormalize.midString(firstSlash, pathToNormalize.getLength());
                    }
                }
            }
            pathToNormalize = outputStack;
        }
        // Check if this URL is valid
        bool URL::isValid() const { return authority.getLength() > 0; }

        // Construct a text from this URL
        String URL::asURI(const String & defaultScheme) const
        {
            String schemeTmp = scheme.getLength() ? scheme : defaultScheme;
            if (schemeTmp.getLength()) schemeTmp += "://";
            schemeTmp += authority + ((path.getLength() && path[0] != '/' && authority && schemeTmp) ? "/" : "") + path;
            if (query.getLength()) schemeTmp += "?" + query;
            if (fragment.getLength()) schemeTmp += "#" + fragment;
            return schemeTmp;
        }
        // Append path from the given path
        URL URL::appendRelativePath(String newPath) const
        {
            URL ret(scheme, authority, "", query, fragment);
            // Check if new path contain a fragment or a query
            int fragmentPos = newPath.reverseFind("#", newPath.getLength());
            if (fragmentPos != -1)
            {
                ret.fragment = newPath.midString(fragmentPos+1, newPath.getLength());
                newPath = newPath.midString(0, fragmentPos);
            }
            int queryPos = newPath.reverseFind("?", newPath.getLength());
            if (queryPos != -1)
            {
                ret.query = newPath.midString(queryPos+1, newPath.getLength());
                newPath = newPath.midString(0, queryPos);
            }
            // Let's normalize newPath
            ret.path = (path.upToLast("/") + "/" + newPath).replacedAll("//", "/");
            normalizePath(ret.path);
            return ret;
        }
        // Strip port information from authority and return it if known
        uint16 URL::stripPortFromAuthority(uint16 defaultPortValue, const bool saveNewAuthority)
        {
            int pos = authority.Find(']'); // Check IPV6
            String auth = pos == -1 ? authority : authority.midString(pos + 1, authority.getLength());
            int portPos = auth.reverseFind(':', auth.getLength());
            if (portPos != -1)
            {
                String portValue = auth.midString(portPos+1, auth.getLength());
                if (saveNewAuthority) authority.findAndReplace(":"+portValue, "");
                portPos = defaultPortValue;
                portValue.Scan("%d", &portPos);
                if (portPos < 0) portPos = 0;
                if (portPos > 65535) portPos = 65535;
                return (uint16)portPos;
            }
            return defaultPortValue;
        }
        // Get the address as a UTF-8 textual value
        String URL::asText() const { return asURI(); }
        // Set the address as a UTF-8 textual value
        bool URL::asText(const String & textualValue) { return splitURI(textualValue); }

        static BaseAddress::Protocol getProtocolFromScheme(const String & scheme)
        {
            if (scheme == "echo") return BaseAddress::Echo;
            else if (scheme == "discard")   return BaseAddress::Discard;
            else if (scheme == "daytime")   return BaseAddress::DayTime;
            else if (scheme == "ftp")       return BaseAddress::FTP;
            else if (scheme == "ssh")       return BaseAddress::SSH;
            else if (scheme == "telnet")    return BaseAddress::Telnet;
            else if (scheme == "smtp")      return BaseAddress::SMTP;
            else if (scheme == "time")      return BaseAddress::Time;
            else if (scheme == "wins")      return BaseAddress::WINS;
            else if (scheme == "whois")     return BaseAddress::WhoIs;
            else if (scheme == "dns")       return BaseAddress::DNS;
            else if (scheme == "dhcp")      return BaseAddress::DHCP;
            else if (scheme == "tftp")      return BaseAddress::TFTP;
            else if (scheme == "http")      return BaseAddress::HTTP;
            else if (scheme == "pop")       return BaseAddress::POP3;
            else if (scheme == "nntp")      return BaseAddress::NNTP;
            else if (scheme == "ntp")       return BaseAddress::NTP;
            else if (scheme == "imap")      return BaseAddress::IMAP;
            else if (scheme == "snmp")      return BaseAddress::SNMP;
            else if (scheme == "irc")       return BaseAddress::IRC;
            else if (scheme == "ldap")      return BaseAddress::LDAP;
            else if (scheme == "https")     return BaseAddress::HTTPS;
            else if (scheme == "smb")       return BaseAddress::Samba;
            else if (scheme == "smtps")     return BaseAddress::SMTPS;
            else if (scheme == "rtsp")      return BaseAddress::RTSP;
            else if (scheme == "nntps")     return BaseAddress::NNTPS;
            return BaseAddress::Unknown;
        }

        // Get the port if specified in the address (0 otherwise)
        uint16 URL::getPort() const
        {
            uint16 port = const_cast<URL*>(this)->stripPortFromAuthority(0, false);
            if (port == 0 && getProtocolFromScheme(scheme) != BaseAddress::Unknown)
                return (uint16)getProtocol();
            return port;
        }
        // Set the port if possible in this address
        bool URL::setPort(const uint16 port)
        {
            stripPortFromAuthority(0, true);
            authority += String().Format(":%d", port);
            return true;
        }

        // Get the protocol (if specified in this address)
        URL::Protocol URL::getProtocol() const
        {
            URL::Protocol protocol = getProtocolFromScheme(scheme);
            if (protocol == BaseAddress::Unknown)
                return getProtocolFromPortNumber(getPort());
            return protocol;
        }
        // Set the protocol (if specified in this address)
        bool URL::setProtocol(const URL::Protocol protocol)
        {
            return setPort((uint16)protocol);
        }

        // Get a bindable address.
        const BaseAddress & URL::getBindableAddress() const
        {   // Default is to return IPv4 address
            address = IPV4::queryNameServer(authority);
            if (address.isValid())
            {
                address.setPort(getPort());
                return address;
            }
            address6 = IPV6::queryNameServer(authority);
            if (address6.isValid())
            {
                address6.setPort(getPort());
                return address6;
            }
            return address;
        }
        // Get the low level object for this address
        const void * URL::getLowLevelObject() const
        {
            const BaseAddress & addr = getBindableAddress();
            return addr.getLowLevelObject();
        }
        // Get the low level object for this address
        const int URL::getLowLevelObjectSize() const
        {
            const BaseAddress & addr = getBindableAddress();
            return addr.getLowLevelObjectSize();
        }

        // Append a variable to the query string (this perform URL text escaping on-the-way)
        bool URL::appendQueryVariable(String name, const String & value, const bool forceAppending)
        {
            // Check if the variable already exist
            name = escapedURI(name, QueryOnly);
            int pos = query.caselessFind("&" + name);
            if (pos == -1) pos = query.midString(0, name.getLength()) == name ? 0 : -1;
            if (pos != -1 && !forceAppending) return false;
            query += (query ? "&" : "") + name + "=" + escapedURI(value, QueryOnly, false);
            return pos == -1;
        }

        // Find a variable value in the query
        bool URL::findQueryVariable(String name, String & value) const
        {
            name = escapedURI(name, QueryOnly);
            String queryClean = query;
            bool start = queryClean.midString(0, name.getLength()) == name;
            if (!start) queryClean = query.fromFirst("&" + name);
            else queryClean.splitAt(name.getLength());
            if ( start || queryClean )
            {
                if (queryClean[0] == '=') { queryClean.splitAt(1); value = unescapedURI(queryClean.upToFirst("&")); }
                else if (queryClean.midString(0, 2) == "[]=") { queryClean.splitAt(3); value = unescapedURI(queryClean.upToFirst("&")); }
                return true;
            }
            return false;
        }

        bool URL::clearQueryVariable(String name)
        {
            name = escapedURI(name, QueryOnly);
            String beginOfTheQuery = query.midString(0, name.getLength());
            if(beginOfTheQuery == name)
                query.splitUpTo("&");
            else
            {
                int posBegin = query.caselessFind("&" + name);
                // Not in the query ?
                if (posBegin == -1) return false;

                int posEnd = query.caselessFind("&", posBegin + 1);
                // At the end ?
                if(posEnd == -1) query = query.midString(0, posBegin);
                // In the middle
                else             query.Remove(posBegin, posEnd - posBegin);
            }
            return true;
        }

#if _LINUX == 1
        // Get the address as a UTF-8 textual value
        String Ethernet::asText() const
        {
            return String::Print("%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3], address[4], address[5]);
        }
        // Set the address as a UTF-8 textual value
        bool Ethernet::asText(const String & textualValue)
        {
            String rawBytes = textualValue.Trimmed().replacedAll(":", "");
            if ((size_t)rawBytes.getLength() < ArrSz(address) * 2) return false;
            for (size_t i = 0; i < ArrSz(address); i++)
            {
                int consumed = 0;
                address[i] = (uint8)rawBytes.splitAt(2).parseInt(16, &consumed);
                if (consumed < 2) return false;
            }
            return true;
        }


        // Get the protocol (if specified in this address)
        BaseAddress::Protocol Ethernet::getProtocol() const { return (Protocol)protocol; }
        // Set the protocol (if specified in this address)
        bool Ethernet::setProtocol(const Protocol proto) { protocol = (uint16)proto; return true; }

        // Check if the address is valid
        bool Ethernet::isValid() const
        {   // Prevent broadcast and 0 address from validating
            static uint8 invalidAddress[12] = { 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
            return memcmp(address, invalidAddress, ArrSz(address)) != 0 && memcmp(address, &invalidAddress[6], ArrSz(address)) != 0;
        }

        // Get a bindable address.
        const BaseAddress & Ethernet::getBindableAddress() const
        {
            // Need to enumerate the interfaces to find out this address
            static IPV4 address;
            const String & text = asText();
            int index = 0; IPV4 mask, gateway; IPV4::Type type; String macAddr;
            while (IPV4::getLocalInterfaceDetails(index, address, mask, gateway, type, macAddr))
            {
                if (macAddr.caselessEqual(text)) return address;
                index++;
            }
            address.asText("0.0.0.0");
            return address;
        }

        // Get the adapter's name for the given address
        String Ethernet::getAdapterName() const
        {
            int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
            if (descriptor == -1) return "";
            // Then set the available options
            struct ifconf ifc = {}; // To enumerate all interfaces
            // Need to get the interface name based on its mac address
            // Get all interfaces
            char buf[1024];
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            if (ioctl(descriptor, SIOCGIFCONF, &ifc) < 0) { close(descriptor); return ""; }

            // Find the given local interface
            struct ifreq * ifr = ifc.ifc_req;
    	    int nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
    	    for(int i = 0; i < nInterfaces; i++)
    	    {
                // Get HW address for this interface
                if (ioctl(descriptor, SIOCGIFHWADDR, &ifr[i]) < 0) continue;
    		    if (memcmp(ifr[i].ifr_hwaddr.sa_data, address, ArrSz(address)) == 0)
                {
                    close(descriptor);
                    return ifr[i].ifr_name;
                }
            }
            close(descriptor);
            return "";
        }
        // Construct the address from the given adapter name
        Ethernet * Ethernet::fromAdapterName(const String & name)
        {
            int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
            if (descriptor == -1) return 0;

            struct ifreq ifr;
            ifr.ifr_addr.sa_family = AF_INET;
            strncpy(ifr.ifr_name, name, IFNAMSIZ-1);
            Ethernet * ether = 0;

            if (ioctl(descriptor, SIOCGIFHWADDR, &ifr) >= 0)
                ether = new Ethernet((const uint8*)ifr.ifr_hwaddr.sa_data, 6);
            close(descriptor);
            return ether;
        }

#endif

    }
}
