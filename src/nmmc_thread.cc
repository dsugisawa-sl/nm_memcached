//
// Created by dsugisawa on 2017/07/17.
//

#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_thread.hpp"
#include "inc/nmmc_server.hpp"
#include "inc/nmmc_arch_if.hpp"

using namespace NMMC;

#ifdef __APPLE__
#define NMMC_THREAD (2)
#else
#define NMMC_THREAD (8)
#endif

static const int s_thread_count = NMMC_THREAD;
static Thread  s_thread_00;
static Thread  s_thread_01;
static Thread  s_thread_02;
static Thread  s_thread_03;
static Thread  s_thread_04;
static Thread  s_thread_05;
static Thread  s_thread_06;
static Thread  s_thread_07;
static Thread  s_thread_08;
static Thread  s_thread_09;
static Thread  s_thread_10;
static Thread  s_thread_11;
static Thread  s_thread_12;
static Thread  s_thread_13;
static Thread  s_thread_14;
static Thread  s_thread_15;

static void* thread_func(void*);

Thread::Thread(){
    id_ = fd_ = cancel_ = affinity_ = port_= 0;
    pkts_ = bytes_ = 0;
    bzero(&thread_, sizeof(thread_));
    bzero(ifnm_,sizeof(ifnm_));
}

void Thread::Start(const char* ifnm){
    strncpy(ifnm_, ifnm, sizeof(ifnm_)-1);
    if (pthread_create(&thread_, NULL, thread_func, this) == -1) {
        LOG("Unable to create thread %d: %s", id_, strerror(errno));
        throw std::runtime_error("Unable to create thread");
    }
}
void Thread::Join(){
    for(auto n = 0;n < Count();n++){
        auto t = Ref(n);
        pthread_join(t->thread_, NULL);
    }
}

int Thread::Count(void){
    return(s_thread_count);
}
Thread* NMMC::Thread::Ref(int id){
    switch(id){
        case 0: return(&s_thread_00);
        case 1: return(&s_thread_01);
        case 2: return(&s_thread_02);
        case 3: return(&s_thread_03);
        case 4: return(&s_thread_04);
        case 5: return(&s_thread_05);
        case 6: return(&s_thread_06);
        case 7: return(&s_thread_07);

        case 8: return(&s_thread_08);
        case 9: return(&s_thread_09);
        case 10:return(&s_thread_10);
        case 11:return(&s_thread_11);
        case 12:return(&s_thread_12);
        case 13:return(&s_thread_13);
        case 14:return(&s_thread_14);
        case 15:return(&s_thread_15);
        default:
            throw std::runtime_error("invalid index.");
    }
}
void* thread_func(void* arg){
    auto t = (Thread*)arg;

    if (t->affinity_ >= 0) {
        NMMC::SetAffinity(pthread_self(),t->affinity_);
    }
    // サーバ、ネットワークインスタンス
    auto server = Server::Create(t->id_);
    auto network = ArchIf::Create();

    if (network->Open(t->ifnm_, t->affinity_,t->port_)){
        throw std::runtime_error("can not open network interface.");
    }
    while(!t->cancel_){
        network->Poll(server);
    }
    //
    ArchIf::Release(&network);
    Server::Release(&server);

    return(NULL);
}
