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

#ifndef _CIPHERSUITE_H_
#define _CIPHERSUITE_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "SecurityConfig.h"

namespace dtn {

class BP_Local_CS;

/**
 * Block processor superclass for ciphersutes
 * This level handles suite registration and
 * similar activities but no specific work
 */
    class Ciphersuite {
public:
    /// For local binary (non-string) stuff we use
    /// a scratch buffer but with minimal initial allocation
    typedef oasys::ScratchBuffer<u_char*, 16> LocalBuffer;

    /// @{ Import some typedefs from other classes
    typedef BlockInfo::list_owner_t list_owner_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;
    /// @}
    
    /**
     * Values for security flags that appear in the 
     * ciphersuite field.
     */
    typedef enum {
        CS_BLOCK_HAS_SOURCE      = 0x10,
        CS_BLOCK_HAS_DEST        = 0x08,
        CS_BLOCK_HAS_PARAMS      = 0x04,
        CS_BLOCK_HAS_CORRELATOR  = 0x02,
        CS_BLOCK_HAS_RESULT      = 0x01
    } ciphersuite_flags_t;
    
    /**
     * Values for flags that appear in the 
     * proc_flags_ field.
     */
    typedef enum {
        CS_BLOCK_RESERVED0                      = 0x01,
        CS_BLOCK_PROCESSED                      = 0x02,
        CS_BLOCK_DID_NOT_FAIL                   = 0x04,
        CS_BLOCK_FAILED_VALIDATION              = 0x08,
        CS_BLOCK_PASSED_VALIDATION              = 0x10,
        CS_BLOCK_COMPLETED_DO_NOT_FORWARD       = 0x20,
        CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND  = 0x40
    } proc_flags_t;
    
    /**
     * Values for security types that appear in the 
     * ciphersuite parameters and results fields.
     */
    typedef enum {
        CS_reserved0                        =  0,
        CS_IV_field                         =  1,
        CS_reserved2_field                  =  2,
        CS_key_info_field                   =  3,
        CS_fragment_offset_and_length_field =  4,
        CS_signature_field                  =  5,
        CS_reserved6                        =  6,
        CS_PC_block_salt                    =  7,
        CS_PC_block_ICV_field               =  8,
        CS_reserved9                        =  9,
        CS_encap_block_field                = 10,
        CS_reserved11                       = 11
    } ciphersuite_fields_t;
    
    /// Constructor
    Ciphersuite();
    Ciphersuite(SecurityConfig *config);

    /**
     * Virtual destructor.
     */
    virtual ~Ciphersuite();

    static void register_ciphersuite(Ciphersuite* cs);

    static Ciphersuite* find_suite(u_int16_t num);

    static void init_default_ciphersuites(void);

    static void shutdown(void);
    
    virtual u_int16_t cs_num();

    virtual size_t    result_len()  { return 0; }
    
    static void parse(BlockInfo* block);
    
    /**
     * First callback for parsing blocks that is expected to append a
     * chunk of the given data to the given block. When the block is
     * completely received, this should also parse the block into any
     * fields in the bundle class.
     *
     * The base class implementation parses the block preamble fields
     * to find the length of the block and copies the preamble and the
     * data in the block's contents buffer.
     *
     * This and all derived implementations must be able to handle a
     * block that is received in chunks, including cases where the
     * preamble is split into multiple chunks.
     *
     * @return the amount of data consumed or -1 on error
     */
    virtual int consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len) = 0;

    virtual int reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block);

    /**
     * Validate the block. This is called after all blocks in the
     * bundle have been fully received.
     *
     * @return true if the block passes validation
     */
    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason) = 0;

    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list) = 0;
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.  It MUST add any EID references it needs by
     * calling block->add_eid(), then call generate_preamble(), which
     * will add the EIDs to the primary block's dictionary and write
     * their offsets to this block's preamble.
     */
    virtual int generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last) = 0;
    
    /**
     * Third callback for transmitting a bundle. This pass should
     * generate any data (such as security signatures) for the block
     * that may depend on other blocks' contents.
     *
     * The base class implementation does nothing. 
     * 
     * We pass xmit_blocks explicitly to indicate that ALL blocks
     * might be changed by finalize, typically by being encrypted.
     * Parameters such as length might also change due to padding
     * and encapsulation.
     */
    virtual int finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link) = 0;

    /**
     * Check the block list for validation flags for the
     * specified ciphersuite
     */
    static bool check_validation(const Bundle*        bundle, 
                                 const BlockInfoVec*  block_list, 
                                 u_int16_t            num);
                                 
    /**
     * Ccreate a correlator for this block-list. Include part
     * of the fragment-offset is this bundle is a fragment.
     */
    static u_int64_t create_correlator(const Bundle*        bundle, 
                                       const BlockInfoVec*  block_list);

    /**
     * Convenience methods to test if the security source/destination
     * is an endpoint registered at the local node.
     */
    static bool source_is_local_node(const Bundle*    bundle,
                                     const BlockInfo* block);

    static bool destination_is_local_node(const Bundle*    bundle,
                                          const BlockInfo* block);
    
    
    virtual void init_locals(BlockInfo* block);

    static SecurityConfig *config;
    
