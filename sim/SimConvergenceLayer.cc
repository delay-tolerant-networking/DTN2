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

#include <oasys/util/StringBuffer.h>

#include "SimConvergenceLayer.h"
#include "Topology.h"
#include "bundling/BundleList.h"

namespace dtnsim {

/******************************************************************************
 *
 * SimConvergenceLayer
 *
 *****************************************************************************/
SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("/cl/sim")
{
}

void
SimConvergenceLayer::init()
{
}

void
SimConvergenceLayer::fini()
{
}

bool
SimConvergenceLayer::validate(const std::string& admin)
{
    NOTIMPLEMENTED;
}

bool
SimConvergenceLayer::match(const std::string& demux, const std::string& admin)
{
    NOTIMPLEMENTED;
}


void 
SimConvergenceLayer::send_bundles(Contact* contact)
{
 
    SimContact* sc = dtnlink2simlink(contact->link()); 
    
    Bundle* bundle;
    BundleList* blist = contact->bundle_list();
    
    Bundle* iter_bundle;
    BundleList::iterator iter;
    oasys::ScopeLock lock(blist->lock());
    
    log_info("N[%d] CL: current bundle list:",sc->src()->id());
        
    for (iter = blist->begin(); 
         iter != blist->end(); ++iter) {
        iter_bundle = *iter;
        log_info("\tbundle:%d pending:%d",
                 iter_bundle->bundleid_,iter_bundle->pendingtx());
    }
    // check, if the contact is open. If yes, send one msg from the queue
    if (sc->is_open()) {
        bundle = blist->pop_front();
        if(bundle) {
            log_info("\tsending bundle:%d",bundle->bundleid_);
            Message* msg = SimConvergenceLayer::bundle2msg(bundle);
            sc->chew_message(msg);
        }
    }
}

bool 
SimConvergenceLayer::open_contact(Contact* contact) 
{
//    PANIC("can not explicitly open contact using simulator convergence layer");
//  Do nothing, as by default all simulator contacts are already open
    return true;
}
    
bool 
SimConvergenceLayer::close_contact(Contact* contact) 
{
    //PANIC("can not explicitly close contact using simulator convergence layer");
    //this is a callback from the Contact - some state cleanup might
    //be necessary
    return true;
}


/******************************************************************
 * Static functions follow (for maintaining mappings)
 ***************************************************************** */

SimContact*
SimConvergenceLayer::dtnlink2simlink(Link* link)
{
   // use contact info in contact to find this out
    ASSERT(link != NULL);
    int id = ((SimCLInfo*)link->cl_info())->id();
    SimContact* s =     Topology::contact(id);
    if (s == NULL) {
        PANIC("undefined contact mapping with stored id %d",id);
    }
    return s;
}


std::string
SimConvergenceLayer::id2node(int i) 
{
    oasys::StringBuffer buf;
    buf.appendf("bundles://sim/simcl://%d", i);
    std::string ret(buf.c_str());
    return ret;
}

void
SimConvergenceLayer::create_ct(int id) 
{
    ASSERT(id >=0 && id < MAX_LINKS);

    BundleTuple tuple(id2node(id));

    printf("link creating %s \n",tuple.c_str());
    fflush(stdout);

    // XXX/demmer fix this to use just the admin as well
    Link*  link = Link::create_link(tuple.c_str(), Link::ONDEMAND,
                                    ConvergenceLayer::find_clayer("simcl"),
                                    tuple.c_str(), 0, NULL);
    
    link->set_cl_info(new SimCLInfo(id));
    links_[id] = link;
}

Link*
SimConvergenceLayer::simlink2dtnlink(SimContact* simlink)
{
    ASSERT(simlink != NULL);
    Link* link =  links_[simlink->id()] ; 
    ASSERT(link != NULL);
    return link;

}

Contact*
SimConvergenceLayer::simlink2ct(SimContact* simlink)
{
    return simlink2dtnlink(simlink)->contact();
}

Message*
SimConvergenceLayer::bundle2msg(Bundle* b) 
{
    ASSERT(b != NULL);
    // no fragmentation is supported
    Message* m =  messages_[b->bundleid_];
    ASSERT(m != NULL);
    return m;
}

Bundle*
SimConvergenceLayer::msg2bundle(Message* m) 
{
    ASSERT(m != NULL);
    // no fragmentation is supported, messages/bundles uniquely identified by id's
    
    ASSERT(m->id() >= 0 && m->id() < MAX_BUNDLES);

    Bundle* b = bundles_[m->id()];
    messages_[m->id()] = m;
    
    if (b == NULL) {
        b  = new Bundle(m->id(), BundlePayload::NODATA);
        //b->bundleid_ = m->id();
    
        const std::string src = id2node(m->src());
        const std::string dst = id2node(m->dst());

        b->source_.assign(src);
        b->dest_.assign(dst);
        b->payload_.set_length((int)m->size());
        bundles_[m->id()] = b;
    }
    return  b;

}

int 
SimConvergenceLayer::node2id(BundleTuplePattern src) 
{
    
    std::string admin = src.admin();
    std::string id;
    size_t start  = admin.find(":",0);
    id.assign(admin, start+3, std::string::npos);
    return atoi(id.c_str());

}

Bundle*  SimConvergenceLayer::bundles_[MAX_BUNDLES];
Message* SimConvergenceLayer::messages_[MAX_BUNDLES];
//Contact* SimConvergenceLayer::contacts_[MAX_CONTACTS];
Link* SimConvergenceLayer::links_[MAX_LINKS];
    

} // namespace dtnsim
