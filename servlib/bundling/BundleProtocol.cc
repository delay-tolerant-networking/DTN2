
#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleProtocol.h"
#include "debug/Debug.h"
#include "routing/BundleRouter.h"
#include <netinet/in.h>

int
BundleProtocol::fill_iov(const Bundle* bundle, struct iovec* iov, int* iovcnt)
{
    // make sure that iovcnt is big enough for:
    // 1) primary header
    // 2) dictionary header and string data
    // 3) payload header
    // 4) payload data
    //
    // optional ones including:
    // 4) fragmentation header
    // 4) authentication header
    ASSERT(*iovcnt >= 4);

    // first of all, the primary header
    PrimaryHeader* primary      = (PrimaryHeader*)malloc(sizeof (PrimaryHeader));
    primary->version 		= CURRENT_VERSION;
    primary->class_of_service 	= format_cos(bundle);
    primary->payload_security 	= format_payload_security(bundle);
    primary->creation_ts 	= ((u_int64_t)htonl(bundle->creation_ts_.tv_sec)) << 32;
    primary->creation_ts	|= (u_int64_t)htonl(bundle->creation_ts_.tv_usec);
    primary->expiration 	= htonl(bundle->expiration_);

    // now figure out how many unique tuples there are, caching
    // pointers to the unique ones in a temp array and assign the
    // string ids in the primary header
    const BundleTuple* tuples[4];
    int ntuples = 1;
    tuples[0] = &bundle->source_;
    primary->source_id = 0;
    
    if (bundle->dest_.equals(bundle->source_)) {
        primary->dest_id = primary->source_id;
    } else {
        tuples[ntuples] = &bundle->dest_;
        primary->dest_id = ntuples;
        ntuples++;
    }

    if (bundle->custodian_.equals(bundle->source_)) {
        primary->custodian_id = primary->source_id;

    } else if (bundle->custodian_.equals(bundle->source_)) {
        primary->custodian_id = primary->source_id;

    } else {
        tuples[ntuples] = &bundle->custodian_;
        primary->custodian_id = ntuples;
        ntuples++;
    }

    if (bundle->replyto_.equals(bundle->source_)) {
        primary->replyto_id = primary->source_id;

    } else if (bundle->replyto_.equals(bundle->source_)) {
        primary->replyto_id = primary->source_id;

    } else if (bundle->replyto_.equals(bundle->custodian_)) {
        primary->replyto_id = primary->custodian_id;

    } else {
        tuples[ntuples] = &bundle->replyto_;
        primary->replyto_id = ntuples;
        ntuples++;
    }

    // now figure out the size of the dictionary
    size_t dictlen = sizeof(DictionaryHeader);
    for (int i = 0; i < ntuples; ++i) {
        dictlen += 1;
        dictlen += tuples[i]->length();
    }

    // and fill in the dictionary
    DictionaryHeader* dictionary = (DictionaryHeader*)malloc(dictlen);
    dictionary->string_count = ntuples;

    u_char* records = &dictionary->records[0];
    for (int i = 0; i < ntuples; ++i) {
        *records++ = tuples[i]->length();
        memcpy(records, tuples[i]->data(), tuples[i]->length());
        records += tuples[i]->length();
    }
    
    // and the payload header
    PayloadHeader* payload = (PayloadHeader*)malloc(sizeof(PayloadHeader));
    payload->length = htonl(bundle->payload_.length());

    // set up the header type chain
    primary->next_header_type 	 = DICTIONARY;
    dictionary->next_header_type = PAYLOAD;
    payload->next_header_type    = NONE;

    // assign the iovec
    iov[0].iov_base = primary;
    iov[0].iov_len  = sizeof(PrimaryHeader);
    iov[1].iov_base = dictionary;
    iov[1].iov_len  = dictlen;
    iov[2].iov_base = payload;
    iov[2].iov_len  = sizeof(PayloadHeader);
    iov[3].iov_base = (void*)bundle->payload_.data();
    iov[3].iov_len  = bundle->payload_.length();

    *iovcnt = 4;

    return iov[0].iov_len + iov[1].iov_len + iov[2].iov_len + iov[3].iov_len;
}

void
BundleProtocol::free_iovmem(const Bundle* bundle, struct iovec* iov, int iovcnt)
{
    free(iov[0].iov_base); // primary header
    free(iov[1].iov_base); // dictionary header
    free(iov[2].iov_base); // payload header
}

