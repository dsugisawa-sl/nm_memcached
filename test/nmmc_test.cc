//
// Created by daisuke.sugisawa.ts on 2017/07/24.
//

#include "gtest/gtest.h"


#include <event2/event.h>
#include <event2/thread.h>
#include <stdexcept>
#include <memory>
#include <functional>
#include <vector>
#include <string>

#include "../inc/nmmc_def.h"
#include "../inc/nmmc.inl"

using namespace NMMC;

#define TEST_KEYMAX (0x01FFF)

#define THREAD_CNT  (4)
typedef struct thread_arg{
    pthread_t       thread;
    struct event_base *event_base;
    uint32_t        seq;
    int             sock;
    struct event    *recv_event;
    struct event    *send_event;
    uint64_t        send_count;
    uint64_t        send_error;
    uint64_t        recv_count;
    uint64_t        recv_error;
}thread_arg_t,*thread_arg_ptr;



static int quit_ = 0;
static std::string src_interface;
static std::string dst_interface;

static thread_arg_t thread_args[THREAD_CNT];

//
static void OnRecv(evutil_socket_t, short, void *);
static void OnSend(evutil_socket_t, short, void *);

#include <iostream>
#include <sstream>
#include <bitset>
#include <string>
TEST(NmmcTest, binstr){
    unsigned char val[32] = {
        0x12,0x34,0x56,0x78,
        0x9a,0xbc,0xde,0xf0,
        0x12,0x34,0x56,0x78,
        0x9a,0xbc,0xde,0xf0,
        0x12,0x34,0x56,0x78,
        0x9a,0xbc,0xde,0xf0,
        0x9a,0xbc,0xde,0xf0,
        0x9a,0xbc,0xde,0xf0,
        };

    std::string sql = "INSERT INTO (key,val)VALUES(123,0x";
    sql += Bin2Hex(val,32);
    sql += ");";
    LOG("%s", sql.c_str());
}

//
TEST(NmmcTest, Init){
    struct sockaddr_in addr;
    int on = 1;
#ifdef	EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#endif
    // storess socket.
    for(auto n = 0;n < THREAD_CNT;n++){
        bzero(&thread_args[n], sizeof(thread_args[n]));
        thread_args[n].seq = 1;
        //
        if (!(thread_args[n].event_base = event_base_new())){
            throw std::runtime_error("malloc event base..");
        }
        if ((thread_args[n].sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
            throw std::runtime_error("failed. socket ..");
        }
        bzero(&addr, sizeof(addr));
        addr.sin_family    = AF_INET;
        addr.sin_port      = htons(20000 + n);
        inet_pton(AF_INET, src_interface.c_str(),&addr.sin_addr.s_addr);
        //
        if (bind(thread_args[n].sock, (struct sockaddr*)&addr, sizeof(addr))){
            throw std::runtime_error("failed. bind ..");
        }
        if (setsockopt(thread_args[n].sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))< 0){
            throw std::runtime_error("setsockopt(SO_REUSEADDR)..");
        }
        auto flags = fcntl(thread_args[n].sock,F_GETFL,0);
        if (flags < 0){
            throw std::runtime_error("fcntl(F_GETFL)..");
        }
        fcntl(thread_args[n].sock,F_SETFL,flags|O_NONBLOCK);
        thread_args[n].recv_event = event_new(thread_args[n].event_base, thread_args[n].sock, EV_READ|EV_PERSIST, OnRecv, &thread_args[n]);
        thread_args[n].send_event = event_new(thread_args[n].event_base, thread_args[n].sock, EV_WRITE|EV_PERSIST, OnSend,&thread_args[n]);
        if (!thread_args[n].recv_event || !thread_args[n].send_event){
            throw std::runtime_error("malloc event..");
        }
        if (event_add(thread_args[n].recv_event, NULL)) { throw std::runtime_error("event_add.."); }
        if (event_add(thread_args[n].send_event, NULL)) { throw std::runtime_error("event_add.."); }
    }
}
void OnSend(evutil_socket_t sock, short what, void *arg){
    auto ptr = (thread_arg_ptr)arg;
    char wbf[ETHER_MAX_LEN];
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family    = AF_INET;
    addr.sin_port      = htons(11211);
    inet_pton(AF_INET, dst_interface.c_str(),&addr.sin_addr.s_addr);
    //

    auto rh = (protocol_binary_request_header*)wbf;
    bzero(rh, sizeof(*rh));
    rh->request.magic   = MAGIC_REQUEST;
    rh->request.opcode  = OPCODE_GET;
    rh->request.keylen  = htons(sizeof(uint32_t));
    rh->request.extlen  = 0;
    rh->request.bodylen = htonl(sizeof(uint32_t));
    auto key = (uint32_t*)(rh+1);
    (*key) = htonl(ptr->seq);
    auto len = sizeof(*rh) + sizeof(uint32_t);

//  fprintf(stderr, "OnSend(%u : %u : %u)\n", sock, (unsigned)len, ptr->seq);
    //
    auto ret = sendto(sock, wbf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == len){
        ptr->send_count ++;
    }else{
        ptr->send_error ++;
    }
    ptr->seq ++;
    if (ptr->seq >= TEST_KEYMAX){
        ptr->seq = 1;
    }
    usleep(80);
}
void OnRecv(evutil_socket_t sock, short what, void *arg){
    auto ptr = (thread_arg_ptr)arg;
    char rbuf[ETHER_MAX_LEN];

//  fprintf(stderr, "OnRecv(%u)\n", sock);


    // まずヘッダを読み取る(読み込みポジションは進めない)
    auto rlen = recvfrom(sock, rbuf, sizeof(protocol_binary_request_header), MSG_PEEK, NULL, NULL);
    // ヘッダ長まで読めていないのは、読み捨てる
    if (rlen != sizeof(protocol_binary_request_header)){
        recvfrom(sock, rbuf, 4, 0, NULL, NULL);
        LOG("invalid read header(%d)", (int)rlen);
        ptr->recv_error++;
        return;
    }
    // 残りのペイロードを読み取る
    auto ph = (protocol_binary_request_header*)rbuf;
    rlen = recvfrom(sock, rbuf, ntohl(ph->request.bodylen), 0, NULL, NULL);
    if (rlen != ntohl(ph->request.bodylen)){
        recvfrom(sock, rbuf, 4, 0, NULL, NULL);
        LOG("invalid read body(%d)", (int)rlen);
        ptr->recv_error++;
        return;
    }
    if (ph->request.status == 0){
        ptr->recv_count++;
    }else{
        ptr->recv_error++;
    }
}

