/*
 *    Copyright 2007 Baylor University
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

#include <arpa/inet.h> // for hton[ls] and ntoh[ls]
#include "BundleTLV.h"

namespace prophet
{

size_t BundleTLV::write_bundle_entry(u_int32_t cts, u_int32_t seq,
                                     u_int16_t sid, bool custody,
                                     bool accept, bool ack,
                                     BundleTLVEntry::bundle_entry_t type, 
                                     u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;
    if (type == BundleTLVEntry::UNDEFINED) return 0;

    // check that lengths match up
    if (BundleEntrySize > len) return 0;

    // start writing out to buffer
    BundleEntry* b = (BundleEntry*) bp;
    memset(b, 0, BundleEntrySize);
    if (custody) b->b_flags |= CUSTODY;
    if (accept)  b->b_flags |= ACCEPTED;
    if (ack)     b->b_flags |= ACK;
    b->dest_string_id     = htons(sid);
    b->creation_timestamp = htonl(cts);
    b->seqno              = htonl(seq);

    return BundleEntrySize;
}

size_t BundleTLV::read_bundle_entry(u_int32_t *cts, u_int32_t *seq,
                                    u_int16_t *sid, bool *custody,
                                    bool *accept, bool *ack,
                                    BundleTLVEntry::bundle_entry_t *type,
                                    const u_char* bp, size_t len)
{
    // weed out the oddball
    if (bp      == NULL ||
        cts     == NULL ||
        seq     == NULL ||
        sid     == NULL ||
        custody == NULL ||
        accept  == NULL ||
        ack     == NULL ||
        type    == NULL)   return 0;

    // check that lengths match up
    if (BundleEntrySize > len) return 0;

    // start reading in the entry
    BundleEntry* b = (BundleEntry*) bp;
    u_int8_t flags = b->b_flags & 0xff; // mask out one byte
    *custody = ((flags & CUSTODY)  == CUSTODY);
    *accept  = ((flags & ACCEPTED) == ACCEPTED);
    *ack     = ((flags & ACK)      == ACK);
    // infer whether OFFER or RESPONSE
    *type = BundleTLVEntry::decode_flags(*custody,*accept,*ack);
    *sid = ntohs(b->dest_string_id);
    *cts = ntohl(b->creation_timestamp);
    *seq = ntohl(b->seqno);

    return BundleEntrySize;
}

};
