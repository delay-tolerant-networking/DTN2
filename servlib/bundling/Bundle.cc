
#include "Bundle.h"
#include "iostream"
using namespace std;

Bundle::Bundle()
{

    expiration_ = 100000;
    priority_ = 1;
    delivery_opts_= 0;
}

Bundle::~Bundle()
{
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
