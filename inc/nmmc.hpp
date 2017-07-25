//
// Created by dsugisawa on 2017/07/16.
//

#ifndef NM_MEMCACHED_NM_MEMCACHED_HPP
#define NM_MEMCACHED_NM_MEMCACHED_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include <net/netmap_user.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>


#ifndef __APPLE__
#include <linux/if_ether.h>
#include <linux/sockios.h>
#else
#include <libgen.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#endif

#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <ifaddrs.h>
#include <time.h>
#include <pthread.h>
#include <poll.h>

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// stl
#include <iostream>
#include <sstream>
#include <bitset>
#include <string>

namespace NMMC{
  int SetAffinity(pthread_t, int);
  int GetInterface(const char* , char* , unsigned , char* , unsigned );

}; // namespace NMMC



#endif //NM_MEMCACHED_NM_MEMCACHED_HPP
