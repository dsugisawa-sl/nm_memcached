//
// Created by dsugisawa on 2017/07/17.
//
#include "inc/nmmc.inl"
#include "inc/nmmc_arch_impl.hpp"
#include "inc/nmmc_packet.hpp"
#include "inc/nmmc_def.h"
#include "inc/nmmc_server.hpp"
#include <event2/event.h>
#include <event2/thread.h>

using namespace NMMC;

/**
 * bsd-socketモードは、ロジック：デバッグ用途
 *
 */
static const int TIMEOUT_MSEC = (1000);
static const int NMMC_BUFFER_SIZE = (2048);



class SocketSingleton{
public:
    SocketSingleton():ref_(0),to_counter_(0),fd_(-1){
        pthread_mutex_init(&mtx_,NULL);
#ifdef	EVTHREAD_USE_PTHREADS_IMPLEMENTED
        evthread_use_pthreads();
#endif
    }
    static void OnRecv(evutil_socket_t sock, short what, void *arg){
        SocketSingleton* inst = (SocketSingleton*)arg;
        Server* server = inst->server_;
        struct sockaddr_in sa;
        socklen_t salen = sizeof(sa);
        struct sockaddr_in ssa;
        socklen_t ssalen = sizeof(ssa);
        char rbuf[NMMC_BUFFER_SIZE];

        // まずヘッダを読み取る(読み込みポジションは進めない)
        auto rlen = recvfrom(sock, rbuf, sizeof(protocol_binary_request_header), MSG_PEEK, (struct sockaddr*)&ssa, &ssalen);
        // ヘッダ長まで読めていないのは、読み捨てる
        if (rlen != sizeof(protocol_binary_request_header)){
            recvfrom(sock, rbuf, 4, 0, NULL, NULL);
            LOG("invalid read header(%d)", (int)rlen);
            return;
        }
        // 全体ペイロードを読み込む
        auto ph = (protocol_binary_request_header*)rbuf;
        rlen = recvfrom(sock, rbuf, sizeof(*ph) + ntohl(ph->request.bodylen), 0, (struct sockaddr*)&ssa, &ssalen);
        if (rlen != ntohl(ph->request.bodylen) + sizeof(*ph)){
            recvfrom(sock, rbuf, 4, 0, NULL, NULL);
            LOG("invalid read body(%d - %u)", (int)rlen,ntohl(ph->request.bodylen));
            return;
        }
        // 汎用ヘッダのバリデート
        if (!validate_header(ph, rlen)){
            LOG("invalid validate header(%d)", (int)rlen);
            return;
        }
        //
        if (getsockname(sock, (struct sockaddr *)&sa, &salen) < 0){
            LOG("getsockname(%d)", sock);
            return;
        }
        // ロジック実行
        // プトロコルスタックがボトルネックとなるので
        // 内部経路へのスケールアウトはなし
        Packet pkth((char*)ph, rlen, &ssa, ssalen);
        if (server->OnRecieve(&pkth) != 0){
            inst->Send_Unsafe(&pkth);
        }
    }
    static void OnTimeOut(evutil_socket_t sock, short what, void *arg){
        SocketSingleton* inst = (SocketSingleton*)arg;
        inst->to_counter_++;
        LOG("OnTimeOut.(%u)",inst->to_counter_);
    }
    int Poll(Server* server){
        int ret = 0;
        pthread_mutex_lock(&mtx_);
        server_ = server;
        ret = event_base_loop(event_base_, EVLOOP_ONCE);
        pthread_mutex_unlock(&mtx_);
        return(ret);
    }
    void Lock(void){
        pthread_mutex_lock(&mtx_);
    }
    void UnLock(void){
        pthread_mutex_unlock(&mtx_);
    }
    int Send_Unsafe(Packet* pkt){
        int ret = 0;
        ret = sendto(fd_, pkt->Header(Packet::PAYLOAD), pkt->Length(), 0, (struct sockaddr*)pkt->Addr(),(socklen_t)pkt->AddrLen());
        if (ret != pkt->Length()){
            LOG("Send(%u!=%u)", ret, pkt->Length());
        }
        return(ret);
    }
    int AddRef(const char* ifnm,int port){
        unsigned ref = 0;
        int on = 1;
        //
        pthread_mutex_lock(&mtx_);
        ref = ref_;
        ref_++;
        if (ref == 0){
            if ((fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
                throw std::runtime_error("failed. udp socket..");
            }
            addr_.sin_family    = AF_INET;
            addr_.sin_port      = htons(port);
            addr_.sin_addr.s_addr = htonl(INADDR_ANY);
            //
            if (bind(fd_, (struct sockaddr*)&addr_, sizeof(addr_))) {
                throw std::runtime_error("bind..");
            }
            if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))< 0){
                throw std::runtime_error("setsockopt(SO_REUSEADDR)..");
            }
            auto flags = fcntl(fd_,F_GETFL,0);
            if (flags < 0){
                throw std::runtime_error("fcntl(F_GETFL)..");
            }
            fcntl(fd_,F_SETFL,flags|O_NONBLOCK);
            //
            if (!(event_base_ = event_base_new())){
                throw std::runtime_error("malloc event base..");
            }
            recv_event_     = event_new(event_base_, fd_, EV_READ|EV_PERSIST, SocketSingleton::OnRecv, this);
            timeout_event_  = event_new(event_base_, -1, EV_TIMEOUT|EV_PERSIST, SocketSingleton::OnTimeOut, this);
            if (!recv_event_ || !timeout_event_){
                throw std::runtime_error("malloc event..");
            }
            struct timeval now;
            now.tv_sec  = TIMEOUT_MSEC / 1000;
            now.tv_usec = (TIMEOUT_MSEC % 1000) * 1000;
            //
            if (event_add(recv_event_, NULL) || event_add(timeout_event_, &now)) {
                throw std::runtime_error("event_add..");
            }
        }
        pthread_mutex_unlock(&mtx_);
        return(fd_);
    }
    void Release(void){
        unsigned ref = 0;
        pthread_mutex_lock(&mtx_);
        ref = ref_;
        ref_--;
        if (ref == 1){
            event_base_loopexit(event_base_, NULL);
            LOG("Release %d", fd_);
            close(fd_);
        }
        pthread_mutex_unlock(&mtx_);
    }
private:
    int fd_,addrlen_;
    unsigned ref_,to_counter_;
    pthread_mutex_t mtx_;
    struct sockaddr_in  addr_;
    struct event_base   *event_base_;
    struct event        *recv_event_;
    struct event        *timeout_event_;
    Server* server_;
};

static SocketSingleton s_sock;

Socket::Socket():fd_(-1){}

Socket::~Socket(){
    LOG("Socket Close %d", fd_);
    if (fd_ != -1){
        s_sock.Release();
    }
}
int Socket::Open(const char* ifnm,int affinity, int port){
    // 対象インタフェイスのMACアドレスを保持
    FindMacAddr(ifnm);
    //
    fd_ = s_sock.AddRef(ifnm, port);
    if (fd_ == -1) {
        LOG("Unable to open %s: %s", ifnm, strerror(errno));
        throw std::runtime_error("Unable open netmap desc");
    }
    LOG("success open %s [%d]", ifnm, port);

    return(0);
}
int Socket::Poll(Server* server){
    return(s_sock.Poll(server));
}
int Socket::Send(Packet* pkt){
    int ret = 0;
    s_sock.Lock();
    ret = s_sock.Send_Unsafe(pkt);
    s_sock.UnLock();
    return(ret);
}