bool
BundleProtocol::process_buf(u_char* buf, size_t len, Bundle** bundlep)
{
    static const char* log = "/bundle/protocol";

    Bundle* bundle = new Bundle();
    
    // start off with the primary header
    if (len < sizeof(PrimaryHeader)) {
 tooshort:
        logf(log, LOG_ERR, "bundle too short (length %d)", len);
 bail:
        delete bundle;
        return false;
    }
    PrimaryHeader* primary = (PrimaryHeader*)buf;
    buf += sizeof(PrimaryHeader);
    len -= sizeof(PrimaryHeader);

    if (primary->version != CURRENT_VERSION) {
        logf(log, LOG_WARN, "protocol version mismatch %d != %d",
             primary->version, CURRENT_VERSION);
        goto bail;
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
        goto bail;
    }

    if (len < sizeof(DictionaryHeader)) {
        goto tooshort;
    }

    // skip past the header itself
    DictionaryHeader* dictionary = (DictionaryHeader*)buf;
    buf += sizeof(DictionaryHeader);
    len -= sizeof(DictionaryHeader);

    // pull out the tuple data into local temp arrays
    u_char* tupledata[4];
    size_t  tuplelen[4];
    for (int i = 0; i < dictionary->string_count; ++i) {
        tuplelen[i] = *buf;
        if (len < tuplelen[i])
            goto tooshort;

        tupledata[i] = &buf[1];
        buf += tuplelen[i] + 1;
        len -= tuplelen[i] + 1;
    }

    // parse the source tuple
    bundle->source_.set_tuple(tupledata[primary->source_id],
                              tuplelen[primary->source_id]);
    if (!bundle->source_.valid()) {
        logf(log, LOG_ERR, "invalid tuple '%s'", bundle->source_.c_str());
        goto bail;
    }
    logf(log, LOG_DEBUG, "parsed source tuple (id %d) %s",
         primary->source_id, bundle->source_.c_str());
    
    // parse the dest tuple
    bundle->dest_.set_tuple(tupledata[primary->dest_id],
                            tuplelen[primary->dest_id]);
    if (!bundle->dest_.valid()) {
        logf(log, LOG_ERR, "invalid tuple '%s'", bundle->dest_.c_str());
        goto bail;
    }
    logf(log, LOG_DEBUG, "parsed dest tuple (id %d) %s",
         primary->dest_id, bundle->dest_.c_str());
    
    // parse the replyto tuple
    bundle->replyto_.set_tuple(tupledata[primary->replyto_id],
                               tuplelen[primary->replyto_id]);
    if (!bundle->replyto_.valid()) {
        logf(log, LOG_ERR, "invalid tuple '%s'", bundle->replyto_.c_str());
        goto bail;
    }
    logf(log, LOG_DEBUG, "parsed replyto tuple (id %d) %s",
         primary->replyto_id, bundle->replyto_.c_str());
    
    // parse the custodian tuple
    bundle->custodian_.set_tuple(tupledata[primary->custodian_id],
                                 tuplelen[primary->custodian_id]);
    if (!bundle->custodian_.valid()) {
        logf(log, LOG_ERR, "invalid tuple '%s'", bundle->custodian_.c_str());
        goto bail;
    }
    logf(log, LOG_DEBUG, "parsed custodian tuple (id %d) %s",
         primary->custodian_id, bundle->custodian_.c_str());
    

    // time for the payload header
    if (len < sizeof(PayloadHeader)) {
        goto tooshort;
    }

    PayloadHeader* payload = (PayloadHeader*)buf;
    buf += sizeof(PayloadHeader);
    len -= sizeof(PayloadHeader);

    int payloadlen = ntohl(payload->length);
    bundle->payload_.set_data(payload->data, payloadlen);

    logf(log, LOG_DEBUG, "parsed payload length %d", payloadlen);
    
    // that should be it
    len -= payloadlen;
    if (len != 0) {
        logf(log, LOG_ERR, "got %d extra bytes in bundle", len);
        goto bail;
    }

    // Bundle looks good. Notify the router of the new arrival
    BundleRouter::dispatch(new BundleReceivedEvent(bundle));
    ASSERT(bundle->refcount() > 0);

    // If the caller requests it, return a pointer to the new bundle.
    if (bundlep)
        *bundlep = bundle;
    
    return true;
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
