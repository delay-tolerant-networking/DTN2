
#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleProtocol.h"
#include "debug/Debug.h"
#include "routing/BundleRouter.h"
#include <netinet/in.h>
#include <algorithm>
#include <string>
#include <vector>

typedef std::vector<std::string> stringvector_t;

/**
 * For the region and admin parts of the given tuple, fill in the
 * corresponding index into the string vector.
 */
static inline void
add_to_dict(u_int8_t* id, const BundleTuple& tuple,
            stringvector_t* tuples, size_t* dictlen)
{
    u_int8_t region_id, admin_id;
    stringvector_t::iterator iter;

    // first the region string
    iter = std::find(tuples->begin(), tuples->end(), tuple.region());
    if (iter == tuples->end()) {
        tuples->push_back(tuple.region());
        region_id = tuples->size() - 1;
        *dictlen += (tuple.region().length() + 1);
    } else {
        region_id = iter - tuples->begin();
    }

    // then the admin string
    iter = std::find(tuples->begin(), tuples->end(), tuple.admin());
    if (iter == tuples->end()) {
        tuples->push_back(tuple.admin());
        admin_id = tuples->size() - 1;
        *dictlen += (tuple.admin().length() + 1);
    } else {
        admin_id = iter - tuples->begin();
    }

    // cons up the id field, making sure each id fits in 4 bits
    ASSERT((region_id <= 0xf) && (admin_id <= 0xf));
    *id = ((region_id << 4) | admin_id);
}

size_t
BundleProtocol::fill_header_iov(const Bundle* bundle, struct iovec* iov, int* iovcnt)
{
    // make sure that iovcnt is big enough for:
    // 1) primary header
    // 2) dictionary header and string data
    // 3) payload header
    // 4) payload data
    //
    // optional ones including:
    // 5) fragmentation header
    // 6) authentication header
    ASSERT(*iovcnt >= 6);
    
    // first of all, the primary header
    PrimaryHeader* primary      = (PrimaryHeader*)malloc(sizeof (PrimaryHeader));
    primary->version 		= CURRENT_VERSION;
    primary->class_of_service 	= format_cos(bundle);
    primary->payload_security 	= format_payload_security(bundle);
    primary->creation_ts 	= ((u_int64_t)htonl(bundle->creation_ts_.tv_sec)) << 32;
    primary->creation_ts	|= (u_int64_t)htonl(bundle->creation_ts_.tv_usec);
    primary->expiration 	= htonl(bundle->expiration_);

    // now figure out how many unique tuples there are, storing the
    // unique ones in a temp vector and assigning the corresponding
    // string ids in the primary header. keep track of the total
    // length of the dictionary header in the process
    stringvector_t tuples;
    size_t dictlen = sizeof(DictionaryHeader);
    add_to_dict(&primary->source_id,    bundle->source_,    &tuples, &dictlen);
    add_to_dict(&primary->dest_id,      bundle->dest_,      &tuples, &dictlen);
    add_to_dict(&primary->custodian_id, bundle->custodian_, &tuples, &dictlen);
    add_to_dict(&primary->replyto_id,   bundle->replyto_,   &tuples, &dictlen);
                      
    // now allocate and fill in the dictionary
    DictionaryHeader* dictionary = (DictionaryHeader*)malloc(dictlen);
    int ntuples = tuples.size();
    dictionary->string_count = ntuples;
    
    u_char* records = &dictionary->records[0];
    for (int i = 0; i < ntuples; ++i) {
        *records++ = tuples[i].length();
        memcpy(records, tuples[i].data(), tuples[i].length());
        records += tuples[i].length();
    }
    
    // and last, the payload header. the actual data will immediately
    // follow the payload header in the iovec construction below
    PayloadHeader* payload = (PayloadHeader*)malloc(sizeof(PayloadHeader));
    u_int32_t payloadlen = htonl(bundle->payload_.length());
    memcpy(&payload->length, &payloadlen, 4);
    
    // all done... set up the header type chaining
    primary->next_header_type 	 = DICTIONARY;
    dictionary->next_header_type = PAYLOAD;
    payload->next_header_type    = NONE;
    
    // fill in the iovec
    iov[0].iov_base = primary;
    iov[0].iov_len  = sizeof(PrimaryHeader);
    iov[1].iov_base = dictionary;
    iov[1].iov_len  = dictlen;
    iov[2].iov_base = payload;
    iov[2].iov_len  = sizeof(PayloadHeader);

    // store the count and return the total length
    *iovcnt = 3;
    return iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
}

void
BundleProtocol::free_header_iovmem(const Bundle* bundle, struct iovec* iov, int iovcnt)
{
    free(iov[0].iov_base); // primary header
    free(iov[1].iov_base); // dictionary header
    free(iov[2].iov_base); // payload header
}

