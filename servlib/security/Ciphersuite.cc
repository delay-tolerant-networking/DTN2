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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite.h"
#include "BA_BlockProcessor.h"
#include "Ciphersuite_BA1.h"
#include "PI_BlockProcessor.h"
#include "Ciphersuite_PI2.h"
#include "PC_BlockProcessor.h"
#include "Ciphersuite_PC3.h"
#include "ES_BlockProcessor.h"
#include "Ciphersuite_ES4.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"
#include "bundling/SDNV.h"

namespace dtn {


#define CS_MAX 1024
Ciphersuite* Ciphersuite::ciphersuites_[CS_MAX];
SecurityConfig* Ciphersuite::config=NULL;

static const char* log = "/dtn/bundle/ciphersuite";

bool Ciphersuite::inited = false;

//----------------------------------------------------------------------
Ciphersuite::Ciphersuite()
//    : BlockProcessor(block_type)
{
}
Ciphersuite::Ciphersuite(SecurityConfig *c) {
    config = c;
}

//----------------------------------------------------------------------
Ciphersuite::~Ciphersuite()
{ 
}
void Ciphersuite::shutdown() {
    delete config;
}

//----------------------------------------------------------------------
void
Ciphersuite::register_ciphersuite(Ciphersuite* cs)
{
    log_debug_p(log, "Ciphersuite::register_ciphersuite()");
    u_int16_t    num = cs->cs_num();
    
    if ( num <= 0 || num >= CS_MAX )
        return;            //out of range
        
    // don't override an existing suite
    ASSERT(ciphersuites_[num] == 0);
    ciphersuites_[num] = cs;
}

//----------------------------------------------------------------------
Ciphersuite*
Ciphersuite::find_suite(u_int16_t num)
{
    Ciphersuite* ret = NULL;
    log_debug_p(log, "Ciphersuite::find_suite()");
    
    if ( num > 0 && num < CS_MAX )  // entry for element zero is illegal
        ret = ciphersuites_[num];

    return ret;
}

//----------------------------------------------------------------------
void
Ciphersuite::init_default_ciphersuites()
{
    log_debug_p(log, "Ciphersuite::init_default_ciphersuites()");
    if ( ! inited ) {
        config = new SecurityConfig();
        // register default block processor handlers
        BundleProtocol::register_processor(new BA_BlockProcessor());
        BundleProtocol::register_processor(new PI_BlockProcessor());
        BundleProtocol::register_processor(new PC_BlockProcessor());
        BundleProtocol::register_processor(new ES_BlockProcessor());
        
        // register mandatory ciphersuites
        register_ciphersuite(new Ciphersuite_BA1());
        register_ciphersuite(new Ciphersuite_PI2());
        register_ciphersuite(new Ciphersuite_PC3());
        register_ciphersuite(new Ciphersuite_ES4());
        
        inited = true;
    }
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite::cs_num(void)
{

    return 0;
}

//----------------------------------------------------------------------
void
Ciphersuite::parse(BlockInfo* block)
{
    Ciphersuite*    cs_owner = NULL;
    BP_Local_CS*    locals = NULL;
    u_char*         buf;
    size_t          len;
    u_int64_t       cs_flags;
    u_int64_t       suite_num;
    int             sdnv_len;
    u_int64_t       security_correlator    = 0LL;
    u_int64_t       field_length           = 0LL;
    bool            has_source;    
    bool            has_dest;      
    bool            has_params;    
    bool            has_correlator;
    bool            has_result;   
    EndpointIDVector::const_iterator iter;
    
// XXX/pl todo  think about a "const" version of parse() since there
//              are several callers who need parsing but have a const block
    
    log_debug_p(log, "Ciphersuite::parse() block %p", block);
    ASSERT(block != NULL);
    
    // preamble has already been parsed and stored, so we skip over it here
    buf = block->contents().buf() + block->data_offset();
    len = block->data_length();
    
    // get ciphersuite and flags
    sdnv_len = SDNV::decode(buf,
                            len,
                            &suite_num);
    buf += sdnv_len;
    len -= sdnv_len;

    sdnv_len = SDNV::decode(buf,
                            len,
                            &cs_flags);
    buf += sdnv_len;
    len -= sdnv_len;

    has_source     = (cs_flags & CS_BLOCK_HAS_SOURCE)     != 0;
    has_dest       = (cs_flags & CS_BLOCK_HAS_DEST)       != 0;
    has_params     = (cs_flags & CS_BLOCK_HAS_PARAMS)     != 0;
    has_correlator = (cs_flags & CS_BLOCK_HAS_CORRELATOR) != 0;
    has_result     = (cs_flags & CS_BLOCK_HAS_RESULT)     != 0;
    log_debug_p(log, "Ciphersuite::parse() suite_num %llu cs_flags 0x%llx",
                U64FMT(suite_num), U64FMT(cs_flags));
    
    cs_owner = dynamic_cast<Ciphersuite*>(find_suite(suite_num));
    
    if ( ciphersuites_[suite_num] != NULL )            // get specific subclass if it's present
        cs_owner = ciphersuites_[suite_num];
    
    if ( block->locals() == NULL ) {
        if ( cs_owner != NULL )
            cs_owner->init_locals(block);                // get owning class to allocate locals
        else
            block->set_locals( new BP_Local_CS );
    }
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    ASSERT ( locals != NULL );

    ASSERT ( suite_num < 65535 );
    locals->set_owner_cs_num(suite_num);

    // set cs_flags
    ASSERT ( cs_flags  < 65535  );
    locals->set_cs_flags(cs_flags);
    
    // get correlator, if present
    if ( has_correlator ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &security_correlator);
        buf += sdnv_len;
        len -= sdnv_len;
    }
    log_debug_p(log, "Ciphersuite::parse() correlator %llu",
                U64FMT(security_correlator));
    locals->set_correlator(security_correlator);
    

    // get cs params length, and data
    if ( has_params ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &field_length);
        buf += sdnv_len;
        len -= sdnv_len;
        // make sure the buffer has enough space, copy data in
        locals->writable_security_params()->reserve(field_length);
        memcpy( locals->writable_security_params()->end(), buf, field_length);
        locals->writable_security_params()->set_len(field_length);
        buf += field_length;
        len -= field_length;
        log_debug_p(log, "Ciphersuite::parse() security_params len %llu",
                    U64FMT(field_length));
    }
    

