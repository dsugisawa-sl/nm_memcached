//
// Created by dsugisawa on 2017/07/16.
//
#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
using namespace NMMC;

int NMMC::SetAffinity(pthread_t me, int i){
#ifdef cpuset_t
    cpuset_t cpumask;
    if (i == -1) { return(0); }
    CPU_ZERO(&cpumask);
    CPU_SET(i, &cpumask);
    if (pthread_setaffinity_np(me, sizeof(cpuset_t), &cpumask) != 0) {
        fprintf(stderr, "Unable to set affinity: %s\n", strerror(errno));
        return(1);
    }
#endif
    return(0);
}

int NMMC::GetInterface(const char* ifname, char* hwaddr, unsigned hwaddrlen, char* in_addr, unsigned in_addrlen){
    int		layer2_socket;
    struct ifreq		ifr;
    memset(&ifr,0,sizeof(ifr));

    if (hwaddrlen != ETHER_ADDR_LEN || in_addrlen != sizeof(uint32_t)){
        LOG("buffer length\n");
        return(-1);
    }
#ifdef __APPLE__
    if ((layer2_socket = socket(PF_ROUTE,SOCK_RAW,htons(SOCK_RAW))) < 0){
#else
    if ((layer2_socket = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL))) < 0){
#endif
        LOG("wrong socket\n");
        return(-1);
    }
    strcpy(ifr.ifr_name,ifname);
#ifdef __APPLE__
    struct ifaddrs *ifa_list, *ifa;
    struct sockaddr_dl *dl = NULL;

    if (getifaddrs(&ifa_list) < 0) {
        LOG("not found..interfaces.");
        return(-1);
    }
    for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
        dl = (struct sockaddr_dl*)ifa->ifa_addr;
        if (dl->sdl_family == AF_LINK && dl->sdl_type == IFT_ETHER &&
            memcmp(dl->sdl_data, ifname, MIN(sizeof(dl->sdl_data), strlen(ifname)))== 0) {
            // set result(mac)
            memcpy(hwaddr, LLADDR(dl), ETHER_ADDR_LEN);
            // set result(ipaddr)
            memcpy(in_addr, &((struct sockaddr_in *)&dl->sdl_data)->sin_addr,4);
            goto INTERFACE_FOUND;
        }
    }
    LOG("not found...(%s)", ifname);
    return(-1);
INTERFACE_FOUND:
    freeifaddrs(ifa_list);
#else
    if (ioctl(layer2_socket, SIOCGIFHWADDR, &ifr) < 0){
          LOG("failed.ioctl.(SIOCGIFHWADDR) %s", ifname);
          close(layer2_socket);
          return(-1);
      }
      // set result(mac)
      memcpy(hwaddr, ifr.ifr_hwaddr.sa_data,ETHER_ADDR_LEN);
      if (ioctl(layer2_socket, SIOCGIFADDR, &ifr) < 0){
          LOG("failed.ioctl.(SIOCGIFADDR) %s", ifname);
      }else{
          // set result(ipaddr)
          memcpy(in_addr, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,4);
      }
#endif
    close(layer2_socket);

    return(0);
}
