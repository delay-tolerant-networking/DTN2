/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/util/UnitTest.h>

#include "bundling/BundleActions.h"
#include "contacts/Link.h"
#include "conv_layers/NullConvergenceLayer.h"
#include "routing/RouteTable.h"


using namespace oasys;
using namespace dtn;


NullConvergenceLayer* cl;
Link* l1;
Link* l2;
Link* l3;

DECLARE_TEST(Init) {

    cl = new NullConvergenceLayer();
    
    l1 = Link::create_link("l1", Link::OPPORTUNISTIC, cl,
                           "l1-dest", 0, NULL);

    l2 = Link::create_link("l2", Link::OPPORTUNISTIC, cl,
                           "l2-dest", 0, NULL);

    l3 = Link::create_link("l3", Link::OPPORTUNISTIC, cl,
                           "l3-dest", 0, NULL);

    return UNIT_TEST_PASSED;
}

bool
add_entry(RouteTable* t, const std::string& dest, Link* link)
{
    return t->add_entry(new RouteEntry(
                            EndpointIDPattern(dest),
                            link,
                            FORWARD_COPY,
                            CustodyTimerSpec()));
}

DECLARE_TEST(GetMatching) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->pattern_.c_str(), "dtn://d1");
    CHECK(v[0]->next_hop_ == l1);
    v.clear();
    
    CHECK(add_entry(&t, "*:*", l1));
    CHECK(add_entry(&t, "dtn://d2/*", l2));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d2"), &v), 3);
    CHECK_EQUAL(v.size(), 3);
    CHECK_EQUALSTR(v[0]->pattern_.c_str(), "dtn://d2");
    CHECK(v[0]->next_hop_ == l2);
    CHECK_EQUALSTR(v[1]->pattern_.c_str(), "*:*");
    CHECK(v[1]->next_hop_ == l1);
    CHECK_EQUALSTR(v[2]->pattern_.c_str(), "dtn://d2/*");
    CHECK(v[2]->next_hop_ == l2);

    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d1/*", l2));
    CHECK(add_entry(&t, "dtn://d1", l3));

    CHECK_EQUAL(t.size(), 8);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 5);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1/test"), &v), 2);
                             
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntry) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->pattern_.c_str(), "dtn://d1");
    CHECK(v[0]->next_hop_ == l1);
    v.clear();

    CHECK(t.del_entry(EndpointIDPattern("dtn://d1"), l1));
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);

    CHECK(! t.del_entry(EndpointIDPattern("dtn://d1"), l1));
    CHECK(! t.del_entry(EndpointIDPattern("dtn://d2"), l1));
    CHECK(! t.del_entry(EndpointIDPattern("dtn://d3"), l2));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntries) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->pattern_.c_str(), "dtn://d1");
    CHECK(v[0]->next_hop_ == l1);
    v.clear();

    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);
    
    CHECK_EQUAL(t.size(), 2);

    CHECK(add_entry(&t, "*:*", l1));
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("*:*")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d2")), 1);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d3")), 1);
    CHECK_EQUAL(t.size(), 0);
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));

    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d1")), 3);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d3")), 3);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("dtn://d2")), 3);
    CHECK_EQUAL(t.size(), 0);
    CHECK_EQUAL(t.del_entries(EndpointIDPattern("*:*")), 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(DelEntriesForNextHop) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->pattern_.c_str(), "dtn://d1");
    CHECK(v[0]->next_hop_ == l1);
    v.clear();

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 1);
    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 0);
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 0);
    
    CHECK_EQUAL(t.size(), 2);

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 0);
    CHECK_EQUAL(t.del_entries_for_nexthop(l2), 1);
    CHECK_EQUAL(t.del_entries_for_nexthop(l3), 1);
    CHECK_EQUAL(t.size(), 0);
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));

    CHECK_EQUAL(t.del_entries_for_nexthop(l1), 3);
    CHECK_EQUAL(t.del_entries_for_nexthop(l3), 3);
    CHECK_EQUAL(t.del_entries_for_nexthop(l2), 3);
    CHECK_EQUAL(t.size(), 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(RouteTableTest) {
    ADD_TEST(Init);
    ADD_TEST(GetMatching);
    ADD_TEST(DelEntry);
    ADD_TEST(DelEntries);
    ADD_TEST(DelEntriesForNextHop);
}

DECLARE_TEST_FILE(RouteTableTest, "route table test");
