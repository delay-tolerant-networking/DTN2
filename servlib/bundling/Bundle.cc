
#include "Bundle.h"

void
Bundle::init()
{
    refcount_		= 0;
    bundleid_		= 0;
    priority_		= COS_NORMAL;
    expiration_		= 0;
    custreq_		= false;
    custody_rcpt_	= false;
    recv_rcpt_		= false;
    fwd_rcpt_		= false;
    return_rcpt_	= false;
    expiration_		= 0; // XXX/demmer
    
    gettimeofday(&creation_ts_, 0);
}

Bundle::Bundle()
{
    init();
}

Bundle::Bundle(const std::string& source,
               const std::string& dest,
               const std::string& payload)
{
    init();
    source_.set_tuple(source);
    replyto_.set_tuple(source);
    custodian_.set_tuple(source);
    dest_.set_tuple(dest);
    payload_.set_data(payload);
}

Bundle::~Bundle()
{
}

int
Bundle::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "bundle id %d %s -> %s (%d bytes payload)",
                    bundleid_, source_.c_str(), dest_.c_str(),
                    payload_.length());
}

void
Bundle::serialize(SerializeAction* a)
{
    a->process("bundleid", &bundleid_);
    a->process("source", &source_);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("priority", &priority_);
    a->process("custreq", &custreq_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("recv_rcpt", &recv_rcpt_);
    a->process("fwd_rcpt", &fwd_rcpt_);
    a->process("return_rcpt", &return_rcpt_);
    a->process("creation_ts_sec",  (u_int32_t*)&creation_ts_.tv_sec);
    a->process("creation_ts_usec", (u_int32_t*)&creation_ts_.tv_usec);
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
}

