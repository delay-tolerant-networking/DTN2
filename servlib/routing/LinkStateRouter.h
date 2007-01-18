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

#ifndef _LINKSTATE_ROUTER_H_
#define _LINKSTATE_ROUTER_H_

#include "bundling/BundleEventHandler.h"
#include "contacts/Link.h"
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
    void send_announcement(LinkStateGraph::Edge* edge,
                           const LinkRef& outgoing_link, bool exists);

    void handle_bundle_received(BundleReceivedEvent* event);


 class LSRegistration : public Registration {
  public:
    LSRegistration(LinkStateRouter* router);

    LinkStateRouter* router_;
    
    /**
     * Deliver the given bundle, queueing it if required.
     */
    void deliver_bundle(Bundle* bundle);

 };

   LSRegistration* reg_;

};

} // namespace dtn

#endif /* _LINKSTATE_ROUTER_H_ */
