/*
 *    Copyright 2005-2008 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>
#include "bundling/SequenceID.h"

using namespace dtn;
using namespace oasys;

DECLARE_TEST(Basic) {
    SequenceID seq;
    seq.add(EndpointID("dtn://a"), 1);
    seq.add(EndpointID("dtn:none"), 2);
    seq.add(EndpointID("http://www.foo.com"), 10);
    
    CHECK_EQUALSTR(seq.to_str().c_str(), "<(dtn://a 1) (dtn:none 2) (http://www.foo.com 10)>");

    CHECK_EQUAL_U64(seq.get_counter(EndpointID("dtn://a")), 1);
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("dtn:none")), 2);
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("http://www.foo.com")), 10);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Parse) {
    SequenceID seq;
    
    CHECK(seq.parse("<>"));

    CHECK(seq.parse("<(dtn://foo.com 23)>"));
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("dtn://foo.com")), 23);
    
    CHECK(seq.parse("<(dtn:none 1)>"));
    CHECK(seq.parse("<       (dtn:none 1)>"));
    CHECK(seq.parse("<(dtn:none        1)>"));
    CHECK(seq.parse("<(dtn:none 1)        >"));
    CHECK(seq.parse("<(dtn:none 1)           (dtn://foo 2)       >"));
    CHECK(! seq.parse("<( dtn:none 1)>"));
    CHECK(! seq.parse("<(dtn:none 1 )>"));
    
    CHECK(seq.parse("<(dtn://foo.com 999) (dtn:none 0) (http://www.google.com 1000)>"));
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("dtn://foo.com")), 999);
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("dtn:none")), 0);
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("http://www.google.com")), 1000);
    CHECK_EQUAL_U64(seq.get_counter(EndpointID("http://www.dtnrg.org")), 0);

    CHECK(! seq.parse(""));    
    CHECK(! seq.parse("<(dtn:none 1 >"));
    CHECK(! seq.parse("<(dtn:none 1>"));
    CHECK(! seq.parse("<(dtn:none 1)"));

    CHECK(! seq.parse("<( 1)>"));
    CHECK(! seq.parse("<(dtn:none )>"));
    CHECK(! seq.parse("<(dtn:none foo)>"));
    CHECK(! seq.parse("<(foo 1)>"));

    return UNIT_TEST_PASSED;
}

// test brevity
#define LT  SequenceID::LT
#define GT  SequenceID::GT
#define EQ  SequenceID::EQ
#define NEQ SequenceID::NEQ

SequenceID
parse_sequence_id(std::string s)
{
    SequenceID seq;
    bool ok = seq.parse(s);
    ASSERTF(ok, "sequence id string '%s' invalid", s.c_str());
    return seq;
}

// macro to invert order and try again
#define CHECK_COMPARE(s1str, comp, s2str)                               \
do {                                                                    \
    switch (comp) {                                                     \
    case LT:                                                            \
        CHECK(parse_sequence_id(s1str) < parse_sequence_id(s2str));     \
        CHECK(parse_sequence_id(s2str) > parse_sequence_id(s1str));     \
        break;                                                          \
    case SequenceID::GT:                                                \
        CHECK(parse_sequence_id(s1str) > parse_sequence_id(s2str));     \
        CHECK(parse_sequence_id(s2str) < parse_sequence_id(s1str));     \
        break;                                                          \
    case SequenceID::EQ:                                                \
        CHECK(parse_sequence_id(s1str) == parse_sequence_id(s2str));    \
        CHECK(parse_sequence_id(s2str) == parse_sequence_id(s1str));    \
        break;                                                          \
    case SequenceID::NEQ:                                               \
        CHECK(parse_sequence_id(s1str) != parse_sequence_id(s2str));    \
        CHECK(parse_sequence_id(s2str) != parse_sequence_id(s1str));    \
        break;                                                          \
    default:                                                            \
        NOTREACHED;                                                     \
    };                                                                  \
} while (0)

DECLARE_TEST(Compare) {
    CHECK_COMPARE("<>", EQ,
                  "<>");
    
    CHECK_COMPARE("<(dtn://host-0 0)>", EQ,
                  "<(dtn://host-0 0)>");

    CHECK_COMPARE("<(dtn://host-0 1)>", LT,
                  "<(dtn://host-0 2)>");
    
    CHECK_COMPARE("<(dtn://host-0 2)>", GT,
                  "<(dtn://host-0 1)>");

    CHECK_COMPARE("<(dtn://host-0 1)>", NEQ,
                  "<(dtn://host-1 1)>");
    
    CHECK_COMPARE("<(dtn://host-0 1) (dtn://host-1 0)>", NEQ,
                  "<(dtn://host-0 0) (dtn://host-1 1)>");
    
    CHECK_COMPARE("<(dtn://host-0 1) (dtn://host-1 0)>", LT,
                  "<(dtn://host-0 1) (dtn://host-1 1)>");
    
    CHECK_COMPARE("<(dtn://host-0 1) >", LT,
                  "<(dtn://host-0 1) (dtn://host-1 1)>");
    
    CHECK_COMPARE("<(dtn://host-0 1) (dtn://host-2 2)>", GT,
                  "<(dtn://host-0 1) >");

    CHECK_COMPARE("<(dtn://host-0 1) (dtn://host-2 2)>", GT,
                  "<>");

    CHECK_COMPARE("<(dtn://host-0 2)>", NEQ,
                  "<(dtn://host-0 1) (dtn://host-1 10)>");
    
    CHECK_COMPARE("<(dtn://host-0 0) (dtn://host-1 1)>", NEQ,
                  "<(dtn://host-0 0) (dtn://host-2 1)>");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Update) {
    SequenceID seq;

    DO(seq.update(parse_sequence_id("<>")));
    CHECK_EQUALSTR(seq.to_str(), "<>");
    
    DO(seq.update(parse_sequence_id("<(dtn://host-0 1)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 1)>");
    
    DO(seq.update(parse_sequence_id("<(dtn://host-1 1)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 1) (dtn://host-1 1)>");
    
    DO(seq.update(parse_sequence_id("<(dtn://host-0 2) (dtn://host-1 2)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 2) (dtn://host-1 2)>");
    
    DO(seq.update(parse_sequence_id("<(dtn://host-0 1) (dtn://host-1 1)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 2) (dtn://host-1 2)>");
    
    DO(seq.update(parse_sequence_id("<(dtn://host-0 2) (dtn://host-1 2)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 2) (dtn://host-1 2)>");

    DO(seq.update(parse_sequence_id("<(dtn://host-0 0) (dtn://host-1 0) (dtn://host-2 0)>")));
    CHECK_EQUALSTR(seq.to_str(), "<(dtn://host-0 2) (dtn://host-1 2) (dtn://host-2 0)>");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(SequenceIDTester) {
    ADD_TEST(Basic);
    ADD_TEST(Parse);
    ADD_TEST(Compare);
    ADD_TEST(Update);
}

DECLARE_TEST_FILE(SequenceIDTester, "sequence id test");