    // get sec-src length and data
    log_debug_p(log, "Ciphersuite::parse() eid_list().size() %zu has_source %u has_dest %u", 
                block->eid_list().size(), has_source, has_dest);
    
    // XXX/pl - temp fix for blocks loaded from store
    if ( block->eid_list().size() > 0 ) {
        iter = block->eid_list().begin();
        if ( has_source ) {    
            locals->set_security_src( iter->str() );
            iter++;
        }
        

        // get sec-dest length and data
        if ( has_dest ) {    
            locals->set_security_dest( iter->str() );
        }
    }
    

    // get sec-result length and data, if present
    if ( has_result ) {    
        sdnv_len = SDNV::decode(buf,
                                len,
                                &field_length);
        buf += sdnv_len;
        len -= sdnv_len;
        // make sure the buffer has enough space, copy data in
        locals->writable_security_result()->reserve(field_length);
        memcpy( locals->writable_security_result()->end(), buf, field_length);
        locals->writable_security_result()->set_len(field_length);
        buf += field_length;
        len -= field_length;
        
    }
    



}


//----------------------------------------------------------------------
int
Ciphersuite::reload_post_process(Bundle*       bundle,
                                 BlockInfoVec* block_list,
                                 BlockInfo*    block)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    
    // Received blocks might be stored and reloaded and
    // some fields aren't reset.
    // This allows BlockProcessors to reestablish what they
    // need. The default implementation does nothing.
    // In general that's appropriate, as the BlockProcessor
    // will have called parse() and that's the main need. 
    // Individual ciphersuites can override this if their 
    // usage requires it.
    
    block->set_reloaded(false);
    return 0;

}

//----------------------------------------------------------------------
void
Ciphersuite::generate_preamble(BlockInfoVec* xmit_blocks,
                               BlockInfo*    block,
                               u_int8_t      type,
                               u_int64_t     flags,
                               u_int64_t     data_length)
{
    block->owner()->generate_preamble(xmit_blocks, block, type,
                                      flags, data_length);
}

//----------------------------------------------------------------------
bool
Ciphersuite::destination_is_local_node(const Bundle*    bundle,
                                       const BlockInfo* block)
{
    u_int16_t       cs_flags = 0;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals;
    bool            result = false;     //default is "no" even in case of errors

    log_debug_p(log, "Ciphersuite::destination_is_local_node()");
    if ( block == NULL )
        return false;
        
    if ( block->locals() == NULL )       // then the block is broken
        return false;
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    if ( locals == NULL )
        return false;

    cs_flags = locals->cs_flags();
    
    // this is a very clunky way to get the "base" portion of the bundle destination
    std::string bundle_dest_str = bundle->dest().uri().scheme() + "://" +
                                  bundle->dest().uri().host();
    EndpointID        dest_node(bundle_dest_str);
    
    
    // if this is security-dest, or there isn't one and this is bundle dest
    if (  (  (cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) && (local_eid == locals->security_dest())) ||
          ( !(cs_flags & Ciphersuite::CS_BLOCK_HAS_DEST) && (local_eid == dest_node)              )    ) 
    {  //yes - this is ours 
        result = true;
    }
    
    return result;
}

//----------------------------------------------------------------------
bool
Ciphersuite::source_is_local_node(const Bundle* bundle, const BlockInfo* block)
{
    u_int16_t       cs_flags = 0;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals;
    bool            result = false;     //default is "no" even in case of errors

    log_debug_p(log, "Ciphersuite::source_is_local_node()");
    if ( block == NULL )
        return false;
        
    if ( block->locals() == NULL )       // then the block is broken
        return false;
    
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    if ( locals == NULL )
        return false;

    cs_flags = locals->cs_flags();
    
    // this is a very clunky way to get the "base" portion of the bundle destination
    std::string bundle_src_str = bundle->source().uri().scheme() + "://" +
                                 bundle->source().uri().host();
    EndpointID        src_node(bundle_src_str);
    
    
    // if this is security-src, or there isn't one and this is bundle source
    if (  (  (cs_flags & Ciphersuite::CS_BLOCK_HAS_SOURCE) && (local_eid == locals->security_src())) ||
          ( !(cs_flags & Ciphersuite::CS_BLOCK_HAS_SOURCE) && (local_eid == src_node)              )    ) 
    {  //yes - this is ours 
        result = true;
    }
    
    return result;
}

