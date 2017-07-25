//
// Created by dsugisawa on 2017/07/17.
//

#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_arch_impl.hpp"
#include "inc/nmmc_arch_if.hpp"

using namespace NMMC;

ArchIf* ArchIf::Create(void){
#ifdef __APPLE__
    return(new Socket());
#else
    return(new Netmap());
#endif
}
void ArchIf::Release(ArchIf** arch){
#ifdef __APPLE__
    delete ((Socket*)(*arch));
#else
    delete ((Netmap*)(*arch));
#endif
    (*arch) = NULL;
}

void ArchIf::FindMacAddr(const char* ifnm){
    struct ether_arp  arp;
    // インタフェイスMACアドレス
    memset(&arp, 0, sizeof(arp));
    if (GetInterface(ifnm, (char*)arp.arp_sha, sizeof(arp.arp_sha), (char*)arp.arp_spa, sizeof(arp.arp_spa)) < 0){
        throw std::runtime_error("invalid interface.");
    }
    memcpy(dmac_, arp.arp_sha, ETHER_ADDR_LEN);
}



