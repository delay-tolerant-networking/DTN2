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

#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/BundleMapping.h"

using namespace dtn;

#define COUNT 10

// some hackery to check for mapping memory leaks

int _Live_Mapping_Count = 0;

void*
operator new(size_t sz) throw (std::bad_alloc)
{
    if (sz == sizeof(BundleMapping)) {
        _Live_Mapping_Count++;
    }

    void* ret = malloc(sz + sizeof(size_t));
    
    *((size_t*)ret) = sz;
    return ((size_t*)ret) + 1;
}

void
operator delete(void* p) throw()
{
    p = ((size_t*)p) - 1;
    
    if (*((size_t*)p) == sizeof(BundleMapping)) {
        _Live_Mapping_Count--;
    }
    free(p);
}

int
main(int argc, const char** argv)
{
    oasys::Log::init(oasys::LOG_INFO);

    Bundle* b;
    BundleList* l;
    BundleMapping *m;
    
    Bundle* bundles[COUNT];
    BundleList::iterator iter;
    Bundle::MappingsIterator map_iter;

    for (int i = 0; i < COUNT; ++i) {
        bundles[i] = new Bundle(i, BundlePayload::NODATA);
        bundles[i]->add_ref("test");
    }

    BundleList l1("list1");
    BundleList l2("list2");
    BundleList l3("list3");

    ASSERT(_Live_Mapping_Count == 0);

    log_info("/test", " ");
    log_info("/test", "*** TEST BASIC PUSH/POP ***");
    log_info("/test", " ");
        
    for (int i = 0; i < COUNT; ++i) {
        l1.push_back(bundles[i], NULL);
        ASSERT(bundles[i]->refcount() == 2);
    }

    ASSERT(_Live_Mapping_Count == 10);

    for (int i = 0; i < COUNT; ++i) {
        b = l1.pop_front();
        ASSERT(b == bundles[i]);
        b->del_ref("test");
    }

    ASSERT(l1.size() == 0);
    ASSERT(_Live_Mapping_Count == 0);
    
    for (int i = 0; i < COUNT; ++i) {
        l1.push_front(bundles[i], NULL);
        ASSERT(bundles[i]->refcount() == 2);
    }

    ASSERT(_Live_Mapping_Count == 10);
    
    for (int i = 0; i < COUNT; ++i) {
        b = l1.pop_back();
        ASSERT(b == bundles[i]);
        b->del_ref("test");
    }
    
    ASSERT(l1.size() == 0);
    ASSERT(_Live_Mapping_Count == 0);

    log_info("/test", " ");
    log_info("/test", "*** TEST MAPPINGS FOR REMOVAL ***");
    log_info("/test", " ");
        
    for (int i = 0; i < COUNT; ++i) {
        l1.push_back(bundles[i], NULL);
    }

    for (int i = 0; i < COUNT; ++i) {
        m = bundles[i]->get_mapping(&l1);
        ASSERT(m);
        l1.erase(m->position_);

        m = bundles[i]->get_mapping(&l1);
        ASSERT(!m);
    }

    ASSERT(_Live_Mapping_Count == 0);
    

    log_info("/test", " ");
    log_info("/test", "*** TEST MULTIPLE LISTS ***");
    log_info("/test", " ");
        
    BundleMapping mapping;
    mapping.action_ = FORWARD_COPY;
    mapping.mapping_grp_ = 0xabcd;
    mapping.timeout_ = 0x1234;
    mapping.router_info_ = (RouterInfo*)0xbaddf00d;
    
    for (int i = 0; i < COUNT; ++i) {
        l1.push_back(bundles[i], &mapping);

        if ((i % 2) == 0) {
            l2.push_back(bundles[i], &mapping);
        } else {
            l2.push_front(bundles[i], &mapping);
        }

        if ((i % 3) == 0) {
            l3.push_back(bundles[i], &mapping);
        } else if ((i % 3) == 1) {
            l3.push_front(bundles[i], &mapping);
        }
    }

    b = bundles[0];
    ASSERT(b->num_mappings() == 3);
    b->lock_.lock();
    for (map_iter = b->mappings_begin();
         map_iter != b->mappings_end();
         ++map_iter)
    {
        l = map_iter->first;
        m = map_iter->second;

        l->erase(m->position_, &m);

        ASSERT(m->action_      == mapping.action_);
        ASSERT(m->mapping_grp_ == mapping.mapping_grp_);
        ASSERT(m->timeout_     == mapping.timeout_);
        ASSERT(m->router_info_ == mapping.router_info_);

        delete m;
    }
    b->lock_.unlock();
    ASSERT(b->num_mappings() == 0);

    log_info("/test", " ");
    log_info("/test", "*** TEST MULTIPLE LIST ITERATED REMOVAL ***");
    log_info("/test", " ");
        
    l3.lock()->lock();
    for (iter = l3.begin(); iter != l3.end();)
    {
        b = *iter;
        ++iter; // increment before removal

        b->lock_.lock();
        ASSERT(b->num_mappings() == 3);
        
        for (map_iter = b->mappings_begin();
             map_iter != b->mappings_end();
             ++map_iter)
        {
            l = map_iter->first;
            m = map_iter->second;

            if (l == &l3) {
                BundleList::iterator tmp = iter;
                tmp--;
                ASSERT(m->position_ == tmp);
            }

            l->erase(m->position_);
        }
        b->lock_.unlock();
    }
    l3.lock()->unlock();

    ASSERT(l3.size() == 0);

    for (int i = 0; i < COUNT; ++i) {
        if (i == 0) continue;
        if ((i % 3) == 2) {
            ASSERT(bundles[i]->num_mappings() == 2);
        } else {
            ASSERT(bundles[i]->num_mappings() == 0);
        }
    }

    l1.clear();
    l2.clear();
    
    for (int i = 0; i < COUNT; ++i) {
        if (i == 0) continue;
        ASSERT(bundles[i]->num_mappings() == 0);
    }

    ASSERT(_Live_Mapping_Count == 0);

    log_info("/test", " ");
    log_info("/test", "*** TEST MOVE CONTENTS ***");
    log_info("/test", " ");
        
    for (int i = 0; i < COUNT; ++i) {
        l1.push_back(bundles[i], NULL);
    }
    ASSERT(_Live_Mapping_Count == 10);
    
    l1.move_contents(&l2);
    ASSERT(l1.size() == 0);
    ASSERT(l2.size() == COUNT);

    l2.clear();
    ASSERT(l1.size() == 0);
    
    ASSERT(_Live_Mapping_Count == 0);
}
