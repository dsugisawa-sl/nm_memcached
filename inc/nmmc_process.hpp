//
// Created by dsugisawa on 2017/07/16.
//

#ifndef NM_MEMCACHED_NMMC_PROCESS_HPP
#define NM_MEMCACHED_NMMC_PROCESS_HPP
#include "inc/nmmc.hpp"

namespace NMMC{
  class Process {
  public:
      Process(const char*);
  public:
      void Setup(int argc, char** argv);
      void Start(void);
      void Run(void);
      void Stop(void);
  public:
      int     affinity_,debug_,port_;
      char    ifnm_[64];
      pthread_t thread_;
  private:
      Process(){}
  };// class Process
};


#endif //NM_MEMCACHED_NMMC_PROCESS_HPP
