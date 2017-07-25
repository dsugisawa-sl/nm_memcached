//
// Created by dsugisawa on 2017/07/17.
//

#ifndef NM_MEMCACHED_NMMC_ARCH_IMPL_HPP
#define NM_MEMCACHED_NMMC_ARCH_IMPL_HPP

#include "inc/nmmc.hpp"
#include "inc/nmmc_arch_if.hpp"

// アーキテクチャインタフェイス：
// netmap /bsd socket(simulation)
namespace NMMC{
  class Server;
  //
  class Socket:public ArchIf{
  public:
      Socket();
      virtual ~Socket();
  public:
      virtual int Open(const char*,int,int);
      virtual int Poll(Server*);
      virtual int Send(Packet*);
  private:
      int                 fd_;
  }; // class Socket

  class Netmap:public ArchIf{
  public:
      Netmap();
      virtual ~Netmap();
  public:
      virtual int Open(const char*,int,int);
      virtual int Poll(Server*);
      virtual int Send(Packet*);
  private:
      int SearchValidRing(void);
      int ProcessRing(void* ,void*);
      int OnArp(Packet*);
      int OnIcmp(Packet*);
      int OnUdp(Packet*);
      void SwapMac(Packet*);
  private:
      int affinity_;
      struct pollfd     pollfd_;
      struct nm_desc   *desc_;
      void*             rxslot_;
      void*             txslot_;
      uint64_t          pkt_count_,evt_count_;
      Server*           server_;
  }; // class Netmap

};


#endif //NM_MEMCACHED_NMMC_ARCH_IMPL_HPP
