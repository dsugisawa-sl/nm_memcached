//
// Created by dsugisawa on 2017/07/17.
//
#include "inc/nmmc.inl"
#include "inc/nmmc_def.h"
#include "inc/nmmc_arch_impl.hpp"
#include "inc/nmmc_packet.hpp"
#include "inc/nmmc_server.hpp"

using namespace NMMC;

#define NMRING(r)  ((struct netmap_ring*)r)
#define NMSLOT(s)  ((struct netmap_slot*)s)
#define NMDESC(n)  ((struct nm_desc*)n)
#define NMRING_SPACE(r)   nm_ring_space(NMRING(r))
#define NMRING_EMPTY(r)   nm_ring_empty(NMRING(r))
#define NMRING_NEXT(r,c)  nm_ring_next(NMRING(r), c);
#define WAIT_LINKSEC    (4)


Netmap::Netmap():affinity_(0),desc_(NULL){
}

Netmap::~Netmap(){
    LOG("Netmap shutdown");
    if (desc_){
        LOG("nm_close(%d)",desc_->fd);
        nm_close(desc_);
    }
    desc_ = NULL;
}

int Netmap::Open(const char* ifnm,int affinity, int port){
    char nmbf[64] = {0};
    struct nmreq base_nmd;

    affinity_ = affinity;
    // 対象インタフェイスのMACアドレスを保持
    FindMacAddr(ifnm);
    //
    bzero(&base_nmd, sizeof(base_nmd));
    snprintf(nmbf,sizeof(nmbf)-1,"netmap:%s-%d", ifnm,affinity_);
    desc_ = nm_open(nmbf,  &base_nmd, 0, NULL);
    if (desc_ == NULL) {
        LOG("Unable to open %s: %s", nmbf, strerror(errno));
        throw std::runtime_error("Unable open netmap desc");
    }
    LOG("success open %s", nmbf);
    LOG("<PA>%d tx /%d rx rings. cpu[%u] %s",
                   desc_->req.nr_tx_rings, desc_->req.nr_rx_rings, affinity, nmbf);
    /* setup poll(2) variables. */
    pollfd_.fd = desc_->fd;
    sleep(WAIT_LINKSEC);

    return(0);
}
int Netmap::Poll(Server* server){
    int ret,pktcnt = 0;
    // initialize event flag.
    pollfd_.revents = 0;
    pollfd_.events  = POLLIN;
    ioctl(pollfd_.fd, NIOCRXSYNC, NULL);
    ioctl(pollfd_.fd, NIOCTXSYNC, NULL);
    //
    ret = poll(&pollfd_, 1, 200);
    if (ret <= 0) {
        if (ret < 0 && errno != EAGAIN && errno != EINTR){
            LOG("poll error %s", strerror(errno));
            return(-1);
        }
        return(0);
    }
    //
    if (pollfd_.revents & POLLIN) {
        server_ = server;
        if ((pktcnt = SearchValidRing()) > 0){
            pkt_count_ += pktcnt;
            evt_count_ ++;
        }
    }
    return(0);
}
int Netmap::Send(Packet* pkt){
    // 送信 = swap index (ts -> rs for zero copy.)
    auto tidx   = NMSLOT(txslot_)->buf_idx;
    NMSLOT(txslot_)->buf_idx = NMSLOT(rxslot_)->buf_idx;
    NMSLOT(rxslot_)->buf_idx = tidx;
    //
    NMSLOT(rxslot_)->len = pkt->Length();
    NMSLOT(txslot_)->len = NMSLOT(rxslot_)->len;
    NMSLOT(txslot_)->flags |= NS_BUF_CHANGED;
    NMSLOT(rxslot_)->flags |= NS_BUF_CHANGED;

    return(0);
}
// private function's
int Netmap::SearchValidRing(void){
    // ネットマップ:NICスロットをバッチ処理
    u_int m = 0;
    auto rxringidx = desc_->first_rx_ring;
    auto txringidx = desc_->first_tx_ring;
    //
    while (rxringidx <= desc_->last_rx_ring && txringidx <= desc_->last_tx_ring) {
        void* rxr = NETMAP_RXRING(desc_->nifp, rxringidx);
        void* txr = NETMAP_TXRING(desc_->nifp, txringidx);
        if (NMRING_EMPTY(rxr)) {
            rxringidx++;
            continue;
        }
        if (NMRING_EMPTY(txr)) {
            txringidx++;
            continue;
        }
        m += ProcessRing(rxr, txr);
    }
    return (m);
}


