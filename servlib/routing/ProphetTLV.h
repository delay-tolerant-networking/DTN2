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

#ifndef _DTN_PROPHET_TLV_
#define _DTN_PROPHET_TLV_

#include <oasys/debug/Log.h>
#include "naming/EndpointID.h"
#include <sys/types.h> 
#include <netinet/in.h>

#include <vector>

#include "routing/Prophet.h"
#include "routing/ProphetNode.h"
#include "routing/ProphetLists.h"

#include <oasys/util/ScratchBuffer.h>

#define LOGPATH "/dtn/router/prophet/tlv"

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 */

namespace dtn {

template<class TLV>
struct TLVFactory
{
    static TLV* deserialize(u_char* bp,size_t len,const char* logpath)
    {
        TLV* t = new TLV(logpath);
        if (t->deserialize(bp,len))
            return t;
        delete t;
        return NULL;
    }
}; // TLVFactory template

class BaseTLV : public oasys::Logger
{
public:
    virtual ~BaseTLV() {}

    Prophet::prophet_tlv_t typecode() { return typecode_; }
    u_int8_t flags() { return flags_; }
    u_int16_t length() { return length_; }

    virtual size_t serialize(u_char* bp,size_t len) = 0;

protected:
    BaseTLV(Prophet::prophet_tlv_t typecode = Prophet::UNKNOWN_TLV,
            const char* logpath = LOGPATH) :
        oasys::Logger("TLVParser",logpath),
        typecode_(typecode),
        flags_(0),
        length_(0)
    {} 

    virtual bool deserialize(u_char* bp,size_t len) = 0;

    Prophet::prophet_tlv_t typecode_;
    u_int8_t flags_;
    u_int16_t length_;
}; // BaseTLV template

class HelloTLV : public BaseTLV
{
public:
    HelloTLV(Prophet::hello_hf_t hf,
             u_int8_t timer,
             const EndpointID& eid,
             const char* logpath) :
        BaseTLV(Prophet::HELLO_TLV,logpath),
        hf_(hf), timer_(timer), sender_(eid)
    {
        length_ = Prophet::HelloTLVHeaderSize + eid.length();
        length_ = FOUR_BYTE_ALIGN(length_);
    }

    virtual ~HelloTLV() {}

    Prophet::hello_hf_t hf() { return hf_; }
    u_int8_t timer() { return timer_; }
    const EndpointID& sender() { return sender_; }

    size_t serialize(u_char*,size_t);

protected:
    friend class TLVFactory<HelloTLV>;

    HelloTLV(const char* logpath) :
        BaseTLV(Prophet::HELLO_TLV,logpath),
        hf_(Prophet::HF_UNKNOWN),
        timer_(0),
        sender_(EndpointID::NULL_EID())
    {}

    bool deserialize(u_char*,size_t);

    Prophet::hello_hf_t hf_;
    u_int8_t timer_;
    EndpointID sender_;
}; // HelloTLV

class RIBDTLV : public BaseTLV
{
public:
    RIBDTLV(const ProphetDictionary& ribd,
            const char* logpath) :
        BaseTLV(Prophet::RIBD_TLV,logpath),
        ribd_(ribd)
    {
        length_ = Prophet::RIBDTLVHeaderSize;
        length_ += ribd_.guess_ribd_size();
    }
    
    virtual ~RIBDTLV() {}

    const ProphetDictionary& ribd() const { return ribd_; }

    size_t serialize(u_char*,size_t);
protected:
    /**
     * Serialize EndpointID out to buffer size len using
     * RoutingAddressString struct; return value is total bytes written
     */
    size_t write_ras_entry(u_int16_t sid,
                           EndpointID eid,
                           u_char* buffer,
                           size_t len);

    /**
     * Deserialize RoutingAddressString struct from buffer size len into
     * node, setting *sid; return value is total bytes read
     */
    size_t read_ras_entry(u_int16_t* sid,
                          EndpointID* eid,
                          u_char* buffer,
                          size_t len);

    friend class TLVFactory<RIBDTLV>;

    RIBDTLV(const char* logpath) :
        BaseTLV(Prophet::RIBD_TLV,logpath),
        ribd_(EndpointID::NULL_EID(),EndpointID::NULL_EID())
    {}

    bool deserialize(u_char*,size_t);

    ProphetDictionary ribd_;
}; // RIBDTLV

class RIBTLV : public BaseTLV
{
public:
    typedef PointerList<RIBNode> List;
    typedef List::iterator iterator;

    RIBTLV(const List& nodes,
           bool relay,
           bool custody,
           bool internet,
           const char* logpath) :
        BaseTLV(Prophet::RIB_TLV,logpath),
        nodes_(nodes),
        relay_(relay),
        custody_(custody),
        internet_(internet)
    {
        length_ = Prophet::RIBTLVHeaderSize;
        length_ += Prophet::RIBEntrySize * nodes_.size();
    }

