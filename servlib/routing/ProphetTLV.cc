/*
 *    Copyright 2006 Baylor University
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

#include "ProphetTLV.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

void
BaseTLV::dump(oasys::StringBuffer* buf)
{
    buf->appendf("-+-+-+-\nTLV Type: %s\n" 
                 "Flags: %d\n"
                 "Length: %d\n",
                 Prophet::tlv_to_str(typecode_),flags_,length_);
}

void
HelloTLV::dump(oasys::StringBuffer* buf)
{
    BaseTLV::dump(buf);
    buf->appendf("HF: %s\n"
                 "Timer: %d\n"
                 "Sender: %s\n",
                 Prophet::hf_to_str(hf_),timer_,sender_.c_str());
}

void
RIBDTLV::dump(oasys::StringBuffer* buf)
{
    BaseTLV::dump(buf);
    buf->appendf("RIBD Entries: %zu\n",ribd_.size());
    // ProphetDictionary
    ribd_.dump(buf);
}

void
RIBTLV::dump(oasys::StringBuffer* buf)
{
    BaseTLV::dump(buf);
    buf->appendf("Relay: %s\n"
                 "Custody: %s\n"
                 "Internet GW: %s\n"
                 "RIB Entries: %zu\n",
                 relay_ ? "true" : "false",
                 custody_ ? "true" : "false",
                 internet_ ? "true" : "false",
                 nodes_.size());
    for(PointerList<RIBNode>::iterator i = nodes_.begin();
        i != nodes_.end();
        i++)
    {
        (*i)->dump(buf);
    }
}

void
BundleTLV::dump(oasys::StringBuffer* buf)
{
    BaseTLV::dump(buf);
    buf->appendf("Type: %s\n"
                 "Bundle Entries: %zu\n",
                 BundleOffer::type_to_str(list_.type()),
                 list_.size());
    // BundleOfferList
    list_.dump(buf);
}

void
ProphetTLV::dump(oasys::StringBuffer* buf)
{
    buf->appendf("ProphetTLV Header\n-----------------\n" 
                 "Result: %s\n"
                 "Sender: %d\n"
                 "Receiver: %d\n"
                 "TransactionID: %d\n"
                 "Length: %d\n"
                 "Entries: %zu\n",
                 Prophet::result_to_str(result_),
                 sender_instance_,
                 receiver_instance_,
                 tid_,
                 parsedlen_,
                 num_tlv());
    for(iterator i = list_.begin(); i != list_.end(); i++)
    {
        switch((*i)->typecode())
        {
            case Prophet::HELLO_TLV:
            {
                HelloTLV* p = dynamic_cast<HelloTLV*>(*i);
                p->dump(buf);
                break;
            }
            case Prophet::RIBD_TLV:
            {
                RIBDTLV* p = dynamic_cast<RIBDTLV*>(*i);
                p->dump(buf);
                break;
            }
            case Prophet::RIB_TLV:
            {
                RIBTLV* p = dynamic_cast<RIBTLV*>(*i);
                p->dump(buf);
                break;
            }
            case Prophet::BUNDLE_TLV:
            {
                BundleTLV* p = dynamic_cast<BundleTLV*>(*i);
                p->dump(buf);
                break;
            }
            case Prophet::UNKNOWN_TLV:
            default:
                buf->appendf("Unknown TLV\n");
                break;
        }
    }
    buf->appendf("\n");
}

bool
HelloTLV::deserialize(u_char* buffer, size_t len)
{
    size_t hdrsz = Prophet::HelloTLVHeaderSize;
    Prophet::HelloTLVHeader *hdr = (Prophet::HelloTLVHeader*) buffer;

    // shouldn't be here if not HELLO!!
    if (hdr->type != Prophet::HELLO_TLV) {
        log_err("looking for TLV type %u but got %u",
                Prophet::HELLO_TLV,hdr->type);
        return false;
    }

    // need at lease header size to parse out this TLV
    if (len < hdrsz)
        return false;

    // something's wrong if TLV shows greater length than inbound buffer
    size_t hello_len = ntohs(hdr->length);
    if (len < hello_len)
        return false;

    // don't want to read past the end of the buffer
    if (hello_len < hdr->name_length)
        return false;

    hf_     = (Prophet::hello_hf_t) hdr->HF;
    length_ = hello_len;
    timer_  = hdr->timer;

    std::string name((char*)&hdr->sender_name[0],(int)hdr->name_length);
    sender_.assign(name);

    return true;
}

size_t
RIBDTLV::read_ras_entry(u_int16_t* sid,
                        EndpointID* eid,
                        u_char* buffer,
                        size_t len)
{
    size_t ras_sz = Prophet::RoutingAddressStringSize;
    if (len <= ras_sz)
        return 0;
    Prophet::RoutingAddressString* ras =
        (Prophet::RoutingAddressString*) buffer;
    size_t retval = ras_sz;
    ASSERT(sid != NULL);
    *sid = ntohs(ras->string_id);
    ASSERT(*sid != ProphetDictionary::INVALID_SID);
    // must be even multiple of 4 bytes
    size_t copylen = FOUR_BYTE_ALIGN(ras->length);
    if (len - retval >= copylen) {
        std::string eidstr((char*)&ras->ra_string[0],ras->length);
        ASSERT(eid != NULL);
        eid->assign(eidstr);
        ASSERT(eid->equals(EndpointID::NULL_EID())==false);
        retval += copylen;
    }
    log_debug("read_ras_entry: read %zu bytes from %zu byte buffer",
              retval,len);
    return retval;
}

bool
RIBDTLV::deserialize(u_char* buffer, size_t len)
{
    Prophet::RIBDTLVHeader* hdr = (Prophet::RIBDTLVHeader*) buffer;
    if (hdr->type != typecode_) {
        log_err("looking for TLV type %u but got %u",
                typecode_,hdr->type);
        return false;
    }

    if (len < Prophet::RIBDTLVHeaderSize)
        return false;
    
    length_ = ntohs(hdr->length);

    if (len < length_)
        return false;

    flags_  = hdr->flags;

    size_t ribd_entry_count = ntohs(hdr->entry_count);
    u_char* bp = buffer + Prophet::RIBDTLVHeaderSize;

    size_t amt_read = Prophet::RIBDTLVHeaderSize;
    len -= Prophet::RIBDTLVHeaderSize;

    u_int16_t sid;
    EndpointID eid;
    ASSERT(ribd_.size() == 0);
    while (ribd_entry_count-- > 0) {

        // deserialize RAS from buffer
        size_t bytes_read = read_ras_entry(&sid,&eid,bp,len);
        if (bytes_read == 0) {
            // log an error?
            break;
        }

        // store this dictionary entry
        if(ribd_.assign(eid,sid) == false) {
            // log an error
            break;
        }

        len      -= bytes_read;
        bp       += bytes_read;
        amt_read += bytes_read;
    }

    return (amt_read == length_);
}

size_t
RIBTLV::read_rib_entry(u_int16_t* sid, double* pvalue, bool* relay,
                       bool* custody, bool* internet,
                       u_char* buffer, size_t len)
{
    size_t rib_sz = Prophet::RIBEntrySize;
    if (len < rib_sz)
        return 0;
    Prophet::RIBEntry* rib = (Prophet::RIBEntry*) buffer;
    ASSERT(sid != NULL);
    *sid = ntohs(rib->string_id);
    ASSERT(pvalue != NULL);
    *pvalue   = ((rib->pvalue & 0xff) + 0.0) / (256.0);
    ASSERT(relay != NULL);
    *relay    = ((rib->flags & Prophet::RELAY_NODE) ==
                               Prophet::RELAY_NODE);
    ASSERT(custody != NULL);
    *custody  = ((rib->flags & Prophet::CUSTODY_NODE) ==
                               Prophet::CUSTODY_NODE);
    ASSERT(internet != NULL);
    *internet = ((rib->flags & Prophet::INTERNET_GW_NODE) ==
                               Prophet::INTERNET_GW_NODE);
    log_debug("read_rib_entry: read %zu bytes from %zu byte buffer",
              rib_sz,len);
    return rib_sz;
}

bool
RIBTLV::deserialize(u_char* buffer, size_t len)
{
    size_t hdrsz = Prophet::RIBTLVHeaderSize;
    Prophet::RIBTLVHeader* hdr = (Prophet::RIBTLVHeader*) buffer;
    if (hdr->type != typecode_) {
        log_err("looking for TLV type %u but got %u",
                typecode_,hdr->type);
        return 0;
    }

    if (len < hdrsz)
        return 0;

    length_ = ntohs(hdr->length);

    if (len < length_)
        return 0;

    flags_  = hdr->flags;

    size_t rib_entry_count = ntohs(hdr->rib_string_count);
    u_char* bp = buffer + hdrsz;

    size_t amt_read = hdrsz;
    len -= hdrsz;

    // local represents our copy of ProphetNode 
    // remote represents the remote copy (from the TLV)
    size_t ribsz = Prophet::RIBEntrySize;
    while (rib_entry_count-- > 0) {

        RIBNode remote; 
        u_int16_t sid = 0;
        double pvalue = 0;
        bool relay = false;
        bool custody = false;
        bool internet = false;

        // deserialize RIBEntry from buffer
        size_t bytes_read = read_rib_entry(&sid, &pvalue, &relay,
                                           &custody, &internet, bp, len);
        if (bytes_read != ribsz) {
            // log an error
            break;
        }

        remote.set_pvalue(pvalue);
        remote.set_relay(relay);
        remote.set_custody(custody);
        remote.set_internet_gw(internet);
        remote.sid_ = sid;

        // store this RIBEntry in nodes_
        nodes_.push_back(new RIBNode(remote));

        len      -= bytes_read;
        bp       += bytes_read;
        amt_read += bytes_read; 
    }

    return (amt_read == length_);
}

size_t
BundleTLV::read_bundle_offer(u_int32_t *cts, u_int32_t *seq,
                             u_int16_t *sid,
                             bool *custody, bool *accept, bool *ack,
                             BundleOffer::bundle_offer_t *type,
                             u_char* bp, size_t len)
{
    size_t boe_sz = Prophet::BundleOfferEntrySize;
    if (len < boe_sz) {
        log_debug("not enough buffer to parse in Bundle entry, "
                  "needed %zu but got %zu",boe_sz,len);
        return 0;
    }
    Prophet::BundleOfferEntry* p =
        (Prophet::BundleOfferEntry*) bp;

    u_int8_t offer_mask =
        (Prophet::CUSTODY_OFFERED | Prophet::PROPHET_ACK) & 0xff;

    u_int8_t response_mask =
        (Prophet::CUSTODY_ACCEPTED | Prophet::BUNDLE_ACCEPTED) & 0xff; 

    u_int8_t flags = p->b_flags & 0xff; // mask out one byte

    // infer whether Bundle offer or Bundle response or error
    if ((flags & offer_mask) == flags)
    {
        ASSERT (type != NULL);
        *type = BundleOffer::OFFER;
        ASSERT (custody != NULL);
        *custody = ((flags & Prophet::CUSTODY_OFFERED) == 
                            Prophet::CUSTODY_OFFERED);
        ASSERT (ack != NULL);
        *ack = ((flags & Prophet::PROPHET_ACK) ==
                         Prophet::PROPHET_ACK);
    }
    else
    if ((flags & response_mask) == flags)
    {
        ASSERT (type != NULL);
        *type = BundleOffer::RESPONSE;
        ASSERT (custody != NULL);
        *custody = ((flags & Prophet::CUSTODY_ACCEPTED) == 
                             Prophet::CUSTODY_ACCEPTED);
        ASSERT (accept != NULL);
        *accept = ((flags & Prophet::BUNDLE_ACCEPTED) ==
                            Prophet::BUNDLE_ACCEPTED);
    }
    else
    {
        ASSERT (type != NULL);
        *type = BundleOffer::UNDEFINED;
        log_debug("illegal flag on Bundle entry: %x",flags);
        return 0;
    }

    ASSERT (sid != NULL);
    *sid = ntohs(p->dest_string_id);
    ASSERT (cts != NULL);
    *cts = ntohl(p->creation_timestamp);
    ASSERT (seq != NULL);
    *seq = ntohl(p->seqno);
    return boe_sz;
}

bool
BundleTLV::deserialize(u_char* buffer, size_t len)
{
    Prophet::BundleOfferTLVHeader* hdr = 
        (Prophet::BundleOfferTLVHeader*) buffer;

    size_t hdrlen = Prophet::BundleOfferTLVHeaderSize;
    size_t amt_read = 0;
    if (hdr->type != typecode_) {
        log_debug("deserialize: looking for TLV type %u but got %u",
                  typecode_,hdr->type);
        return false;
    }

    if (len < hdrlen) {
        return false;
    }

    length_ = ntohs(hdr->length);

    if (len < length_) {
        return false;
    }

    flags_  = hdr->flags;

    size_t offer_count = ntohs(hdr->offer_count);

    buffer     += hdrlen;
    len        -= hdrlen;
    amt_read   += hdrlen;

    ASSERT(list_.empty());
    ASSERT(list_.type() == BundleOffer::UNDEFINED);

    size_t entrylen = Prophet::BundleOfferEntrySize;
    while (len >= entrylen &&
           amt_read + entrylen <= length_ &&
           offer_count-- > 0)
    {
        u_int32_t cts = 0;
        u_int32_t seq = 0;
        u_int16_t sid = 0;
        bool custody = false,
             accept = false,
             ack = false;
        BundleOffer::bundle_offer_t type = BundleOffer::UNDEFINED;

        if (read_bundle_offer(&cts,&seq,&sid,&custody,&accept,&ack,&type,
                    buffer,len) == 0)
            break;

        // initialize to inferred type, if not yet defined
        if (list_.type() == BundleOffer::UNDEFINED) { 
            ASSERT(list_.empty());
            list_.set_type(type);
        }

        ASSERTF(list_.type() == type,"\n%s != %s\n"
                "cts %d\n"
                "seq %d\n"
                "sid %d\n"
                "custody %s\n"
                "accept %s\n"
                "ack %s\n"
                "offer_count %d\n",
                BundleOffer::type_to_str(list_.type()),
                BundleOffer::type_to_str(type),
                cts,seq,sid,
                custody ? "true" : "false",
                accept ? "true" : "false",
                ack ? "true" : "false",
                ntohs(hdr->offer_count));

        list_.add_offer(cts,seq,sid,custody,accept,ack);

        len        -= entrylen;
        amt_read   += entrylen;
        buffer     += entrylen;
    }

    return (amt_read == length_);
}

ProphetTLV::ProphetTLV(const char* logpath) :
    oasys::Logger("ProphetTLV",logpath),
    result_(Prophet::UnknownResult),
    sender_instance_(0),
    receiver_instance_(0),
    tid_(0),
    parsedlen_(0)
{
    list_.clear();
}

ProphetTLV::ProphetTLV(Prophet::header_result_t result,
                       u_int16_t local_instance,
                       u_int16_t remote_instance,
                       u_int32_t tid,
                       const char* logpath) :
    oasys::Logger("ProphetTLV",logpath),
    result_(result),
    sender_instance_(local_instance),
    receiver_instance_(remote_instance),
    tid_(tid),
    parsedlen_(Prophet::ProphetHeaderSize)
{
    ASSERTF(local_instance != 0, "0 is not a valid sender_instance id");
    list_.clear();
}

ProphetTLV::~ProphetTLV()
{
    for(iterator i = list_.begin();
        i != list_.end();
        i++)
    {
        BaseTLV* b = *i;
        switch(b->typecode()) {
            case Prophet::HELLO_TLV:
                delete (HelloTLV*) b;
                break;
            case Prophet::RIBD_TLV:
                delete (RIBDTLV*) b;
                break;
            case Prophet::RIB_TLV:
                delete (RIBTLV*) b;
                break;
            case Prophet::BUNDLE_TLV:
                delete (BundleTLV*) b;
                break;
            case Prophet::UNKNOWN_TLV:
            default:
                PANIC("Unexpected typecode in ~ProphetTLV");
                break;
        }
    }
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

void
ProphetTLV::add_tlv(BaseTLV* tlv)
{
    ASSERT(tlv != NULL);
    parsedlen_ += tlv->length();
    list_.push_back(tlv);
}

ProphetTLV*
ProphetTLV::deserialize(Bundle* b,
                        EndpointID* local,
                        EndpointID* remote,
                        const char* logpath)
{
    // set up the ScratchBuffer
    size_t buflen = b->payload_.length();
    oasys::ScratchBuffer<u_char*> buf(buflen);

    // read out the bundle payload
    u_char* bp = (u_char*)b->payload_.read_data(0, buflen,
                                (u_char*)buf.buf(buflen));

    ProphetTLV* pt = new ProphetTLV(logpath);
    if (local != NULL) local->assign(b->dest_);
    if (remote != NULL) remote->assign(b->source_);

    if ( pt->deserialize(bp,buflen) ) {
        return pt;
    }

    // error in deserializing 
    delete pt;
    return NULL;
}

bool
ProphetTLV::deserialize(u_char* bp, size_t buflen)
{
    // shouldn't get here otherwise
    ASSERT(parsedlen_ == 0);

    if (bp == NULL)
    {
        log_err("can't deserialize with uninitialized pointer");
        return false;
    }

    if (buflen < Prophet::ProphetHeaderSize)
    {
        log_err("not enough buffer to deserialize: got %zu need "
                "more than %zu",buflen,Prophet::ProphetHeaderSize);
        return false;
    }

    Prophet::ProphetHeader* hdr = (Prophet::ProphetHeader*) bp;

    if (hdr->version != Prophet::PROPHET_VERSION)
    {
        log_err("unsupported Prophet version %x",hdr->version);
        return false;
    }

    if (hdr->flags != 0)
    {
        log_err("unsupported Prophet header flags %x",hdr->flags);
        return false;
    }

    if (hdr->code != 0)
    {
        log_err("unsupported Prophet header code %x",hdr->code);
        return false;
    }

    if (ntohs(hdr->length) != buflen)
    {
        log_err("badly formatted Prophet header");
        return false;
    }

    result_            = (Prophet::header_result_t) hdr->result;
    sender_instance_   = ntohs(hdr->sender_instance);
    receiver_instance_ = ntohs(hdr->receiver_instance);
    tid_               = ntohl(hdr->transaction_id);
    bool submessage_flag = (hdr->submessage_flag == 0x1);
    u_int16_t submessage_num = ntohs(hdr->submessage_num);

    if (submessage_flag == true || submessage_num != 0) {
        // log something ... Prophet fragmentation is unsupported
        return false;
    }

    // move past header
    size_t hdrsz = Prophet::ProphetHeaderSize;
    parsedlen_ = hdrsz;
    u_char* p = bp + hdrsz;

    // now peel off however many TLV's
    Prophet::prophet_tlv_t typecode = Prophet::UNKNOWN_TLV;
    size_t len = buflen - hdrsz;
    BaseTLV* tlv = NULL;

    while (len > 0) {
        typecode = (Prophet::prophet_tlv_t)*p;
        switch (typecode) {
            case Prophet::HELLO_TLV:
                tlv = TLVFactory<HelloTLV>::deserialize(p,len,logpath_);
                break;
            case Prophet::RIBD_TLV:
                tlv = TLVFactory<RIBDTLV>::deserialize(p,len,logpath_);
                break;
            case Prophet::RIB_TLV:
                tlv = TLVFactory<RIBTLV>::deserialize(p,len,logpath_);
                break;
            case Prophet::BUNDLE_TLV:
                tlv = TLVFactory<BundleTLV>::deserialize(p,len,logpath_);
                break;
            default:
                break;
        }
        if (tlv == NULL)
            break;
        // increments parsedlen_ by tlv->length()
        add_tlv(tlv);
        // move pointer forward by appropriate length
        p += tlv->length();
        len -= tlv->length();
    }

    return (parsedlen_ == buflen);
}

bool
ProphetTLV::create_bundle(Bundle* b,
                          const EndpointID& local, 
                          const EndpointID& remote) const
{
    // set up the ScratchBuffer
    oasys::ScratchBuffer<u_char*> buf(parsedlen_);

    u_char* bp = buf.buf(parsedlen_);
    if (serialize(bp,parsedlen_) != parsedlen_) {
        return false;
    }

    ASSERT (b != NULL);

    b->source_.assign(local);
    b->dest_.assign(remote);
    b->replyto_.assign(EndpointID::NULL_EID());
    b->custodian_.assign(EndpointID::NULL_EID());
    b->expiration_ = 3600;

    b->payload_.set_data(bp,parsedlen_);

    return true;
}

size_t
ProphetTLV::serialize(u_char* bp, size_t len) const
{
    size_t hdr_sz = Prophet::ProphetHeaderSize;
    size_t buflen = hdr_sz;

    if (len < parsedlen_) {
        // log something
        return 0;
    }

    // skip past the header, for now
    u_char* p = bp + hdr_sz;
    len -= hdr_sz;

    // iterate over the TLVs and serialize each one to the buffer
    for(const_iterator ti = (const_iterator) list_.begin();
        ti != (const_iterator) list_.end() && len > 0;
        ti++)
    {
        BaseTLV* tlv = *ti;
        size_t tlvsize = tlv->serialize(p, len);
        if (tlvsize != tlv->length())
            break;
        buflen += tlvsize;
        len -= tlvsize;
        p += tlvsize;
    }

    // if all were successful, the math should line up
    if(parsedlen_ != buflen) {
        // log something
        return 0;
    }

    // now write out the Prophet header
    Prophet::ProphetHeader* hdr = (Prophet::ProphetHeader*) bp;
    memset(hdr,0,hdr_sz);
    hdr->version           = Prophet::PROPHET_VERSION;
    hdr->flags             = 0; // TBD
    hdr->result            = result_;
    hdr->code              = 0; // TBD
    hdr->sender_instance   = htons(sender_instance_);
    hdr->receiver_instance = htons(receiver_instance_);
    hdr->transaction_id    = htonl(tid_);
    hdr->length            = htons(parsedlen_);

    return parsedlen_;
}

size_t
HelloTLV::serialize(u_char* bp, size_t len)
{
    size_t eidsz = sender_.length();
    size_t hdrsz = Prophet::HelloTLVHeaderSize;

    // align to four-byte boundary
    size_t hello_sz = FOUR_BYTE_ALIGN(hdrsz + eidsz);

    // not enough buffer space to serialize into
    if (len < hello_sz) 
        return 0;

    length_ = hello_sz;

    Prophet::HelloTLVHeader *hdr = (Prophet::HelloTLVHeader*) bp;
    // baseline the header to zero
    memset(bp,0,length_);
    hdr->type = typecode_;
    hdr->HF = hf_;
    hdr->length = htons(length_);
    hdr->timer = timer_;
    // mask out one-byte value
    hdr->name_length = eidsz & 0xff;
    
    size_t buflen = hello_sz;
    memcpy(&hdr->sender_name[0],sender_.c_str(),eidsz);

    // zero out slack in packet
    while(buflen-- > hdrsz + eidsz)
        bp[buflen] = '\0';

    return hello_sz;
}

size_t
RIBDTLV::write_ras_entry(u_int16_t sid,
                         EndpointID eid,
                         u_char* buffer,
                         size_t len)
{
    size_t ras_sz = Prophet::RoutingAddressStringSize;
    if (len <= ras_sz)
        return 0;

    // baseline to zero
    memset(buffer,0,ras_sz);

    Prophet::RoutingAddressString* ras =
        (Prophet::RoutingAddressString*) buffer;

    ASSERT(eid.equals(EndpointID::NULL_EID())==false);
    size_t retval = ras_sz;
    ras->string_id = htons(sid);
    ras->length = eid.length() & 0xff;
    // must be even multiple of 4
    size_t copylen = FOUR_BYTE_ALIGN(ras->length);
    if (len - retval >= copylen) {
        memcpy(&ras->ra_string[0],eid.c_str(),ras->length);
        retval += copylen;
        // zero out slack
        while (--copylen > ras->length)
            ras->ra_string[copylen] = 0;
    }

    log_debug("write_ras_entry: wrote %zu bytes to %zu byte buffer",
              retval,len);
    return retval;
}

size_t
RIBDTLV::serialize(u_char* bp, size_t len)
{
    // estimate size of RIBD
    size_t ribd_sz = ribd_.guess_ribd_size();
    size_t hdrsz = Prophet::RIBDTLVHeaderSize;
    if ( len < ribd_sz + hdrsz) {
        log_err("serialize buffer length too short - needed > %zu, got %zu",
                ribd_sz + hdrsz,len);
        return 0;
    }

    Prophet::RIBDTLVHeader* hdr = (Prophet::RIBDTLVHeader*) bp;
    size_t ribd_entry_count = 0;
    length_ = 0;

    // baseline the header to zero
    memset(hdr,0,hdrsz);

    // advance buffer pointer past the header
    bp += hdrsz;
    length_ += hdrsz;

    // iterate over ProphetDictionary
    for (ProphetDictionary::const_iterator it = ribd_.begin();
         it != ribd_.end();
         it++) {

        if ((*it).first == 0 || (*it).first == 1)
            continue; // local and remote are implied as 0, 1

        size_t bytes_written = 0;
        // serialize node out to buffer
        if ((bytes_written=write_ras_entry((*it).first,
                                           (*it).second,
                                           bp,len)) == 0)
            break;

        bp      += bytes_written;
        len     -= bytes_written;
        length_ += bytes_written;

        ribd_entry_count++;
    }

    // Now that sizes are known, fill in the header
    hdr->type = Prophet::RIBD_TLV;
    hdr->flags = 0;
    hdr->length = htons(length_);
    hdr->entry_count = htons(ribd_entry_count);

    log_debug("serialize wrote %zu of %zu RAS entries for a total %d bytes",
              ribd_entry_count,ribd_.size(),length_);
    return length_;
}

size_t
RIBTLV::write_rib_entry(u_int16_t sid, double pvalue,
                        bool relay, bool custody, bool internet,
                        u_char* buffer, size_t len)
{
    ASSERT(sid != ProphetDictionary::INVALID_SID);
    size_t rib_sz = Prophet::RIBEntrySize;
    if (len < rib_sz)
        return 0;

    // baseline to zero
    memset(buffer,0,rib_sz);

    Prophet::RIBEntry* rib = (Prophet::RIBEntry*) buffer;

    rib->string_id = htons(sid);
    rib->pvalue = (u_int8_t) ( (int) (pvalue * (256.0)) ) & 0xff;
    rib->flags = 0;
    if (relay)    rib->flags |= Prophet::RELAY_NODE;
    if (custody)  rib->flags |= Prophet::CUSTODY_NODE;
    if (internet) rib->flags |= Prophet::INTERNET_GW_NODE;
    log_debug("write_rib_entry: wrote %zu bytes to %zu byte buffer",
              rib_sz,len);
    return rib_sz;
}

size_t
RIBTLV::serialize(u_char* bp, size_t len)
{
    size_t hdrsz = Prophet::RIBTLVHeaderSize;
    size_t rib_entry_count = nodes_.size();
    length_ = hdrsz + rib_entry_count * Prophet::RIBEntrySize;

    // buffer must be large enough to contain header + all RIB entries
    if ( len < length_ ) {
        log_err("serialize buffer length too short - needed %d got %zu",
                length_,len);
        return 0;
    }

    Prophet::RIBTLVHeader* hdr = (Prophet::RIBTLVHeader*) bp;

    // just guesses; zero out and calculate actual values
    length_ = 0;
    rib_entry_count = 0;

    // move buffer pointer past the header; encode each entry
    bp += hdrsz;
    length_ += hdrsz;

    RIBNode *node = NULL;

    for(iterator it = nodes_.begin(); 
        it != nodes_.end();
        it++) {

        node = (*it);

        size_t bytes_written = 0;
        if ((bytes_written=write_rib_entry(node->sid_,
                                           node->p_value(),
                                           node->relay(),
                                           node->custody(),
                                           node->internet_gw(),
                                           bp,
                                           len)) == 0) {
            break;
        }

        bp      += bytes_written;
        len     -= bytes_written;
        length_ += bytes_written;

        rib_entry_count++;

        node = NULL;
    }

    // fill out the header
    hdr->type = typecode_;
    hdr->length = htons(length_);
    hdr->rib_string_count = htons(rib_entry_count);
    hdr->flags = 0;

    if (relay_)    hdr->flags |= Prophet::RELAY_NODE;
    if (custody_)  hdr->flags |= Prophet::CUSTODY_NODE;
    if (internet_) hdr->flags |= Prophet::INTERNET_GW_NODE;

    return length_;
}

size_t
BundleTLV::write_bundle_offer(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                              bool custody, bool accept, bool ack,
                              BundleOffer::bundle_offer_t type,
                              u_char* bp, size_t len)
{
    size_t boe_sz = Prophet::BundleOfferEntrySize;
    if (len < boe_sz) {
        log_debug("not enough buffer to write out Bundle entry, "
                  "needed %zu but got %zu",boe_sz,len);
        return 0;
    }
    Prophet::BundleOfferEntry* p = (Prophet::BundleOfferEntry*) bp;
    memset(p,0,boe_sz);
    if (type == BundleOffer::OFFER) {
        if (custody)
            p->b_flags |= Prophet::CUSTODY_OFFERED;
        if (ack)
            p->b_flags |= Prophet::PROPHET_ACK;
    }
    else
    if (type == BundleOffer::RESPONSE) {
        if (custody)
            p->b_flags |= Prophet::CUSTODY_ACCEPTED;
        if (accept)
            p->b_flags |= Prophet::BUNDLE_ACCEPTED;
    }
    else 
    {
        log_debug("Bundle entry with undefined type (%d), skipping",
                  (int)type);
        return 0;
    }
    p->dest_string_id = htons(sid);
    p->creation_timestamp = htonl(cts);
    p->seqno = htonl(seq);
    return boe_sz;
}

size_t
BundleTLV::serialize(u_char* bp, size_t len)
{
    // estimate output size
    size_t tlv_size = Prophet::BundleOfferTLVHeaderSize +
                      list_.guess_size();
    length_ = 0;

    // all or nothing
    if(len < tlv_size)
        return 0;

    // fill in the header
    Prophet::BundleOfferTLVHeader* hdr =
        (Prophet::BundleOfferTLVHeader*) bp;
    memset(hdr,0,Prophet::BundleOfferTLVHeaderSize);
    hdr->type = Prophet::BUNDLE_TLV;
    hdr->flags = 0; // TBD
    hdr->offer_count = htons(list_.size());

    length_ += Prophet::BundleOfferTLVHeaderSize;
    bp += Prophet::BundleOfferTLVHeaderSize;
    len -= Prophet::BundleOfferTLVHeaderSize;

    // iterate over the bundle offer and serialize the offer entries
    oasys::ScopeLock l(list_.lock(),"BundleTLV::serialize");
    BundleOfferList::iterator i = list_.begin();
    while (i != list_.end()) {
        BundleOffer* bo = *i;

        size_t boe_sz = write_bundle_offer(bo->creation_ts(),bo->seqno(),
                            bo->sid(),bo->custody(),bo->accept(),bo->ack(),
                            bo->type(),bp,len);

        if (boe_sz == 0)
            break;

        length_ += boe_sz;
        bp += boe_sz;
        len -= boe_sz;
        i++;
    }

    hdr->length = htons(length_);
    return length_; 
}

} // namespace dtn
