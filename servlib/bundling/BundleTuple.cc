
#include "debug/Debug.h"
#include "BundleTuple.h"

BundleTuple::BundleTuple()
    : valid_(false)
{
}

BundleTuple::BundleTuple(const char* tuple)
    : valid_(false), tuple_(tuple)
{
    parse_tuple();
}

BundleTuple::BundleTuple(const std::string& tuple)
    : valid_(false), tuple_(tuple)
{
    parse_tuple();
}

BundleTuple::BundleTuple(const BundleTuple& other)
    : valid_(other.valid_),
      tuple_(other.tuple_),
      region_(other.region_),
      admin_(other.admin_)
{
}

BundleTuple::~BundleTuple()
{
}

void
BundleTuple::parse_tuple()
{
    size_t beg, end;
    
    valid_ = false;

    // skip to the end of 'bundles://'
    if ((end = tuple_.find('/')) == std::string::npos) {
        // an empty string is common enough that it's kinda annoying
        // to keep emitting these
        if (tuple_.length() > 0)
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