//----------------------------------------------------------------------
bool
Ciphersuite::check_validation(const Bundle*       bundle,
                              const BlockInfoVec* block_list,
                              u_int16_t           num)
{
    (void)bundle;
    u_int16_t       proc_flags = 0;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;

    log_debug_p(log, "Ciphersuite::check_validation(%hu)", num);
    if ( block_list == NULL )
        return false;
        
    for ( iter = block_list->begin();
          iter != block_list->end();
          ++iter)
    {
        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
        if (locals->owner_cs_num() != num )
            continue;
        
        // OK - this is one of interest
        proc_flags |= locals->proc_flags();
    }
    
    // Now check what we have collected
    // If we positively validated, we succeeded
    if ( proc_flags & CS_BLOCK_PASSED_VALIDATION ) {
        log_debug_p(log, "Ciphersuite::check_validation(%hu) returning true because CS_BLOCK_PASSED_VALIDATION", num);
        return true;
    }
    
    // If we had no positives, then any failure is failure
    if ( proc_flags & CS_BLOCK_FAILED_VALIDATION ) {
        log_debug_p(log, "Ciphersuite::check_validation(%hu) returning false because CS_BLOCK_FAILED_VALIDATION", num);
        return false;
    }
    
    // If no positives but no failure, then did we have 
    // a block which we couldn't test
    if ( proc_flags & CS_BLOCK_DID_NOT_FAIL ) {
        log_debug_p(log, "Ciphersuite::check_validation(%hu) returning true because CS_BLOCK_DID_NOT_FAIL", num);
        return true;
    }
    log_debug_p(log, "Ciphersuite::check_validation(%hu) returning false because we didn't find a block at all", num);
    
    return false;   // no blocks of wanted type
}

//----------------------------------------------------------------------
u_int64_t
Ciphersuite::create_correlator(const Bundle*       bundle,
                               const BlockInfoVec* block_list)
{
    (void)bundle;
    u_int64_t       result = 0LLU;
    u_int16_t       high_val = 1;
    u_int16_t       value;
    BP_Local_CS*    locals;
    BlockInfoVec::const_iterator iter;

    log_debug_p(log, "Ciphersuite::create_correlator()");
    if ( bundle == NULL )
        return 1LLU;        // and good luck :)
        
    if ( block_list == NULL )
        return 1LLU;
        
    if ( bundle->is_fragment() ) {
        result = bundle->frag_offset() << 24;
    }
    
    for ( iter = block_list->begin();
          iter != block_list->end();
          ++iter)
    {
        if ( iter->locals() == NULL )       // then of no interest
            continue;
        
        locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( locals == NULL )
            continue;
    
        value = locals->correlator();       // only low-order 16 bits
        
        value = value > high_val ? value : high_val;
    }
    
    result |= high_val;     // put high_val into low-order two bytes
    
    return result;
}

//----------------------------------------------------------------------
void
Ciphersuite::init_locals(BlockInfo* block)
{
    /* Create new locals block but do not overwrite
     * if one already exists.
     * Derived classes may wish to change this behavior
     * and map old-to-new, or whatever
     */
     
    if ( block->locals() == NULL )
        block->set_locals( new BP_Local_CS );
    
}

//----------------------------------------------------------------------
BP_Local_CS::BP_Local_CS()
    : BP_Local(),
      cs_flags_(0), 
      security_result_offset_(0), 
      correlator_(0LL),
      security_params_(),
      security_result_(),
      owner_cs_num_(0)
{
}

//----------------------------------------------------------------------
BP_Local_CS::BP_Local_CS(const BP_Local_CS& b)
    : BP_Local(),
      cs_flags_(b.cs_flags_), 
      security_result_offset_(b.security_result_offset_), 
      correlator_(b.correlator_),
      security_params_(b.security_params_),
      security_result_(b.security_result_),
      owner_cs_num_(b.owner_cs_num_)
    //XXX-pl  might need more items copied
{
}

//----------------------------------------------------------------------
BP_Local_CS::~BP_Local_CS()
{
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_key(u_char* k, size_t len)
{
    key_.reserve(len);
    key_.set_len(len);
    memcpy(key_.buf(), k, len);
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_salt(u_char* s, size_t len)
{
    salt_.reserve(len);
    salt_.set_len(len);
    memcpy(salt_.buf(), s, len);
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_iv(u_char* iv, size_t len)
{
    iv_.reserve(len);
    iv_.set_len(len);
    memcpy(iv_.buf(), iv, len);
}


} // namespace dtn

#endif /* BSP_ENABLED */
