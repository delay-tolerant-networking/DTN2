
#include <oasys/debug/Debug.h>

#include "BundleTuple.h"
#include "AddressFamily.h"
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
      family_(other.family_),
      tuple_(other.tuple_),
      region_(other.region_),
      admin_(other.admin_)
{
}

void
BundleTuple::assign(const BundleTuple& other)
{
    valid_ = other.valid_;
    family_ = other.family_;
    tuple_.assign(other.tuple_);
    region_.assign(other.region_);
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

    if (tuple->admin.admin_len > 0 &&
        tuple->admin.admin_val[tuple->admin.admin_len - 1] == '\0')
        tuple->admin.admin_len -= 1;
    
    tuple_.append(tuple->admin.admin_val,
                  tuple->admin.admin_len);
    
    parse_tuple();
}

void
BundleTuple::copyto(dtn_tuple_t* tuple)
{
    memcpy(tuple->region, region_.data(), region_.length());
    tuple->region[region_.length()] = '\0';

    // XXX/demmer tuple memory management
    tuple->admin.admin_val = strdup(admin_.c_str());
    tuple->admin.admin_len = strlen(tuple->admin.admin_val);
}


BundleTuple::~BundleTuple()
{
}

void
BundleTuple::parse_tuple()
{
    size_t beg, end;
    size_t admin_len, tuple_len;

    tuple_len = tuple_.length();
    
    valid_ = false;
    
    // skip to the end of 'bundles://'
    if ((end = tuple_.find('/')) == std::string::npos) {
        // an empty string is common enough that it's kinda annoying
        // to keep emitting these, even as debug messages
        if (tuple_.length() > 0)
            log_debug("/bundle/tuple",
                      "no leading slash in tuple '%s'", tuple_.c_str());
        return;
    }
    
    if (tuple_[++end] != '/') {
        log_debug("/bundle/tuple",
                  "no second slash in tuple '%s'", tuple_.c_str());
        return;
    }
    beg = end + 1;

    // exract the <region>/
    if ((end = tuple_.find('/', beg)) == std::string::npos) {
        log_debug("/bundle/tuple", 
                  "no slash at region end in tuple '%s'", tuple_.c_str());
        return;
    }
    region_.assign(tuple_, beg, end - beg);

    // make sure there's an admin part
    beg = end + 1;
    admin_len = tuple_len - beg;
    if (admin_len == 0) {
        log_debug("/bundle/tuple", "no admin part of tuple");
        return;
    }

    // grab the admin part
    admin_.assign(tuple_, beg, std::string::npos);
    ASSERT(admin_len == admin_.length()); // sanity
    
    // check for a special wildcard admin string that matches all
    // address families
    if (admin_.compare("*") == 0 ||
        admin_.compare("*:*") == 0)
    {
        family_ = AddressFamilyTable::instance()->wildcard_family();
        valid_ = true;
        return;
    }
    
    // check if the admin part is a fixed-length integer
    if (admin_len == 1 || admin_len == 2) {
        family_ = AddressFamilyTable::instance()->fixed_family();
        valid_ = true;
        return;
    }

    // otherwise, the admin should be a URI, so extract the schema
    if ((end = tuple_.find(':', beg)) == std::string::npos) {
        log_debug("/bundle/tuple",
                  "no colon for schemacol in '%s'", tuple_.c_str());
        return;
    }

    // try to find the address family
    std::string schema(tuple_, beg, end - beg);
    family_ = AddressFamilyTable::instance()->lookup(schema);

    if (! family_) {
        log_err("/bundle/tuple", "unknown schema '%s' in tuple '%s'",
                schema.c_str(), tuple_.c_str());
        return;
    }

    valid_ = family_->valid(admin_);
}

void
BundleTuple::serialize(SerializeAction* a)
{
    a->process("tuple", &tuple_);
    if (a->action() == Serialize::UNMARSHAL) {
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
BundleTuplePattern::match(const BundleTuple& tuple) const
{
    ASSERT(valid() && tuple.valid());

    if (! match_region(tuple.region()))
        return false;

    if (!family_->match(admin_, tuple.admin()))
        return false;

    return true;
}
