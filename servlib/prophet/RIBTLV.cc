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

#include <cstring> // for memset
#include <arpa/inet.h> // for hton[ls] and ntoh[ls]
#include "Util.h"
#include "RIBTLV.h"

namespace prophet
{

RIBTLV::RIBTLV(const RIBNodeList& nodes,
               bool relay, bool custody,
               bool internet)
    : BaseTLV(BaseTLV::RIB_TLV), nodes_(nodes),
      relay_(relay), custody_(custody), internet_(internet)
{
    length_  = RIBTLVHeaderSize;
    length_ += RIBEntrySize * nodes_.size();
}

RIBTLV::RIBTLV()
    : BaseTLV(BaseTLV::RIB_TLV), nodes_(),
      relay_(false), custody_(false), internet_(false) {}

size_t
RIBTLV::serialize(u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL || typecode_ != BaseTLV::RIB_TLV) return 0;

    // check for adequate buffer length
    if (RIBTLVHeaderSize + RIBEntrySize * nodes_.size() > len) return 0;

    // initialize accounting
    length_ = 0;
    size_t rib_entry_count = 0;
    RIBTLVHeader* hdr = (RIBTLVHeader*) bp;

    // start writing RIB entries out to buffer, skipping header for now
    bp      += RIBTLVHeaderSize;
    length_ += RIBTLVHeaderSize;
    RIBNode* node = NULL;
    for (const_iterator i = nodes_.begin(); i != nodes_.end(); i++)
    {
        node = (*i);

        size_t bytes_written = 0;
        if ((bytes_written =
                    write_rib_entry(node->sid_, node->p_value(),
                                    node->relay(), node->custody(),
                                    node->internet_gw(), bp, len)) == 0)
            break;
        
        bp      += bytes_written;
        len     -= bytes_written;
        length_ += bytes_written;

        rib_entry_count++;

        node = NULL;
    }

    // fill out the header with how many entries successfully written
    hdr->type             = typecode_;
    hdr->length           = htons(length_);
    hdr->rib_string_count = htons(rib_entry_count);
    hdr->flags            = 0;

    if (relay_)    hdr->flags |= RELAY_NODE;
    if (custody_)  hdr->flags |= CUSTODY_NODE;
    if (internet_) hdr->flags |= INTERNET_GW_NODE;

    return length_;
}

void
RIBTLV::decode_flags(u_int8_t flags, bool* relay,
                     bool* custody, bool* internet)
{
    // weed out the oddball
    if (relay == NULL || custody == NULL || internet == NULL) return;

    //XXX/wilson CUSTODY_NODE doesn't make much sense when !RELAY_NODE but
    //           how's the best way to enforce that?
    *relay    = ((flags & RELAY_NODE)       == RELAY_NODE);
    *custody  = ((flags & CUSTODY_NODE)     == CUSTODY_NODE);
    *internet = ((flags & INTERNET_GW_NODE) == INTERNET_GW_NODE);
}

size_t
RIBTLV::write_rib_entry(u_int16_t sid, double pvalue, bool relay,
                        bool custody, bool internet, u_char* bp,
                        size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;

    // check the length of incoming buffer
    if (RIBEntrySize > len) return 0;

    // cast buffer pointer to RIB, zero, and start writing
    RIBEntry* rib = (RIBEntry*) bp;
    memset(rib,0,RIBEntrySize);
    rib->string_id = htons(sid);
    // scale double to 8 bits
    rib->pvalue = (u_int8_t) ( (int) (pvalue * (255.0)) ) & 0xff;
    rib->flags  = 0;
    if (relay)    rib->flags |= RELAY_NODE;
    if (custody)  rib->flags |= CUSTODY_NODE;
    if (internet) rib->flags |= INTERNET_GW_NODE;

    return RIBEntrySize;
}

size_t
RIBTLV::read_rib_entry(u_int16_t* sid, double* pvalue,
                       bool* relay, bool* custody, bool* internet,
                       const u_char* bp, size_t len)
{
    // weed out the oddball, check for adequate length
    if (RIBEntrySize > len ||
        sid == NULL        ||
        pvalue == NULL     ||
        relay == NULL      ||
        custody == NULL    ||
        internet == NULL   ||
        bp == NULL)           return 0;

    // start reading in from buffer
    RIBEntry* rib = (RIBEntry*) bp;
    *sid = ntohs(rib->string_id);
    // scale 8 bits to double, use 0xff as denominator
    *pvalue = ((rib->pvalue & 0xff) + 0.0) / (255.0);
    decode_flags(rib->flags, relay, custody, internet);
    return RIBEntrySize;
}

bool
RIBTLV::deserialize(const u_char* bp, size_t len)
{
    // weed out the oddball
    if (bp == NULL || typecode_ != BaseTLV::RIB_TLV) return 0;

    // check for adequate length
    if (RIBTLVHeaderSize > len) return 0;

    RIBTLVHeader* hdr = (RIBTLVHeader*) bp;

    // enforce typecode expectation
    if (hdr->type != BaseTLV::RIB_TLV || typecode_ != BaseTLV::RIB_TLV)
        return 0;

    // check incoming header for length
    length_ = ntohs(hdr->length);
    if (length_ > len) return 0;

    // read information from the header
    flags_ = hdr->flags;
    decode_flags(flags_,&relay_,&custody_,&internet_);
    size_t rib_entry_count = ntohs(hdr->rib_string_count);

    // skip past the header, account for how much has been read so far
    bp              += RIBTLVHeaderSize;
    len             -= RIBTLVHeaderSize;
    size_t amt_read  = RIBTLVHeaderSize;

    // read in each RIB entry
    while (rib_entry_count-- > 0)
    {
        RIBNode remote;
        u_int16_t sid = 0;
        double pvalue = 0.0;
        bool relay    = false;
        bool custody  = false;
        bool internet = false;

        size_t bytes_read = 
            read_rib_entry(&sid,&pvalue,&relay,&custody,&internet,bp,len);

        if (bytes_read != RIBEntrySize) break;

        remote.set_pvalue(pvalue);
        remote.set_relay(relay);
        remote.set_custody(custody);
        remote.set_internet_gw(internet);
        remote.sid_ = sid;

        // store this RIB entry
        nodes_.push_back(new RIBNode(remote));

        // account for bytes read
        bp       += bytes_read;
        len      -= bytes_read;
        amt_read += bytes_read;
    }

    return (amt_read == length_);
}

}; // namespace prophet
