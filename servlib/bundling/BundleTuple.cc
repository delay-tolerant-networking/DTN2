
#include "debug/Debug.h"
#include "BundleTuple.h"
#include "applib/dtn_types.h"

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
      proto_(other.proto_),
      admin_(other.admin_)
{
}

void
BundleTuple::assign(const BundleTuple& other)
{
    valid_ = other.valid_;
    tuple_.assign(other.tuple_);
    region_.assign(other.region_);
    proto_.assign(other.proto_);
    admin_.assign(other.admin_);
}

void
BundleTuple::assign(dtn_tuple_t* tuple)
{
    tuple_.assign("bundles://");
    tuple_.append(tuple->region);
    
    if (tuple_[tuple_.length() - 1] != '/') {
        tuple_.push_back('/');
    }

    if (tuple->admin.admin_val[tuple->admin.admin_len - 1] == '\0')
        tuple->admin.admin_len -= 1;
    
    tuple_.append(tuple->admin.admin_val,
                  tuple->admin.admin_len);
    
    parse_tuple();
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

    if (beg == tuple_.length()) {
        logf("/bundle/tuple", LOG_DEBUG, "no admin part of tuple");
        return;
    }

    // extract the proto from the admin string
    beg = end + 1;
    if ((end = tuple_.find(':', beg)) == std::string::npos) {

        // but add a special case for a * wildcard (safe to
        // dereference because of the length check above)
        if (tuple_[beg] == '*') {
            end = beg + 1; // fall through
        } else {
            logf("/bundle/tuple", LOG_DEBUG, "no colon for protocol in '%s'",
                 tuple_.c_str());
            return;
        }
    }
    proto_.assign(tuple_, beg, end - beg);

    // but still assign the whole admin string (including proto) into admin
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

bool
BundleTuplePattern::match_region(const std::string& tuple_region) const
{
    // XXX/demmer todo: implement dns style matching
    
    if (region_.compare("*") == 0)
        return true;

    if (region_.compare(tuple_region) == 0)
        return true;

    return false;
}

bool
BundleTuplePattern::match_admin(const std::string& tuple_admin) const
{
    if (proto_.compare("*") == 0)
        return true; // special case wildcard protocol

    size_t adminlen = admin_.length() - 1;

    if (adminlen >= 1 && admin_[adminlen] == '*') {
        adminlen--;
    }

    if (admin_.compare(tuple_admin) == 0)
        return true;

    // XXX/demmer dispatch to ConvergenceLayer to match admin strings
    
    return false;
}

bool
BundleTuplePattern::match(const BundleTuple& tuple) const
{
    ASSERT(valid() && tuple.valid());

    if (match_region(tuple.region()) &&
        match_admin(tuple.admin()))
    {
        return true;
    }

    return false;
}
