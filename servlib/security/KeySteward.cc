/*
 *    Copyright 2006 SPARTA Inc
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */



/********************************************************************************
 *                                                                              *
 *                           ***   WARNING  ***                                 *
 *                                                                              *
 *    This is an interface example ONLY. It does NOT do any key stewardship     *
 *    but merely packages the key so it can be recovered later.                 *
 *                                                                              *
 *    Implementations must provide their own security management for keys.      *
 *                                                                              *
 *                                                                              *
 ********************************************************************************/


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include <limits.h>
#include "Ciphersuite.h"
#include "bundling/Bundle.h"
#include "contacts/Link.h"
#include "security/KeySteward.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";


//----------------------------------------------------------------------
int
KeySteward::encrypt(const Bundle*     b,
                    KeyParameterInfo* kpi,
                    const LinkRef&    link,
                    std::string       security_dest,
                    u_char*           data,
                    size_t            data_len,
                    DataBuffer&       db)
{
    (void) b;
    (void) kpi;
    (void) link;
    (void) security_dest;
    
    u_char*   buf;
    u_int16_t len;
    size_t    size;
    
    if ( data_len > USHRT_MAX )
        return -1;
    
    len = data_len;
    len = htons(len);
    size = std::max( static_cast<unsigned long>(data_len + sizeof(len)), 512UL );
    db.reserve(size);
    db.set_len(size);
    buf = db.buf();
    memcpy(buf, &len, sizeof(len));
    buf += sizeof(len);
    memcpy(buf, data, data_len);
    
    return 0;    
}

//----------------------------------------------------------------------
int
KeySteward::decrypt(const Bundle* b,
                    std::string   security_src,
                    u_char*       enc_data,
                    size_t        enc_data_len,
                    DataBuffer&   db)
{
    (void) b;
    (void) security_src;
    
    u_int16_t    len;
    u_char*        buf = enc_data;
    
    if (enc_data_len < 2)
        return -1;
    
    memcpy(&len, buf, sizeof(len));
    buf += sizeof(len);
    
    len = ntohs(len);
    if ( enc_data_len < len + sizeof(len) )
        return -1;
    
    db.reserve(len);
    db.set_len(len);
    memcpy(db.buf(), buf, len);
    
    return 0;    
}

//----------------------------------------------------------------------
int
KeySteward::sign(const Bundle*     b,
                 KeyParameterInfo* kpi,
                 const LinkRef&    link,
                 DataBuffer&       db_digest,
                 DataBuffer&       db_signed)
{
    (void) b;
    (void) kpi;
    (void) link;
    
    u_char*   buf;
    u_int16_t len;
    size_t    size;
    size_t    data_len = db_digest.len();
    
    if ( data_len > USHRT_MAX )
        return -1;
    
    len = data_len;
    len = htons(len);
    size = std::max( static_cast<unsigned long>(data_len + sizeof(len)), 512UL );
    db_signed.reserve(size);
    db_signed.set_len(size);
    buf = db_signed.buf();
    memcpy(buf, &len, sizeof(len));
    buf += sizeof(len);
    memcpy(buf, db_digest.buf(), data_len);
    
    return 0;
}

//----------------------------------------------------------------------
int
KeySteward::signature_length(const Bundle*     b,
                             KeyParameterInfo* kpi,
                             const LinkRef&    link,
                             size_t            data_len,
                             size_t&           out_len)
{
    (void) b;
    (void) kpi;
    (void) link;
    (void) data_len;
    
    out_len = 512;
    
    return 0;
}

//----------------------------------------------------------------------
int
KeySteward::verify(const Bundle* b,
                   u_char*       enc_data,
                   size_t        enc_data_len,
                   u_char*       data,
                   size_t        data_len)
{
    (void) b;
    (void) enc_data;
    (void) enc_data_len;
    
    u_int16_t len;
    u_char*   buf = enc_data;
    
    log_debug_p(log, "KeySteward::verify() enc_data_len %lu\n", enc_data_len);
    if (enc_data_len < 2)
        return -1;
    
    memcpy(&len, buf, sizeof(len));
    buf += sizeof(len);
    
    len = ntohs(len);
    log_debug_p(log, "KeySteward::verify() len %u\n", len);
    if ( enc_data_len < len + sizeof(len) )
        return -1;
    
    if ( len != data_len )
        return -1;
    
    log_debug_p(log, "KeySteward::verify() original digest    0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
           buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],buf[16],buf[17],buf[18],buf[19]);
    if ( memcmp( buf, data, len ) != 0 )
        return -1;

    return 0;
}

} // namespace dtn

#endif /* BSP_ENABLED */
