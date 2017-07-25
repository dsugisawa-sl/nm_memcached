//
// Created by dsugisawa on 2017/07/17.
//
#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_packet.hpp"

using namespace NMMC;

Packet::Packet(char* data,int len,struct sockaddr_in* sa,unsigned int salen):data_(data),len_(len),len_vlan_(0),type_(Packet::DC),len_ipv_(0){
    len_vlan_ = 0;
    len_ipv_ = 0;
    if (sa){
        memcpy(&sa_,sa,sizeof(sa_));
        salen_ = salen;
    }
    //
    offset_[Packet::ETHER] = 0;
    offset_[Packet::IP]    = 0;
    offset_[Packet::UDP]   = 0;
    offset_[Packet::ARP]   = 0;
    offset_[Packet::ICMP]  = 0;
    offset_[Packet::PAYLOAD] = 0;
    type_ = Packet::UDP;
}
Packet::Packet(char* data,int len):data_(data),len_(len),len_vlan_(0),type_(Packet::DC),len_ipv_(0){
    struct ether_header *eh = (struct ether_header*)data;
    struct ip           *ip = (struct ip*)(data + sizeof(struct ether_header) + LEN_VLAN(eh));
    len_vlan_ = LEN_VLAN(eh);
    bzero(&sa_,sizeof(sa_));
    salen_ = 0;
    // ipv4/6
    if (likely(ip->ip_v==IPVERSION)){
        len_ipv_= (ip->ip_hl*4);
    }else{
        len_ipv_= sizeof(struct ip6_hdr);
    }
    //
    offset_[Packet::ETHER] = 0;
    offset_[Packet::IP]    = sizeof(struct ether_header) + LEN_VLAN(eh);
    offset_[Packet::UDP]   = sizeof(struct ether_header) + LEN_VLAN(eh) + (len_ipv_);
    offset_[Packet::ARP]   = sizeof(struct ether_header) + LEN_VLAN(eh);
    offset_[Packet::ICMP]  = offset_[Packet::UDP];
    offset_[Packet::PAYLOAD] = sizeof(struct ether_header) + LEN_VLAN(eh) + (len_ipv_) + sizeof(struct udphdr);
    // パケットタイプ
    if (likely(CHK_ETHERTYPE(EH(this),ETHERTYPE_IP))){
        if (likely(IP(this)->ip_p == IPPROTO_UDP)){
            type_ = Packet::UDP;
        }else if (IP(this)->ip_p == IPPROTO_ICMP && IP(this)->ip_ttl != 0x00){
            type_ = Packet::ICMP;
        }
    }else if (CHK_ETHERTYPE(EH(this),ETHERTYPE_IPV6)){
        if (IPV6(this)->ip6_nxt == IPPROTO_ICMPV6 && IPV6(this)->ip6_hlim != 0x00){
            type_ = Packet::ICMP;
        }else if (IPV6(this)->ip6_nxt == IPPROTO_UDP){
            type_ = Packet::UDP;
        }
    }else if (CHK_ETHERTYPE(EH(this), ETHERTYPE_ARP)) {
        type_ = Packet::ARP;
    }
}
Packet::~Packet(){}
//
void* Packet::Header(int id){
    if (likely(id > Packet::DC && id < Packet::MAX)){
        return(data_ + offset_[id]);
    }
    return(NULL);
}
int Packet::Length(void){ return(len_); }
void Packet::Length(int len){ len_=len; }
int Packet::Type(void){ return(type_); }
int Packet::LengthVlan(void){ return(len_vlan_); }
int Packet::LengthIp(void){ return(len_ipv_); }
struct sockaddr_in* Packet::Addr(void){ return(&sa_); }
unsigned int Packet::AddrLen(void){ return(salen_); }
