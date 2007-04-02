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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string>
#include <sys/types.h>
#include <netinet/in.h>

#include "Bundle.h"
#include "BundleProtocol.h"
#include "PrimaryBlockProcessor.h"
#include "naming/EndpointID.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
struct DictionaryEntry {
    DictionaryEntry(const std::string& s, size_t off)
        : str(s), offset(off) {}

    std::string str;
    size_t offset;
};

class DictionaryVector : public std::vector<DictionaryEntry> {};

//----------------------------------------------------------------------
PrimaryBlockProcessor::PrimaryBlockProcessor()
    : BlockProcessor(BundleProtocol::PRIMARY_BLOCK)
{
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::add_to_dictionary(const EndpointID& eid,
                                         DictionaryVector* dict,
                                         u_int64_t* dictlen,
                                         u_int64_t* scheme_offset,
                                         u_int64_t* ssp_offset)
                                         
{
    /*
     * For the scheme and ssp parts of the given endpoint id, see if
     * they've already appeared in the vector. If not, add them, and
     * record their length (with the null terminator) in the running
     * length total.
     */
    DictionaryVector::iterator iter;
    bool found_scheme = false;
    bool found_ssp = false;

    for (iter = dict->begin(); iter != dict->end(); ++iter) {
        if (iter->str == eid.scheme_str()) {
            found_scheme = true;
            *scheme_offset = iter->offset;
        }

        if (iter->str == eid.ssp()) {
            found_ssp = true;
            *ssp_offset = iter->offset;
        }
    }

    if (found_scheme == false) {
        *scheme_offset = *dictlen;
        dict->push_back(DictionaryEntry(eid.scheme_str(), *dictlen));
        *dictlen += (eid.scheme_str().length() + 1);
    }

    if (found_ssp == false) {
        *ssp_offset = *dictlen;
        dict->push_back(DictionaryEntry(eid.ssp(), *dictlen));
        *dictlen += (eid.ssp().length() + 1);
    }
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::get_dictionary_offsets(DictionaryVector *dict,
                                              const EndpointID& eid,
                                              u_int16_t* scheme_offset,
                                              u_int16_t* ssp_offset)
{
    u_int16_t offset;
    DictionaryVector::iterator iter;
    for (iter = dict->begin(); iter != dict->end(); ++iter) {
        if (iter->str == eid.scheme_str()) {
            offset = htons(iter->offset);
            memcpy(scheme_offset, &offset, sizeof(offset));
        }

        if (iter->str == eid.ssp()) {
            offset = htons(iter->offset);
            memcpy(ssp_offset, &offset, sizeof(offset));
        }
    }
}

//----------------------------------------------------------------------
bool
PrimaryBlockProcessor::extract_dictionary_eid(EndpointID* eid,
                                              const char* what,
                                              u_int64_t scheme_offset,
                                              u_int64_t ssp_offset,
                                              u_char* dictionary,
                                              u_int64_t dictionary_len)
{
    static const char* log = "/dtn/bundle/protocol";

    if (scheme_offset >= (dictionary_len - 1)) {
        log_err_p(log, "illegal offset for %s scheme dictionary offset: "
                  "offset %llu, total length %llu", what,
                  scheme_offset, dictionary_len);
        return false;
    }

    if (ssp_offset >= (dictionary_len - 1)) {
        log_err_p(log, "illegal offset for %s ssp dictionary offset: "
                  "offset %llu, total length %llu", what,
                  ssp_offset, dictionary_len);
        return false;
    }
    
    eid->assign((char*)&dictionary[scheme_offset],
                (char*)&dictionary[ssp_offset]);

    if (! eid->valid()) {
        log_err_p(log, "invalid %s endpoint id '%s': "
                  "scheme '%s' offset %llu/%llu ssp '%s' offset %llu/%llu",
                  what, eid->c_str(),
                  eid->scheme_str().c_str(),
                  scheme_offset, dictionary_len,
                  eid->ssp().c_str(),
                  ssp_offset, dictionary_len);
        return false;                                                      
    }                                                                   
    
    log_debug_p(log, "parsed %s eid (offsets %llu, %llu) %s", 
                what, scheme_offset, ssp_offset, eid->c_str());
    return true;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::debug_dump_dictionary(const char* bp,
                                             const PrimaryBlock& primary)
{
#ifndef NDEBUG
    oasys::StringBuffer dict_copy;

    const char* end = bp + primary.dictionary_length;
    ASSERT(end[-1] == '\0');
    
    while (bp != end) {
        dict_copy.appendf("%s ", bp);
        bp += strlen(bp) + 1;
    }

    log_debug_p("/dtn/bundle/protocol",
                "dictionary len %llu, value: '%s'",
                primary.dictionary_length,
                dict_copy.c_str());
                  
    log_debug_p("/dtn/bundle/protocol",
                "dictionary offsets: dest %llu,%llu source %llu,%llu, "
                "custodian %llu,%llu replyto %llu,%llu",
                primary.dest_scheme_offset,
                primary.dest_ssp_offset,
                primary.source_scheme_offset,
                primary.source_ssp_offset,
                primary.custodian_scheme_offset,
                primary.custodian_ssp_offset,
                primary.replyto_scheme_offset,
                primary.replyto_ssp_offset);
#else
    (void)bp;
    (void)primary;
#endif
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_bundle_flags(const Bundle* bundle)
{
    u_int64_t flags = 0;

    if (bundle->is_fragment_) {
        flags |= BUNDLE_IS_FRAGMENT;
    }

    if (bundle->is_admin_) {
        flags |= BUNDLE_IS_ADMIN;
    }

    if (bundle->do_not_fragment_) {
        flags |= BUNDLE_DO_NOT_FRAGMENT;
    }

    if (bundle->custody_requested_) {
        flags |= BUNDLE_CUSTODY_XFER_REQUESTED;
    }

    if (bundle->singleton_dest_) {
        flags |= BUNDLE_SINGLETON_DESTINATION;
    }
    
    if (bundle->app_acked_rcpt_) {
        flags |= BUNDLE_ACK_BY_APP;
    }
    
    return flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_bundle_flags(Bundle* bundle, u_int64_t flags)
{
    if (flags & BUNDLE_IS_FRAGMENT) {
        bundle->is_fragment_ = true;
    } else {
        bundle->is_fragment_ = false;
    }

    if (flags & BUNDLE_IS_ADMIN) {
        bundle->is_admin_ = true;
    } else {
        bundle->is_admin_ = false;
    }

    if (flags & BUNDLE_DO_NOT_FRAGMENT) {
        bundle->do_not_fragment_ = true;
    } else {
        bundle->do_not_fragment_ = false;
    }

    if (flags & BUNDLE_CUSTODY_XFER_REQUESTED) {
        bundle->custody_requested_ = true;
    } else {
        bundle->custody_requested_ = false;
    }

    if (flags & BUNDLE_SINGLETON_DESTINATION) {
        bundle->singleton_dest_ = true;
    } else {
        bundle->singleton_dest_ = false;
    }
    
    if (flags & BUNDLE_ACK_BY_APP) {
        bundle->app_acked_rcpt_ = true;
    } else {
        bundle->app_acked_rcpt_ = false;
    }
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_cos_flags(const Bundle* b)
{
    u_int64_t cos_flags = 0;

    cos_flags = ((b->priority_ & 0x3) << 7);

    return cos_flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_cos_flags(Bundle* b, u_int64_t cos_flags)
{
    b->priority_ = ((cos_flags >> 7) & 0x3);
}

//----------------------------------------------------------------------
u_int64_t
PrimaryBlockProcessor::format_srr_flags(const Bundle* b)
{
    u_int64_t srr_flags = 0;
    
    if (b->receive_rcpt_)
        srr_flags |= BundleProtocol::STATUS_RECEIVED;

    if (b->custody_rcpt_)
        srr_flags |= BundleProtocol::STATUS_CUSTODY_ACCEPTED;

    if (b->forward_rcpt_)
        srr_flags |= BundleProtocol::STATUS_FORWARDED;

    if (b->delivery_rcpt_)
        srr_flags |= BundleProtocol::STATUS_DELIVERED;

    if (b->deletion_rcpt_)
        srr_flags |= BundleProtocol::STATUS_DELETED;

    return srr_flags;
}
    
//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_srr_flags(Bundle* b, u_int64_t srr_flags)
{
    if (srr_flags & BundleProtocol::STATUS_RECEIVED)
        b->receive_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_CUSTODY_ACCEPTED)
        b->custody_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_FORWARDED)
        b->forward_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_DELIVERED)
        b->delivery_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_DELETED)
        b->deletion_rcpt_ = true;
}

//----------------------------------------------------------------------
size_t
PrimaryBlockProcessor::get_primary_len(const Bundle* bundle,
                                       DictionaryVector* dict,
                                       PrimaryBlock* primary)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t primary_len = 0;
    primary->dictionary_length = 0;
    primary->block_length = 0;
    
    /*
     * We need to figure out the total length of the primary block,
     * except for the SDNVs used to encode flags and the length itself and
     * the one byte version field.
     *
     * First, we determine the size of the dictionary by first
     * figuring out all the unique strings, and in the process,
     * remembering their offsets and summing up their lengths
     * (including the null terminator for each).
     */
    add_to_dictionary(bundle->dest_, dict, &primary->dictionary_length,
                      &primary->dest_scheme_offset,
                      &primary->dest_ssp_offset);
    primary->block_length += SDNV::encoding_len(primary->dest_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->dest_ssp_offset);
    
    add_to_dictionary(bundle->source_, dict, &primary->dictionary_length,
                      &primary->source_scheme_offset,
                      &primary->source_ssp_offset);
    primary->block_length += SDNV::encoding_len(primary->source_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->source_ssp_offset);

    add_to_dictionary(bundle->replyto_, dict, &primary->dictionary_length,
                      &primary->replyto_scheme_offset,
                      &primary->replyto_ssp_offset);
    primary->block_length += SDNV::encoding_len(primary->replyto_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->replyto_ssp_offset);

    add_to_dictionary(bundle->custodian_, dict, &primary->dictionary_length,
                      &primary->custodian_scheme_offset,
                      &primary->custodian_ssp_offset);
    primary->block_length += SDNV::encoding_len(primary->custodian_scheme_offset);
    primary->block_length += SDNV::encoding_len(primary->custodian_ssp_offset);
    
    (void)log; // in case NDEBUG is defined
    log_debug_p(log, "generated dictionary length %llu",
                primary->dictionary_length);
    
    primary->block_length += SDNV::encoding_len(bundle->creation_ts_.seconds_);
    primary->block_length += SDNV::encoding_len(bundle->creation_ts_.seqno_);
    primary->block_length += SDNV::encoding_len(bundle->expiration_);

    primary->block_length += SDNV::encoding_len(primary->dictionary_length);
    primary->block_length += primary->dictionary_length;
    
    /*
     * If the bundle is a fragment, we need to include space for the
     * fragment offset and the original payload length.
     *
     * Note: Any changes to this protocol must be reflected into the
     * FragmentManager since it depends on this length when
     * calculating fragment sizes.
     */
    if (bundle->is_fragment_) {
        primary->block_length += SDNV::encoding_len(bundle->frag_offset_);
        primary->block_length += SDNV::encoding_len(bundle->orig_length_);
    }
    
    // Format the processing flags.
    primary->processing_flags = format_bundle_flags(bundle);
    primary->processing_flags |= format_cos_flags(bundle);
    primary->processing_flags |= format_srr_flags(bundle);

    /*
     * Finally, add up the initial preamble and the variable
     * length part.
     */
    primary_len = 1 + SDNV::encoding_len(primary->processing_flags) +
                  SDNV::encoding_len(primary->block_length) +
                  primary->block_length;
    
    log_debug_p(log, "get_primary_len(bundle %d): %zu",
                bundle->bundleid_, primary_len);
    
    // Fill in the remaining values of 'primary' just for the sake of returning
    // a complete data structure.
    primary->version = BundleProtocol::CURRENT_VERSION;
    primary->creation_time = bundle->creation_ts_.seconds_;
    primary->creation_sequence = bundle->creation_ts_.seqno_;
    primary->lifetime = bundle->expiration_;
    
    return primary_len;
}

//----------------------------------------------------------------------
size_t
PrimaryBlockProcessor::get_primary_len(const Bundle* bundle)
{
    DictionaryVector dict;
    PrimaryBlock primary;

    return get_primary_len(bundle, &dict, &primary);
}

//----------------------------------------------------------------------
int
PrimaryBlockProcessor::consume(Bundle* bundle, BlockInfo* block, u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t consumed = 0;
    PrimaryBlock primary;
    
    ASSERT(! block->complete());
    
    memset(&primary, 0, sizeof(primary));
    
    /*
     * Now see if this completes the primary by calling into
     * BlockProcessor::consume() to get the default behavior (now that
     * we've found the data length above).
     */
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return consumed + cc;
    }

    /*
     * Ok, now the block is complete so we can parse it. We'll return
     * the total amount consumed (or -1 for protocol error) when
     * finished.
     */
    consumed += cc;

    size_t primary_len = block->full_length();

    buf = block->writable_contents()->buf();
    len = block->writable_contents()->len();

    ASSERT(primary_len == len);

    primary.version = *(u_int8_t*)buf;
    buf += 1;
    len -= 1;
    
    if (primary.version != BundleProtocol::CURRENT_VERSION) {
        log_warn_p(log, "protocol version mismatch %d != %d",
                   primary.version, BundleProtocol::CURRENT_VERSION);
        return -1;
    }
    
#define PBP_READ_SDNV(location) { \
    int sdnv_len = SDNV::decode(buf, len, location); \
    if (sdnv_len < 0) \
        goto tooshort; \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Grab the SDNVs representing the flags and the block length.
    PBP_READ_SDNV(&primary.processing_flags);
    PBP_READ_SDNV(&primary.block_length);

    log_debug_p(log, "parsed primary block: version %d length %u",
                primary.version, block->data_length());    
    
    // Parse the flags.
    parse_bundle_flags(bundle, primary.processing_flags);
    parse_cos_flags(bundle, primary.processing_flags);
    parse_srr_flags(bundle, primary.processing_flags);
    
    // What remains in the buffer should now be equal to what the block-length
    // field advertised.
    ASSERT(len == block->data_length());
    
    // Read the various SDNVs up to the start of the dictionary.
    PBP_READ_SDNV(&primary.dest_scheme_offset);
    PBP_READ_SDNV(&primary.dest_ssp_offset);
    PBP_READ_SDNV(&primary.source_scheme_offset);
    PBP_READ_SDNV(&primary.source_ssp_offset);
    PBP_READ_SDNV(&primary.replyto_scheme_offset);
    PBP_READ_SDNV(&primary.replyto_ssp_offset);
    PBP_READ_SDNV(&primary.custodian_scheme_offset);
    PBP_READ_SDNV(&primary.custodian_ssp_offset);
    PBP_READ_SDNV(&primary.creation_time);
    PBP_READ_SDNV(&primary.creation_sequence);
    PBP_READ_SDNV(&primary.lifetime);
    PBP_READ_SDNV(&primary.dictionary_length);
    
    // Make sure that the creation timestamp parts and the lifetime fit into
    // a 32 bit integer.
    if (primary.creation_time > 0xffffffff) {
        log_err_p(log, "creation timestamp time is too large: %llu",
                  primary.creation_time);
        return -1;
    }

    if (primary.creation_sequence > 0xffffffff) {
        log_err_p(log, "creation timestamp sequence is too large: %llu",
                  primary.creation_sequence);
        return -1;
    }
    
    if (primary.lifetime > 0xffffffff) {
        log_err_p(log, "lifetime is too large: %llu", primary.lifetime);
        return -1;
    }
    
    bundle->creation_ts_.seconds_ = (u_int32_t)primary.creation_time;
    bundle->creation_ts_.seqno_ = (u_int32_t)primary.creation_sequence;
    bundle->expiration_ = (u_int32_t)primary.lifetime;

    /*
     * Verify that we have the whole dictionary.
     */
    if (len < primary.dictionary_length) {
tooshort:
        log_err_p(log, "primary block advertised incorrect length %u",
                  block->data_length());
        return -1;
    }

    /*
     * Make sure that the dictionary ends with a null byte.
     */
    if (buf[primary.dictionary_length - 1] != '\0') {
        log_err_p(log, "dictionary does not end with a NULL character!");
        return -1;
    }

    /*
     * Now use the dictionary buffer to parse out the various endpoint
     * identifiers, making sure that none of them peeks past the end
     * of the dictionary block.
     */
    u_char* dictionary = buf;
    buf += primary.dictionary_length;
    len -= primary.dictionary_length;

    extract_dictionary_eid(&bundle->source_, "source",
                           primary.source_scheme_offset,
                           primary.source_ssp_offset,
                           dictionary, primary.dictionary_length);
    
    extract_dictionary_eid(&bundle->dest_, "dest",
                           primary.dest_scheme_offset,
                           primary.dest_ssp_offset,
                           dictionary, primary.dictionary_length);
    
    extract_dictionary_eid(&bundle->replyto_, "replyto",
                           primary.replyto_scheme_offset,
                           primary.replyto_ssp_offset,
                           dictionary, primary.dictionary_length);
    
    extract_dictionary_eid(&bundle->custodian_, "custodian",
                           primary.custodian_scheme_offset,
                           primary.custodian_ssp_offset,
                           dictionary, primary.dictionary_length);
    
    debug_dump_dictionary((char*)dictionary, primary);
    
    // If the bundle is a fragment, grab the fragment offset and original
    // bundle size (and make sure they fit in a 32 bit integer).
    if (bundle->is_fragment_) {
        u_int64_t sdnv_buf = 0;
        PBP_READ_SDNV(&sdnv_buf);
        if (sdnv_buf > 0xffffffff) {
            log_err_p(log, "fragment offset is too large: %llu", sdnv_buf);
            return -1;
        }
        
        bundle->frag_offset_ = (u_int32_t)sdnv_buf;
        sdnv_buf = 0;
        
        PBP_READ_SDNV(&sdnv_buf);
        if (sdnv_buf > 0xffffffff) {
            log_err_p(log, "fragment original length is too large: %llu",
                      sdnv_buf);
            return -1;
        }
        
        bundle->orig_length_ = (u_int32_t)sdnv_buf;

        log_debug_p(log, "parsed fragmentation info: offset %u, orig_len %u",
                    bundle->frag_offset_, bundle->orig_length_);
    }

#undef PBP_READ_SDNV
    
    return consumed;
}

//----------------------------------------------------------------------
bool
PrimaryBlockProcessor::validate(const Bundle* bundle, BlockInfo* block,
                     BundleProtocol::status_report_reason_t* reception_reason,
                     BundleProtocol::status_report_reason_t* deletion_reason)
{
    (void)block;
    (void)reception_reason;
    static const char* log = "/dtn/bundle/protocol";
    
    // Make sure all four EIDs are valid.
    bool eids_valid = true;
    eids_valid &= bundle->source_.valid();
    eids_valid &= bundle->dest_.valid();
    eids_valid &= bundle->custodian_.valid();
    eids_valid &= bundle->replyto_.valid();
    
    if (!eids_valid) {
        log_err_p(log, "bad value for one or more EIDs");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
    
    // According to BP section 3.3, there are certain things that a bundle
    // with a null source EID should not try to do. Check for these cases
    // and reject the bundle if any is true.
    if (bundle->source_ == EndpointID::NULL_EID()) {
        if (bundle->receipt_requested() || bundle->app_acked_rcpt_) { 
            log_err_p(log, "bundle with null source eid has requested a "
                    "report; rejection it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    
        if (bundle->custody_requested_) {
            log_err_p(log, "bundle with null source eid has requested custody "
                    "transfer; rejection it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }

        if (!bundle->do_not_fragment_) {
            log_err_p(log, "bundle with null source eid has not set "
                    "'do-not-fragment' flag; rejection it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }
    
    // Admin bundles cannot request custody transfer.
    if (bundle->is_admin_) {
        if (bundle->custody_requested_) {
            log_err_p(log, "admin bundle requested custody transfer; "
                      "rejection it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }

        if ( bundle->receive_rcpt_ ||
             bundle->custody_rcpt_ ||
             bundle->forward_rcpt_ ||
             bundle->delivery_rcpt_ ||
             bundle->deletion_rcpt_ ||
             bundle->app_acked_rcpt_ ) {
            log_err_p(log, "admin bundle has requested a report; rejection it");
            *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::generate(const Bundle*  bundle,
                                const LinkRef& link,
                                BlockInfo*     block,
                                bool           last)
{
    (void)link;

    /*
     * The primary can't be last since there must be a payload block
     */
    ASSERT(!last);

    static const char* log = "/dtn/bundle/protocol";
    DictionaryVector dict;
    size_t primary_len = 0;     // total length of the primary block
    PrimaryBlock primary;
    
    memset(&primary, 0, sizeof(primary));

    /*
     * Calculate the primary block length and initialize the buffer.
     */
    primary_len = get_primary_len(bundle, &dict, &primary);
    block->writable_contents()->reserve(primary_len);
    block->writable_contents()->set_len(primary_len);
    block->set_data_length(primary_len);
    
    /*
     * Advance buf and decrement len as we go through the process.
     */
    u_char* buf = block->writable_contents()->buf();
    int     len = primary_len;
    
    (void)log; // in case NDEBUG is defined
    log_debug_p(log, "generating primary: length %zu", primary_len);
    
    // Stick the version number in the first byte.
    *buf = BundleProtocol::CURRENT_VERSION;
    ++buf;
    --len;
    
#define PBP_WRITE_SDNV(value) { \
    int sdnv_len = SDNV::encode(value, buf, len); \
    ASSERT(sdnv_len > 0); \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Write out all of the SDNVs
    PBP_WRITE_SDNV(primary.processing_flags);
    PBP_WRITE_SDNV(primary.block_length);
    PBP_WRITE_SDNV(primary.dest_scheme_offset);
    PBP_WRITE_SDNV(primary.dest_ssp_offset);
    PBP_WRITE_SDNV(primary.source_scheme_offset);
    PBP_WRITE_SDNV(primary.source_ssp_offset);
    PBP_WRITE_SDNV(primary.replyto_scheme_offset);
    PBP_WRITE_SDNV(primary.replyto_ssp_offset);
    PBP_WRITE_SDNV(primary.custodian_scheme_offset);
    PBP_WRITE_SDNV(primary.custodian_ssp_offset);
    PBP_WRITE_SDNV(bundle->creation_ts_.seconds_);
    PBP_WRITE_SDNV(bundle->creation_ts_.seqno_);
    PBP_WRITE_SDNV(bundle->expiration_);
    PBP_WRITE_SDNV(primary.dictionary_length);
    
    // Add the dictionary.
    DictionaryVector::iterator dict_iter;
    for (dict_iter = dict.begin(); dict_iter != dict.end(); ++dict_iter) {
        strcpy((char*)buf, dict_iter->str.c_str());
        buf += dict_iter->str.length() + 1;
        len -= dict_iter->str.length() + 1;
    }
    
    debug_dump_dictionary((char*)buf - primary.dictionary_length, primary);
    
    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (bundle->is_fragment_) {
        PBP_WRITE_SDNV(bundle->frag_offset_);
        PBP_WRITE_SDNV(bundle->orig_length_);
    }
    
#undef PBP_WRITE_SDNV

    // XXX/jward what is this?
/*#ifndef NDEBUG
    {
        DictionaryVector dict2;
        size_t dict2_len = 0;
        size_t p2len;
        size_t len2 = get_primary_len(bundle, &dict2, &dict2_len, &p2len);
        ASSERT(len2 == primary_len);
    }
#endif*/

    /*
     * Asuming that get_primary_len is written correctly, len should
     * now be zero since we initialized it to primary_len at the
     * beginning of the function.
     */
    ASSERT(len == 0);
}


} // namespace dtn
