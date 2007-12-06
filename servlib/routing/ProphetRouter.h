/*
 *    Copyright 2007 Baylor University
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

#include "prophet/Alarm.h"
#include "prophet/Params.h"
#include "prophet/Controller.h"
#include "ProphetBundleCore.h"
#include "BundleRouter.h"
#include <oasys/thread/SpinLock.h>

namespace dtn
{

class ProphetRouter : public BundleRouter
{
public:
    typedef prophet::ProphetParams Params;

    /**
     * Constructor
     */
    ProphetRouter();

    /**
     * Destructor
     */
    virtual ~ProphetRouter();

    /**
     * Post-constructor initialization routine
     */
    void initialize();

    /**
     * Query the router as to whether initialization is complete
     */
    static bool is_init() { return is_init_; }

    /**
     * Write out routing state to StringBuffer
     */
    void get_routing_state(oasys::StringBuffer*);

    ///@{ Virtual from BundleRouter
    bool accept_bundle(Bundle*,int*);
    void handle_event(BundleEvent*);
    void handle_bundle_received(BundleReceivedEvent*);
    void handle_bundle_delivered(BundleDeliveredEvent*);
    void handle_bundle_expired(BundleExpiredEvent*);
    void handle_bundle_transmitted(BundleTransmittedEvent*);
    void handle_contact_up(ContactUpEvent*);
    void handle_contact_down(ContactDownEvent*);
    void handle_link_available(LinkAvailableEvent*);
    void shutdown();
    ///@}

    ///@{ Callback methods for handling runtime configuration changes
    void set_queue_policy();
    void set_hello_interval();
    void set_max_route();
    ///@}

    /**
     * Prophet's configuration and default values
     */
    static prophet::ProphetParams params_;

protected:

    ProphetBundleCore* core_; ///< facade interface into BundleDaemon, etc
    prophet::Controller* oracle_; ///< list of active Prophet peering sessions
    oasys::SpinLock* lock_; ///< control concurrent access to core_ and oracle_
    static bool is_init_; ///< flag to indicate whether initialization has run

}; // class ProphetRouter

}; // namespace dtn

#endif // _PROPHET_ROUTER_H_