protected:
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.
     */
    void generate_preamble(BlockInfoVec*  xmit_blocks, 
                           BlockInfo* block,
                           u_int8_t type,
                           u_int64_t flags,
                           u_int64_t data_length);


private:

    /**
     * Array of registered BlockProcessor-derived handlers for
     * various ciphersuites -- fixed size for now [maybe make adjustable later]
     */
    static Ciphersuite* ciphersuites_[1024];
    
    static bool inited;
};

class BP_Local_CS : public BP_Local {
public:
    /// Use the same LocalBuffer type as Ciphersuite
    typedef Ciphersuite::LocalBuffer LocalBuffer;
    
    /**
     * Constructor.
     */
    BP_Local_CS();
    
    /**
     * Copy constructor.
     */
    BP_Local_CS(const BP_Local_CS&);
    
    /**
     * Virtual destructor.
     */
    virtual ~BP_Local_CS();
    
    /// @{ Accessors
    // need to think about which ones map to the locals and which
    // are derived
      
    u_int16_t           cs_flags()               const { return cs_flags_; }
    u_int16_t           owner_cs_num()           const { return owner_cs_num_; }
    u_int32_t           security_result_offset() const { return security_result_offset_; }
    u_int64_t           correlator()             const { return correlator_; }
    u_int16_t           correlator_sequence()    const { return correlator_sequence_; }
    const LocalBuffer&  key()                    const { return key_; }
    const LocalBuffer&  salt()                   const { return salt_; }
    const LocalBuffer&  iv()                     const { return iv_; }
    const LocalBuffer&  security_params()        const { return security_params_; }
    std::string         security_src()           const { return security_src_; }
    std::string         security_dest()          const { return security_dest_; }
    const LocalBuffer&  security_result()        const { return security_result_; }
    BlockInfo::list_owner_t list_owner()         const { return list_owner_; }
    u_int16_t           proc_flags()             const { return proc_flags_; }
    bool                proc_flag(u_int16_t f)   const { return (proc_flags_ & f) != 0; }
    /// @}


    /// @{ Mutating accessors
    void         set_cs_flags(u_int16_t f)            { cs_flags_ = f; }
    void         set_owner_cs_num(u_int16_t n)        { owner_cs_num_ = n; }
    void         set_security_result_offset(u_int64_t o){ security_result_offset_ = o; }
    void         set_key(u_char* k, size_t len);
    void         set_salt(u_char* s, size_t len);
    void         set_iv(u_char* iv, size_t len);
    void         set_correlator(u_int64_t c)          { correlator_ = c; }
    void         set_correlator_sequence(u_int16_t c) { correlator_sequence_ = c; }
    LocalBuffer* writable_security_params()           { return &security_params_; }    
    void         set_security_src(std::string s)      { security_src_ = s; }
    void         set_security_dest(std::string d)     { security_dest_ = d; }
    LocalBuffer* writable_security_result()           { return &security_result_; }  
    void         set_list_owner(BlockInfo::list_owner_t o) { list_owner_ = o; }
    void         set_proc_flags(u_int16_t f)          { proc_flags_ = f; }
    void         set_proc_flag(u_int16_t f)           { proc_flags_ |= f; }
    /// @}

    
protected:
    
    u_int16_t               cs_flags_;
    u_int16_t               correlator_sequence_;
    u_int32_t               security_result_offset_;
    u_int64_t               correlator_;
    LocalBuffer             key_;    
    LocalBuffer             iv_;    
    LocalBuffer             salt_;    
    LocalBuffer             security_params_;    
    std::string             security_src_;
    std::string             security_dest_;
    LocalBuffer             security_result_;    
    BlockInfo::list_owner_t list_owner_;
    u_int16_t               owner_cs_num_; 
    u_int16_t               proc_flags_;  ///< Flags tracking processing status etc

};  /* BP_Local_CS */

} // namespace dtn

#define CS_FAIL_IF(x)                                              \
    do { if ( (x) ) {                                              \
        log_err_p(log, "TEST FAILED (%s) at %s:%d\n",              \
                (#x), __FILE__, __LINE__);                         \
        goto fail;                                                 \
    } } while(0);

#define CS_FAIL_IF_NULL(x)                                         \
    do { if ( (x) == NULL) {                                       \
        log_err_p(log, "TEST FAILED (%s == NULL) at %s:%d\n",      \
                (#x), __FILE__, __LINE__);                         \
        goto fail;                                                 \
    } } while(0);

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_H_ */
