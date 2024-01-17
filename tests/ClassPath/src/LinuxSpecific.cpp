#include "../../lib/include/Types.hpp"

#if defined(_LINUX)
    // We have to build this out of the standard header, as we are using kernel stuff that conflicts with userspace include
    #include <sys/ioctl.h>
    #include <linux/if.h>
    #include <linux/wireless.h>
    #include <linux/rtnetlink.h>
    #include <linux/sockios.h>

    typedef uint32_t __u32;         /* hack to avoid including kernel header in userspace */
    typedef uint16_t __u16;         /* ditto */
    typedef uint8_t __u8;           /* ditto */
    // For camir version of #include <linux/ethtool.h>
    typedef uint32_t  u32;            /* hack to avoid including kernel header in userspace */
    typedef uint16_t  u16;            /* ditto */
    typedef uint8_t   u8;             /* ditto */
    typedef uint64_t  u64;            /* ditto */
    #include <linux/ethtool.h>

int getEthernetRate(int sock, struct ifreq * item)
{
    struct ethtool_cmd eth_data = {0};
    item->ifr_data=(char *)&eth_data;
    eth_data.cmd = ETHTOOL_GSET; /* get setting */
    if (ioctl(sock, SIOCETHTOOL, item) >= 0)
        return eth_data.speed;
    return 0;
}


int getWIFIRate(int sock, const char * name)
{
    struct iwreq wreq = {0};
    strncpy(wreq.ifr_name, name, IFNAMSIZ);
    if (ioctl(sock, SIOCGIWRATE, &wreq) >= 0)
        return wreq.u.bitrate.value;
    return 0;
}

#endif
