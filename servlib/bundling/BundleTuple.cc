
#include "debug/Debug.h"
#include "BundleTuple.h"

BundleTuple::BundleTuple()
    : valid_(false)
{
}

BundleTuple::BundleTuple(const std::string& tuple)
    : valid_(false), tuple_(tuple)
{
    parse_tuple();
}

BundleTuple::~BundleTuple()
{
}

void
BundleTuple::set_tuple(const std::string& tuple)
{
    tuple_.assign(tuple);
    parse_tuple();
}
    
void
BundleTuple::parse_tuple()
{
    size_t beg, end;
    
    valid_ = false;

    // skip to the end of 'bundles://'
    if ((end = tuple_.find('/')) == std::string::npos) {
        logf("/bundle/tuple", LOG_DEBUG, "no leading slash in tuple '%s'",
             tuple_.c_str());
        return;
    }
    if (tuple_[++end] != '/') {
        logf("/bundle/tuple", LOG_DEBUG, "no second slash in tuple '%s'",
             tuple_.c_str());
        return;
    }
    beg = end + 1;

    // exract the <region>/
    if ((end = tuple_.find('/', beg)) == std::string::npos) {
        logf("/bundle/tuple", LOG_DEBUG, "no slash at region end in tuple '%s'",
             tuple_.c_str());
        return;
    }
    region_.assign(tuple_, beg, end - beg);

    beg = end + 1;
    admin_.assign(tuple_, beg, std::string::npos);

    valid_ = true;
}

void
BundleTuple::serialize(SerializeAction* a)
{
    a->process("tuple", &tuple_);
    if (a->type() == SerializeAction::UNMARSHAL) {
        parse_tuple();
    }
}
