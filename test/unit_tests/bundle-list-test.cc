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


#include <oasys/util/UnitTest.h>

#include "bundling/Bundle.h"
#include "bundling/BundleList.h"

using namespace oasys;
using namespace dtn;

#define COUNT 10

Bundle* bundles[COUNT];
BundleList::iterator iter;
Bundle::MappingsIterator map_iter;

BundleList *l1, *l2, *l3;

DECLARE_TEST(Init) {
    for (int i = 0; i < COUNT; ++i) {
        bundles[i] = new Bundle(oasys::Builder::builder());
        bundles[i]->bundleid_ = i;
        bundles[i]->payload_.init(i, BundlePayload::NODATA);
        bundles[i]->add_ref("test");
    }

    l1 = new BundleList("list1");
    l2 = new BundleList("list2");
    l3 = new BundleList("list3");

    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), 0);
    CHECK_EQUAL(l3->size(), 0);

    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), 0);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BasicPushPop) {
    BundleRef b("BasicPushPop temporary");

    CHECK(l1->front() == NULL);
    CHECK(l1->back() == NULL);
    
    for (int i = 0; i < COUNT; ++i) {
        l1->push_back(bundles[i]);
        CHECK(l1->back() == bundles[i]);
        CHECK_EQUAL(bundles[i]->refcount(), 2);
    }

    CHECK(l1->front() == bundles[0]);
    CHECK(l1->back() == bundles[COUNT-1]);

    for (int i = 0; i < COUNT; ++i) {
        b = l1->pop_front();
        CHECK(b == bundles[i]);
        CHECK_EQUAL(b->refcount(), 2);
        b = NULL;
        CHECK_EQUAL(bundles[i]->refcount(), 1);
    }

    CHECK(l1->front() == NULL);
    CHECK(l1->back() == NULL);
    
    for (int i = 0; i < COUNT; ++i) {
        l1->push_front(bundles[i]);
        CHECK_EQUAL(bundles[i]->refcount(), 2);
    }

    CHECK_EQUAL(l1->size(), COUNT);
    
    for (int i = 0; i < COUNT; ++i) {
        b = l1->pop_back();
        CHECK(b == bundles[i]);
        b = NULL;
        CHECK_EQUAL(bundles[i]->refcount(), 1);
    }

    CHECK_EQUAL(l1->size(), 0);

    return UNIT_TEST_PASSED;
}


DECLARE_TEST(ContainsAndErase) {
    for (int i = 0; i < COUNT; ++i) {
        l1->push_back(bundles[i]);
        CHECK(l1->contains(bundles[i]));
    }
    CHECK_EQUAL(l1->size(), COUNT);

    CHECK(l1->erase(bundles[0]));
    CHECK(! l1->contains(bundles[0]));
    CHECK_EQUAL(bundles[0]->refcount(), 1);
    CHECK_EQUAL(bundles[0]->num_mappings(), 0);
    CHECK_EQUAL(l1->size(), COUNT - 1);
    
    CHECK(! l1->erase(bundles[0]));
    CHECK(! l1->contains(bundles[0]));
    CHECK_EQUAL(bundles[0]->refcount(), 1);
    CHECK_EQUAL(bundles[0]->num_mappings(), 0);
    CHECK_EQUAL(l1->size(), 9);

    CHECK(l1->erase(bundles[5]));
    CHECK(! l1->contains(bundles[5]));
    CHECK_EQUAL(bundles[5]->refcount(), 1);
    CHECK_EQUAL(bundles[5]->num_mappings(), 0);
    CHECK_EQUAL(l1->size(), COUNT - 2);

    CHECK(! l1->contains(NULL));
    CHECK(! l1->erase(NULL));
    CHECK_EQUAL(l1->size(), COUNT - 2);

    l1->clear();
    CHECK_EQUAL(l1->size(), 0);
    for (int i = 0; i < COUNT; ++i) {
        CHECK(! l1->contains(bundles[i]));
        CHECK(! l1->erase(bundles[i]));
        CHECK_EQUAL(bundles[i]->refcount(), 1);
        CHECK_EQUAL(bundles[i]->num_mappings(), 0);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultipleLists) {
    Bundle* b;
    BundleList* l;
    
    for (int i = 0; i < COUNT; ++i) {
        l1->push_back(bundles[i]);

        if ((i % 2) == 0) {
            l2->push_back(bundles[i]);
        } else {
            l2->push_front(bundles[i]);
        }

        if ((i % 3) == 0) {
            l3->push_back(bundles[i]);
        } else if ((i % 3) == 1) {
            l3->push_front(bundles[i]);
        }
    }

    b = bundles[0];
    CHECK_EQUAL(b->num_mappings(), 3);
    b->lock_.lock("test lock");
    for (map_iter = b->mappings_begin();
         map_iter != b->mappings_end();
         ++map_iter)
    {
        l = *map_iter;
        CHECK(l->contains(b));
    }
    
    for (map_iter = b->mappings_begin();
         map_iter != b->mappings_end();
         ++map_iter)
    {
        l = *map_iter;
        b->lock_.unlock();
        CHECK(l->erase(b));
        b->lock_.lock("test lock");
        CHECK(! l->contains(b));
    }

    b->lock_.unlock();
    CHECK_EQUAL(b->num_mappings(), 0);

    // list contents fall through to next test
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultipleListRemoval) {
    Bundle* b;
    BundleList* l;

    l3->lock()->lock("test lock");
    for (iter = l3->begin(); iter != l3->end(); )
    {
        b = *iter;
        ++iter; // increment before removal

        b->lock_.lock("test lock");
        CHECK_EQUAL(b->num_mappings(), 3);
        
        for (map_iter = b->mappings_begin();
             map_iter != b->mappings_end();
             ++map_iter)
        {
            l = *map_iter;
            
            CHECK(l->contains(b));
            b->lock_.unlock();
            CHECK(l->erase(b));
            b->lock_.lock("test lock");
            CHECK(! l->contains(b));
        }
        
        b->lock_.unlock();
    }
    l3->lock()->unlock();

    ASSERT(l3->size() == 0);

    for (int i = 0; i < COUNT; ++i) {
        if (i == 0)
            continue;
        
        if ((i % 3) == 2) {
            CHECK_EQUAL(bundles[i]->num_mappings(), 2);
        } else {
            CHECK_EQUAL(bundles[i]->num_mappings(), 0);
        }
    }

    l1->clear();
    l2->clear();
    
    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), 0);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MoveContents) {
    for (int i = 0; i < COUNT; ++i) {
        l1->push_back(bundles[i]);
    }
    
    l1->move_contents(l2);
    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), COUNT);

    l2->clear();
    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(BundleListTest) {
    ADD_TEST(Init);
    ADD_TEST(BasicPushPop);
    ADD_TEST(ContainsAndErase);
    ADD_TEST(MultipleLists);
    ADD_TEST(MultipleListRemoval);
    ADD_TEST(MoveContents);
}

DECLARE_TEST_FILE(BundleListTest, "bundle list test");
