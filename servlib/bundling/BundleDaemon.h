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

class AdminRegistration;
class Bundle;
class BundleAction;
class BundleActions;
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
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */
    static void post(BundleEvent* event);
 
    /**
     * Queues the event at the head of the queue for processing by the
     * daemon thread.
     */
    static void post_at_head(BundleEvent* event);
    
    /**
     * Post the given event and wait for it to be processed by the
     * daemon thread or the given timeout to elapse.
     */
    static bool post_and_wait(BundleEvent* event, oasys::Notifier* notifier,
                              int timeout = -1);

   /**
     * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

    /**
     * Returns the current bundle router.
     */
    BundleRouter* router()
    {
        ASSERT(router_ != NULL);
        return router_;
    }

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
    const RegistrationTable* reg_table() { return reg_table_; }

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
     * Format the given StringBuffer with the current bundle
     * statistics.
     */
    void get_bundle_stats(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current internal
     * statistics value.
     */
    void get_daemon_stats(oasys::StringBuffer* buf);

    /**
     * Reset all internal stats.
     */
    void reset_stats();

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

    /**
     * General daemon parameters, initialized in ParamCommand.
     */
    struct Params {
        /// Whether or not to delete bundles before they're expired if
        /// all routers / registrations have handled it
        bool early_deletion_;

        /// Whether or not to accept custody when requested
        bool accept_custody_;

        /// Whether or not reactive fragmentation is enabled
        bool reactive_frag_enabled_;
        
        /// Whether or not to retry unacked transmissions on reliable CLs.
        bool retry_reliable_unacked_;

        /// Threshold for proactive fragmentation
        size_t proactive_frag_threshold_;

        /// Test hook to permute bundles before delivering to registrations
        bool test_permuted_delivery_;
    };

    static Params params_;

    /**
     * Typedef for a shutdown procedure.
     */
    typedef void (*ShutdownProc) (void* args);
    
    /**
     * Set an application-specific shutdown handler.
     */
    void set_app_shutdown(ShutdownProc proc, void* data)
    {
        app_shutdown_proc_ = proc;
        app_shutdown_data_ = data;
    }
    
protected:
    /**
     * Initialize and load in the registrations.
     */
    void load_registrations();
        
    /**
     * Initialize and load in stored bundles.
     */
    void load_bundles();
        
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
    void handle_bundle_transmit_failed(BundleTransmitFailedEvent* event);
    void handle_bundle_delivered(BundleDeliveredEvent* event);
    void handle_bundle_expired(BundleExpiredEvent* event);
    void handle_bundle_free(BundleFreeEvent* event);
    void handle_registration_added(RegistrationAddedEvent* event);
    void handle_registration_removed(RegistrationRemovedEvent* event);
    void handle_registration_expired(RegistrationExpiredEvent* event);
    void handle_contact_up(ContactUpEvent* event);
    void handle_contact_down(ContactDownEvent* event);
    void handle_link_available(LinkAvailableEvent* event);    
    void handle_link_unavailable(LinkUnavailableEvent* event);
    void handle_link_state_change_request(LinkStateChangeRequest* request);
    void handle_reassembly_completed(ReassemblyCompletedEvent* event);
    void handle_route_add(RouteAddEvent* event);
    void handle_route_del(RouteDelEvent* event);
    void handle_custody_signal(CustodySignalEvent* event);
    void handle_custody_timeout(CustodyTimeoutEvent* event);
    void handle_shutdown_request(ShutdownRequest* event);
    void handle_status_request(StatusRequest* event);
    ///@}

    typedef BundleProtocol::custody_signal_reason_t custody_signal_reason_t;
    typedef BundleProtocol::status_report_flag_t status_report_flag_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;
    
    /**
     * Locally generate a status report for the given bundle.
     */
    void generate_status_report(Bundle* bundle,
                                status_report_flag_t flag,
                                status_report_reason_t reason =
                                BundleProtocol::REASON_NO_ADDTL_INFO);

    /**
     * Generate a custody signal to be sent to the current custodian.
     */
    void generate_custody_signal(Bundle* bundle, bool succeeded,
                                 custody_signal_reason_t reason);
    
    /**
     * Cancel any pending custody timers for the bundle.
     */
    void cancel_custody_timers(Bundle* bundle);

    /**
     * Take custody for the given bundle, sending the appropriate
     * signal to the current custodian.
     */
    void accept_custody(Bundle* bundle);

    /**
     * Release custody of the given bundle, sending the appropriate
     * signal to the current custodian.
     */
    void release_custody(Bundle* bundle);

    /**
     * Add the bundle to the pending list and (optionally) the
     * persistent store, and set up the expiration timer for it.
     *
     * @return true if the bundle is legal to be delivered and/or
     * forwarded, false if it's already expired
     */
    bool add_to_pending(Bundle* bundle, bool add_to_store);
    
    /**
     * Remove the bundle from the pending list and data store, and
     * cancel the expiration timer.
     */
    void delete_from_pending(Bundle* bundle, status_report_reason_t reason);

    /**
     * Check if we should delete this bundle, called once it's been
     * transmitted or delivered at least once. If so, call
     * delete_from_pending.
     */
    void try_delete_from_pending(Bundle* bundle);

    /**
     * Check if there are any bundles in the pending queue that match
     * the source id, timestamp, and fragmentation offset/length
     * fields.
     */
    Bundle* find_duplicate(Bundle* bundle);

    /**
     * Deliver the bundle to the given registration
     */
    void deliver_to_registration(Bundle* bundle, Registration* registration);
    
    /**
     * Check the registration table and deliver the bundle to any that
     * match.
     */
    void check_registrations(Bundle* bundle);
    
    /// The active bundle router
    BundleRouter* router_;

    /// The active bundle actions handler
    BundleActions* actions_;

    /// The administrative registration
    AdminRegistration* admin_reg_;

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

    /// Statistics structure definition
    struct Stats {
        u_int32_t bundles_received_;
        u_int32_t bundles_delivered_;
        u_int32_t bundles_generated_;
        u_int32_t bundles_transmitted_;
        u_int32_t bundles_expired_;
        u_int32_t duplicate_bundles_;
        u_int32_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    /// Application-specific shutdown handler
    ShutdownProc app_shutdown_proc_;
 
    /// Application-specific shutdown data
    void* app_shutdown_data_;

    /// The static instance
    static BundleDaemon* instance_;
};

} // namespace dtn

#endif /* _BUNDLE_DAEMON_H_ */
