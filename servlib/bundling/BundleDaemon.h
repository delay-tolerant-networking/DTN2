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
#ifndef _BUNDLE_DAEMON_H_
#define _BUNDLE_DAEMON_H_

#include <vector>

#include <oasys/compat/inttypes.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/StringBuffer.h>

#include "BundleEvent.h"
#include "BundleEventHandler.h"
#include "BundleProtocol.h"

namespace dtn {

class Bundle;
class BundleAction;
class BundleActions;
class BundleConsumer;
class BundleList;
class BundleRouter;
class ContactManager;
class FragmentManager;
class RegistrationTable;

/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class BundleDaemon : public BundleEventHandler, public oasys::Thread {
public:
    /**
     * Singleton accessor.
     */
    static BundleDaemon* instance() {
        if (instance_ == NULL) {
            PANIC("BundleDaemon::init not called yet");
        }
        return instance_;
    }

    /**
     * Constructor.
     */
    BundleDaemon();

    /**
     * Virtual initialization function, overridden in the simulator to
     * install the modified event queue (with no notifier) and the
     * SimBundleActions class.
     */
    virtual void do_init();
    
    /**
     * Boot time initializer.
     */
    static void init()
    {       
        if (instance_ != NULL) 
        {
            PANIC("BundleDaemon already initialized");
        }

        instance_ = new BundleDaemon();     
        instance_->do_init();
    }

    /**
     * Dispatches to the virtual post_event implementation.
     */
    static void post(BundleEvent* event);

    /**
     * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event);

    /**
     * Returns the current bundle router.
     */
    BundleRouter* router()
    {
        ASSERT(router_ != NULL);
        return router_;
    }

    /**
     * Sets the active router.
     */
    void set_router(BundleRouter* router) { router_ = router; }

    /**
     * Return the current actions handler.
     */
    BundleActions* actions() { return actions_; }

    /**
     * Accessor for the contact manager.
     */
    ContactManager* contactmgr() { return contactmgr_; }

    /**
     * Accessor for the fragmentation manager.
     */
    FragmentManager* fragmentmgr() { return fragmentmgr_; }

    /**
     * Accessor for the registration table.
     */
    RegistrationTable* reg_table() { return reg_table_; }

    /**
     * Accessor for the pending bundles list.
     */
    BundleList* pending_bundles() { return pending_bundles_; }
    
    /**
     * Accessor for the custody bundles list.
     */
    BundleList* custody_bundles() { return custody_bundles_; }
    
    /**
     * Format the given StringBuffer with current routing info.
     */
    void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current statistics value.
     */
    void get_statistics(oasys::StringBuffer* buf);

    /**
     * Return the local endpoint identifier.
     */
    const EndpointID& local_eid() { return local_eid_; }

    /**
     * Set the local endpoint id.
     */
    void set_local_eid(const char* eid_str) {
        local_eid_.assign(eid_str);
    }

    struct Params {
        /// Whether or not to delete bundles before they're expired if
        /// all routers / registrations have handled it
        bool early_deletion_;
        
        /// Threshold for proactive fragmentation
        size_t proactive_frag_threshold_;
    };

    static Params params_;
    
protected:
    /**
     * Main thread function that dispatches events.
     */
    void run();

    /**
     * Main event handling function.
     */
    void handle_event(BundleEvent* event);

    /// @{
    /**
     * Event type specific handlers.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
    void handle_bundle_expired(BundleExpiredEvent* event);
    void handle_bundle_free(BundleFreeEvent* event);
    void handle_registration_added(RegistrationAddedEvent* event);
    void handle_contact_up(ContactUpEvent* event);
    void handle_contact_down(ContactDownEvent* event);
    void handle_link_available(LinkAvailableEvent* event);    
    void handle_link_unavailable(LinkUnavailableEvent* event);
    void handle_link_state_change_request(LinkStateChangeRequest* request);
    void handle_reassembly_completed(ReassemblyCompletedEvent* event);
    ///@}

    typedef BundleProtocol::status_report_flag_t status_report_flag_t;
    
    /**
     * Locally generate a status report for the given bundle.
     */
    void generate_status_report(Bundle* bundle, status_report_flag_t flag);
    
    /**
     * Add the bundle to the pending list and (optionally) the
     * persistent store, and set up the expiration timer for it.
     */
    void add_to_pending(Bundle* bundle, bool add_to_store);
    
    /**
     * Remove the bundle from the pending list and data store, and
     * cancel the expiration timer.
     */
    void delete_from_pending(Bundle* bundle);
        
    /**
     * Deliver the bundle to the given registration
     */
    void deliver_to_registration(Bundle* bundle, Registration* registration);
    
    /**
     * Check the registration table and deliver the bundle to any that
     * match.
     */
    void check_registrations(Bundle* bundle);
    
    /**
     * Update statistics based on the event arrival.
     */
    void update_statistics(BundleEvent* event);
    
    /// The active bundle router
    BundleRouter* router_;

    /// The active bundle actions handler
    BundleActions* actions_;

    /// The contact manager
    ContactManager* contactmgr_;

    /// The fragmentation / reassembly manager
    FragmentManager* fragmentmgr_;

    /// The table of active registrations
    RegistrationTable* reg_table_;

    /// The list of all bundles still pending delivery
    BundleList* pending_bundles_;

    /// The list of all bundles that we have custody of
    BundleList* custody_bundles_;
    
    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// The default endpoint id for reaching this daemon, used for
    /// bundle status reports, routing, etc.
    EndpointID local_eid_;

    /// Statistics
    u_int32_t bundles_received_;
    u_int32_t bundles_delivered_;
    u_int32_t bundles_transmitted_;
    u_int32_t bundles_expired_;

    static BundleDaemon* instance_;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
