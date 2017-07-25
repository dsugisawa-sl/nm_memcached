//
// Created by dsugisawa on 2017/07/16.
//

#ifndef NM_MEMCACHED_NMMC_INL
#define NM_MEMCACHED_NMMC_INL

#include "inc/nmmc.hpp"

#define LOG(_fmt, ...) \
	do {    \
		struct timeval _t0; \
		gettimeofday(&_t0, NULL);   \
		fprintf(stdout, "%p(%06d)[%5d:%s] " _fmt "\n",  \
		    (void*)(pthread_self()), (int)getpid(), \
		    __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } while (0)
#define LEN_VLAN(e) (e->ether_type==htons(ETHERTYPE_VLAN)?((*(((u_short*)(e+1))+1))==htons(ETHERTYPE_VLAN)?8:4):0)
#define EH(p)   ((struct ether_header*)p->Header(Packet::ETHER))
#define IP(p)   ((struct ip*)p->Header(Packet::IP))
#define IPV6(p) ((struct ip6_hdr*)p->Header(Packet::IP))
#define ARP(p)  ((struct ether_arp*)p->Header(Packet::ARP))
#define UDP(p)  ((struct udphdr*)p->Header(Packet::UDP))
#define BFD(p)  ((bfd_ptr)p->Header(Packet::BFD))
#define ICMP(p) ((struct icmp*)p->Header(Packet::ICMP))
#define ICMPV6(p) ((struct icmp6_hdr*)p->Header(Packet::ICMP))
#define PAYLOAD(p)  ((char*)p->Header(Packet::PAYLOAD))

#define CHK_ETHERTYPE(e,t)  (e->ether_type==htons(ETHERTYPE_VLAN)?\
    (*(((u_short*)(e+1))+1))==htons(ETHERTYPE_VLAN)?\
        (*(((u_short*)(e+1))+2))==htons(t):(*(((u_short*)(e+1))+1))==htons(t):e->ether_type==htons(t))
#ifndef MIN
# define MIN(a,b) (a<b?a:b)
#endif
#ifndef MAX
# define MAX(a,b) (a>b?a:b)
#endif
#ifndef IFT_ETHER
#define IFT_ETHER 0x6/* Ethernet CSMACD */
#endif

namespace NMMC{
  static inline uint32_t CheckSum(const void *data, uint16_t len, uint32_t sum){
      unsigned  _sum   = sum,_checksum = 0;
      unsigned  _count = len;
      unsigned short* _addr  = (unsigned short*)data;
      //
      while( _count > 1 ) {
          _sum += ntohs(*_addr);
          _addr++;
          _count -= 2;
      }
      if(_count > 0 ){
          _sum += ntohs(*_addr);
      }
      while (_sum>>16){
          _sum = (_sum & 0xffff) + (_sum >> 16);
      }
      return(~_sum);
  }
  static inline unsigned short Wrapsum(unsigned sum){
      sum = sum & 0xFFFF;
      return (htons(sum));
  }
  static inline uint32_t Hash32(const char* key,uint32_t len){
      uint32_t hash = 5381;
      const char* rp = (char*)(key + len);
      while(len--){
          hash = ((hash << 5) + hash) ^ *(uint8_t*)--rp;
      }
      return(hash);
  }
  static inline unsigned long long GetMicrosecondArround(void){
      struct timeval	tv;
      gettimeofday(&tv,NULL);
      return((((uint64_t)tv.tv_sec <<19) + ((uint64_t)tv.tv_usec)));
  }
  static inline const char *Norm2(char *buf, double val, const char *fmt){
      const char *units[] = { "", "K", "M", "G", "T" };
      u_int i;
      for (i = 0; val >=1000 && i < sizeof(units)/sizeof(char *) - 1; i++)
          val /= 1000;
      sprintf(buf, fmt, val, units[i]);
      return(buf);
  }
  static inline const char *Norm(char *buf, double val){
      return Norm2(buf, val, "%.3f %s");
  }
  static inline std::string Bin2Hex(unsigned char* v,int l){
      std::string ret;
      for(int n = 0;n < l;n++){
          auto p = (uint8_t)v[n];
          std::stringstream ss;
          ss << std::hex << (int)p;
          ret += ss.str();
      }
      return(ret);
  }

};
#endif //NM_MEMCACHED_NMMC_INL
