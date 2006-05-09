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
