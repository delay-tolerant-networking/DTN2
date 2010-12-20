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
#  include <dtn-config.h>
#endif

#include <oasys/util/ScratchBuffer.h>
#include "CustodySignal.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
void
CustodySignal::create_custody_signal(Bundle*           bundle,
                                     const Bundle*     orig_bundle,
                                     const EndpointID& source_eid,
                                     bool              succeeded,
                                     reason_t          reason)
{
    bundle->mutable_source()->assign(source_eid);
    if (orig_bundle->custodian().equals(EndpointID::NULL_EID())) {
        PANIC("create_custody_signal(*%p): "
              "custody signal cannot be generated to null eid",
              orig_bundle);
    }
    bundle->mutable_dest()->assign(orig_bundle->custodian());
    bundle->mutable_replyto()->assign(EndpointID::NULL_EID());
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_is_admin(true);

    // use the expiration time from the original bundle
    // XXX/demmer maybe something more clever??
    bundle->set_expiration(orig_bundle->expiration());

    int sdnv_encoding_len = 0;
    int signal_len = 0;
    
    // we generally don't expect the Custody Signal length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;
    
    // format of custody signals:
    //
    // 1 byte admin payload type and flags
    // 1 byte status code
    // SDNV   [Fragment Offset (if present)]
    // SDNV   [Fragment Length (if present)]
    // SDNVx2 Time of custody signal
    // SDNVx2 Copy of bundle X's Creation Timestamp
    // SDNV   Length of X's source endpoint ID
    // vari   Source endpoint ID of bundle X

    //
    // first calculate the length
    //

    // the non-optional, fixed-length fields above:
    signal_len =  1 + 1;

    // the 2 SDNV fragment fields:
    if (orig_bundle->is_fragment()) {
        signal_len += SDNV::encoding_len(orig_bundle->frag_offset());
        signal_len += SDNV::encoding_len(orig_bundle->orig_length());
    }
    
    // Time field, set to the current time:
    BundleTimestamp now;
    now.seconds_ = BundleTimestamp::get_current_time();
    now.seqno_   = 0;
    signal_len += BundleProtocol::ts_encoding_len(now);
    
    // The bundle's creation timestamp:
    signal_len += BundleProtocol::ts_encoding_len(orig_bundle->creation_ts());

    // the Source Endpoint ID length and value
int ipn = 0;
if(strncmp((const char*)orig_bundle->source().c_str(), "ipn", 3) == 0)
 ipn = 2;
    signal_len += SDNV::encoding_len(orig_bundle->source().length() - ipn) +
                  orig_bundle->source().length() - ipn;

    //
    // now format the buffer
    //
    u_char* bp = scratch.buf(signal_len);
    int len = signal_len;
    
    // Admin Payload Type and flags
    *bp = (BundleProtocol::ADMIN_CUSTODY_SIGNAL << 4);
    if (orig_bundle->is_fragment()) {
        *bp |= BundleProtocol::ADMIN_IS_FRAGMENT;
    }
    bp++;
    len--;
    
    // Success flag and reason code
    *bp++ = ((succeeded ? 1 : 0) << 7) | (reason & 0x7f);
    len--;
    
    // The 2 Fragment Fields
    if (orig_bundle->is_fragment()) {
        sdnv_encoding_len = SDNV::encode(orig_bundle->frag_offset(), bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
        
        sdnv_encoding_len = SDNV::encode(orig_bundle->orig_length(), bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
    }
    
    sdnv_encoding_len = BundleProtocol::set_timestamp(bp, len, now);
    ASSERT(sdnv_encoding_len > 0);
    bp  += sdnv_encoding_len;
    len -= sdnv_encoding_len;

    // Copy of bundle X's Creation Timestamp
    sdnv_encoding_len = 
        BundleProtocol::set_timestamp(bp, len, orig_bundle->creation_ts());
    ASSERT(sdnv_encoding_len > 0);
    bp  += sdnv_encoding_len;
    len -= sdnv_encoding_len;

    // The Endpoint ID length and data
char buf[80];
u_int64_t a, b;
size_t eidlen;

if(sscanf((const char*)orig_bundle->source().c_str(), "ipn://%" PRIu64 "/%" PRIu64, &a, &b) == 2)
{
        sprintf(buf, "ipn:%" PRIu64 ".%" PRIu64, a, b);
        eidlen = strlen(buf);
	sdnv_encoding_len = SDNV::encode(eidlen, bp, len);
	ASSERT(sdnv_encoding_len > 0);
	len -= sdnv_encoding_len;
	bp += sdnv_encoding_len;

        memcpy(bp, buf, len);
}
else
{

    sdnv_encoding_len = SDNV::encode(orig_bundle->source().length(), bp, len);
    ASSERT(sdnv_encoding_len > 0);
    len -= sdnv_encoding_len;
    bp  += sdnv_encoding_len;
    
    ASSERT((u_int)len == orig_bundle->source().length());

    memcpy(bp, orig_bundle->source().c_str(), orig_bundle->source().length());
}
//printf("WTF %s\n", orig_bundle->source().c_str()); 
    // 
    // Finished generating the payload
    //
    bundle->mutable_payload()->set_data(scratch.buf(), signal_len);
}

//----------------------------------------------------------------------
bool
CustodySignal::parse_custody_signal(data_t* data,
                                    const u_char* bp, u_int len)
{
    // 1 byte Admin Payload Type + Flags:
    if (len < 1) { return false; }
    data->admin_type_  = (*bp >> 4);
    data->admin_flags_ = *bp & 0xf;
    bp++;
    len--;

    // validate the admin type
    if (data->admin_type_ != BundleProtocol::ADMIN_CUSTODY_SIGNAL) {
        return false;
    }

    // Success flag and reason code
    if (len < 1) { return false; }
    data->succeeded_ = (*bp >> 7);
    data->reason_    = (*bp & 0x7f);
    bp++;
    len--;
    
    // Fragment SDNV Fields (offset & length), if present:
    if (data->admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT)
    {
        int sdnv_bytes = SDNV::decode(bp, len, &data->orig_frag_offset_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
        
        sdnv_bytes = SDNV::decode(bp, len, &data->orig_frag_length_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
    }
    
    int ts_len;

    // The signal timestamp
    ts_len = BundleProtocol::get_timestamp(&data->custody_signal_tv_, bp, len);
    if (ts_len < 0) { return false; }
    bp  += ts_len;
    len -= ts_len;

    // Bundle Creation Timestamp
    ts_len = BundleProtocol::get_timestamp(&data->orig_creation_tv_, bp, len);
    if (ts_len < 0) { return false; }
    bp  += ts_len;
    len -= ts_len;

    // Source Endpoint ID of Bundle
    u_int64_t EID_len;
    int num_bytes = SDNV::decode(bp, len, &EID_len);
    if (num_bytes == -1) { return false; }
    bp  += num_bytes;
    len -= num_bytes;

    if (len != EID_len) { return false; }

    bool ok;
    u_int64_t a, b;
    if(sscanf((const char*)bp, "ipn:%" PRIu64 ".%" PRIu64, &a, &b) == 2)
    {
       char buf[80];
       sscanf((const char*)bp, "ipn:%" PRIu64 ".%" PRIu64, &a, &b);
       sprintf(buf, "ipn://%" PRIu64 "/%" PRIu64, a, b);
       len = strlen(buf);
       ok = data->orig_source_eid_.assign(std::string((const char*)buf, len));
    }
    else
    {
       ok = data->orig_source_eid_.assign(std::string((const char*)bp, len));
    }
    if (!ok) {
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
const char*
CustodySignal::reason_to_str(u_int8_t reason)
{
    switch (reason) {
    case BundleProtocol::CUSTODY_NO_ADDTL_INFO:
        return "no additional info";
        
    case BundleProtocol::CUSTODY_REDUNDANT_RECEPTION:
        return "redundant reception";
        
    case BundleProtocol::CUSTODY_DEPLETED_STORAGE:
        return "depleted storage";
        
    case BundleProtocol::CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE:
        return "eid unintelligible";
        
    case BundleProtocol::CUSTODY_NO_ROUTE_TO_DEST:
        return "no route to dest";
        
    case BundleProtocol::CUSTODY_NO_TIMELY_CONTACT:
        return "no timely contact";
        
    case BundleProtocol::CUSTODY_BLOCK_UNINTELLIGIBLE:
        return "block unintelligible";
    }

    static char buf[64];
    snprintf(buf, 64, "unknown reason %d", reason);
    return buf;
}

} // namespace dtn
