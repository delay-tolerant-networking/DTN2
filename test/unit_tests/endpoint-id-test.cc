/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    CHECK_EQUAL((p.assign(_pattern), eid.assign(_eid), p.match(eid)),   \
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
 
DECLARE_TEST(DTN) {
    // test parsing for some valid names
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu");
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu/");
    EIDCHECK(VALID,   KNOWN, "dtn://tier.cs.berkeley.edu/demux");
    EIDCHECK(VALID,   KNOWN, "dtn://10.0.0.1/");
    EIDCHECK(VALID,   KNOWN, "dtn://10.0.0.1/demux/some/more");
    EIDCHECK(VALID,   KNOWN, "dtn://255.255.255.255/");
    EIDCHECK(VALID,   KNOWN, "dtn://host/demux");
    EIDCHECK(VALID,   KNOWN, "dtn://host:1000/demux");

    // and the lone valid null scheme
    EIDCHECK(VALID,   KNOWN, "dtn:none");

    // and some invalid ones
    EIDCHECK(INVALID, KNOWN, "dtn:/");
    EIDCHECK(INVALID, KNOWN, "dtn://");
    EIDCHECK(INVALID, KNOWN, "dtn:host");
    EIDCHECK(INVALID, KNOWN, "dtn:/host");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DTNMatch) {
    // valid matches
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux",
             "dtn://tier.cs.berkeley.edu/demux");
    
    EIDMATCH(MATCH,
             "dtn://10.0.0.1/demux",
             "dtn://10.0.0.1/demux");
    
    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu/demux");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/*",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(MATCH,
             "dtn://*",
             "dtn://tier.cs.berkeley.edu");

    EIDMATCH(MATCH,
             "dtn://*/",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://*/*",
             "dtn://tier.cs.berkeley.edu/");

    EIDMATCH(MATCH,
             "dtn://*/*",
             "dtn://tier.cs.berkeley.edu/something");

    EIDMATCH(MATCH,
             "dtn://*/demux/*",
             "dtn://tier.cs.berkeley.edu/demux/something");

    EIDMATCH(MATCH,
             "dtn://*.edu/foo",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/foo",
             "dtn://*.edu/foo");
    
    EIDMATCH(MATCH,
             "dtn://tier.*.berkeley.*/foo",
             "dtn://tier.cs.berkeley.edu/foo");
    
    EIDMATCH(MATCH,
             "dtn://*.edu/*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://*.*.*.edu/*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://*.edu/f*",
             "dtn://tier.cs.berkeley.edu/foo");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=",
             "dtn://tier.cs.berkeley.edu/demux?foo=");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&bar=baz",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&bar=baz");

    EIDMATCH(MATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4");

    // invalid matches
    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host2/demux");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1:9999/demux");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1/demux2");

    EIDMATCH(NOMATCH,
             "dtn://host1/demux",
             "dtn://host1/demux/something");

    EIDMATCH(NOMATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=",
             "dtn://tier.cs.berkeley.edu/demux?bar=");

    EIDMATCH(NOMATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?foo=baz");

    EIDMATCH(NOMATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar",
             "dtn://tier.cs.berkeley.edu/demux?bar=foo");

    EIDMATCH(NOMATCH,
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=3&d=4",
             "dtn://tier.cs.berkeley.edu/demux?foo=bar&a=1&b=2&c=4&d=3");


    // the none ssp never matches
    EIDMATCH(NOMATCH,
             "dtn:none",
             "dtn://none");
    
    EIDMATCH(NOMATCH,
             "dtn:none",
             "dtn:none");

    // finally, don't match other schemes
    EIDMATCH(NOMATCH,
             "dtn://host",
             "dtn2://host");

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
    EIDMATCH(MATCH, "*:*", "dtn://host/demux");

    // now we match the null eid as well
    EIDMATCH(MATCH, "*:*", "dtn:none");
    
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

DECLARE_TEST(String) {
    EIDCHECK(VALID,   KNOWN, "str:anything");
    EIDCHECK(VALID,   KNOWN, "str://anything");
    EIDCHECK(VALID,   KNOWN, "str:/anything");

    EIDCHECK(INVALID, KNOWN, "str:");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(StringMatch) {
    EIDMATCH(MATCH, "str:foo", "str:foo");
    EIDMATCH(MATCH, "str://anything", "str://anything");

    EIDMATCH(NOMATCH, "str:none", "dtn:none");
    EIDMATCH(NOMATCH, "dtn:none", "str:none");

    EIDMATCH(NOMATCH, "str://host", "dtn://host");
    EIDMATCH(NOMATCH, "dtn://host", "str://host");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(EndpointIDTester) {
    ADD_TEST(Invalid);
    ADD_TEST(Unknown);
    ADD_TEST(DTN);
    ADD_TEST(DTNMatch);
    ADD_TEST(Wildcard);
    ADD_TEST(WildcardMatch);
#ifdef __linux__
    ADD_TEST(Ethernet);
#endif
    ADD_TEST(String);
    ADD_TEST(StringMatch);
}

DECLARE_TEST_FILE(EndpointIDTester, "endpoint id test");