    virtual ~RIBTLV() {}

    const List& nodes() { return nodes_; }
    bool relay_node() { return relay_; }
    bool custody_node() { return custody_; }
    bool internet_gateway() { return internet_ ;}

    size_t serialize(u_char*,size_t);
protected:
    friend class TLVFactory<RIBTLV>;

    size_t write_rib_entry(u_int16_t sid,
                           double pvalue,
                           bool relay,
                           bool custody,
                           bool internet,
                           u_char* buffer,
                           size_t len);

    size_t read_rib_entry(u_int16_t* sid,
                          double* pvalue,
                          bool* relay,
                          bool* custody,
                          bool* internet,
                          u_char* buffer,
                          size_t len);

    RIBTLV(const char* logpath) :
        BaseTLV(Prophet::RIB_TLV,logpath),
        relay_(false),
        custody_(false),
        internet_(false)
    {}

    bool deserialize(u_char*,size_t);

    List nodes_;
    bool relay_;
    bool custody_;
    bool internet_;
}; // RIBTLV

class BundleTLV : public BaseTLV
{
public:
    BundleTLV(const BundleOfferList& list,
              const char* logpath) :
        BaseTLV(Prophet::BUNDLE_TLV,logpath),
        list_(list)
    {
        length_ = Prophet::BundleOfferTLVHeaderSize;
        length_ += list_.size() * Prophet::BundleOfferEntrySize;
    }

    virtual ~BundleTLV() { list_.clear(); }

    /**
     * Returns resultant list after serialize/deserialize
     */
    const BundleOfferList& list() { return list_; }

    BundleOffer::bundle_offer_t type() { return list_.type(); }

    size_t serialize(u_char*,size_t);
protected:
    friend class TLVFactory<BundleTLV>;

    size_t read_bundle_offer(u_int32_t *cts, u_int16_t *sid, 
                             bool *custody, bool *accept, bool *ack,
                             BundleOffer::bundle_offer_t *type,
                             u_char* bp, size_t len);

    size_t write_bundle_offer(u_int32_t cts, u_int16_t sid,
                              bool custody, bool accept, bool ack,
                              BundleOffer::bundle_offer_t type,
                              u_char* bp, size_t len);

    BundleTLV(const char* logpath) :
        BaseTLV(Prophet::BUNDLE_TLV,logpath)
    {}

    bool deserialize(u_char*,size_t);

    BundleOfferList list_;
}; // BundleTLV

/**
 * Helper class for serializing/deserializing Prophet control messages
 * to/from Bundles
 */
class ProphetTLV : public oasys::Logger
{
public:
    typedef std::list<BaseTLV*> List;
    typedef List::iterator iterator; 
    typedef List::const_iterator const_iterator; 

    ProphetTLV(Prophet::header_result_t result,
               u_int16_t local_instance,
               u_int16_t remote_instance,
               u_int32_t tid,
               const char* logpath = LOGPATH);

    virtual ~ProphetTLV();

    // caller is responsible for cleaning up Bundle*
    bool create_bundle(Bundle* bundle,
                       const EndpointID& local,
                       const EndpointID& remote) const;

    // caller is responsible for cleaning up ProphetTLV*
    static ProphetTLV* deserialize(Bundle* b,
                                   EndpointID* local = NULL,
                                   EndpointID* remote = NULL,
                                   const char* logpath = LOGPATH);

    Prophet::header_result_t result() { return result_; }
    u_int16_t sender_instance()   { return sender_instance_; }
    u_int16_t receiver_instance() { return receiver_instance_; }
    u_int32_t transaction_id()    { return tid_; }
    u_int16_t length()            { return parsedlen_; }
    size_t num_tlv()              { return list_.size(); }

    // caller is responsible for memory cleanup on returned pointer
    BaseTLV* get_tlv(); 
    // ProphetTLV will clean up memory on submitted pointer
    void add_tlv(BaseTLV* tlv);

    // look but don't touch!
    const List& list() const { return list_; }

#ifndef PROPHET_TLV_TEST
protected:
#endif
    ProphetTLV(const char *logpath = LOGPATH);

    bool deserialize(u_char* bp, size_t buflen);
    size_t serialize(u_char*,size_t) const;

    Prophet::header_result_t result_;
    u_int16_t sender_instance_;
    u_int16_t receiver_instance_;
    u_int32_t tid_;
    u_int16_t parsedlen_;
    List list_;

}; // ProphetTLV

}; // dtn

#endif // _DTN_PROPHET_ENCOUNTER_
