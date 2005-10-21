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
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "bundling/FragmentManager.h"

#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include <stdlib.h>

#include "LinkStateRouter.h"
#include "LinkStateGraph.h"

namespace dtn {

#define ROUTER_BCAST_EID "str://linkstate.router"

LinkStateRouter::LinkStateRouter()
    : BundleRouter("linkstate")
{
    log_info("initializing LinkStateRouter");
}

void 
LinkStateRouter::initialize() 
{
    reg_=new LSRegistration(this);
    BundleDaemon::instance()->post(new RegistrationAddedEvent(reg_));
}

void
LinkStateRouter::handle_contact_up(ContactUpEvent* event)
{
    BundleRouter::handle_contact_up(event);
    LinkStateGraph::Vertex *peer, *local;

    log_info("Contact Up. Adding edges %s <-> %s",
             BundleDaemon::instance()->local_eid().c_str(),
             event->contact_->nexthop());

    local=graph_.getVertex(BundleDaemon::instance()->local_eid().c_str());
    peer=graph_.getVertex(event->contact_->nexthop());   

    /* 
     *  Contact Up means we are able to transmit to the node at the other side. 
     *  Although we _could_ make an assumption of symmetric links, we don't really
     *  need to, the peer node will send an update about the other direction.
     */
    LinkStateGraph::Edge *e=graph_.addEdge(local, peer, 1);
    if(e) flood_announcement(e, true);

    /*
     *  Now record this event in the Log for this edge.
     */
    e=graph_.getEdge(local,peer);
    //  e->contact_.start = currentmillis()?
    //  e->contact_.duration = 0;

    /*
     *  Finally, transmit the entire link state database over to the newly discovered peer.
     *  An optimized implementation would send multiple updates in a single bundle, and 
     *  possibly use bloom filters to minimize the amount of state we need to send.
     *
     *  For now, we'll just send a lot of little link update bundles.
     */
    std::set<LinkStateGraph::Edge*> edges=graph_.edges();
    for(std::set<LinkStateGraph::Edge*>::iterator i=edges.begin(); i!=edges.end(); i++)
        send_announcement((*i), event->contact_->link(), 1);
}

void
LinkStateRouter::handle_contact_down(ContactDownEvent* event)
{
    // XXX/jakob - store this event in the link log!
    //             also, might want to trigger a schedule detection event here, since we have a new entry
    //             in the log. If there is a schedule detected, we might want to advertise it too!

    LinkStateGraph::Vertex *peer, *local;

    log_info("Contact Down. Removing edges %s <-> %s",
             BundleDaemon::instance()->local_eid().c_str(),
             event->contact_->nexthop());
    
    peer=graph_.getVertex(event->contact_->nexthop());   
    local=graph_.getVertex(BundleDaemon::instance()->local_eid().c_str());

    /*
     *  Record this event in the Log for this edge.
     */
//    e=graph_.getEdge(local,peer);
//    e->contact_.duration=currentmillis()->e->contact_.start;    
//    e->log_.push_back(e->contact_);

    /*
     *  XXX/jakob - ok, so here we probably want to run the find_schedule thingy.
     *              if we find a schedule, we would like to announce it to the world.
     *              however, there are some finer points here: what if it changes?
     *              what if it changes ever so slightly? when do we announce? always?
     */      
    // Log *schedule = find_schedule(e->log_);
    // if(schedule) announce it!
    // else ignore it

    /*
     * Contact Down means we can't send to a peer any more. This theoretically has nothing to do with our
     * ability to receive from that node. For now, we make the assumption of symmetric links here, tearing
     * down both directions of a link as one direction fails. If we don't do this, we might end up with
     * orphan links in the graph in the event of a graph partitioning.
     *
     * XXX/jakob - there must be a way to handle the tearing down of links assymetrically...
     */
    LinkStateGraph::Edge *e=local->outgoing_edges_[peer];
    if(e) {
        graph_.removeEdge(e);        
        flood_announcement(e,false);
    }
    e=local->incoming_edges_[peer];
    if(e) {
        graph_.removeEdge(e);        
        flood_announcement(e,false);
    }

    BundleRouter::handle_contact_down(event);
}

void
LinkStateRouter::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle=event->bundleref_.object();

    // we don't want any bundles that are already owned by someone
    if(bundle->owner_ != "") {
        log_debug("Skipping bundle ID %u owned by %s.\n",bundle->bundleid_,bundle->owner_.c_str());
        return;
    }
    
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    
    /* XXX/jakob
     *
     * This isn't going to work if there are more than one EID on a node. What needs to be done is to listen to
     * registration events to see what EID's are local. Then, when finding the next hop, we would start at 
     * any local EID but send the bundle directly to the next _non-local_ EID in the graph.
     */
    const char* local_eid = BundleDaemon::instance()->local_eid().c_str();
    LinkStateGraph::Vertex* nextHop=
        graph_.findNextHop(graph_.getVertex(local_eid),
                           // getMatchingVertex allows for some *-matching
                           graph_.getMatchingVertex(bundle->dest_.c_str())); 
    
