/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/io/IO.h>
#include <oasys/util/Time.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemonOutput.h"
#include "SDNV.h"

#include "contacts/ContactManager.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "naming/IPNScheme.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "storage/BundleStore.h"

#include "bundling/S10Logger.h"

#ifdef BSP_ENABLED
#  include "security/Ciphersuite.h"
#  include "security/SPD.h"
#  include "security/KeyDB.h"
#endif

#ifdef ACS_ENABLED
#  include "BundleDaemonACS.h"
#endif // ACS_ENABLED

template <>
dtn::BundleDaemonOutput* oasys::Singleton<dtn::BundleDaemonOutput, false>::instance_ = NULL;

namespace dtn {

BundleDaemonOutput::Params::Params()
    :  not_currently_used_(false)
{}

BundleDaemonOutput::Params BundleDaemonOutput::params_;

bool BundleDaemonOutput::shutting_down_ = false;

//----------------------------------------------------------------------
BundleDaemonOutput::BundleDaemonOutput()
    : BundleEventHandler("BundleDaemonOutput", "/dtn/bundle/daemon/output"),
      Thread("BundleDaemonOutput", CREATE_JOINABLE)
{
    daemon_ = NULL;
    actions_ = NULL;
    eventq_ = NULL;
    all_bundles_     = NULL;
    pending_bundles_ = NULL;
    custody_bundles_ = NULL;
    contactmgr_ = NULL;
    fragmentmgr_ = NULL;
    reg_table_ = NULL;
    
#ifdef BPQ_ENABLED
    bpq_cache_ = NULL;
#endif /* BPQ_ENABLED */

    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
BundleDaemonOutput::~BundleDaemonOutput()
{
    delete actions_;
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post(BundleEvent* event)
{
    instance_->post_event(event);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post_at_head(BundleEvent* event)
{
    instance_->post_event(event, false);
}

//----------------------------------------------------------------------
bool
BundleDaemonOutput::post_and_wait(BundleEvent* event,
                            oasys::Notifier* notifier,
                            int timeout, bool at_back)
{
    /*
     * Make sure that we're either already started up or are about to
     * start. Otherwise the wait call below would block indefinitely.
     */
    ASSERT(! oasys::Thread::start_barrier_enabled());
    ASSERT(event->processed_notifier_ == NULL);
    event->processed_notifier_ = notifier;
    if (at_back) {
        post(event);
    } else {
        post_at_head(event);
    }
    return notifier->wait(NULL, timeout);

}

//----------------------------------------------------------------------
void
BundleDaemonOutput::post_event(BundleEvent* event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("%zu pending_events -- "
                 "%"PRIu32" processed_events",
                 event_queue_size(),
                 stats_.events_processed_);
}


//----------------------------------------------------------------------
void
BundleDaemonOutput::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_received(BundleReceivedEvent* event)
{
    (void) event;
    // Nothing to do here - just passing through to the router to decide next action
}
    
//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_injected(BundleInjectedEvent* event)
{
    (void) event;
    // Nothing to do here - just passing through to the router to decide next action
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_bundle_send(BundleSendRequest* event)
{
    LinkRef link = contactmgr_->find_link(event->link_.c_str());
    if (link == NULL){
        log_err("Cannot send bundle on unknown link %s", event->link_.c_str()); 
        return;
    }

    BundleRef br = event->bundle_;
    if (! br.object()){
        log_err("NULL bundle object in BundleSendRequest");
        return;
    }

    ForwardingInfo::action_t fwd_action =
        (ForwardingInfo::action_t)event->action_;

    actions_->queue_bundle(br.object(), link,
        fwd_action, CustodyTimerSpec::defaults_);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::event_handlers_completed(BundleEvent* event)
{
    log_debug("event handlers completed for (%p) %s", event, event->type_str());
    
    /**
     * Once bundle reception, transmission or delivery has been
     * processed by the router, pass the event to the main BundleDaemon
     * to see if it can be deleted.
     */
    if (event->type_ == BUNDLE_RECEIVED) {
        /*
         * Generate a new event to be processed by the main BundleDaemon
         * to check to see if the bundle can be deleted.
         */
        BundleReceivedEvent* bre = dynamic_cast<BundleReceivedEvent*>(event);
        BundleReceivedEvent* ev = 
            new BundleReceivedEvent_MainProcessor(bre->bundleref_.object(),
                                                  (event_source_t)bre->source_,
                                                  bre->registration_);
        ev->bytes_received_ = bre->bytes_received_;
        ev->prevhop_ = bre->prevhop_;
        ev->registration_ = bre->registration_;
        ev->link_ = bre->link_.object();
        ev->duplicate_ = bre->duplicate_;
        ev->daemon_only_ = true;

        BundleDaemon::post(ev);
    } else if (event->type_ == BUNDLE_INJECTED) {
        BundleInjectedEvent* bie = dynamic_cast<BundleInjectedEvent*>(event);
        BundleInjectedEvent* ev = 
            new BundleInjectedEvent_MainProcessor(bie->bundleref_.object(),
                                                  bie->request_id_);
        BundleDaemon::post(ev);
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::handle_event(BundleEvent* event, bool closeTransaction)
{
    (void)closeTransaction;
    dispatch_event(event);
   
    if (! event->daemon_only_) {
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(event);
        contactmgr_->handle_event(event);
    }

    // only check to see if bundle can be deleted if event is daemon_only
    event_handlers_completed(event);

    stats_.events_processed_++;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemonOutput::run()
{
    static const char* LOOP_LOG = "/dtn/bundle/daemon/output/loop";
    
    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }

    BundleEvent* event;

    daemon_ = BundleDaemon::instance();
    all_bundles_ = daemon_->all_bundles();
    pending_bundles_ = daemon_->pending_bundles();
    custody_bundles_ = daemon_->custody_bundles();

#ifdef BPQ_ENABLED
    bpq_cache_ = daemon_->bpq_cache();
#endif /* BPQ_ENABLED */

    contactmgr_ = daemon_->contactmgr();
    fragmentmgr_ = daemon_->fragmentmgr();
    router_ = daemon_->router();
    reg_table_ = daemon_->reg_table();

    last_event_.get_time();
    
    struct pollfd pollfds[1];
    struct pollfd* event_poll = &pollfds[0];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    int timeout = 1000;

    while (1) {
        if (should_stop()) {
            log_debug("BundleDaemonOutput: stopping - event queue size: %zu",
                      eventq_->size());
            break;
        }

        log_debug_p(LOOP_LOG, 
                    "BundleDaemonOutput: checking eventq_->size() > 0, its size is %zu", 
                    eventq_->size());

        if (eventq_->size() > 0) {
            bool ok = eventq_->try_pop(&event);
            ASSERT(ok);
            
            oasys::Time now;
            now.get_time();

            if (now >= event->posted_time_) {
                oasys::Time in_queue;
                in_queue = now - event->posted_time_;
                if (in_queue.sec_ > 2) {
                    log_warn_p(LOOP_LOG, "event %s was in queue for %u.%u seconds",
                               event->type_str(), in_queue.sec_, in_queue.usec_);
                }
            } else {
                log_warn_p(LOOP_LOG, "time moved backwards: "
                           "now %u.%u, event posted_time %u.%u",
                           now.sec_, now.usec_,
                           event->posted_time_.sec_, event->posted_time_.usec_);
            }
            
            
            log_debug_p(LOOP_LOG, "BundleDaemonOutput: handling event %s",
                        event->type_str());
            // handle the event
            handle_event(event);

            int elapsed = now.elapsed_ms();
            if (elapsed > 2000) {
                log_warn_p(LOOP_LOG, "event %s took %u ms to process",
                           event->type_str(), elapsed);
            }

            // record the last event time
            last_event_.get_time();

            log_debug_p(LOOP_LOG, "BundleDaemonOutput: deleting event %s",
                        event->type_str());
            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;

        log_debug_p(LOOP_LOG, "BundleDaemonOutput: poll_multiple waiting for %d ms", 
                    timeout);
        int cc = oasys::IO::poll_multiple(pollfds, 1, timeout);
        log_debug_p(LOOP_LOG, "poll returned %d", cc);

        if (cc == oasys::IOTIMEOUT) {
            log_debug_p(LOOP_LOG, "poll timeout");
            continue;

        } else if (cc <= 0) {
            log_err_p(LOOP_LOG, "unexpected return %d from poll_multiple!", cc);
            continue;
        }

        // if the event poll fired, we just go back to the top of the
        // loop to drain the queue
        if (event_poll->revents != 0) {
            log_debug_p(LOOP_LOG, "poll returned new event to handle");
        }
    }
}

} // namespace dtn
