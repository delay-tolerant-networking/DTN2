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

#include "ProphetTLV.h"
#include "HelloTLV.h"
#include "RIBDTLV.h"
#include "RIBTLV.h"
#include "OfferTLV.h"
#include "ResponseTLV.h"
#include "Params.h"

namespace prophet
{

ProphetTLV::ProphetTLV()
    : result_(UnknownResult), sender_instance_(0), receiver_instance_(0),
      tid_(0), length_(ProphetHeaderSize)
{
}

ProphetTLV::ProphetTLV(const std::string& src, const std::string& dst, 
                       header_result_t result, u_int16_t local_instance,
                       u_int16_t remote_instance, u_int32_t tid)
    : src_(src), dst_(dst), result_(result),
      sender_instance_(local_instance), 
      receiver_instance_(remote_instance), tid_(tid),
      length_(ProphetHeaderSize)
{
}

ProphetTLV::ProphetTLV(const ProphetTLV& p)
    : src_(p.src_), dst_(p.dst_), result_(p.result_),
      sender_instance_(p.sender_instance_),
      receiver_instance_(p.receiver_instance_),
      tid_(p.tid_), length_(p.length_)
{
    list_.assign(p.list_.begin(),p.list_.end());
}

ProphetTLV::~ProphetTLV()
{
    for (iterator i = list_.begin(); i != list_.end(); i++)
        delete *i;
}

size_t
ProphetTLV::serialize(u_char* bp, size_t len) const
{
    // weed out the oddball
    if (bp == NULL) return 0;

    // check that lengths match up
    if (ProphetHeaderSize > len) return 0;

    // initialize some accounting
    length_ = 0;

    // save pointer to header
    ProphetHeader* hdr = (ProphetHeader*) bp;
    memset(hdr,0,ProphetHeaderSize);

    // skip past header for now
    bp += ProphetHeaderSize;

    // write out each TLV to buffer
    for (const_iterator i = list_.begin(); i != list_.end(); i++)
    {
        BaseTLV* tlv = *i;
        size_t bytes_written = tlv->serialize(bp,len);
        if (bytes_written != tlv->length())
            break;

        length_ += bytes_written;
        len     -= bytes_written;
        bp      += bytes_written;
    }

    // if all went well, the math should line up
    if (ProphetHeaderSize > len) return 0;

    // now write out the header
    length_ += ProphetHeaderSize;
    len     -= ProphetHeaderSize;

    hdr->version           = ProphetParams::PROPHET_VERSION;
    hdr->flags             = 0;
    hdr->result            = result_;
    hdr->code              = 0;
    hdr->sender_instance   = htons(sender_instance_);
    hdr->receiver_instance = htons(receiver_instance_);
    hdr->transaction_id    = htonl(tid_);
    hdr->length            = htons(length_);

    return length_;
}

ProphetTLV*
ProphetTLV::deserialize(const std::string& src,
                        const std::string& dst,
                        const u_char* bp, size_t len)
{
    // weed out the oddball
    if (bp == NULL) return NULL;

    // check that lengths match up
    if (ProphetHeaderSize > len) return NULL;

    // start reading and validating header from buffer
    ProphetHeader* hdr = (ProphetHeader*) bp; 
    if (hdr->version != ProphetParams::PROPHET_VERSION) return NULL;
    if (hdr->flags != 0) return NULL;
    if (hdr->code != 0) return NULL;
    if (static_cast<size_t>(ntohs(hdr->length)) > len) return NULL;

    // Create object and begin copying in from buffer
    ProphetTLV* p = new ProphetTLV();
    p->src_               = src;
    p->dst_               = dst;
    p->result_            = (header_result_t) hdr->result;
    p->sender_instance_   = ntohs(hdr->sender_instance);
    p->receiver_instance_ = ntohs(hdr->receiver_instance);
    p->tid_               = ntohl(hdr->transaction_id);
    bool submessage_flag = (hdr->submessage_flag == 0x1);
    u_int16_t submessage_num = ntohs(hdr->submessage_num);

    // prophet tlv fragmentation is not supported by this implementation
    if (submessage_flag == true || submessage_num != 0)
    {
        delete p;
        return NULL;
    }

    // move past header
    p->length_  = ProphetHeaderSize;
    len        -= ProphetHeaderSize;
    bp         += ProphetHeaderSize;

    // begin reading TLVs in from buffer
    BaseTLV::prophet_tlv_t typecode = BaseTLV::UNKNOWN_TLV;
    BaseTLV* tlv = NULL;
    while (len > 0)
    {
        typecode = (BaseTLV::prophet_tlv_t)*bp;
        switch (typecode)
        {
            case BaseTLV::HELLO_TLV:
                tlv = TLVFactory<HelloTLV>::deserialize(bp,len);
                break;
            case BaseTLV::RIBD_TLV:
                tlv = TLVFactory<RIBDTLV>::deserialize(bp,len);
                break;
            case BaseTLV::RIB_TLV:
                tlv = TLVFactory<RIBTLV>::deserialize(bp,len);
                break;
            case BaseTLV::OFFER_TLV:
                tlv = TLVFactory<OfferTLV>::deserialize(bp,len);
                break;
            case BaseTLV::RESPONSE_TLV:
                tlv = TLVFactory<ResponseTLV>::deserialize(bp,len);
                break;
            default:
                tlv = NULL;
                break;
        }
        if (tlv == NULL) break;
        // increments length_ by tlv->length()
        p->add_tlv(tlv);
        // move pointer past the parsed TLV
        bp  += tlv->length();
        len -= tlv->length();
    }
    return p;
}

BaseTLV*
ProphetTLV::get_tlv()
{
    if (list_.empty())
        return NULL;
    BaseTLV* t = list_.front();
    list_.pop_front();
    return t;
}

bool
ProphetTLV::add_tlv(BaseTLV* tlv)
{
    // weed out the oddball
    if (tlv == NULL) return false;

    length_ += tlv->length();
    list_.push_back(tlv);
    return true;
}

}; // namespace prophet
