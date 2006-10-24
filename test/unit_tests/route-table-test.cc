/*
 *    Copyright 2006 Intel Corporation
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
    return t->add_entry(new RouteEntry(EndpointIDPattern(dest), link));
}

DECLARE_TEST(GetMatching) {
    RouteTable t("test");
    RouteEntryVec v;
    
    CHECK(add_entry(&t, "dtn://d1", l1));
    CHECK(add_entry(&t, "dtn://d2", l2));
    CHECK(add_entry(&t, "dtn://d3", l3));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d1"), &v), 1);
    CHECK_EQUAL(v.size(), 1);
    CHECK_EQUALSTR(v[0]->dest_pattern_.c_str(), "dtn://d1");
    CHECK(v[0]->next_hop_ == l1);
    v.clear();
    
    CHECK(add_entry(&t, "*:*", l1));
    CHECK(add_entry(&t, "dtn://d2/*", l2));
    
    CHECK_EQUAL(t.get_matching(EndpointID("dtn://d2"), &v), 3);
    CHECK_EQUAL(v.size(), 3);
    CHECK_EQUALSTR(v[0]->dest_pattern_.c_str(), "dtn://d2");
    CHECK(v[0]->next_hop_ == l2);
    CHECK_EQUALSTR(v[1]->dest_pattern_.c_str(), "*:*");
    CHECK(v[1]->next_hop_ == l1);
    CHECK_EQUALSTR(v[2]->dest_pattern_.c_str(), "dtn://d2/*");
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
    CHECK_EQUALSTR(v[0]->dest_pattern_.c_str(), "dtn://d1");
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
    CHECK_EQUALSTR(v[0]->dest_pattern_.c_str(), "dtn://d1");
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
    CHECK_EQUALSTR(v[0]->dest_pattern_.c_str(), "dtn://d1");
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
