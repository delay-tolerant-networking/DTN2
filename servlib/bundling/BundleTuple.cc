/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <oasys/debug/Debug.h>

#include "BundleTuple.h"
#include "AddressFamily.h"
#include "applib/dtn_types.h"

namespace dtn {

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

    // lookup the correct address family for this admin string
    // and make sure it's valid
    family_ = AddressFamilyTable::instance()->lookup(admin_, &valid_);

    if (!family_) {
        log_err("/bundle/tuple", "no address family for admin in '%s'",
                tuple_.c_str());
    }

    if (!valid_) {
        log_err("/bundle/tuple", "invalid admin tuple in '%s'",
                tuple_.c_str());
    }
}

void
BundleTuple::serialize(oasys::SerializeAction* a)
{
    a->process("tuple", &tuple_);
    if (a->action() == oasys::Serialize::UNMARSHAL) {
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

} // namespace dtn
