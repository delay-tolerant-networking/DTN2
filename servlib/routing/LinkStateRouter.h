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
#ifndef _LINKSTATE_ROUTER_H_
#define _LINKSTATE_ROUTER_H_

#include "bundling/BundleEventHandler.h"
#include "BundleRouter.h"
#include "LinkStateGraph.h"
#include "reg/Registration.h"
#include <set>

namespace dtn {

class LinkStateRouter : public BundleRouter {
public:
    typedef enum {
        LS_ANNOUNCEMENT  // Link state announcement
    } ls_type;

    /* 
     * These link state announcements aren't exactly elegant. However, it makes for 
     * simple code. Optimization will be needed at some point. 
     */
    struct LinkStateAnnouncement {
        static const u_int8_t LINK_DOWN = 255;

        ls_type type;
        char from[MAX_EID]; 
        char to[MAX_EID];          
        u_int8_t cost;      
    } __attribute__((packed));

    LinkStateRouter();

    void handle_event(BundleEvent* event)
    {
        dispatch_event(event);
    }

    void handle_contact_down(ContactDownEvent* event);
    void handle_contact_up(ContactUpEvent* event);
    LinkStateGraph* graph() { return &graph_; }

    void initialize();
    void get_routing_state(oasys::StringBuffer* buf);

protected:
    LinkStateGraph graph_;

    /*
     * Send link-state announcement to all neighbors. 
     *
     * exists - true if this announcement is about an existing link\
     * edge - the edge we're announcing
     */
    void flood_announcement(LinkStateGraph::Edge* edge, bool exists);

    /*
     * Send link-state announcement to one neighbor. Used for transferring
     * the entire routing database.
     *
     * exists - the current state of the edge
     * outgoing_link - the link through which to send the announcement
     * edge - the edge that the announcement is all about
     */
    void send_announcement(LinkStateGraph::Edge* edge, Link* outgoing_link, bool exists);

    void handle_bundle_received(BundleReceivedEvent* event);


 class LSRegistration : public Registration {
  public:
    LSRegistration(LinkStateRouter* router);

    /**
     * Consume the given bundle, queueing it if required.
     */
    LinkStateRouter* router_;
    
    void consume_bundle(Bundle* bundle);

 };

   LSRegistration* reg_;

};

} // namespace dtn

#endif /* _LINKSTATE_ROUTER_H_ */
