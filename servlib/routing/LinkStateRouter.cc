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

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/ContactManager.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include <stdlib.h>

#include "LinkStateRouter.h"
#include "LinkStateGraph.h"

namespace dtn {

#define ROUTER_EID "flood://BS/string://linkstate.router"

LinkStateRouter::LinkStateRouter()
    : BundleRouter("linkstate")
{
    log_info("initializing LinkStateRouter");
}

void 
LinkStateRouter::initialize() 
{
    reg=new LSRegistration();    
}

void
LinkStateRouter::handle_contact_up(ContactUpEvent* event)
{
    BundleRouter::handle_contact_up(event);
    LinkStateGraph::Vertex *peer, *local;

    char tuplestring[255];
    sprintf(tuplestring, "bundles://internet/%s",event->contact_->nexthop());

    log_info("Contact Up. Adding edges %s <-> %s", local_tuple_.c_str(), tuplestring);

    local=graph.getVertex(local_tuple_.c_str());
    peer=graph.getVertex(tuplestring);   

    graph.addEdge(peer, local, 1);     
    graph.addEdge(local, peer, 1);
}

void
LinkStateRouter::handle_contact_down(ContactDownEvent* event)
{
    LinkStateGraph::Vertex *peer, *local;

    char tuplestring[255];
    sprintf(tuplestring, "bundles://internet/%s",event->contact_->nexthop());

    log_info("Contact Down. Removing edges %s <-> %s", local_tuple_.c_str(), tuplestring);
    
    peer=graph.getVertex(tuplestring);   
    local=graph.getVertex(local_tuple_.c_str());

    graph.removeEdge(peer, local);     
    graph.removeEdge(local, peer);
    
    BundleRouter::handle_contact_down(event);
}

int
LinkStateRouter::fwd_to_matching(Bundle* bundle, bool include_local)
{
    //XXX/jakob - this only works if there's one local tuple per node
    ContactManager* cm = BundleDaemon::instance()->contactmgr();

    LinkStateGraph::Vertex* nextHop=graph.findNextHop(graph.getVertex(local_tuple_.c_str()),
                                       graph.getVertex(bundle->dest_.c_str()));

    if(!nextHop) return 0;

    log_debug("Next hop from %s to %s is %s.",
              local_tuple_.c_str(),
              bundle->dest_.c_str(),
              nextHop->eid_);              
    
    Link* l=cm->find_link_to(nextHop->eid_);

    ASSERT(l!=0); // if the link is in the graph, it better exist too

    // Enqueue the bundle on the appropriate link. If the link is up, it gets
    // put on the contact automatically. 

    actions_->enqueue_bundle(bundle, 
                             l,
                             FORWARD_UNIQUE,  //XXX/jakob - just making stuff up here...
                             0); //XXX/jakob - again, I have no clue what I'm doing            
    
    return 1;
}

void
LinkStateRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Link state router:");
    buf->appendf("XXX/jacob TODO");
}


void
LinkStateRouter::send_announcement(LinkStateGraph::Edge* edge)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    LinkStateAnnouncement lsa;
    memset(&lsa,0,sizeof(lsa));

    // XXX/jakob these announcements should be nicely compressed, not
    //           just send the whole char[]!
    
    strcpy(lsa.from, (edge->from_)->eid_);
    strcpy(lsa.to, (edge->to_)->eid_);
    lsa.cost=edge->cost_;

    // make a bundle
    Bundle *b=new Bundle();
    b->source_.assign(local_tuple_);
    b->replyto_.assign(local_tuple_);
    b->dest_.assign(ROUTER_EID); //XXX/jakob - temporary

    b->payload_.set_data((const u_char*)&lsa,sizeof(lsa));
    
    // send the bundle across all links
    LinkSet* links=cm->links();
    for(LinkSet::iterator i=links->begin(); i!=links->end();i++)
        actions_->enqueue_bundle(b, (*i), 
                                 FORWARD_COPY, 0); //XXX/jakob last param needs fixing

}


LinkStateRouter::LSRegistration::LSRegistration()
    : Registration(12, //XXX/jakob - just making stuff up again. GlobalStore wouldn't give me a valid one automatically
                   BundleTuple(ROUTER_EID),
                   ABORT)
{
    logpathf("/reg/admin");

    if (! RegistrationTable::instance()->add(this)) {
        PANIC("unexpected error adding linkstate registration to table");
    }

    BundleDaemon::post(new RegistrationAddedEvent(this));
}

void
LinkStateRouter::LSRegistration::consume_bundle(Bundle* bundle,
                                                const BundleMapping* mapping)
{
    u_char typecode;      

    size_t payload_len = bundle->payload_.length();
    oasys::StringBuffer payload_buf(payload_len);
    log_debug("lsregistration got %u byte bundle", (u_int)payload_len);

    if (payload_len == 0) {
        log_err("linkstate registration got 0 byte bundle *%p", bundle);
        goto done;
    }

    /*
     * As outlined in the bundle specification, the first byte of all
     * administrative bundles is a type code, with the following
     * values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - echo request
     * 0x4     - null request
     * (other) - reserved
     */
    typecode = *(bundle->payload_.read_data(0, 1, (u_char*)payload_buf.data()));

    switch(typecode) {
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleDaemon::post(
        new BundleTransmittedEvent(bundle, this, payload_len, true));
}

} // namespace dtn
