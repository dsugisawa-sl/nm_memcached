//
// Created by dsugisawa on 2017/07/16.
//

#ifndef NM_MEMCACHED_NMMC_THREAD_HPP
#define NM_MEMCACHED_NMMC_THREAD_HPP
#include "inc/nmmc.hpp"

namespace NMMC{
  class Observe {
  public:
      Observe():pkts_(0),bytes_(0),events_(0),drop_(0){
          gettimeofday(&tm_);
      }
      Observe(uint64_t pkts, uint64_t bytes, uint64_t events, uint64_t drop, struct timeval tm):
          pkts_(pkts),bytes_(bytes),events_(events),drop_(drop),tm_(tm){
      }
      Observe(const Observe& cp){
          pkts_ = cp.pkts_;
          bytes_ = cp.bytes_;
          events_ = cp.events_;
          drop_ = cp.drop_;
          tm_ = cp.tm_;
      }
  public:
      uint64_t   pkts_,bytes_,events_,drop_;
      struct timeval tm_;
  };// class Observe
};


#endif //NM_MEMCACHED_NMMC_THREAD_HPP