int Netmap::ProcessRing(void* rxr, void* txr){
    int ret = 0;
    int process_count = 0;
    uint32_t txring_space_work;
    auto rxring_cur_work = NMRING(rxr)->cur;                  // RX
    auto txring_cur_work = NMRING(txr)->cur;                  // TX
    auto limit = NMRING(rxr)->num_slots;

    /* print a warning if any of the ring flags is set (e.g. NM_REINIT) */
    if (NMRING(rxr)->flags || NMRING(txr)->flags){
        LOG("rxflags %x txflags %x", NMRING(rxr)->flags, NMRING(txr)->flags);
        NMRING(rxr)->flags = 0;
        NMRING(txr)->flags = 0;
    }
    //
    txring_space_work = NMRING_SPACE(rxr);
    if (txring_space_work < limit){
        limit = txring_space_work;
    }
    txring_space_work = NMRING_SPACE(txr);
    if (txring_space_work < limit){
        limit = txring_space_work;
    }
    while (limit-- > 0) {
        // init processed flag
        void *rxslot = &NMRING(rxr)->slot[rxring_cur_work];
        void *txslot = &NMRING(txr)->slot[txring_cur_work];
        char* req = NETMAP_BUF(NMRING(rxr), NMSLOT(rxslot)->buf_idx);
        char* res = NETMAP_BUF(NMRING(txr), NMSLOT(txslot)->buf_idx);
        uint16_t reslen = NMSLOT(rxslot)->len;
        uint16_t reqlen = NMSLOT(rxslot)->len;
        // prefetch the buffer for next loop.
        __builtin_prefetch(req);

        /* swap packets */
        if (NMSLOT(txslot)->buf_idx < 2 || NMSLOT(rxslot)->buf_idx < 2) {
            LOG("wrong index rx[%d] = %d  -> tx[%d] = %d",
                                   rxring_cur_work, NMSLOT(rxslot)->buf_idx, txring_cur_work, NMSLOT(txslot)->buf_idx);
            sleep(2);
        }
        /* copy the packet length. */
        if (NMSLOT(rxslot)->len > 2048) {
            LOG("wrong len %d rx[%d] -> tx[%d]", NMSLOT(rxslot)->len, rxring_cur_work, txring_cur_work);
            NMSLOT(rxslot)->len = 0;
        }
        // パケットパース
        Packet pkth(req, NMSLOT(rxslot)->len);
        auto ptype =pkth.Type();
        ret = 0;
        //
        if (likely(ptype == Packet::UDP)){
            ret = OnUdp(&pkth);
        }else if (ptype == Packet::ARP){
            ret = OnArp(&pkth);
        }else if (ptype == Packet::ICMP){
            ret = OnIcmp(&pkth);
        }
        // zero copy send.
        if (ret & NEED_SEND){
            rxslot_ = rxslot;
            txslot_ = txslot;
            // Reflect on Same NIC.(swap index.)
            Send(&pkth);
        }
        rxring_cur_work = NMRING_NEXT(rxr, rxring_cur_work);
        if (ret & NEED_SEND){
            txring_cur_work = NMRING_NEXT(txr, txring_cur_work);
        }
        process_count ++;
    }
    NMRING(rxr)->head = NMRING(rxr)->cur = rxring_cur_work;
    NMRING(txr)->head = NMRING(txr)->cur = txring_cur_work;

    return (process_count);
}
int Netmap::OnArp(Packet* pkt){
    struct ether_header *eh  = EH(pkt);
    struct ether_arp    *arp = ARP(pkt);
    struct ether_arp    tarp;

    SwapMac(pkt);
    //
    arp->ea_hdr.ar_hrd = ntohs(ARPHRD_ETHER);
    arp->ea_hdr.ar_pro = ntohs(ETHERTYPE_IP);
    arp->ea_hdr.ar_hln = ETHER_ADDR_LEN;
    arp->ea_hdr.ar_pln = sizeof(in_addr_t);
    arp->ea_hdr.ar_op  = ntohs(ARPOP_REPLY);
    // find own mac.
    memcpy(&tarp, arp, sizeof(tarp));
    memcpy(arp->arp_tpa, tarp.arp_spa, sizeof(arp->arp_tpa));
    memcpy(arp->arp_tha, tarp.arp_sha, sizeof(arp->arp_tha));
    memcpy(arp->arp_spa, tarp.arp_tpa, sizeof(arp->arp_spa));
    memcpy(arp->arp_sha, dmac_, ETHER_ADDR_LEN);
    // eh->ether_type = ntohs(ETHERTYPE_ARP);
    memcpy(eh->ether_shost, arp->arp_sha, ETHER_ADDR_LEN);
    return(NEED_SEND);
}
int Netmap::OnIcmp(Packet* pkt){
    auto    *ip_i = IP(pkt);
    auto    *ip_6 = IPV6(pkt);
    uint32_t hlen = 0;
    //
    SwapMac(pkt);
    // ipv4
    if (likely(CHK_ETHERTYPE(EH(pkt),ETHERTYPE_IP))){
        hlen = (sizeof(struct ether_header) + pkt->LengthVlan() + pkt->LengthIp());
        auto tmp_addr = IP(pkt)->ip_dst;
        IP(pkt)->ip_dst = IP(pkt)->ip_src;
        IP(pkt)->ip_src = tmp_addr;
        IP(pkt)->ip_sum = 0;
        IP(pkt)->ip_sum = Wrapsum(CheckSum(IP(pkt), sizeof(struct ip), 0));
        //
        ICMP(pkt)->icmp_type = ICMP_ECHOREPLY;
        ICMP(pkt)->icmp_cksum = 0;
        ICMP(pkt)->icmp_cksum = Wrapsum(CheckSum(ICMP(pkt), pkt->Length() - hlen, 0));
    }else{
        hlen = (sizeof(struct ether_header) + pkt->LengthVlan() + pkt->LengthIp());
        __uint8_t tmp_addr[16];
        memcpy(tmp_addr, ip_6->ip6_dst.s6_addr, sizeof(tmp_addr));
        memcpy(ip_6->ip6_dst.s6_addr, ip_6->ip6_src.s6_addr, sizeof(tmp_addr));
        memcpy(ip_6->ip6_src.s6_addr, tmp_addr, sizeof(tmp_addr));
        // ICMP echo
        if (ICMPV6(pkt)->icmp6_type == ICMP6_ECHO_REQUEST){
            ICMPV6(pkt)->icmp6_type = ICMP6_ECHO_REPLY;
            ICMPV6(pkt)->icmp6_cksum = 0;
            ICMPV6(pkt)->icmp6_cksum = Wrapsum(CheckSum(ICMPV6(pkt), pkt->Length() - hlen, 0));
        // ICMP neighbor solicitaion(arp相当)
        }else if (ICMPV6(pkt)->icmp6_type == ND_NEIGHBOR_SOLICIT){
            struct nd_neighbor_solicit* nnsl = (struct nd_neighbor_solicit*)ICMPV6(pkt);
            struct nd_neighbor_advert*  nnad = (struct nd_neighbor_advert*)ICMPV6(pkt);
            struct nd_opt_hdr*          nopt = (struct nd_opt_hdr*)(nnad+1);
            char*                       maca = (char*)(nopt+1);
            if ((pkt->Length() - hlen) != (sizeof(struct nd_neighbor_advert) + sizeof(struct nd_opt_hdr) + ETHER_ADDR_LEN)){
                LOG("invalid request length");
            }
            LOG("target:(%04X%04X%04X%04X-%04X%04X%04X%04X)",
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[0]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[2]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[4]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[6]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[8]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[10]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[12]),
                *((uint16_t*)&nnsl->nd_ns_target.s6_addr[14]));

            nnad->nd_na_flags_reserved = (ND_NA_FLAG_SOLICITED|ND_NA_FLAG_OVERRIDE);
            nnad->nd_na_type  = ND_NEIGHBOR_ADVERT;
            nnad->nd_na_code  = 0;
            nnad->nd_na_cksum = 0;
            nnad->nd_na_cksum = Wrapsum(CheckSum(nnad, pkt->Length() - hlen, 0));
            nopt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
            nopt->nd_opt_len  = 1;    //  The length of the option in units of 8 octets
            memcpy(maca, dmac_, ETHER_ADDR_LEN);
        }
    }
    return(NEED_SEND);
}
int Netmap::OnUdp(Packet* pkt){
    SwapMac(pkt);
    // ip = 0000FF00
    auto oct3 = (uint8_t)((IP(pkt)->ip_dst.s_addr>>8)&0xFF);
    if (EXTERNAL_IP_OCT3 == oct3){
        // キーハッシュから、インターナル経路に分散
        if (pkt->Length() > sizeof(protocol_binary_request_header)){
            auto h = (protocol_binary_request_header*)PAYLOAD(pkt);
            auto key = (char*)(h+1);
            if ((ntohl(h->request.bodylen) + sizeof(protocol_binary_request_header)) == pkt->Length()){
                // ヘッダ長が有効 -> キーからハッシュ値を計算
                auto hash = Hash32(key, ntohs(h->request.keylen));
                // 内部経路で負荷分散
                // （Switch に当該経路を設定要）
                IP(pkt)->ip_dst.s_addr = (IP(pkt)->ip_dst.s_addr&0xFFFF00FF);
                IP(pkt)->ip_dst.s_addr |= (uint32_t)(((uint8_t)((INTERNAL_IP_OCT3 + (hash%USECORE_COUNT))&0xFF))<<8);
                //
                return(NEED_SEND);
            }
        }
    }else if(INTERNAL_IP_OCT3 == oct3){
        return(server_->OnRecieve(pkt));
    }
    return(0);
}

void Netmap::SwapMac(Packet* pkt){
    // swap dst <-> src
    uint8_t tmp_mac[ETHER_ADDR_LEN];
    memcpy(tmp_mac, EH(pkt)->ether_dhost, ETHER_ADDR_LEN);
    memcpy(EH(pkt)->ether_dhost, EH(pkt)->ether_shost, ETHER_ADDR_LEN);
    memcpy(EH(pkt)->ether_shost, tmp_mac, ETHER_ADDR_LEN);
}
