
#include "Bundle.h"
#include "BundleProtocol.h"
#include "debug/Debug.h"
#include "util/StringUtils.h"
#include <netinet/in.h>
#include <algorithm>

/**
 * For the region and admin parts of the given tuple, fill in the
 * corresponding index into the string vector.
 */
static inline void
add_to_dict(u_int8_t* id, const BundleTuple& tuple,
            StringVector* tuples, size_t* dictlen)
{
    u_int8_t region_id, admin_id;
    StringVector::iterator iter;

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

/**
 * Fill in the given iovec with the formatted bundle header.
 *
 * @return the total length of the header or -1 on error
 */
size_t
BundleProtocol::format_headers(const Bundle* bundle,
                               struct iovec* iov, int* iovcnt)
{
    // make sure that iovcnt is big enough for:
    // 1) primary header
    // 2) dictionary header and string data
    // 3) payload header
    //
    // optional ones including:
    // 4) fragmentation header
    // 5) authentication header
    // 6) extension headers (someday)
    ASSERT(*iovcnt >= 6);

    // by definition, all headers have next_header_type as their first
    // byte, so this can be used to set the type field as we progress
    // through building the vector.
    u_int8_t* next_header_type;

    //
    // primary header
    //
    PrimaryHeader* primary      = (PrimaryHeader*)malloc(sizeof (PrimaryHeader));
    primary->version 		= CURRENT_VERSION;
    primary->class_of_service 	= format_cos(bundle);
    primary->payload_security 	= format_payload_security(bundle);
    primary->creation_ts 	= ((u_int64_t)htonl(bundle->creation_ts_.tv_sec)) << 32;
    primary->creation_ts	|= (u_int64_t)htonl(bundle->creation_ts_.tv_usec);
    primary->expiration 	= htonl(bundle->expiration_);

    next_header_type = &primary->next_header_type;
    iov[0].iov_base = primary;
    iov[0].iov_len  = sizeof(PrimaryHeader);
    *iovcnt = 1;

    // now figure out how many unique tuples there are, storing the
    // unique ones in a temp vector and assigning the corresponding
    // string ids in the primary header. keep track of the total
    // length of the dictionary header in the process
    StringVector tuples;
    size_t dictlen = sizeof(DictionaryHeader);
    add_to_dict(&primary->source_id,    bundle->source_,    &tuples, &dictlen);
    add_to_dict(&primary->dest_id,      bundle->dest_,      &tuples, &dictlen);
    add_to_dict(&primary->custodian_id, bundle->custodian_, &tuples, &dictlen);
    add_to_dict(&primary->replyto_id,   bundle->replyto_,   &tuples, &dictlen);
                      
    //
    // dictionary header (must follow the primary)
    //
    DictionaryHeader* dictionary = (DictionaryHeader*)malloc(dictlen);
    int ntuples = tuples.size();
    dictionary->string_count = ntuples;
    
    u_char* records = &dictionary->records[0];
    for (int i = 0; i < ntuples; ++i) {
        *records++ = tuples[i].length();
        memcpy(records, tuples[i].data(), tuples[i].length());
        records += tuples[i].length();
    }

    *next_header_type = DICTIONARY;
    next_header_type = &dictionary->next_header_type;
    iov[*iovcnt].iov_base = dictionary;
    iov[*iovcnt].iov_len  = dictlen;
    (*iovcnt)++;

    //
    // fragment header (if it's a fragment)
    //
    if (bundle->is_fragment_) {
        FragmentHeader* fragment = (FragmentHeader*)malloc(sizeof(FragmentHeader));
        u_int32_t orig_length = htonl(bundle->orig_length_);
        u_int32_t frag_offset = htonl(bundle->frag_offset_);
        memcpy(&fragment->orig_length, &orig_length, sizeof(orig_length));
        memcpy(&fragment->frag_offset, &frag_offset, sizeof(frag_offset));

        *next_header_type = FRAGMENT;
        next_header_type = &fragment->next_header_type;
        iov[*iovcnt].iov_base = fragment;
        iov[*iovcnt].iov_len  = sizeof(FragmentHeader);
        (*iovcnt)++;
    }

    //
    // payload header (must be last)
    //
    PayloadHeader* payload = (PayloadHeader*)malloc(sizeof(PayloadHeader));
    u_int32_t payloadlen = htonl(bundle->payload_.length());
    memcpy(&payload->length, &payloadlen, 4);
    
    *next_header_type = PAYLOAD;
    payload->next_header_type = NONE;
    iov[*iovcnt].iov_base = payload;
    iov[*iovcnt].iov_len  = sizeof(PayloadHeader);
    (*iovcnt)++;

    // calculate the total length and then we're done
    int total_len = 0;
    for (int i = 0; i < *iovcnt; i++) {
        total_len += iov[i].iov_len;
    }
    return total_len;
}

void
BundleProtocol::free_header_iovmem(const Bundle* bundle, struct iovec* iov, int iovcnt)
{
    for (int i = 0; i < iovcnt; ++i) {
        free(iov[i].iov_base);
    }
}

int
BundleProtocol::parse_headers(Bundle* bundle, u_char* buf, size_t len)
{
    static const char* log = "/bundle/protocol";
    size_t origlen = len;
    u_int8_t next_header_type;

    //
    // primary header
    //
    PrimaryHeader* primary;
    if (len < sizeof(PrimaryHeader)) {
 tooshort:
        logf(log, LOG_ERR, "bundle too short (length %d)", len);
        return -1;
    }
    
    primary = (PrimaryHeader*)buf;
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

    //
    // dictionary header (must follow primary)
    //
    DictionaryHeader* dictionary;
    if (primary->next_header_type != DICTIONARY) {
        logf(log, LOG_ERR, "dictionary header doesn't follow primary");
        return -1;
    }

    if (len < sizeof(DictionaryHeader)) {
        goto tooshort;
    }

    // grab the header and advance the pointers
    dictionary = (DictionaryHeader*)buf;
    next_header_type = dictionary->next_header_type;
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
    bundle->what_.assign(tupledata[region_id], tuplelen[region_id],     \
                         tupledata[admin_id],  tuplelen[admin_id]);     \
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

    // start a loop until we've consumed all the other headers
    while (next_header_type != NONE) {
        switch (next_header_type) {
        case PRIMARY:
            logf(log, LOG_ERR, "found a second primary header");
            return -1;

        case DICTIONARY:
            logf(log, LOG_ERR, "found a second dictionary header");
            return -1;

        case FRAGMENT: {
            u_int32_t orig_length, frag_offset;
            
            if (len < sizeof(FragmentHeader)) {
                goto tooshort;
            }
            
            FragmentHeader* fragment = (FragmentHeader*)buf;
            buf += sizeof(FragmentHeader);
            len -= sizeof(FragmentHeader);
            next_header_type = fragment->next_header_type;
            
            bundle->is_fragment_ = true;
            memcpy(&orig_length, &fragment->orig_length, sizeof(orig_length));
            memcpy(&frag_offset, &fragment->frag_offset, sizeof(frag_offset));
            bundle->orig_length_ = htonl(orig_length);
            bundle->frag_offset_ = htonl(frag_offset);
            break;
        }

        case PAYLOAD: {
            u_int32_t payload_len;
            
            if (len < sizeof(PayloadHeader)) {
                goto tooshort;
            }

            PayloadHeader* payload = (PayloadHeader*)buf;
            buf += sizeof(PayloadHeader);
            len -= sizeof(PayloadHeader);

            if (payload->next_header_type != NONE) {
                logf(log, LOG_ERR, "payload header must be last (next type %d)",
                     payload->next_header_type);
                return -1;
            }

            next_header_type = payload->next_header_type;
            
            memcpy(&payload_len, &payload->length, 4);
            bundle->payload_.set_length(ntohl(payload_len));

            logf(log, LOG_DEBUG, "parsed payload length %d",
                 bundle->payload_.length());
            break;
        }

        default:
            logf(log, LOG_ERR, "unknown header type %d", next_header_type);
            return -1;
        }
    }
    
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
