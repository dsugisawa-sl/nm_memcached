//
// Created by dsugisawa on 2017/07/17.
//
#include "binlog.h"
#include "mysql.h"

#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_def.h"
#include "inc/nmmc_thread.hpp"
#include "inc/nmmc_packet.hpp"
#include "inc/nmmc_server.hpp"
#include "inc/nmmc_tpl.hpp"


using namespace NMMC;
using binary_log::Binary_log;
using binary_log::system::create_transport;
using binary_log::system::Binary_log_driver;

/*
 * ===========================
CREATE TABLE `kvs` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `key` bigint(20) NOT NULL DEFAULT 0,
  `val` BLOB,
  PRIMARY KEY (`id`),
  UNIQUE KEY `key` (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
 * ===========================
 *
 * */

#define TBL_KVS  ("kvs")
typedef enum KVS_COL_{
    INTIDX = 0,
    KEY, VALUE,
    MAX
}KVS_COL;

#define ITEM_LEN    (1024)
typedef struct nmmc_item{
    struct _stat{
        uint32_t valid:1;
        uint32_t datalen:11;
        uint32_t padd:20;
    }stat;
    uint32_t    expire;
    uint8_t     data[ITEM_LEN];
}nmmc_item_t,*nmmc_item_ptr;

typedef union {
    struct {
        uint32_t id:20;
        uint32_t prifix:4;
        uint32_t reserved:8;
    } k;
    uint32_t key;
}nmmc_key;

typedef protocol_binary_request_header PBRH;
#define VAL         LookupTable<nmmc_item_t,(1<<20),uint32_t>
#define PVAL(a)     ((VAL*)a)
#define REQH(a)     (((PBRH*)a)->request)
#define EXTRA(a,t)  (t*)(((PBRH*)a)+1)
#define KEY(a,t)    (t*)(((char*)(((PBRH*)a)+1)) + REQH(a).extlen)
#define REQ_VAL(a)  (((char*)(((PBRH*)a)+1)) + REQH(a).extlen + ntohs(REQH(a).keylen))
#define REQ_VALL(a) (int)(ntohl(REQH(a).bodylen) - REQH(a).extlen - ntohs(REQH(a).keylen))