    if(!nextHop) {
        log_debug("No LSRoute to destination %s",bundle->dest_.c_str());
        return;
    }

    log_debug("Next hop from %s to %s is %s.",
              local_eid,
              bundle->dest_.c_str(),
              nextHop->eid_);              
    
    Link* link=cm->find_link_to(nextHop->eid_);

    ASSERT(link!=0); // if the link is in the graph, it better exist too

    // Send the bundle on the appropriate link.
    actions_->send_bundle(bundle, link); 
}

void
LinkStateRouter::get_routing_state(oasys::StringBuffer* buf)
{
    // dump the link state graph to the buffer.
    graph_.dumpGraph(buf);
}


/* 
 * flood_announcement sends link state announcements to all neighbors. For now, we're using
 * unicast to each and every neighbor. It would really be much better if we could use a 
 * broadcast, but the convergence layers don't support that yet.
 */
void 
LinkStateRouter::flood_announcement(LinkStateGraph::Edge* edge, bool exists)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), "flood_announcement");

    const LinkSet* links=cm->links();
    for(LinkSet::const_iterator i=links->begin(); i!=links->end(); i++)
        send_announcement(edge, (*i), exists);
}

void
LinkStateRouter::send_announcement(LinkStateGraph::Edge* edge, Link* outgoing_link, bool exists)
{
    LinkStateAnnouncement lsa;
    memset(&lsa,0,sizeof(lsa));

    // XXX/jakob these announcements should be nicely encoded, not
    //           just send the whole char[]! Bad, lazy me!
    
    sprintf(lsa.from,"%s",(edge->from_)->eid_);
    sprintf(lsa.to,"%s",(edge->to_)->eid_);
    if(exists)
        lsa.cost=edge->cost_;
    else
        lsa.cost=LinkStateAnnouncement::LINK_DOWN;

    lsa.type=LS_ANNOUNCEMENT;

    // make a bundle
    Bundle *b=new Bundle();
    b->source_.assign(BundleDaemon::instance()->local_eid());
    b->replyto_.assign(BundleDaemon::instance()->local_eid());
    b->custodian_.assign(BundleDaemon::instance()->local_eid());
    b->dest_.assign(ROUTER_BCAST_EID); 
    b->payload_.set_data((const u_char*)&lsa,sizeof(lsa));

    // propagate it to the outgoing link
    actions_->inject_bundle(b);
    actions_->send_bundle(b, outgoing_link);
}

LinkStateRouter::LSRegistration::LSRegistration(LinkStateRouter* router)
    : Registration(Registration::LINKSTATEROUTER_REGID,
                   EndpointID(ROUTER_BCAST_EID),
                   Registration::DEFER, 0)
{
    logpathf("/reg/admin");

    router_=router;

    BundleDaemon::post(new RegistrationAddedEvent(this));
}

void
LinkStateRouter::LSRegistration::consume_bundle(Bundle* bundle)
{
    u_char typecode;      

    log_info("LSRegistration Consuming bundle");

    size_t payload_len = bundle->payload_.length();
    const u_char* data = 0;

    oasys::StringBuffer payload_buf(payload_len);

    if (payload_len == 0) {
        log_err("linkstate registration got 0 byte bundle *%p", bundle);
        goto done;
    }

    data = bundle->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());

    // first byte of any bundle sent to a LinkStateRouter EID is a type code
    typecode=*data;

    switch(typecode) {
    case LS_ANNOUNCEMENT:
    {
        LinkStateAnnouncement *lsa=(LinkStateAnnouncement*)data;

        LinkStateGraph *graph=router_->graph();
        LinkStateGraph::Edge *edge;
        
        if(lsa->cost==LinkStateAnnouncement::LINK_DOWN) {
            LinkStateGraph::Vertex *from=graph->getVertex(lsa->from);
            if(!from) {
                log_debug("No such vertex %s.",lsa->from);
                return;
            }

            LinkStateGraph::Vertex *to=graph->getVertex(lsa->to);
            if(!(edge=from->outgoing_edges_[to]))
            {
                log_debug("No such edge %s -> %s.",lsa->from,lsa->to);
                return;
            }
            graph->removeEdge(edge);
        }
        
        // addEdge returns the edge if an edge was added. If so, forward it to all neighbors
        if((edge=graph->addEdge(graph->getVertex(lsa->from), graph->getVertex(lsa->to), lsa->cost)))
        {
            router_->flood_announcement(edge, true);
        }        
        
        break;
    }
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

} // namespace dtn
