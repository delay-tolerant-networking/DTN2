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
#include "ResponseTLV.h"

namespace prophet
{

ResponseTLV::ResponseTLV()
    : BundleTLV(BaseTLV::RESPONSE_TLV) {}

ResponseTLV::ResponseTLV(const BundleResponseList& bl)
    : BundleTLV(BaseTLV::RESPONSE_TLV,0,bl.guess_size(BundleEntrySize)),
      list_(bl) {}

size_t
ResponseTLV::serialize(u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;
    if (typecode_ != BaseTLV::RESPONSE_TLV) return 0;

    // check that lengths match up
    if (BundleTLVHeaderSize +
            list_.guess_size(BundleEntrySize) > len)
        return 0;

    // account for bytes written
    length_ = 0;

    // start writing out to buffer
    BundleTLVHeader* hdr = (BundleTLVHeader*) bp;
    memset(hdr, 0, BundleTLVHeaderSize);
    hdr->type = BaseTLV::RESPONSE_TLV;
    hdr->flags = 0;
    hdr->offer_count = htons(list_.size());

    length_ += BundleTLVHeaderSize;
    bp      += BundleTLVHeaderSize;
    len     -= BundleTLVHeaderSize;

    // write out each Bundle offer
    for (BundleResponseList::const_iterator i = list_.begin();
         i != list_.end();
         i++)
    {
        BundleResponseEntry* b = *i;
        size_t bytes_written =
            BundleTLV::write_bundle_entry(b->creation_ts(),
                                    b->seqno(), b->sid(), b->custody(),
                                    b->accept(), b->ack(), b->type(),
                                    bp, len);

        if (BundleEntrySize > bytes_written) break;

        length_ += bytes_written;
        len     -= bytes_written;
        bp      += bytes_written;
    }

    hdr->length = htons(length_);
    return length_;
}

bool
ResponseTLV::deserialize(const u_char* bp, size_t len)
{
    // weed out the oddball
    if (bp == NULL) return false;
    if (typecode_ != RESPONSE_TLV) return false;
    if (! list_.empty()) return false;

    // check that lengths match up
    if (BundleTLVHeaderSize > len) return false;

    // zero out accounting
    size_t amt_read = 0;
    length_ = 0;

    // parse the header
    BundleTLVHeader* b = (BundleTLVHeader*) bp;
    if (b->type != typecode_) return false;
    length_ = ntohs(b->length);
    if (length_ > len) return false;
    flags_ = b->flags;
    size_t offer_count = ntohs(b->offer_count);

    // now move past the header
    bp       += BundleTLVHeaderSize;
    len      -= BundleTLVHeaderSize;
    amt_read += BundleTLVHeaderSize;

    while (len >= BundleTLVHeaderSize &&
           amt_read + BundleTLVHeaderSize <= length_ &&
           offer_count-- > 0)
    {
        u_int32_t cts = 0;
        u_int32_t seq = 0;
        u_int16_t sid = 0;
        bool custody = false,
             accept = false,
             ack = false;
        BundleTLVEntry::bundle_entry_t type = BundleTLVEntry::UNDEFINED;

        size_t bytes_read =
            BundleTLV::read_bundle_entry(&cts,&seq,&sid,&custody,
                                         &accept,&ack,&type,bp,len);

        if (BundleEntrySize > bytes_read)
            return false;

        if (type != BundleTLVEntry::RESPONSE)
            return false;

        if (! list_.add_response(cts,seq,sid,custody,accept))
            return false;

        // account for what's been read
        amt_read += bytes_read;
        len      -= bytes_read;
        bp       += bytes_read;
    }

    return (amt_read == length_);
}

}; // namespace prophet
