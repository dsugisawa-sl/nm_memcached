//
// Created by dsugisaw on 2017/07/17.
//

#ifndef NM_MEMCACHED_NMMC_ARCH_IF_HPP
#define NM_MEMCACHED_NMMC_ARCH_IF_HPP
#include "inc/nmmc.hpp"

// アーキテクチャインタフェイス：
// netmap /bsd socket(simulation)
namespace NMMC{
  class Packet;
  class Server;
  //
  class ArchIf{
  public:
      static ArchIf* Create(void);
      static void Release(ArchIf**);
  public:
      virtual int Open(const char*,int,int) = 0;
      virtual int Poll(Server*) = 0;
      virtual int Send(Packet*) = 0;
  protected:
      void FindMacAddr(const char*);
  protected:
      char  dmac_[ETHER_ADDR_LEN];
  }; // class ArchIf
};

#endif //NM_MEMCACHED_NMMC_ARCH_IF_HPP