// サーバロジック
// memcached-binary protocol 本体
int Server::OnRecieve(Packet* pkt){
    auto p = (protocol_binary_request_header*)PAYLOAD(pkt);
    // ヘッダバリデート
    if (!validate_header(p, pkt->Length())){ return(0); }
    // 20bit key + プリフィクス4bit(=サーバーidに該当)に応答する
    if (ntohs(p->request.keylen) != sizeof(uint32_t)){ return(0); }
    // 20bitキー
    nmmc_key key;
    key.key = ntohl(*KEY(p, uint32_t));

//  LOG("Server::OnRecieve/key %08x // %u", key.key, key.k.id);

    // GET/SET対応
    if (p->request.opcode == OPCODE_GET){
        return(OnRecieveGet(pkt, key.k.id));
    }else if (p->request.opcode == OPCODE_SET){
        return(OnRecieveSet(pkt, key.k.id, REQ_VAL(p), REQ_VALL(p)));
    }
    return(0);
}
//
int Server::OnRecieveGet(Packet* pkt,uint32_t key){
    const char*     notfound = "not found";
    const size_t    notfoundlen = strlen(notfound);
    auto p = (protocol_binary_request_header*)PAYLOAD(pkt);
    auto tbl = PVAL(lookup_value_);
//  LOG("Server::OnRecieveGet/key %u", key);

    // lookup
    auto it = tbl->Find(key, 0);
    if (it == tbl->End()){ goto notfound; }
    // データ長が0 はNot Foundと同意
    if (it->stat.datalen == 0){ goto notfound; }

    REQH(p).magic   = MAGIC_RESPONSE;
    REQH(p).opcode  = OPCODE_GET;
    REQH(p).keylen  = 0;
    REQH(p).extlen  = 4;
    REQH(p).datatype= 0;
    REQH(p).status  = 0;
//  REQH(p).opaque  = 0;
    REQH(p).cas     = 0;
    REQH(p).bodylen = htonl(it->stat.datalen + 4);

    *(EXTRA(p, uint32_t)) = it->expire;
    memcpy(REQ_VAL(p), it->data, it->stat.datalen);

    pkt->Length(sizeof(*p) + it->stat.datalen + 4);

    return(NEED_SEND);
notfound:
    LOG("Not Found[GET] %u", key);
    REQH(p).magic   = MAGIC_RESPONSE;
    REQH(p).opcode  = OPCODE_GET;
    REQH(p).keylen  = 0;
    REQH(p).extlen  = 0;
    REQH(p).datatype= 0;
    REQH(p).status  = STATUS_ERR;
    REQH(p).opaque  = 0;
    REQH(p).cas     = 0;
    REQH(p).bodylen = htonl(notfoundlen);
    //
    memcpy(REQ_VAL(p), notfound, notfoundlen);

    pkt->Length(sizeof(*p) + notfoundlen);

    return(NEED_SEND);
}
int Server::OnRecieveSet(Packet* pkt,uint32_t key, char* val, int vlen){
    auto p = (protocol_binary_request_header*)PAYLOAD(pkt);
//  LOG("Server::OnRecieveSet/key %u", key);
    uint16_t    stat;
    char        deckey[32] = {0};
    // ETHER_MAX_LEN=最大バイナリサイズ=文字列4096Byte
    snprintf(deckey, sizeof(deckey)-1,"%u", key);
    //
    std::string sql = "INSERT INTO kvs(`key`,`val`)VALUES(";
    std::string hexval = Bin2Hex((unsigned char*)val,vlen);
    sql += deckey;
    sql += ",0x";
    sql += hexval;
    sql += ")ON DUPLICATE KEY UPDATE val = 0x";
    sql += hexval;
    // FIXME:   Database Accessコンテキストを切り離す？
    //          例えば、GAEのDataStoreのように、ここではUUIDを発行して返却
    //          処理結果を、UUIDで問い合わせるようなイメージ
    //          -----
    //          Set（更新）系が少ないことを前提条件とし
    //          本コンテキストで、データベースアクセスを実施
    //
    if (mysql_query((MYSQL*)dbhandle_, sql.c_str()) != 0){
        LOG("Set Error %u", key);
        stat = STATUS_ERR;
    }else{
        stat = 0;
    }
    REQH(p).magic   = MAGIC_RESPONSE;
    REQH(p).opcode  = OPCODE_SET;
    REQH(p).keylen  = 0;
    REQH(p).extlen  = 0;
    REQH(p).datatype= 0;
    REQH(p).status  = stat;
//  REQH(p).opaque  = 0;
    REQH(p).cas     = 0;
    REQH(p).bodylen = 0;

    pkt->Length(sizeof(*p));

    return(NEED_SEND);
}

Server* NMMC::Server::Create(uint16_t server_id){
    return(new Server(server_id));
}
void Server::Release(Server** server){
    (*server)->cancel_ = 1;
    delete (*server);
    (*server) = NULL;
}

