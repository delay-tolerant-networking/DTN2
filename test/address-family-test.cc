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

#include "bundling/AddressFamily.h"
#include "bundling/BundleTuple.h"

#define VALID   true
#define INVALID false

#define MATCH   true
#define NOMATCH false

void
test_parse(bool expect_ok, const char* str )
{
    BundleTuple t(str);

    if (expect_ok) {
        if (t.valid()) {
            log_info("/test", "ok  -- '%s' valid", str);
        } else {
            log_err("/test",  " ERR -- '%s' INVALID expected valid", str);
        }
    } else {
        if (!t.valid()) {
            log_info("/test", "ok  -- '%s' invalid", str);
        } else {
            log_err("/test",  " ERR -- '%s' VALID expected invalid", str);
        }
    }
}

void
test_match(bool expect_match, const char* pattern, const char* tuple)
{
    BundleTuplePattern p(pattern);
    BundleTuple t(tuple);

    ASSERT(p.valid() && t.valid());

    bool match = p.match(t);
    
    if (expect_match) {
        if (match) {
            log_info("/test", "ok  -- '%s' match '%s'", pattern, tuple);
        } else {
            log_err("/test",  " ERR -- '%s' NOMATCH '%s' expected match",
                    pattern, tuple);
        }
    } else {
        if (!match) {
            log_info("/test", "ok  -- '%s' nomatch '%s'", pattern, tuple);
        } else {
            log_err("/test",  " ERR -- '%s' MATCH '%s' expected nomatch",
                    pattern, tuple);
        }
    }
}

int
main(int argc, const char** argv)
{
    Log::init(1, LOG_INFO);
    AddressFamilyTable::init();

    log_info("/test", "address family test starting...");

    // test a bunch of invalid tuples
    test_parse(INVALID, "");
    test_parse(INVALID, "bundles");
    test_parse(INVALID, "bundles:");
    test_parse(INVALID, "bundles:/");
    test_parse(INVALID, "bundles://");
    test_parse(INVALID, "bundles://region");
    test_parse(INVALID, "bundles://region/");
    test_parse(INVALID, "bundles:///admin");
    test_parse(INVALID, "bundles://region/admin");
    test_parse(INVALID, "bundles://region/admin:");
    test_parse(INVALID, "bundles://region/admin:/");
    test_parse(INVALID, "bundles://region/admin://");
    
    // test internet style parsing
    test_parse(VALID,   "bundles://internet/host://tier.cs.berkeley.edu/demux");
    test_parse(VALID,   "bundles://internet/host://host/demux");
    test_parse(VALID,   "bundles://internet/host://host:port/demux");

    // test fixed style
    test_parse(VALID,   "bundles://fixed/1");
    test_parse(VALID,   "bundles://fixed/12");
    test_parse(INVALID, "bundles://fixed/123");
    
    // test internet style matching
    test_match(MATCH,
               "bundles://internet/host://*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://internet/ip://128.32.0.0/16",
               "bundles://internet/ip://128.32.100.56:5000/demux");
    
    test_match(MATCH,
               "bundles://internet/ip://128.32.0.0/255.255.0.0",
               "bundles://internet/ip://128.32.100.56:5000/demux");

    // test fixed matching
    test_match(MATCH,
               "bundles://fixed/1",
               "bundles://fixed/1");

    test_match(MATCH,
               "bundles://fixed/12",
               "bundles://fixed/12");

    test_match(NOMATCH,
               "bundles://fixed/1",
               "bundles://fixed/11");

    test_match(MATCH,
               "bundles://fixed/\a",
               "bundles://fixed/\a");

    test_match(MATCH,
               "bundles://fixed/\n",
               "bundles://fixed/\n");

    // test wildcard matching
    test_match(MATCH,
               "bundles://*/*:*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://internet/*:*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(NOMATCH,
               "bundles://*/*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://*/*:*",
               "bundles://fixed/1");

    test_match(MATCH,
               "bundles://fixed/*:*",
               "bundles://fixed/1");

    char buf[128];
    sprintf(buf, "bundles://fixed/%c", 42);
    test_match(MATCH, "bundles://*/*", buf);

    //test_parse(INVALID,   "bundles://internet/host://:port/demux");

}
