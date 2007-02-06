/*
 *    Copyright 2007 Intel Corporation
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
#include "routing/MultiGraph.h"

using namespace oasys;
using namespace dtn;

DECLARE_TEST(NodeOps) {
    MultiGraph<int, int> g;
    MultiGraph<int, int>::Node *a, *b, *c;

    CHECK(g.find_node("foo") == NULL);

    DO(a = g.add_node("a", 1));
    CHECK(g.find_node("a") == a);
    CHECK(g.find_node("a")->info_ == 1);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n");
        
    DO(b = g.add_node("b", 2));
    CHECK(g.find_node("b") == b);
    CHECK(g.find_node("b")->info_ == 2);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n" "b ->\n");
    
    DO(c = g.add_node("c", 3));
    CHECK(g.find_node("c") == c);
    CHECK(g.find_node("c")->info_ == 3);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n" "b ->\n" "c ->\n");

    CHECK(g.del_node("b"));
    CHECK(g.find_node("a") == a);
    CHECK(g.find_node("b") == NULL);
    CHECK(g.find_node("c") == c);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n" "c ->\n");
    
    CHECK(! g.del_node("b"));
    CHECK(g.find_node("a") == a);
    CHECK(g.find_node("b") == NULL);
    CHECK(g.find_node("c") == c);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n" "c ->\n");
    
    CHECK(g.del_node("c"));
    CHECK(g.find_node("a") == a);
    CHECK(g.find_node("b") == NULL);
    CHECK(g.find_node("c") == NULL);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n");
    
    DO(c = g.add_node("c", 3));
    CHECK(g.find_node("a") == a);
    CHECK(g.find_node("b") == NULL);
    CHECK(g.find_node("c") == c);
    CHECK_EQUALSTR(g.dump().c_str(), "a ->\n" "c ->\n");

    DO(g.clear());
    CHECK_EQUALSTR(g.dump().c_str(), "");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(EdgeOps) {
    MultiGraph<int, int> g;
    MultiGraph<int, int>::Node *a, *b, *c;
    MultiGraph<int, int>::Edge *ab, *ba, *bc, *cb, *ca, *ca2;

    DO(a = g.add_node("a", 1));
    DO(b = g.add_node("b", 2));
    DO(c = g.add_node("c", 3));
    CHECK_EQUALSTR(g.dump().c_str(),
                   "a ->\n"
                   "b ->\n"
                   "c ->\n");

    DO(ab = g.add_edge(a, b, 0));
    DO(ba = g.add_edge(b, a, 1));
    DO(bc = g.add_edge(b, c, 2));
    DO(cb = g.add_edge(c, b, 3));
    DO(ca = g.add_edge(c, a, 4));
    DO(ca2 = g.add_edge(c, a, 5));

    CHECK_EQUALSTR(g.dump().c_str(),
                   "a -> b(0)\n"
                   "b -> a(1) c(2)\n"
                   "c -> b(3) a(4) a(5)\n");

    CHECK(g.del_edge(a, ab));
    CHECK(!g.del_edge(a, ab));
    CHECK(g.del_edge(b, bc));
    CHECK_EQUALSTR(g.dump().c_str(),
                   "a ->\n"
                   "b -> a(1)\n"
                   "c -> b(3) a(4) a(5)\n");
    
    DO(ab = g.add_edge(a, b, 0));
    CHECK_EQUALSTR(g.dump().c_str(),
                   "a -> b(0)\n"
                   "b -> a(1)\n"
                   "c -> b(3) a(4) a(5)\n");

    DO(g.clear());
    CHECK_EQUALSTR(g.dump().c_str(), "");
    
    return UNIT_TEST_PASSED;
}

struct HopCountFn : public MultiGraph<int, int>::WeightFn {
    u_int32_t operator()(MultiGraph<int, int>::Edge* edge)
    {
        (void)edge;
        return 1;
    };
};

struct EvenOddFn : public MultiGraph<int, int>::WeightFn {
    u_int32_t operator()(MultiGraph<int, int>::Edge* edge)
    {
        // even nodes like even links and odd nodes like odd links
        if ((edge->source_->info_ % 2) == (edge->info_)) {
            return 1;
        } else {
            return 10000;
        }
    };
};

struct HopWeightFn : public MultiGraph<int, int>::WeightFn {
    u_int32_t operator()(MultiGraph<int, int>::Edge* edge)
    {
        return edge->info_;
    };
};

DECLARE_TEST(ShortestPathHopCount) {
    MultiGraph<int, int> g;
    MultiGraph<int, int>::Node* nodes[16];
    MultiGraph<int, int>::EdgeVector path;
    char name[256];

    // Add all the nodes
    for (int i = 0; i < 16; ++i) {
        snprintf(name, sizeof(name), "%d", i);
        nodes[i] = g.add_node(name, i);
    }
    
    // Set up a simple ring
    for (int i = 0; i < 16; ++i) {
        g.add_edge(nodes[i], nodes[(i + 1) % 16], 0);
    }

    HopCountFn hop_count_fn;
    EvenOddFn even_odd_fn;
    HopWeightFn hop_weight_fn;
    
    DO(g.shortest_path(nodes[0], nodes[4], &path, &hop_count_fn));
    std::reverse(path.begin(), path.end());
    CHECK_EQUALSTR(path.dump().c_str(), "[0 -> 1(0)] [1 -> 2(0)] [2 -> 3(0)] [3 -> 4(0)]");

    // Add a parallel ring, see that nothing changes
    for (int i = 0; i < 16; ++i) {
        g.add_edge(nodes[i], nodes[(i + 1) % 16], 1);
    }
    
    DO(g.shortest_path(nodes[0], nodes[4], &path, &hop_count_fn));
    std::reverse(path.begin(), path.end());
    CHECK_EQUALSTR(path.dump().c_str(), "[0 -> 1(0)] [1 -> 2(0)] [2 -> 3(0)] [3 -> 4(0)]");

    // Now use a funky even/odd weight function to see that it
    // correctly selects the parallel loop some of the time
    DO(g.shortest_path(nodes[0], nodes[4], &path, &even_odd_fn));
    std::reverse(path.begin(), path.end());
    CHECK_EQUALSTR(path.dump().c_str(), "[0 -> 1(0)] [1 -> 2(1)] [2 -> 3(0)] [3 -> 4(1)]");

    // Remove links, disconnecting node 0
    CHECK(g.del_edge(nodes[0], nodes[0]->out_edges_[0]));
    CHECK(g.del_edge(nodes[0], nodes[0]->out_edges_[0]));
    DO(g.shortest_path(nodes[0], nodes[4], &path, &hop_count_fn));
    CHECK_EQUAL(path.size(), 0);

    // Add a link back, then add backwards links with higher "cost"
    DO(g.add_edge(nodes[0], nodes[1], 0));
    for (int i = 0; i < 16; ++i) {
        g.add_edge(nodes[(i + 1) % 16], nodes[i], 100);
    }

    // Check that these backwards links are used when we're using the
    // hop count weight fn, but not when we use the cost based one
    DO(g.shortest_path(nodes[0], nodes[10], &path, &hop_count_fn));
    std::reverse(path.begin(), path.end());
    CHECK_EQUALSTR(path.dump().c_str(),
                   "[0 -> 15(100)] [15 -> 14(100)] [14 -> 13(100)] "
                   "[13 -> 12(100)] [12 -> 11(100)] [11 -> 10(100)]");

    DO(g.shortest_path(nodes[0], nodes[10], &path, &hop_weight_fn));
    std::reverse(path.begin(), path.end());
    CHECK_EQUALSTR(path.dump().c_str(),
                   "[0 -> 1(0)] [1 -> 2(0)] [2 -> 3(0)] [3 -> 4(0)] [4 -> 5(0)] "
                   "[5 -> 6(0)] [6 -> 7(0)] [7 -> 8(0)] [8 -> 9(0)] [9 -> 10(0)]");
    
    return UNIT_TEST_PASSED;
}


DECLARE_TESTER(RouteMultiGraphTest) {
    ADD_TEST(NodeOps);
    ADD_TEST(EdgeOps);
    ADD_TEST(ShortestPathHopCount);
}

DECLARE_TEST_FILE(RouteMultiGraphTest, "route multigraph test");