static void* thread_loop(void* arg){
    auto ptr = (thread_arg_ptr)arg;
    while(!quit_){
        if (event_base_loop(ptr->event_base, EVLOOP_ONCE) < 0){
            fprintf(stderr, "event exit.\n");
            break;
        }
    }
    return(NULL);
}

#define VALUE_SIZE  (256)

TEST(NmmcTest, WarmUp){
    struct sockaddr_in addr;
    int on = 1;
    int sock;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        throw std::runtime_error("failed. socket ..");
    }
    bzero(&addr, sizeof(addr));
    addr.sin_family    = AF_INET;
    addr.sin_port      = htons(20000 + 1234);
    inet_pton(AF_INET, src_interface.c_str(),&addr.sin_addr.s_addr);
    //
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr))){
        throw std::runtime_error("failed. bind ..");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))< 0){
        throw std::runtime_error("setsockopt(SO_REUSEADDR)..");
    }
    for(auto n = 1;n < TEST_KEYMAX; n++){
        char wbf[ETHER_MAX_LEN];
        bzero(&addr, sizeof(addr));
        addr.sin_family    = AF_INET;
        addr.sin_port      = htons(11211);
        inet_pton(AF_INET, dst_interface.c_str(),&addr.sin_addr.s_addr);
        //
        memset(wbf, (int)n, sizeof(wbf));
        auto rh = (protocol_binary_request_header*)wbf;
        bzero(rh, sizeof(*rh));
        rh->request.magic   = MAGIC_REQUEST;
        rh->request.opcode  = OPCODE_SET;
        rh->request.keylen  = htons(sizeof(uint32_t));
        rh->request.extlen  = 8;
        rh->request.bodylen = htonl(sizeof(uint32_t) + 8 + VALUE_SIZE);
        rh->request.opaque  = n;
        auto ext = (uint32_t*)(rh+1);
        auto ext2 = (ext+1);
        (*ext) = htonl(0xdeadbeef);
        (*ext2) = htonl(0xdeadc0de);
        auto key = (ext+2);
        (*key) = htonl(n);
        auto len = sizeof(*rh) + sizeof(uint32_t) + 8 + VALUE_SIZE;
        //
        auto ret = sendto(sock, wbf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (ret != len){
            LOG("failed. warm up .....(%d)", (int)n);
        }
        usleep(100);
    }
    close(sock);


    LOG("Warmup ended. 4 seconed......");
    sleep(4);
}
TEST(NmmcTest, Run){
    auto clk_s = GetMicrosecondArround();

    for(auto n = 0;n < THREAD_CNT;n++){
        if (pthread_create(&thread_args[n].thread, NULL, thread_loop, &thread_args[n]) == -1) {
            throw std::runtime_error("pthread_create");
        }
    }
    auto c = 0;
    while(!quit_){
        sleep(1);
        if ((c++) > 10){ break; }
    }
    // スレッドの終了を待機
    for(auto n = 0;n < THREAD_CNT;n++){
        pthread_cancel(thread_args[n].thread);
    }
    auto clk_diff = (GetMicrosecondArround() - clk_s);
    sleep(1);
    // 集計する
    uint64_t        send_count = 0;
    uint64_t        send_error = 0;
    uint64_t        recv_count = 0;
    uint64_t        recv_error = 0;
    for(auto n = 0;n < THREAD_CNT;n++){
        send_count += thread_args[n].send_count;
        send_error += thread_args[n].send_error;
        recv_count += thread_args[n].recv_count;
        recv_error += thread_args[n].recv_error;
    }
    auto pps = ((((double)(/*send_count + */recv_count + recv_error) / (double)clk_diff) * 1000000.0e0));
    char bf[128],b1[64],b2[64],b3[64],b4[64],b5[64];
    // 集計
    snprintf(
            bf,sizeof(bf)-1,
            "%spps(send : %s[err : %s]/recv : %s[err : %s])\n%lf..%lf",
            Norm(b1, pps),
            Norm(b2, (double)send_count),
            Norm(b3, (double)send_error),
            Norm(b4, (double)recv_count),
            Norm(b5, (double)recv_error),pps,(double)clk_diff);

    fprintf(stderr, "======================\n");
    fprintf(stderr, "%s\n", bf);
    fprintf(stderr, "======================\n");
}

class TestEnv : public ::testing::Environment {
protected:
    virtual void SetUp() {}
    virtual void TearDown() {}
};
//
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::Environment* lteepc_test_env = ::testing::AddGlobalTestEnvironment(new TestEnv);

    if (argc != 3){
        fprintf(stderr, "[this program] <source interface ip> <dest interface ip>\n");
        exit(0);
    }
    src_interface = argv[1];
    dst_interface = argv[2];
    quit_ = 0;

    return RUN_ALL_TESTS();
}





