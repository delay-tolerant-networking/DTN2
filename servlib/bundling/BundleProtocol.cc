/*
 *    Copyright 2004-2006 Intel Corporation
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


#include <sys/types.h>
#include <netinet/in.h>
#include <algorithm>

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringUtils.h>

#include "BlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleTimestamp.h"
#include "PrimaryBlockProcessor.h"
#include "SDNV.h"
#include "UnknownBlockProcessor.h"

namespace dtn {

//////////////////////////////////////////////////////////////////////

// XXX NEW INTERFACE

BlockProcessor* BundleProtocol::processors_[256];

//----------------------------------------------------------------------
void
BundleProtocol::register_processor(BlockProcessor* bp)
{
    // can't override an existing processor
    ASSERT(processors_[bp->block_type()] == 0);
    processors_[bp->block_type()] = bp;
}

//----------------------------------------------------------------------
BlockProcessor*
BundleProtocol::find_processor(u_int8_t type)
{
    BlockProcessor* ret = processors_[type];
    if (ret == 0) {
        ret = UnknownBlockProcessor::instance();
    }
    return ret;
}

//----------------------------------------------------------------------
int
BundleProtocol::consume(Bundle* bundle,
                        u_char* data,
                        size_t  len,
                        bool*   last)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t origlen = len;
    *last = false;
    
    // special case for first time we get called, since we need to
    // create a BlockInfo struct for the primary block without knowing
    // the typecode or the length
    if (bundle->recv_blocks_.empty()) {
        log_debug(log, "first block... creating primary block info");
        bundle->recv_blocks_.push_back(BlockInfo(find_processor(PRIMARY_BLOCK)));
    }

    // loop as long as there is data left to process
    while (len != 0) {
        log_debug(log, "consume has %zu bytes left to process", len);
        BlockInfo& info = bundle->recv_blocks_.back();

        // if the last received block is complete, create a new one
        // and push it onto the vector. at this stage we consume all
        // blocks, even if there's no BlockProcessor that understands
        // how to parse it
        if (info.complete()) {
            bundle->recv_blocks_.push_back(BlockInfo(find_processor(*data)));
            info = bundle->recv_blocks_.back();
        }
        
        // now we know that the block isn't complete, so we tell it to
        // consume a chunk of data
        log_debug(log, "block type 0x%x incomplete, calling consume again",
                  info.type());
        
        int cc = info.owner()->consume(bundle, &info, data, len);
        if (cc < 0) {
            log_err(log, "protocol error handling block 0x%x", info.type());
            return -1;
        }
        
        // decrement the amount that was just handled from the overall
        // total. verify that the block was either completed or
        // consumed all the data that was passed in.
        len  -= cc;
        data += cc;
        if (info.complete()) {
            log_debug(log, "consumed %u bytes, completed block type 0x%x",
                      cc, info.type());

            // check if we're done with the bundle
            if (info.flags() & BLOCK_FLAG_LAST_BLOCK) {
                log_debug(log, "got last block flag, bundle complete");
                *last = true;
                break;
            }
                
        } else {
            log_debug(log, "consumed %u bytes, block type 0x%x still incomplete",
                      cc, info.type());
            ASSERT(len == 0);
        }
    }
    
    log_debug(log, "consume completed, %zu/%zu bytes consumed",
              origlen - len, origlen);
    return origlen - len;
}

//////////////////////////////////////////////////////////////////////
//
// OLD INTERFACE: only supports primary and payload

//----------------------------------------------------------------------
int
BundleProtocol::format_header_blocks(const Bundle* bundle,
                                     u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    
    BlockProcessor* primary_bp = find_processor(PRIMARY_BLOCK);
    BlockProcessor* payload_bp = find_processor(PAYLOAD_BLOCK);
    
    BlockInfo primary(primary_bp);
    BlockInfo payload(payload_bp);

    primary_bp->generate(bundle, NULL, &primary, false);
    payload_bp->generate(bundle, NULL, &payload, true);

    size_t primary_len = primary.full_length();
    ASSERT(primary_len == primary.contents().len());
    ASSERT(primary_len == primary.data_length());
    ASSERT(primary_len == primary.full_length());
    
    size_t payload_hdr_len = payload.data_offset();
    ASSERT(payload_hdr_len == payload.contents().len());
    ASSERT(payload_hdr_len <= payload.full_length());

    if (primary_len + payload_hdr_len > len) {
        log_debug(log, "format_header_blocks: "
                  "need %zu bytes primary + %zu payload_hdr, only have %zu",
                  primary_len, payload_hdr_len, len);
        return -1; // too short
    }
    
    primary_bp->produce(bundle, &primary, buf, 0, primary_len);
    payload_bp->produce(bundle, &payload, buf + primary_len, 0, payload_hdr_len);
    
    log_debug(log, "format_header_blocks done -- total length %zu",
              (primary_len + payload_hdr_len));
    return primary_len + payload_hdr_len;
}

//----------------------------------------------------------------------
int
BundleProtocol::parse_header_blocks(Bundle* bundle,
                                    u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t origlen = len;

    BlockProcessor* primary_bp = find_processor(PRIMARY_BLOCK);
    BlockProcessor* payload_bp = find_processor(PAYLOAD_BLOCK);

    ASSERT(bundle->recv_blocks_.empty());
    bundle->recv_blocks_.push_back(BlockInfo(primary_bp));
    bundle->recv_blocks_.push_back(BlockInfo(payload_bp));

    BlockInfo& primary = *(bundle->recv_blocks_.begin());
    BlockInfo& payload = *(bundle->recv_blocks_.begin() + 1);

    int primary_len = primary_bp->consume(bundle, &primary, buf, len);

    if (primary_len == -1) {
        log_err(log, "protocol error parsing primary");
        bundle->recv_blocks_.clear();
        return -1;
    }
    
    if (!primary.complete()) {
        log_debug(log, "buffer too short to consume primary");
        bundle->recv_blocks_.clear();
        return -1;
    }

    ASSERT(primary_len <= (int)len);
    ASSERT(primary_len == (int)primary.full_length());
    ASSERT(primary_len == (int)primary.contents().len());

    buf += primary_len;
    len -= primary_len;

    int payload_hdr_len = payload_bp->consume_preamble(&payload, buf, len);

    if (payload_hdr_len == -1) {
        log_err(log, "protocol error parsing payload");
        bundle->recv_blocks_.clear();
        return -1;
    }
    
    if (payload.data_offset() == 0) {
        log_debug(log, "buffer too short to consume primary");
        bundle->recv_blocks_.clear();
        return -1;
    }

    len -= payload_hdr_len;

    bundle->payload_.set_length(payload.data_length());
    
    // that's all we parse, return the amount we consumed
    log_debug(log, "parse_header_blocks succeeded: %d primary %d payload header",
              primary_len, payload_hdr_len);
    
    return origlen - len;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::header_block_length(const Bundle* bundle)
{
    return PrimaryBlockProcessor::get_primary_len(bundle) +
        sizeof(BlockPreamble) +
        SDNV::encoding_len(bundle->payload_.length());
}

//----------------------------------------------------------------------
size_t
BundleProtocol::formatted_length(const Bundle* bundle)
{
    return header_block_length(bundle) +
        bundle->payload_.length() +
        tail_block_length(bundle);
}

//----------------------------------------------------------------------
int
BundleProtocol::format_bundle(const Bundle* bundle, u_char* buf, size_t len)
{
    size_t origlen = len;
    
    // first the header blocks
    int ret = format_header_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    buf += ret;
    len -= ret;

    // then the payload
    size_t payload_len = bundle->payload_.length();
    if (payload_len > len) {
        return -1;
    }
    bundle->payload_.read_data(0, payload_len, buf,
                               BundlePayload::FORCE_COPY);
    len -= payload_len;
    buf += payload_len;

    ret = format_tail_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    len -= ret;
    buf += ret;

    return origlen - len;
}
    
//----------------------------------------------------------------------
int
BundleProtocol::parse_bundle(Bundle* bundle, u_char* buf, size_t len)
{
    size_t origlen = len;
    
    // first the header blocks
    int ret = parse_header_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    buf += ret;
    len -= ret;

    // then the payload
    size_t payload_len = bundle->payload_.length();
    if (payload_len > len) {
        return -1;
    }
    bundle->payload_.set_data(buf, payload_len);
    len -= payload_len;
    buf += payload_len;

    ret = parse_tail_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    len -= ret;
    buf += ret;

    return origlen - len;
}
    
//----------------------------------------------------------------------
void
BundleProtocol::set_timestamp(u_char* ts, const BundleTimestamp* tv)
{
    u_int32_t tmp;

    tmp = htonl(tv->seconds_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
    ts += sizeof(u_int32_t);

    tmp = htonl(tv->seqno_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
}

//----------------------------------------------------------------------
void
BundleProtocol::get_timestamp(BundleTimestamp* tv, const u_char* ts)
{
    u_int32_t tmp;

    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seconds_  = ntohl(tmp);
    ts += sizeof(u_int32_t);
    
    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seqno_ = ntohl(tmp);
}

//----------------------------------------------------------------------
bool
BundleProtocol::get_admin_type(const Bundle* bundle, admin_record_type_t* type)
{
    if (! bundle->is_admin_) {
	return false;
    }

    u_char buf[16];
    const u_char* bp = bundle->payload_.read_data(0, sizeof(buf), buf);

    switch (bp[0] >> 4)
    {
#define CASE(_what) case _what: *type = _what; return true;

	CASE(ADMIN_STATUS_REPORT);
	CASE(ADMIN_CUSTODY_SIGNAL);
	CASE(ADMIN_ANNOUNCE);

#undef  CASE
    default:
        return false; // unknown type
    }

    NOTREACHED;
}

} // namespace dtn