Server::Server(uint16_t server_id):server_id_(server_id){
    lookup_value_ = VAL::Create();
    if (!lookup_value_){
        throw std::runtime_error("faile.dallocate table.");
    }
    // DEBUG設定
    dbuser_ = "lteepc";
    dbpswd_ = "password";
    dbhost_ = "10.4.63.11";
    dbport_ = "3306";
    dbinst_ = "lteepc";
    dbserverid_ = 3000 + server_id;

    if (pthread_create(&thread_, NULL, Server::BinLogLoop, this) == -1) {
        throw std::runtime_error("Unable to create thread");
    }
    // binlog-eventの接続が完了するのを待機
    sleep(3);
    // マスタデータベースアクセス開始
    mysql_thread_init();
    my_bool reconnect = 1;
    int port = atoi(dbport_.c_str());
    dbhandle_   = mysql_init(NULL);
    if (dbhandle_ == NULL){
        throw std::runtime_error("failed. create mysql connection.");
    }
    mysql_options((MYSQL*)dbhandle_, MYSQL_OPT_RECONNECT, &reconnect);
    if (mysql_real_connect((MYSQL*)dbhandle_,
                           dbhost_.c_str(),
                           dbuser_.c_str(),
                           dbpswd_.c_str(),
                           dbinst_.c_str(),
                           port, NULL, 0)) {
        mysql_set_character_set((MYSQL*)dbhandle_, "utf8");
    }else{
        throw std::runtime_error("failed. mysql client connect..");
    }
}
Server::~Server(){
    delete PVAL(lookup_value_);
    pthread_join(thread_, NULL);
    // マスタデータベースアクセス終了
    if (dbhandle_ != NULL) {
        mysql_close((MYSQL*)dbhandle_);
    }
    dbhandle_ = NULL;
    mysql_thread_end();
}
void* Server::BinLogLoop(void* arg){
    Server* inst = (Server*)arg;
    int err;
    auto tbl = PVAL(inst->lookup_value_);
    //
    std::string uri = "mysql://" + inst->dbuser_ + ":" + inst->dbpswd_ + "@" + inst->dbhost_ + ":" + inst->dbport_;

    // インスタンス準備
    std::auto_ptr<Binary_log_driver> drv(create_transport(uri.c_str()));
    drv.get()->set_serverid(inst->dbserverid_);
    Binary_log          binlog(drv.get());
    Decoder             decode;
    Converter           converter;
    Binary_log_event    *tmpevent = NULL;

    // サーバへ接続
    if ((err = binlog.connect()) != ERR_OK){
        LOG("Server::BinLogLoop/Unable to setup connection.(%s):(%s-%d)", str_error(err), uri.c_str(), inst->dbserverid_);
        throw std::runtime_error("failed. connect.(BinLogLoop)");
    }
    if (binlog.set_position(4) != ERR_OK){
        LOG("Server::BinLogLoop/binlog.set_position.(%s)", str_error(err));
        throw std::runtime_error("failed. set_position.(BinLogLoop)");
    }

    LOG("binlog loop ...(%u)", (unsigned)inst->dbserverid_);

    // mysql binlog api
    while(!inst->cancel_){
        std::pair<unsigned char *, size_t> buffer_buflen;
        Binary_log_event *event = NULL;
        const char *msg= NULL;
        static uint64_t counter = 0;
        // 次のイベントを待機
        if ((err = drv->get_next_event(&buffer_buflen)) != ERR_OK){
            LOG("get_next_event.(%s)", str_error(err));
            throw std::runtime_error("get_next_event.[XradiusServer::BinLogLoop]");
        }
        // デコード
        if (!(event = decode.decode_event((char*)buffer_buflen.first, buffer_buflen.second, &msg, 1))){
            LOG("decode_event.(%s)", msg);
            throw std::runtime_error("decode_event.[XradiusServer::BinLogLoop]");
        }
        // イベントタイプ毎の処理
        switch(event->get_event_type()){
            case TABLE_MAP_EVENT:
                if (tmpevent != NULL){ delete tmpevent; }
                tmpevent = NULL;
                // 対象テーブル
                if (strncasecmp(TBL_KVS, ((Table_map_event*)event)->m_tblnam.c_str(), strlen(TBL_KVS))== 0){
                    tmpevent = event;
                    event = NULL;
                }
                break;
            case WRITE_ROWS_EVENT:
            case UPDATE_ROWS_EVENT:
            case DELETE_ROWS_EVENT:
                if (tmpevent != NULL){
                    Row_event_set rows((Rows_event*)event, (Table_map_event*)tmpevent);
                    Row_event_set::iterator itr = rows.begin();
                    int update_idx = 0;
                    long nval = 0;
                    do{
                        Row_of_fields   rof((*itr));
                        int             colid = 0;
                        long            itm_key;
                        unsigned long   itm_len;
                        unsigned char  *itm_adr;
                        nmmc_item_t     itm;
                        bzero(&itm, sizeof(itm));

                        // col
                        for(Row_of_fields::iterator itf = rof.begin();itf != rof.end();++itf,colid++){
                            nval = 0;
                            switch(colid){
                                case KVS_COL::KEY:
                                    converter.to(itm_key, (*itf));
                                    break;
                                case KVS_COL::VALUE:
                                    if ((itm_adr = (*itf).as_blob(itm_len)) != NULL){
                                        memcpy(itm.data, itm_adr, MIN(itm_len, ITEM_LEN));
                                        itm.stat.datalen = (uint32_t)itm_len;
                                    }
                                    break;
                            }
                        }
                        if (event->get_event_type() == WRITE_ROWS_EVENT){
                            LOG("write rows.(%u:%02x)", (unsigned)itm_key, itm.data[0]);
                            tbl->Add(itm_key, &itm, 0);
                        }else if (event->get_event_type() == UPDATE_ROWS_EVENT && update_idx++){
                            if (itm.stat.valid == 0){
                                tbl->Del(itm_key, 0);
                            }else{
                                LOG("write rows.(%u:%02x) update", (unsigned)itm_key, itm.data[0]);
                                tbl->Add(itm_key, &itm, 0);
                            }
                        }else if (event->get_event_type() == DELETE_ROWS_EVENT){
                            tbl->Del(itm_key, 0);
                        }
                    }while (++itr != rows.end());
                }
                break;
            default:
                break;
        }
        if (event != NULL){ delete event; }
        event = NULL;
    }
    return((void*)NULL);
}

