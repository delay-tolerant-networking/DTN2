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

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/UnitTest.h>

#include "naming/EndpointID.h"

using namespace dtn;
using namespace oasys;

typedef enum {
    INVALID = 0,
    VALID   = 1
} valid_t;

typedef enum {
    UNKNOWN = 0,
    KNOWN   = 1
} known_t;

typedef enum {
    NOMATCH = 0,
    MATCH = 1
} match_t;

bool valid(const char* str)
{
    EndpointID eid(str);
    return eid.valid();
}

bool known(const char* str)
{
    EndpointID eid(str);
    return eid.known_scheme();
}

#define EIDCHECK(_valid, _known, _str)          \
do {                                            \
                                                \
    CHECK_EQUAL(valid(_str), _valid);           \
    CHECK_EQUAL(known(_str), _known);           \
} while (0)

#define EIDMATCH(_match, _pattern, _eid)                                \
do {                                                                    \
    EndpointIDPattern p(_pattern);                                      \
    EndpointID eid(_eid);                                               \
                                                                        \
    CHECK(p.valid() && eid.valid());                                    \
                                                                        \
    CHECK_EQUAL((p.assign(_pattern), eid.assign(_eid), p.match(&eid)),  \
                _match);                                                \
} while (0);

DECLARE_TEST(Invalid) {
    // test a bunch of invalid eids
    EIDCHECK(INVALID, UNKNOWN, "");
    EIDCHECK(INVALID, UNKNOWN, "foo");
    EIDCHECK(INVALID, UNKNOWN, "foo:");
    EIDCHECK(INVALID, UNKNOWN, ":foo");
    EIDCHECK(INVALID, UNKNOWN, "1abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "-abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "+abc:bar");
    EIDCHECK(INVALID, UNKNOWN, ".abc:bar");
    EIDCHECK(INVALID, UNKNOWN, "abc@123:bar");
    EIDCHECK(INVALID, UNKNOWN, "abc_123:bar");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Unknown) {
    // test some valid but unknown eids
    EIDCHECK(VALID, UNKNOWN, "foo:bar");
    EIDCHECK(VALID, UNKNOWN, "fooBAR123:bar");
    EIDCHECK(VALID, UNKNOWN, "foo+BAR-123.456:baz");
    EIDCHECK(VALID, UNKNOWN, "foo:bar/baz/0123456789_-+=<>.,?\'\";:`~");

    return UNIT_TEST_PASSED;
}
 
DECLARE_TEST(Internet) {
    // test internet style parsing for some valid names
    EIDCHECK(VALID,   KNOWN, "bp0://tier.cs.berkeley.edu");
    EIDCHECK(VALID,   KNOWN, "bp0://tier.cs.berkeley.edu/");
    EIDCHECK(VALID,   KNOWN, "bp0://tier.cs.berkeley.edu/demux");
    EIDCHECK(VALID,   KNOWN, "bp0://10.0.0.1/");
    EIDCHECK(VALID,   KNOWN, "bp0://10.0.0.1/demux/some/more");
    EIDCHECK(VALID,   KNOWN, "bp0://255.255.255.255/");
    EIDCHECK(VALID,   KNOWN, "bp0://host/demux");
    EIDCHECK(VALID,   KNOWN, "bp0://host:1000/demux");

    EIDCHECK(INVALID, KNOWN, "bp0:/");
    EIDCHECK(INVALID, KNOWN, "bp0://");
    EIDCHECK(INVALID, KNOWN, "bp0:host");
    EIDCHECK(INVALID, KNOWN, "bp0:/host");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(InternetMatch) {
    // test internet style matching
    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/demux",
             "bp0://tier.cs.berkeley.edu/demux");
    
    EIDMATCH(MATCH,
             "bp0://10.0.0.1/demux",
             "bp0://10.0.0.1/demux");
    
    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/*",
             "bp0://tier.cs.berkeley.edu/demux");

    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/*",
             "bp0://tier.cs.berkeley.edu");

    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/*",
             "bp0://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/demux/*",
             "bp0://tier.cs.berkeley.edu/demux/");

    EIDMATCH(MATCH,
             "bp0://tier.cs.berkeley.edu/demux/*",
             "bp0://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(MATCH,
             "bp0://*/demux/*",
             "bp0://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(NOMATCH,
             "bp0://host1/demux",
             "bp0://host2/demux");

    EIDMATCH(NOMATCH,
             "bp0://host1/demux",
             "bp0://host1:9999/demux");

    EIDMATCH(NOMATCH,
             "bp0://host1/demux",
             "bp0://host1/demux2");

    EIDMATCH(NOMATCH,
             "bp0://host1/demux",
             "bp0://host1/demux/something");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Wildcard) {
    // test the special wildcard scheme
    EndpointIDPattern p;
    CHECK_EQUAL(VALID, (p.assign("*:*"), p.valid()));
    CHECK_EQUAL(KNOWN, (p.assign("*:*"), p.known_scheme()));

    CHECK_EQUAL(INVALID, (p.assign("*:"),     p.valid()));
    CHECK_EQUAL(INVALID, (p.assign("*:abc"),  p.valid()));
    CHECK_EQUAL(INVALID, (p.assign("*:*abc"), p.valid()));

    return UNIT_TEST_PASSED;
}
    
DECLARE_TEST(WildcardMatch) {
    // test wildcard matching with random strings
    EIDMATCH(MATCH, "*:*", "foo:bar");
    EIDMATCH(MATCH, "*:*", "flsdfllsdfgj:087490823uodf");
    EIDMATCH(MATCH, "*:*", "bp0://host:1000/demux");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Ethernet) {
    EIDCHECK(VALID,   KNOWN, "eth://01:23:45:67:89:ab");
    EIDCHECK(VALID,   KNOWN, "eth://ff:ff:ff:ff:ff:ff");
    EIDCHECK(VALID,   KNOWN, "eth:01:23:45:67:89:ab");
    EIDCHECK(VALID,   KNOWN, "eth:ff:ff:ff:ff:ff:ff");
    EIDCHECK(INVALID, KNOWN, "eth:/ff:ff:ff:ff:ff:ff");
    EIDCHECK(INVALID, KNOWN, "eth://");
    EIDCHECK(INVALID, KNOWN, "eth://ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:ff:");
    EIDCHECK(INVALID, KNOWN, "eth://ff:ff:ff:ff:ff:");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(None) {
    EIDCHECK(VALID,   KNOWN, "none:.");

    EIDCHECK(INVALID, KNOWN, "none:");
    EIDCHECK(INVALID, KNOWN, "none://");
    EIDCHECK(INVALID, KNOWN, "none:anything");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(String) {
    EIDCHECK(VALID,   KNOWN, "str:anything");
    EIDCHECK(VALID,   KNOWN, "str://anything");
    EIDCHECK(VALID,   KNOWN, "str:/anything");

    EIDCHECK(INVALID, KNOWN, "str:");

    return UNIT_TEST_PASSED;
}


DECLARE_TESTER(EndpointIDTester) {
    ADD_TEST(Invalid);
    ADD_TEST(Unknown);
    ADD_TEST(Internet);
    ADD_TEST(InternetMatch);
    ADD_TEST(Wildcard);
    ADD_TEST(WildcardMatch);
    ADD_TEST(Ethernet);
    ADD_TEST(String);
    ADD_TEST(None);
}

DECLARE_TEST_FILE(EndpointIDTester, "endpoint id test");
