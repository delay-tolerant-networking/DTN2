/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _PRIMARY_BLOCK_PROCESSOR_H_
#define _PRIMARY_BLOCK_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

class DictionaryVector;
class EndpointID;

/**
 * Block processor implementation for the primary bundle block.
 */
class PrimaryBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    PrimaryBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int consume(Bundle* bundle, BlockInfo* block, u_char* buf, size_t len);
    void generate(const Bundle* bundle, const LinkRef& link,
                  BlockInfo* block, bool last);
    /// @}

protected:
    /**
     * Values for bundle processing flags that appear in the primary
     * block.
     */
    typedef enum {
        BUNDLE_IS_FRAGMENT             = 1 << 0,
        BUNDLE_IS_ADMIN                = 1 << 1,
        BUNDLE_DO_NOT_FRAGMENT         = 1 << 2,
        BUNDLE_CUSTODY_XFER_REQUESTED  = 1 << 3,
        BUNDLE_SINGLETON_DESTINATION   = 1 << 4
    } bundle_processing_flag_t;
    
    /**
     * The first fixed-field portion of the primary bundle block
     * preamble structure.
     */
    struct PrimaryBlock1 {
        u_int8_t version;
        u_int8_t bundle_processing_flags;
        u_int8_t class_of_service_flags;
        u_int8_t status_report_request_flags;
        u_char   block_length[0]; // SDNV
    } __attribute__((packed));

    /**
     * The remainder of the fixed-length part of the primary bundle
     * block that immediately follows the block length.
     */
    struct PrimaryBlock2 {
        u_int16_t dest_scheme_offset;
        u_int16_t dest_ssp_offset;
        u_int16_t source_scheme_offset;
        u_int16_t source_ssp_offset;
        u_int16_t replyto_scheme_offset;
        u_int16_t replyto_ssp_offset;
        u_int16_t custodian_scheme_offset;
        u_int16_t custodian_ssp_offset;
        u_int64_t creation_ts;
        u_int32_t lifetime;

    } __attribute__((packed));


    /// @{
    /// Helper functions to parse/format the primary block
    friend class BundleProtocol;
    
    static size_t get_primary_len(const Bundle* bundle,
                                  DictionaryVector* dict,
                                  size_t* dictionary_len,
                                  size_t* primary_var_len);

    static size_t get_primary_len(const Bundle* bundle);

    static void add_to_dictionary(const EndpointID& eid,
                                  DictionaryVector* dict,
                                  size_t* dictlen);
    
    static void get_dictionary_offsets(DictionaryVector *dict,
                                       const EndpointID& eid,
                                       u_int16_t* scheme_offset,
                                       u_int16_t* ssp_offset);
    

    static bool extract_dictionary_eid(EndpointID* eid, const char* what,
                                       u_int16_t* scheme_offsetp,
                                       u_int16_t* ssp_offsetp,
                                       u_char* dictionary,
                                       u_int32_t dictionary_len);

    static void debug_dump_dictionary(const char* bp, size_t len,
                                      PrimaryBlock2* primary2);
    
    static u_int8_t format_bundle_flags(const Bundle* bundle);
    static void parse_bundle_flags(Bundle* bundle, u_int8_t flags);

    static u_int8_t format_cos_flags(const Bundle* bundle);
    static void parse_cos_flags(Bundle* bundle, u_int8_t cos_flags);

    static u_int8_t format_srr_flags(const Bundle* bundle);
    static void parse_srr_flags(Bundle* bundle, u_int8_t srr_flags);
    /// @}
};

} // namespace dtn

#endif /* _PRIMARY_BLOCK_PROCESSOR_H_ */
