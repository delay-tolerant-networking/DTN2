
#include "Bundle.h"
#include "storage/BundleStore.h"

Bundle::Bundle()
{
    bundleid_      = BundleStore::instance()->next_id();
    expiration_    = 0;
    priority_      = COS_NORMAL;
    delivery_opts_ = COS_NONE;
}

Bundle::Bundle(const std::string& source,
               const std::string& dest,
               const std::string& payload)
{
    Bundle::Bundle();
    source_.set_tuple(source);
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
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
    a->process("source", &source_);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("custreq", &custreq_);
    a->process("return_rcpt", &return_rept_);
    a->process("priority", &priority_);
    a->process("delivery_opts", &delivery_opts_);
}
