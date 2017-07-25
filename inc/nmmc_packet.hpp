//
// Created by daisuke.sugisawa.ts on 2017/07/17.
//

#ifndef NM_MEMCACHED_NMMC_PACKET_HPP
#define NM_MEMCACHED_NMMC_PACKET_HPP
#include "inc/nmmc.hpp"

namespace NMMC{
  class Packet{
  public:
      enum { DC = 0, ETHER, IP, UDP, PAYLOAD, ARP, ICMP, MAX };
  public:
      Packet(char*,int);
      Packet(char*,int,struct sockaddr_in*,unsigned int);
      ~Packet();
  public:
      virtual void* Header(int);
      virtual int   Length(void);
      virtual void  Length(int);
      virtual int   Type(void);
      virtual int   LengthVlan(void);
      virtual int   LengthIp(void);
      struct sockaddr_in* Addr(void);
      unsigned int AddrLen(void);
  private:
      char* data_;
      int   len_;
      int   len_vlan_;
      int   len_ipv_;
      int   offset_[Packet::MAX];
      int   type_;
      struct sockaddr_in sa_;
      unsigned int salen_;
  private:
      Packet(){}   // witout copy
  }; // class Packet
};

#endif //NM_MEMCACHED_NMMC_PACKET_HPP
