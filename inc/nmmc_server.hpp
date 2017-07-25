//
// Created by dsugisawa on 2017/07/17.
//

#ifndef NM_MEMCACHED_NMMC_SERVER_HPP
#define NM_MEMCACHED_NMMC_SERVER_HPP
#include "inc/nmmc.hpp"

namespace NMMC{

  // サーバインスタンスはスレッド毎（コア毎）に生成
  // スレッド毎のテーブルとなる
  class Packet;
  //
  class Server {
  public:
      static Server* Create(uint16_t);
      static void Release(Server**);
  public:
      int OnRecieve(Packet*);
  protected:
      int OnRecieveGet(Packet*,uint32_t);
      int OnRecieveSet(Packet*,uint32_t, char*, int);
  private:
      void*         lookup_value_;
      uint16_t      server_id_;
      pthread_t     thread_;
      void*         dbhandle_;
  public:
      int           cancel_;
      int           dbserverid_;
      std::string   dbuser_;
      std::string   dbpswd_;
      std::string   dbhost_;
      std::string   dbport_;
      std::string   dbinst_;
  private:
      Server(uint16_t);
      ~Server();
      static void* BinLogLoop(void*);
  };// class Server
};


#endif //NM_MEMCACHED_NMMC_SERVER_HPP
