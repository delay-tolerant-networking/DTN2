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
#include "Util.h"
#include "RIBDTLV.h"

namespace prophet
{

RIBDTLV::RIBDTLV(const Dictionary& ribd)
    : BaseTLV(BaseTLV::RIBD_TLV), ribd_(ribd)
{
    length_  = RIBDTLVHeaderSize;
    length_ += ribd_.guess_ribd_size(RoutingAddressStringSize);
}

RIBDTLV::RIBDTLV()
    : BaseTLV(BaseTLV::RIBD_TLV) {}

size_t
RIBDTLV::write_ras_entry(u_int16_t sid,
                         const std::string& dest_id,
                         u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;

    // align length of dest_id to 4 byte boundary
    size_t copylen = FOUR_BYTE_ALIGN(dest_id.length());

    // make sure the lengths match up with the provided buffer
    if (RoutingAddressStringSize + copylen > len) return 0;

    // initialize to sizeof overhead
    size_t retval = RoutingAddressStringSize;

    // start writing out to buffer
    RoutingAddressString* ras = (RoutingAddressString*) bp;
    ras->string_id = htons(sid);
    ras->length    = dest_id.length() & 0xff;
    memcpy(&ras->ra_string[0],dest_id.c_str(),dest_id.length());
    retval += copylen; // 32-bit alignment

    // zero out slack, if any
    while (copylen-- > ras->length)
        ras->ra_string[copylen] = '\0';

    return retval;
}

size_t
RIBDTLV::serialize(u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;
    if (typecode_ != BaseTLV::RIBD_TLV) return 0;

    // estimate final size of TLV, check length of inbound buffer
    size_t ribd_sz = ribd_.guess_ribd_size(RoutingAddressStringSize);
    if (ribd_sz + RIBDTLVHeaderSize > len) return 0;

    // initialize accounting
    size_t ribd_entry_count = 0;
    length_ = 0;
    RIBDTLVHeader* hdr = (RIBDTLVHeader*) bp;
    memset(hdr,0,RIBDTLVHeaderSize);

    // write out to buffer, skipping header (for now)
    bp += RIBDTLVHeaderSize;
    length_ += RIBDTLVHeaderSize;

    // write out individual RIBD entries 
    for (Dictionary::const_iterator i = ribd_.begin(); i != ribd_.end(); i++)
    {
        // shouldn't happen, but test anyways
        if ((*i).first < 2)
            continue; // local and remote peers are implied as 0 and 1

        size_t bytes_written = 0;
        if ((bytes_written =
                    write_ras_entry((*i).first,(*i).second,bp,len)) == 0)
            break;

        bp      += bytes_written;
        len     -= bytes_written;
        length_ += bytes_written;

        ribd_entry_count++;
    }

    // fill in header for amounts successfully written 
    hdr->type        = BaseTLV::RIBD_TLV;
    hdr->flags       = 0;
    hdr->length      = htons(length_);
    hdr->entry_count = htons(ribd_entry_count);

    return length_;
}

size_t
RIBDTLV::read_ras_entry(u_int16_t* sid,
                        std::string& dest_id,
                        const u_char* bp, size_t len)
{
    // weed out the oddball
    if (sid == NULL || bp == NULL) return 0;

    // reject too-tight bounds
    if (RoutingAddressStringSize > len) return 0;

    RoutingAddressString* ras = (RoutingAddressString*) bp;

    // initialize to sizeof overhead
    size_t retval = RoutingAddressStringSize;

    // must be even multiple of 4 bytes
    size_t copylen = FOUR_BYTE_ALIGN(ras->length);

    // make sure the lengths match up
    if (copylen > len - retval) return retval;

    // read into memory
    *sid = ntohs(ras->string_id); 
    dest_id.assign((char*)&ras->ra_string[0],ras->length);

    // count what we read
    retval += copylen;

    return retval;
}

bool
RIBDTLV::deserialize(const u_char* bp, size_t len)
{
    RIBDTLVHeader* hdr = (RIBDTLVHeader*) bp;

    // weed out the oddball
    if (bp == NULL) return false;

    // Enforce typecode expectation
    if (hdr->type != RIBD_TLV) return false;

    // Inbound buffer must be at least as big as the overhead
    if (len < RIBDTLVHeaderSize) return false;

    // Fail out if lengths don't match up
    length_ = ntohs(hdr->length);
    if (len < length_) return false;

    flags_  = hdr->flags;

    size_t ribd_entry_count = ntohs(hdr->entry_count);

    // Now that the header is parsed, move past to the first RIBD entry
    bp += RIBDTLVHeaderSize;

    size_t amt_read = RIBDTLVHeaderSize;
    len -= RIBDTLVHeaderSize;

    u_int16_t sid;
    std::string dest_id;
    ribd_.clear();
    while (ribd_entry_count-- > 0)
    {
        // deserialize RAS from buffer
        size_t bytes_read = read_ras_entry(&sid,dest_id,bp,len);

        // abort on error
        if (bytes_read == 0) break;

        // store this dictionary entry
        if(ribd_.assign(dest_id,sid) == false) break;

        len      -= bytes_read;
        bp       += bytes_read;
        amt_read += bytes_read;
    }

    return (amt_read == length_);
}

const Dictionary&
RIBDTLV::ribd(const std::string& sender, const std::string& receiver)
{
    ribd_.assign(sender,0);
    ribd_.assign(receiver,1);
    return ribd_;
}

}; // namespace prophet
