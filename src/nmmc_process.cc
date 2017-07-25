//
// Created by dsugisawa on 2017/07/16.
//
#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_process.hpp"
#include "inc/nmmc_thread.hpp"

using namespace NMMC;

static void sigint_handler(int sig);
static void usage(void);
static int s_quit_ = 0;

Process::Process(const char* nm){
    affinity_ = 0;
    debug_ = 0;
    port_ = 0;
    bzero(ifnm_,sizeof(ifnm_));
    bzero(&thread_, sizeof(thread_));
}
void Process::Start(void){
    int n;
    static const int PHIRESET_WAITSEC = 4;

    for (n = 0; n < Thread::Count(); n++) {
        auto t = Thread::Ref(n);
        //
        t->id_ = n;
        t->affinity_ = (affinity_ + n);
        t->port_ = port_;
        t->Start(ifnm_);
    }
    // phi - resetを待機
    sleep(PHIRESET_WAITSEC);
}
void Process::Run(void){
    while(!s_quit_){
        usleep(100000);
    }
    Stop();
}
void Process::Stop(void){
    int n,cnt = Thread::Count();
    for (n = 0; n < cnt; n++) {
        Thread::Ref(n)->cancel_=1;
    }
    for (n = 0; n < cnt; n++) {
        Thread::Ref(n)->Join();
    }
}


void Process::Setup(int argc, char** argv){
    struct sigaction sa;
    int ch;
    LOG("setup.");

    while ((ch = getopt(argc, argv, "i:c:vp:")) != -1) {
        if (ch == 'i' && optarg){
            strncpy(ifnm_, optarg, sizeof(ifnm_)-1);
        }else if (ch == 'c' && optarg){
            affinity_ = atoi(optarg);
        }else if (ch == 'p' && optarg){
            port_  = atoi(optarg);
        }else if (ch == 'v'){
            debug_ ++;
        }
    }
    sa.sa_handler = sigint_handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        LOG("failed to install ^C handler: %s", strerror(errno));
        throw std::runtime_error("failed to install ^C handler");
    }
    // 引数チェック
    if (!ifnm_[0] || !port_){
        usage();
        throw std::runtime_error("invalid arguments.");
    }
}

void sigint_handler(int sig) {
    int n,cnt = Thread::Count();
    LOG("received control-C on thread %p", (void *)pthread_self());
    for (n = 0; n < cnt; n++) {
        Thread::Ref(n)->cancel_=1;
    }
    s_quit_ = 1;
}
void usage(void){
    const char *cmd = "nmmc";
    fprintf(stderr,
            "Usage:\n"
                    "%s arguments\n"
                    "\t-i interface interface name(ex.netmap:eth4-0)\n"
                    "\t-c affinity start idx\n"
                    "\t-t thread num\n"
                    "\t-p port\n"
                    "\t-v verbose\n"
                    "",
            cmd);
}