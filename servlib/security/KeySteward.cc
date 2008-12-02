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
                 u_char*           data,
                 size_t            data_len,
                 DataBuffer&       db)
{
    (void) b;
    (void) kpi;
    (void) link;
    (void) data;
    (void) data_len;
    (void) db;
    
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
    
    if (enc_data_len < 2)
        return -1;
    
    memcpy(&len, buf, sizeof(len));
    buf += sizeof(len);
    
    len = ntohs(len);
    if ( enc_data_len < len + sizeof(len) )
        return -1;
    
    if ( len != data_len )
        return -1;
    
    if ( memcmp( buf, data, len ) != 0 )
        return -1;

    return 0;
}

} // namespace dtn

#endif /* BSP_ENABLED */
