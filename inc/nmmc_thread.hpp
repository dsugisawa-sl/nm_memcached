//
// Created by dsugisawa on 2017/07/16.
//

#ifndef NM_MEMCACHED_NMMC_THREAD_HPP
#define NM_MEMCACHED_NMMC_THREAD_HPP
#include "inc/nmmc.hpp"


namespace NMMC{
  class Thread {
  public:
      Thread();
  public:
      void Start(const char*);
      void Join();
  public:
      static int Count(void);
      static Thread* Ref(int);
  public:
      int       id_,fd_,cancel_,affinity_,port_;
      pthread_t thread_;
      uint64_t  pkts_;
      uint64_t  bytes_;
      char      ifnm_[64];
  };// class Thread
};


#endif //NM_MEMCACHED_NMMC_THREAD_HPP