int
BundleProtocol::parse_headers(Bundle* bundle, u_char* buf, size_t len)
{
    static const char* log = "/bundle/protocol";
    size_t origlen = len;

    // always starts with the primary header
    if (len < sizeof(PrimaryHeader)) {
 tooshort:
        logf(log, LOG_ERR, "bundle too short (length %d)", len);
        return -1;
    }
    PrimaryHeader* primary = (PrimaryHeader*)buf;
    buf += sizeof(PrimaryHeader);
    len -= sizeof(PrimaryHeader);

    if (primary->version != CURRENT_VERSION) {
        logf(log, LOG_WARN, "protocol version mismatch %d != %d",
             primary->version, CURRENT_VERSION);
        return -1;
    }

    // now pull out the fields from the primary header
    parse_cos(bundle, primary->class_of_service);
    parse_payload_security(bundle, primary->payload_security);
    bundle->creation_ts_.tv_sec = ntohl(primary->creation_ts >> 32);
    bundle->creation_ts_.tv_usec = ntohl((u_int32_t)primary->creation_ts);
    bundle->expiration_  = ntohl(primary->expiration);

    // the dictionary header should follow
    if (primary->next_header_type != DICTIONARY) {
        logf(log, LOG_ERR, "dictionary header doesn't follow primary");
        return -1;
    }

    if (len < sizeof(DictionaryHeader)) {
        goto tooshort;
    }

    // grab the header and advance the pointers
    DictionaryHeader* dictionary = (DictionaryHeader*)buf;
    buf += sizeof(DictionaryHeader);
    len -= sizeof(DictionaryHeader);

    // pull out the tuple data into a local temporary array (there can
    // be at most 8 entries)
    u_char* tupledata[8];
    size_t  tuplelen[8];
    
    for (int i = 0; i < dictionary->string_count; ++i) {
        if (len < 1)
            goto tooshort;
        
        tuplelen[i] = *buf++;
        len--;
        
        if (len < tuplelen[i])
            goto tooshort;
        
        tupledata[i] = buf;
        buf += tuplelen[i];
        len -= tuplelen[i];
    }

    // now, pull out the tuples
    u_int8_t region_id, admin_id;
    
#define EXTRACT_DICTIONARY_TUPLE(what_)                                 \
    region_id = primary->what_##id >> 4;                                \
    admin_id  = primary->what_##id & 0xf;                               \
    ASSERT((region_id <= 0xf) && (admin_id <= 0xf));                    \
                                                                        \
    bundle->what_.set_tuple(tupledata[region_id], tuplelen[region_id],  \
                            tupledata[admin_id],  tuplelen[admin_id]);  \
                                                                        \
    if (! bundle->what_.valid()) {                                      \
        logf(log, LOG_ERR, "invalid %s tuple '%s'", #what_,             \
             bundle->what_.c_str());                                    \
        return -1;                                                      \
    }                                                                   \
    logf(log, LOG_DEBUG, "parsed %s tuple (ids %d, %d) %s", #what_,     \
         region_id, admin_id, bundle->what_.c_str());                   \

    EXTRACT_DICTIONARY_TUPLE(source_);
    EXTRACT_DICTIONARY_TUPLE(dest_);
    EXTRACT_DICTIONARY_TUPLE(custodian_);
    EXTRACT_DICTIONARY_TUPLE(replyto_);
    
    // next should be the payload header
    if (dictionary->next_header_type != PAYLOAD) {
        logf(log, LOG_ERR, "payload header doesn't follow dictionary");
        return -1;
    }
    
    if (len < sizeof(PayloadHeader)) {
        goto tooshort;
    }

    PayloadHeader* payload = (PayloadHeader*)buf;
    buf += sizeof(PayloadHeader);
    len -= sizeof(PayloadHeader);

    u_int32_t payloadlen;
    memcpy(&payloadlen, &payload->length, 4);
    bundle->payload_.set_length(ntohl(payloadlen));

    logf(log, LOG_DEBUG, "parsed payload length %d", bundle->payload_.length());
    
    // that's all we parse, return the amount we consumed
    return origlen - len;
}

u_int8_t
BundleProtocol::format_cos(const Bundle* b)
{
    u_int8_t cos = 0;

    if (b->custreq_)
        cos |= (1 << 7);

    cos |= ((b->priority_ & 0x7) << 4);
    
    if (b->return_rcpt_)
        cos |= (1 << 3);

    if (b->fwd_rcpt_)
        cos |= (1 << 2);

    if (b->recv_rcpt_)
        cos |= (1 << 1);

    if (b->custody_rcpt_)
        cos |= 1;

    return cos;
}

void
BundleProtocol::parse_cos(Bundle* b, u_int8_t cos)
{
    if (cos & (1 << 7))
        b->custreq_ = true;

    b->priority_ = ((cos >> 4) & 0x7);

    if (cos & (1 << 3))
        b->return_rcpt_ = true;

    if (cos & (1 << 2))
        b->fwd_rcpt_ = true;

    if (cos & (1 << 1))
        b->recv_rcpt_ = true;
    
    if (cos & 1)
        b->custody_rcpt_ = true;
}

u_int8_t
BundleProtocol::format_payload_security(const Bundle* bundle)
{
    // TBD
    return 0;
}

void
BundleProtocol::parse_payload_security(Bundle* bundle,
                                       u_int8_t payload_security)
{
}
