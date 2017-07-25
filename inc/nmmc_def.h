//
// Created by dsugisawa on 2017/07/17.
//

#ifndef NM_MEMCACHED_NMMC_DEF_H
#define NM_MEMCACHED_NMMC_DEF_H
#include "inc/nmmc.hpp"

typedef union {
    struct {
        uint8_t magic;
        uint8_t opcode;
        uint16_t keylen;
        uint8_t extlen;
        uint8_t datatype;
        uint16_t status;
        uint32_t bodylen;
        uint32_t opaque;
        uint64_t cas;
    } request;
    uint8_t bytes[24];
} protocol_binary_request_header;

#define MAGIC_REQUEST   (0x80)
#define MAGIC_RESPONSE  (0x81)

#define OPCODE_GET      (0x00)
#define OPCODE_SET      (0x01)
#define OPCODE_ADD      (0x02)
#define OPCODE_REPLACE  (0x03)
#define OPCODE_DELETE   (0x04)

#define STATUS_OK       (0x0000)
#define STATUS_ERR      (0x0100)


#define NEED_SEND  (0x00000001)


#define USECORE_COUNT       (12)
#define EXTERNAL_IP_OCT3    (0x10)
#define INTERNAL_IP_OCT3    (0x80)

static inline bool validate_header(protocol_binary_request_header* ph, uint32_t ln){
    // SET/GETのみ対応
    if (ph->request.magic != MAGIC_REQUEST){ return(false); }
    if (!(ph->request.opcode == OPCODE_GET || ph->request.opcode == OPCODE_SET)){ return(false); }
    if (ph->request.datatype != 0){ return(false); }
    if (ph->request.opcode == OPCODE_GET){
        // --- GET ---
        // MUST NOT have extras.
        // MUST have key.
        // MUST NOT have value.
        if (ph->request.extlen != 0){ return(false); }
        if (ph->request.keylen == 0){ return(false); }
        if ((unsigned)ntohl(ph->request.bodylen) != (unsigned)ntohs(ph->request.keylen)){ return(false); }
    }else{
        // --- SET ---
        // MUST have extras.
        // MUST have key.
        // MUST have value.
        if (ph->request.extlen == 0){ return(false); }
        if (ph->request.keylen == 0){ return(false); }
        if (ph->request.bodylen == 0){ return(false); }
    }
    auto ttllen = (unsigned)ntohl(ph->request.bodylen) + sizeof(*ph);
    if (ttllen != ln){
        return(false);
    }

    return(true);
}

#endif //NM_MEMCACHED_NMMC_DEF_H
