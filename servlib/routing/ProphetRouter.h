/*
 *    Copyright 2006 Baylor University
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

#ifndef _PROPHET_ROUTER_H_
#define _PROPHET_ROUTER_H_

#include "ProphetController.h"
#include "bundling/BundleList.h"
#include "reg/AdminRegistration.h"

namespace dtn {

class ProphetRouter : public BundleRouter {
public:
    ProphetRouter();
    virtual ~ProphetRouter();

    /**
     * post-constructor initialization routine
     */
    void initialize();

    /**
     * Event handler overridden from BundleRouter / BundleEventHandler
     * that dispatches to the type specific handlers where
     * appropriate.
     */
    void handle_event(BundleEvent*);

    /**
     * Dump the routing state.
     */
    void get_routing_state(oasys::StringBuffer*);

    /**
     * Handler for new bundle arrivals
     */
    void handle_bundle_received(BundleReceivedEvent*);

    /**
     * Handler for bundle delivered signal
     */
    void handle_bundle_delivered(BundleReceivedEvent*);

    /**
     * Handler for bundle end-of-life
     */
    void handle_bundle_expired(BundleExpiredEvent*);

    /**
     * Monitor new links, complain if EID is null
     */
    void handle_link_created(LinkCreatedEvent* event);

    /**
     * Prophet's "New Neighbor" signal, section 2.3, p. 13
     */
    void handle_contact_up(ContactUpEvent* );

    /**
     * Prophet's "Neighbor Gone" signal, section 2.3, p. 13
     */
    void handle_contact_down(ContactDownEvent* );

    /**
     * Clear pending outbound queues, if any
     */
    void handle_link_state_change_request(LinkStateChangeRequest*);

    /**
     * Prophet's configuration metrics that are propagated all the
     * way down to ProphetController, ProphetEncounter, and ProphetNode
     */
    static ProphetParams      params_;

protected:
    ProphetController* oracle_;

}; // ProphetRouter

} // namespace dtn

#endif /* _PROPHET_ROUTER_H_ */
