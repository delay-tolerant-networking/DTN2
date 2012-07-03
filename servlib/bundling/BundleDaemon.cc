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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/io/IO.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Time.h>

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleStatusReport.h"
#include "BundleTimestamp.h"
#include "CustodySignal.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "contacts/Link.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "conv_layers/ConvergenceLayer.h"
#include "reg/AdminRegistration.h"
#include "reg/APIRegistration.h"
#include "reg/PingRegistration.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "session/Session.h"
#include "storage/BundleStore.h"
#include "storage/RegistrationStore.h"
#include "storage/LinkStore.h"
#include "bundling/S10Logger.h"

#ifdef BSP_ENABLED
#  include "security/Ciphersuite.h"
#  include "security/SPD.h"
#  include "security/KeyDB.h"
#endif

namespace dtn {

template <>
BundleDaemon* oasys::Singleton<BundleDaemon, false>::instance_ = NULL;

BundleDaemon::Params::Params()
    :  early_deletion_(true),
       suppress_duplicates_(true),
       accept_custody_(true),
       reactive_frag_enabled_(true),
       retry_reliable_unacked_(true),
       test_permuted_delivery_(false),
       injected_bundles_in_memory_(false),
       recreate_links_on_restart_(true)
{}

BundleDaemon::Params BundleDaemon::params_;

bool BundleDaemon::shutting_down_ = false;

//----------------------------------------------------------------------
BundleDaemon::BundleDaemon()
    : BundleEventHandler("BundleDaemon", "/dtn/bundle/daemon"),
      Thread("BundleDaemon", CREATE_JOINABLE),
      load_previous_links_executed_(false)
{
    // default local eid
    local_eid_.assign(EndpointID::NULL_EID());

    actions_ = NULL;
    eventq_ = NULL;
    
    memset(&stats_, 0, sizeof(stats_));

    all_bundles_     = new BundleList("all_bundles");
    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");

#ifdef BPQ_ENABLED
	bpq_cache_ 	     = new BPQCache();
#endif /* BPQ_ENABLED */

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();
    reg_table_ = new RegistrationTable();

    router_ = 0;

    app_shutdown_proc_ = NULL;
    app_shutdown_data_ = NULL;

    rtr_shutdown_proc_ = 0;
    rtr_shutdown_data_ = 0;
}

//----------------------------------------------------------------------
BundleDaemon::~BundleDaemon()
{
    delete pending_bundles_;
    delete custody_bundles_;

#ifdef BPQ_ENABLED
    delete bpq_cache_;
#endif /* BPQ_ENABLED */
    
    delete contactmgr_;
    delete fragmentmgr_;
    delete reg_table_;
    delete router_;

    delete actions_;
    delete eventq_;
}

//----------------------------------------------------------------------
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
    eventq_->notify_when_empty();
    BundleProtocol::init_default_processors();
#ifdef BSP_ENABLED
    Ciphersuite::init_default_ciphersuites();
    SPD::init();
    KeyDB::init();
#endif
}

//----------------------------------------------------------------------
void
BundleDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

//----------------------------------------------------------------------
void
BundleDaemon::post_at_head(BundleEvent* event)
{
    instance_->post_event(event, false);
}

//----------------------------------------------------------------------
bool
BundleDaemon::post_and_wait(BundleEvent* event,
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
BundleDaemon::post_event(BundleEvent* event, bool at_back)
{
    log_debug("posting event (%p) with type %s (at %s)",
              event, event->type_str(), at_back ? "back" : "head");
    event->posted_time_.get_time();
    eventq_->push(event, at_back);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_routing_state(oasys::StringBuffer* buf)
{
    router_->get_routing_state(buf);
    contactmgr_->dump(buf);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_bundle_stats(oasys::StringBuffer* buf)
{
	char bpq_stats[128];

#ifdef BPQ_ENABLED
    snprintf(bpq_stats, 128, "%zu bpq -- ", bpq_cache()->size());
#else
    snprintf(bpq_stats, 128,  "-- bpq -- ");
#endif /* BPQ_ENABLED */


    buf->appendf("%zu pending -- "
                 "%zu custody -- "
            	 "%s bpq -- "
                 "%u received -- "
                 "%u delivered -- "
                 "%u generated -- "
                 "%u transmitted -- "
                 "%u expired -- "
                 "%u duplicate -- "
                 "%u deleted -- "
                 "%u injected",
                 pending_bundles()->size(),
                 custody_bundles()->size(),
                 bpq_stats,
                 stats_.received_bundles_,
                 stats_.delivered_bundles_,
                 stats_.generated_bundles_,
                 stats_.transmitted_bundles_,
                 stats_.expired_bundles_,
                 stats_.duplicate_bundles_,
                 stats_.deleted_bundles_,
                 stats_.injected_bundles_);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("%zu pending_events -- "
                 "%u processed_events -- "
                 "%zu pending_timers",
                 event_queue_size(),
                 stats_.events_processed_,
                 oasys::TimerSystem::instance()->num_pending_timers());
}


//----------------------------------------------------------------------
void
BundleDaemon::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));

    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::reset_stats");
    
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    for (iter = links->begin(); iter != links->end(); ++iter) {
        (*iter)->reset_stats();
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_status_report(Bundle* orig_bundle,
                                     BundleStatusReport::flag_t flag,
                                     status_report_reason_t reason)
{
    log_debug("generating return receipt status report, "
              "flag = 0x%x, reason = 0x%x", flag, reason);
    
    Bundle* report = new Bundle();
    BundleStatusReport::create_status_report(report, orig_bundle,
                                             local_eid_, flag, reason);
    
    BundleReceivedEvent e(report, EVENTSRC_ADMIN);
    handle_event(&e);
	s10_bundle(S10_TXADMIN,report,NULL,0,0,orig_bundle,"status report");
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_custody_signal(Bundle* bundle, bool succeeded,
                                      custody_signal_reason_t reason)
{
    if (bundle->local_custody()) {
        log_err("send_custody_signal(*%p): already have local custody",
                bundle);
        return;
    }

    if (bundle->custodian().equals(EndpointID::NULL_EID())) {
        log_err("send_custody_signal(*%p): current custodian is NULL_EID",
                bundle);
        return;
    }
    
    Bundle* signal = new Bundle();
    CustodySignal::create_custody_signal(signal, bundle, local_eid_,
                                         succeeded, reason);
    
    BundleReceivedEvent e(signal, EVENTSRC_ADMIN);
    handle_event(&e);
	s10_bundle(S10_TXADMIN,signal,NULL,0,0,bundle,"custody signal");

}

//----------------------------------------------------------------------
void
BundleDaemon::cancel_custody_timers(Bundle* bundle)
{
    oasys::ScopeLock l(bundle->lock(), "BundleDaemon::cancel_custody_timers");
    
    CustodyTimerVec::iterator iter;
    for (iter =  bundle->custody_timers()->begin();
         iter != bundle->custody_timers()->end();
         ++iter)
    {
        bool ok = (*iter)->cancel();
        if (!ok) {
            log_crit("unexpected error cancelling custody timer for bundle *%p",
                     bundle);
        }
        
        // the timer will be deleted when it bubbles to the top of the
        // timer queue
    }
    
    bundle->custody_timers()->clear();
}

//----------------------------------------------------------------------
void
BundleDaemon::accept_custody(Bundle* bundle)
{
    log_info("accept_custody *%p", bundle);
    
    if (bundle->local_custody()) {
        log_err("accept_custody(*%p): already have local custody",
                bundle);
        return;
    }

    if (bundle->custodian().equals(local_eid_)) {
        log_err("send_custody_signal(*%p): "
                "current custodian is already local_eid",
                bundle);
        return;
    }
    
    // send a custody acceptance signal to the current custodian (if
    // it is someone, and not the null eid)
    if (! bundle->custodian().equals(EndpointID::NULL_EID())) {
        generate_custody_signal(bundle, true, BundleProtocol::CUSTODY_NO_ADDTL_INFO);
    }
	// next line is  for S10
	EndpointID prev_custodian=bundle->custodian();

    // now we mark the bundle to indicate that we have custody and add
    // it to the custody bundles list
    bundle->mutable_custodian()->assign(local_eid_);
    bundle->set_local_custody(true);
    actions_->store_update(bundle);
    
    custody_bundles_->push_back(bundle);

    // finally, if the bundle requested custody acknowledgements,
    // deliver them now
    if (bundle->custody_rcpt()) {
        generate_status_report(bundle, 
                               BundleStatusReport::STATUS_CUSTODY_ACCEPTED);
    }
	s10_bundle(S10_TAKECUST,bundle,prev_custodian.c_str(),0,0,NULL,NULL);
}

//----------------------------------------------------------------------
void
BundleDaemon::release_custody(Bundle* bundle)
{
    log_info("release_custody *%p", bundle);
    
    if (!bundle->local_custody()) {
        log_err("release_custody(*%p): don't have local custody",
                bundle);
        return;
    }

    cancel_custody_timers(bundle);

    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_local_custody(false);
    actions_->store_update(bundle);

    custody_bundles_->erase(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      Registration* registration)
{
    ASSERT(!bundle->is_fragment());

    ForwardingInfo::state_t state = bundle->fwdlog()->get_latest_entry(registration);
    switch(state) {
    case ForwardingInfo::PENDING_DELIVERY:
        // Expected case
        log_debug("delivering bundle to reg previously marked as PENDING_DELIVERY");
        break;
    case ForwardingInfo::DELIVERED:
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since already delivered",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
        return;
    default:
        log_warn("deliver_to_registration called with bundle not marked " \
                 "as PENDING_DELIVERY.  Delivering anyway...");
        bundle->fwdlog()->add_entry(registration,
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::DELIVERED);
        break;
    }

#if 0
    if (state != ForwardingInfo::NONE)
    {
        ASSERT(state == ForwardingInfo::DELIVERED);
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since already delivered",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
        return;
    }
#endif
    
    // if this is a session registration and doesn't have either the
    // SUBSCRIBE or CUSTODY bits (i.e. it's publish-only), don't
    // deliver the bundle
    if (registration->session_flags() == Session::PUBLISH)
    {
        log_debug("not delivering bundle *%p to registration %d (%s) "
                  "since it's a publish-only session registration",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
        return;
    }

    log_debug("delivering bundle *%p to registration %d (%s)",
              bundle, registration->regid(),
              registration->endpoint().c_str());

    if (registration->deliver_if_not_duplicate(bundle)) {
        // XXX/demmer this action could be taken from a registration
        // flag, i.e. does it want to take a copy or the actual
        // delivery of the bundle

        APIRegistration* api_reg = dynamic_cast<APIRegistration*>(registration);
        if (api_reg == NULL) {
            log_debug("not updating registration %s",
                      registration->endpoint().c_str());
        } else {
            log_debug("updating registration %s",
                      api_reg->endpoint().c_str());
            bundle->fwdlog()->update(registration,
                                     ForwardingInfo::DELIVERED);
            oasys::ScopeLock l(api_reg->lock(), "new bundle for reg");
            api_reg->update();
        }
    } else {
        log_notice("suppressing duplicate delivery of bundle *%p "
                   "to registration %d (%s)",
                   bundle, registration->regid(),
                   registration->endpoint().c_str());
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::check_local_delivery(Bundle* bundle, bool deliver)
{
    log_debug("checking for matching registrations for bundle *%p and deliver(%d)",
              bundle, deliver);

    RegistrationList matches;
    RegistrationList::iterator iter;

    reg_table_->get_matching(bundle->dest(), &matches);

    if (deliver) {
        ASSERT(!bundle->is_fragment());
        for (iter = matches.begin(); iter != matches.end(); ++iter) {
            Registration* registration = *iter;
            // deliver_to_registration(bundle, registration);

            /*
             * Mark the bundle as needing delivery to the registration.
             * Marking is durable and should be transactionalized together
             * with storing the bundle payload and metadata to disk if
             * the storage mechanism supports it (i.e. if transactions are
             * supported, we should be in one).
             */
            bundle->fwdlog()->add_entry(registration,
                                        ForwardingInfo::FORWARD_ACTION,
                                        ForwardingInfo::PENDING_DELIVERY);
            BundleDaemon::post(new DeliverBundleToRegEvent(bundle, registration->regid()));
            log_debug("Marking bundle as PENDING_DELIVERY to reg %d", registration->regid());
        }
    }

    // Durably store our decisions about the registrations to which the bundle
    // should be delivered.  Actual delivery happens when the
    // DeliverBundleToRegEvent we just posted is processed.
    if (matches.size()>0) {
        log_debug("XXX Need to update bundle if not came from store.");
        actions_->store_update(bundle);
    }

    return (matches.size() > 0) || bundle->dest().subsume(local_eid_);
}

//----------------------------------------------------------------------
void
BundleDaemon::check_and_deliver_to_registrations(Bundle* bundle, const EndpointID& reg_eid)
{
    int num;
    log_debug("checking for matching entries in table for %s", reg_eid.c_str());

    RegistrationList matches;
    RegistrationList::iterator iter;

    num = reg_table_->get_matching(reg_eid, &matches);

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        Registration* registration = *iter;
        deliver_to_registration(bundle, registration);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delete(BundleDeleteRequest* request)
{
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (request->bundle_.object()) {
        log_info("BUNDLE_DELETE: bundle *%p (reason %s)",
                 request->bundle_.object(),
                 BundleStatusReport::reason_to_str(request->reason_));
        delete_bundle(request->bundle_, request->reason_);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_acknowledged_by_app(BundleAckEvent* ack)
{
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    log_debug("ack from regid(%d): (%s: (%qu, %qu))",
              ack->regid_,
              ack->sourceEID_.c_str(),
              ack->creation_ts_.seconds_, ack->creation_ts_.seqno_);

    //const RegistrationTable* reg_table = BundleDaemon::instance()->reg_table();

    // Make sure we're happy with the registration provided.
    Registration* reg = reg_table_->get(ack->regid_);
    if ( reg==NULL ) {
        log_debug("BAD: can't get reg from regid(%d)", ack->regid_);
        return;
    }
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_debug("Acking registration is not an APIRegistration");
        return;
    }

    api_reg->bundle_ack(ack->sourceEID_, ack->creation_ts_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_accept(BundleAcceptRequest* request)
{
    *request->result_ =
        router_->accept_bundle(request->bundle_.object(), request->reason_);

    log_info("BUNDLE_ACCEPT_REQUEST: bundle *%p %s (reason %s)",
             request->bundle_.object(),
             *request->result_ ? "accepted" : "not accepted",
             BundleStatusReport::reason_to_str(*request->reason_));
}
    
//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_received(BundleReceivedEvent* event)
{
    log_info_p("/dtn/bundle/protocol", "handle_bundle_received: begin");

    const BundleRef& bundleref = event->bundleref_;
    Bundle* bundle = bundleref.object();

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    // update statistics and store an appropriate event descriptor
    const char* source_str = "";
    switch (event->source_) {
    case EVENTSRC_PEER:
        stats_.received_bundles_++;
		if (event->link_.object()) {
			s10_bundle(S10_RX,bundle,event->link_.object()->nexthop(),0,0,NULL,"link");
		} else {
			s10_bundle(S10_RX,bundle,event->prevhop_.c_str(),0,0,NULL,"nolink");
		}
        break;
        
    case EVENTSRC_APP:
        stats_.received_bundles_++;
        source_str = " (from app)";
	oasys::DurableStore::instance()->make_transaction_durable();

		if (event->registration_ != NULL) {
			s10_bundle(S10_FROMAPP,bundle,event->registration_->endpoint().c_str(),0,0,NULL,NULL);
		} else {
			s10_bundle(S10_FROMAPP,bundle,"dunno",0,0,NULL,NULL);
		}
        break;
        
    case EVENTSRC_STORE:
        source_str = " (from data store)";
		s10_bundle(S10_FROMDB,bundle,NULL,0,0,NULL,NULL);
        break;
        
    case EVENTSRC_ADMIN:
        stats_.generated_bundles_++;
        source_str = " (generated)";
        break;
        
    case EVENTSRC_FRAGMENTATION:
        stats_.generated_bundles_++;
        source_str = " (from fragmentation)";
		s10_bundle(S10_OHCRAP,bundle,NULL,0,0,NULL,"__FILE__:__LINE__");
		log_debug("S10_OHCRAP - event source: %s", source_str);
        break;

    case EVENTSRC_ROUTER:
        stats_.generated_bundles_++;
        source_str = " (from router)";
		s10_bundle(S10_OHCRAP,bundle,NULL,0,0,NULL,"__FILE__:__LINE__");
		log_debug("S10_OHCRAP - event source: %s", source_str);
        break;

    case EVENTSRC_CACHE:
        stats_.generated_bundles_++;
        source_str = " (from cache)";
        s10_bundle(S10_FROMCACHE,bundle,NULL,0,0,NULL,"__FILE__:__LINE__");
        break;

    default:
		s10_bundle(S10_OHCRAP,bundle,NULL,0,0,NULL,"__FILE__:__LINE__");
		log_debug("S10_OHCRAP - unknown event source: %d", event->source_);
        NOTREACHED;
    }

    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<2048> buf;
        buf.appendf("BUNDLE_RECEIVED%s: prevhop %s (%u bytes recvd)\n",
                    source_str, event->prevhop_.c_str(), event->bytes_received_);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_DEBUG, buf.c_str());
    } else {
        log_info("BUNDLE_RECEIVED%s *%p prevhop %s (%u bytes recvd)",
                 source_str, bundle, event->prevhop_.c_str(), event->bytes_received_);
    }
    
    // XXX/kscott Logging bundle reception to forwarding log moved to below.

    // log a warning if the bundle doesn't have any expiration time or
    // has a creation time that's in the future. in either case, we
    // proceed as normal
    if (bundle->expiration() == 0) {
        log_warn("bundle id %d arrived with zero expiration time",
                 bundle->bundleid());
    }

    u_int32_t now = BundleTimestamp::get_current_time();
    if ((bundle->creation_ts().seconds_ > now) &&
        (bundle->creation_ts().seconds_ - now > 30000))
    {
        log_warn("bundle id %d arrived with creation time in the future "
                 "(%llu > %u)",
                 bundle->bundleid(), bundle->creation_ts().seconds_, now);
    }
   
    // log a warning if the bundle's creation timestamp is 0, indicating 
    // that an AEB should exist 
    if (bundle->creation_ts().seconds_ == 0) {
        log_info_p("/dtn/bundle/protocol", "creation time is 0, AEB should exist");
        log_warn_p("dtn/bundle/extblock/aeb", "AEB: bundle id %d arrived with creation time of 0", 
                 bundle->bundleid());
    }

    /*
     * If a previous hop block wasn't included, but we know the remote
     * endpoint id of the link where the bundle arrived, assign the
     * prevhop_ field in the bundle so it's available for routing.
     */
    if (event->source_ == EVENTSRC_PEER)
    {
        if (bundle->prevhop()       == EndpointID::NULL_EID() ||
            bundle->prevhop().str() == "")
        {
            bundle->mutable_prevhop()->assign(event->prevhop_);
        }

        if (bundle->prevhop() != event->prevhop_)
        {
            log_warn("previous hop mismatch: prevhop header contains '%s' but "
                     "convergence layer indicates prevhop is '%s'",
                     bundle->prevhop().c_str(),
                     event->prevhop_.c_str());
        }
    }
    
    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     */
    if (event->source_ == EVENTSRC_PEER) {
        ASSERT(event->bytes_received_ != 0);
        fragmentmgr_->try_to_convert_to_fragment(bundle);
    }

    /*
     * validate a bundle, including all bundle blocks, received from a peer
     */
    if (event->source_ == EVENTSRC_PEER) { 

        /*
         * Check all BlockProcessors to validate the bundle.
         */
        status_report_reason_t
            reception_reason = BundleProtocol::REASON_NO_ADDTL_INFO,
            deletion_reason = BundleProtocol::REASON_NO_ADDTL_INFO;

        
        log_info("handle_bundle_received: validating bundle: calling BlockProcessors?");
        bool valid = BundleProtocol::validate(bundle,
                                              &reception_reason,
                                              &deletion_reason);
        
        /*
         * Send the reception receipt if requested within the primary
         * block or some other error occurs that requires a reception
         * status report but may or may not require deleting the whole
         * bundle.
         */
        if (bundle->receive_rcpt() ||
            reception_reason != BundleProtocol::REASON_NO_ADDTL_INFO)
        {
            generate_status_report(bundle, BundleStatusReport::STATUS_RECEIVED,
                                   reception_reason);
        }

        /*
         * If the bundle is valid, probe the router to see if it wants
         * to accept the bundle.
         */
        bool accept_bundle = false;
        if (valid) {
            int reason = BundleProtocol::REASON_NO_ADDTL_INFO;
            accept_bundle = router_->accept_bundle(bundle, &reason);
            deletion_reason = static_cast<BundleProtocol::status_report_reason_t>(reason);
        }
        
        /*
         * Delete a bundle if a validation error was encountered or
         * the router doesn't want to accept the bundle, in both cases
         * not giving the reception event to the router.
         */
        if (!accept_bundle) {
            delete_bundle(bundleref, deletion_reason);
            event->daemon_only_ = true;
            return;
        }
    }
    
    /*
     * Check if the bundle is a duplicate, i.e. shares a source id,
     * timestamp, and fragmentation information with some other bundle
     * in the system.
     */
    Bundle* duplicate = find_duplicate(bundle);
    if (duplicate != NULL) {
        log_notice("got duplicate bundle: %s -> %s creation %llu.%llu",
                   bundle->source().c_str(),
                   bundle->dest().c_str(),
                   bundle->creation_ts().seconds_,
                   bundle->creation_ts().seqno_);
		s10_bundle(S10_DUP,bundle,NULL,0,0,NULL,"__FILE__:__LINE__");

        stats_.duplicate_bundles_++;
        
        if (bundle->custody_requested() && duplicate->local_custody())
        {
            generate_custody_signal(bundle, false,
                                    BundleProtocol::CUSTODY_REDUNDANT_RECEPTION);
        }

        if (params_.suppress_duplicates_) {
            // since we don't want the bundle to be processed by the rest
            // of the system, we mark the event as daemon_only (meaning it
            // won't be forwarded to routers) and return, which should
            // eventually remove all references on the bundle and then it
            // will be deleted
            event->daemon_only_ = true;
            return;
        }

        // The BP says that the "dispatch pending" retention constraint
        // must be removed from this bundle if there is a duplicate we
        // currently have custody of. This would cause the bundle to have
        // no retention constraints and it now "may" be discarded. Assuming
        // this means it is supposed to be discarded, we have to suppress
        // a duplicate in this situation regardless of the parameter
        // setting. We would then be relying on the custody transfer timer
        // to cause a new forwarding attempt in the case of routing loops
        // instead of the receipt of a duplicate, so in theory we can indeed
        // suppress this bundle. It may not be strictly required to do so,
        // in which case we can remove the following block.
        if (bundle->custody_requested() && duplicate->local_custody()) {
            event->daemon_only_ = true;
            return;
        }

    }

#ifdef BPQ_ENABLED
    /*
     * If the BPQ cache has been enabled and the bundle event is coming from
     * one of these sources. Try to handle the a BPQ extension block. This
     * will bounce back out if there is no extension block present.
     */
    if ( bpq_cache()->cache_enabled_ ) {
    	// try to handle a BPQ block
    	if ( event->source_ == EVENTSRC_APP   ||
			 event->source_ == EVENTSRC_PEER  ||
			 event->source_ == EVENTSRC_STORE ||
			 event->source_ == EVENTSRC_FRAGMENTATION) {

			handle_bpq_block(bundle, event);
		}
    }

    /*
     * If the bundle contains a BPQ query that was successfully answered
     * a response has already been sent and the query need not be forwarded
     * so return from this function
     */
    if ( event->daemon_only_ ) {
        return;
    }
#endif /* BPQ_ENABLED */

    /*
     * Add the bundle to the master pending queue and the data store
     * (unless the bundle was just reread from the data store on startup)
     *
     * Note that if add_to_pending returns false, the bundle has
     * already expired so we immediately return instead of trying to
     * deliver and/or forward the bundle. Otherwise there's a chance
     * that expired bundles will persist in the network.
     *
     * add_to_pending writes the bundle to the durable store
     */
    bool ok_to_route =
        add_to_pending(bundle, (event->source_ != EVENTSRC_STORE));

    if (!ok_to_route) {
        event->daemon_only_ = true;
        return;
    }
    
    // log the reception in the bundle's forwarding log
    if (event->source_ == EVENTSRC_PEER && event->link_ != NULL)
    {
        bundle->fwdlog()->add_entry(event->link_,
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::RECEIVED);
    }
    else if (event->source_ == EVENTSRC_APP)
    {
        if (event->registration_ != NULL) {
            bundle->fwdlog()->add_entry(event->registration_,
                                        ForwardingInfo::FORWARD_ACTION,
                                        ForwardingInfo::RECEIVED);
        }
    }

    /*
     * If the bundle is a custody bundle and we're configured to take
     * custody, then do so. In case the event was delivered due to a
     * reload from the data store, then if we have local custody, make
     * sure it's added to the custody bundles list.
     */
    if (bundle->custody_requested() && params_.accept_custody_
        && (duplicate == NULL || !duplicate->local_custody()))
    {
        if (event->source_ != EVENTSRC_STORE) {
            accept_custody(bundle);
        
        } else if (bundle->local_custody()) {
            custody_bundles_->push_back(bundle);
        }
    }

    /*
     * If this bundle is a duplicate and it has not been suppressed, we
     * can assume the bundle it duplicates has already been delivered or
     * added to the fragment manager if required, so do not do so again.
     * We can bounce out now.
     * XXX/jmmikkel If the extension blocks differ and we care to
     * do something with them, we can't bounce out quite yet.
     */
    if (duplicate != NULL) {
        return;
    }

    /*
     * Check if this is a complete (non-fragment) bundle that
     * obsoletes any fragments that we know about.
     */
    if (! bundle->is_fragment()) {
        fragmentmgr_->delete_obsoleted_fragments(bundle);
    }

    /*
     * Deliver the bundle to any local registrations that it matches,
     * unless it's generated by the router or is a bundle fragment.
     * Delivery of bundle fragments is deferred until after re-assembly.
     */
    if ( event->source_==EVENTSRC_STORE ) {
        generate_delivery_events(bundle);
    } else {
        bool is_local =
            check_local_delivery(bundle,
                                 (event->source_        != EVENTSRC_ROUTER) &&
                                 (bundle->is_fragment() == false));
        /*
         * Re-assemble bundle fragments that are destined to the local node.
         */
        if (bundle->is_fragment() && is_local) {
            log_debug("deferring delivery of bundle *%p "
                      "since bundle is a fragment", bundle);
            fragmentmgr_->process_for_reassembly(bundle);
        }

    }

    /*
     * Finally, bounce out so the router(s) can do something further
     * with the bundle in response to the event before we check to 
     * see if it needs to be delivered locally.
     */
    log_info_p("/dtn/bundle/protocol", "BundleDaemon::handle_bundle_received: end");
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_deliver_bundle_to_reg(DeliverBundleToRegEvent* event)
{
    const RegistrationTable* reg_table = 
            BundleDaemon::instance()->reg_table();

    const BundleRef& bundleref = event->bundleref_;
    u_int32_t regid = event->regid_;
  
    Registration* registration = NULL;

    registration = reg_table->get(regid);

    if (registration==NULL) {
        log_warn("Can't find registration %d any more", regid);
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    Bundle* bundle = bundleref.object();
    log_debug("Delivering bundle id:%d to registration %d %s",
              bundle->bundleid(),
              registration->regid(),
              registration->endpoint().c_str());

    deliver_to_registration(bundle, registration);
 }

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
              bundle->bundleid(),link->name());
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
    // If an event about a bundle bound for particular link is posted after another,
    // which it might contradict, the BundleDaemon need not reprocess the event.
    // The router (DP) might, however, be interested in the new status of the send.
    if(blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_transmit event about "
                 "bundle id:%d -> %s (%s)",
                 bundle->bundleid(),
                 link->name(),
                 link->nexthop());
        return;
    }
    
    /*
     * Update statistics and remove the bundle from the link inflight
     * queue. Note that the link's queued length statistics must
     * always be decremented by the full formatted size of the bundle,
     * yet the transmitted length is only the amount reported by the
     * event.
     */
    size_t total_len = BundleProtocol::total_length(blocks);
    
    stats_.transmitted_bundles_++;
    
    link->stats()->bundles_transmitted_++;
    link->stats()->bytes_transmitted_ += event->bytes_sent_;

    // remove the bundle from the link's in flight queue
    if (link->del_from_inflight(event->bundleref_, total_len)) {
        log_debug("removed bundle id:%d from link %s inflight queue",
                 bundle->bundleid(),
                 link->name());
    } else {
        log_warn("bundle id:%d not on link %s inflight queue",
                 bundle->bundleid(),
                 link->name());
    }
    
    // verify that the bundle is not on the link's to-be-sent queue
    if (link->del_from_queue(event->bundleref_, total_len)) {
        log_warn("bundle id:%d unexpectedly on link %s queue in transmitted event",
                 bundle->bundleid(),
                 link->name());
    }
    
    log_info("BUNDLE_TRANSMITTED id:%d (%u bytes_sent/%u reliable) -> %s (%s)",
             bundle->bundleid(),
             event->bytes_sent_,
             event->reliably_sent_,
             link->name(),
             link->nexthop());
	s10_bundle(S10_TX,bundle,link->nexthop(),0,0,NULL,NULL);


    /*
     * If we're configured to wait for reliable transmission, then
     * check the special case where we transmitted some or all a
     * bundle but nothing was acked. In this case, we create a
     * transmission failed event in the forwarding log and don't do
     * any of the rest of the processing below.
     *
     * Note also the special care taken to handle a zero-length
     * bundle. XXX/demmer this should all go away when the lengths
     * include both the header length and the payload length (in which
     * case it's never zero).
     *
     * XXX/demmer a better thing to do (maybe) would be to record the
     * lengths in the forwarding log as part of the transmitted entry.
     */
    if (params_.retry_reliable_unacked_ &&
        link->is_reliable() &&
        (event->bytes_sent_ != event->reliably_sent_) &&
        (event->reliably_sent_ == 0))
    {
        bundle->fwdlog()->update(link, ForwardingInfo::TRANSMIT_FAILED);
        log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
                  bundle->bundleid(),link->name());
        BundleProtocol::delete_blocks(bundle, link);

        log_warn("XXX/demmer fixme transmitted special case");
        
        return;
    }

    /*
     * Grab the latest forwarding log state so we can find the custody
     * timer information (if any).
     */
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog()->get_latest_entry(link, &fwdinfo);
    if(!ok)
    {
        oasys::StringBuffer buf;
        bundle->fwdlog()->dump(&buf);
        log_debug("%s",buf.c_str());
    }
    ASSERTF(ok, "no forwarding log entry for transmission");
    // ASSERT(fwdinfo.state() == ForwardingInfo::QUEUED);
    if (fwdinfo.state() != ForwardingInfo::QUEUED) {
        log_err("*%p fwdinfo state %s != expected QUEUED",
                bundle, ForwardingInfo::state_to_str(fwdinfo.state()));
    }
    
    /*
     * Update the forwarding log indicating that the bundle is no
     * longer in flight.
     */
    log_debug("updating forwarding log entry on *%p for *%p to TRANSMITTED",
              bundle, link.object());
    bundle->fwdlog()->update(link, ForwardingInfo::TRANSMITTED);
                            
    /*
     * Check for reactive fragmentation. If the bundle was only
     * partially sent, then a new bundle received event for the tail
     * part of the bundle will be processed immediately after this
     * event.
     */
    if (link->reliable_) {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 blocks,
                                                 event->reliably_sent_);
    } else {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 blocks,
                                                 event->bytes_sent_);
    }

    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
    log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
              bundle->bundleid(),link->name());
    BundleProtocol::delete_blocks(bundle, link);
    blocks = NULL;

    /*
     * Generate the forwarding status report if requested
     */
    if (bundle->forward_rcpt()) {
        generate_status_report(bundle, BundleStatusReport::STATUS_FORWARDED);
    }
    
    /*
     * Schedule a custody timer if we have custody.
     */
    if (bundle->local_custody()) {
        bundle->custody_timers()->push_back(
            new CustodyTimer(fwdinfo.timestamp(),
                             fwdinfo.custody_spec(),
                             bundle, link));
        
        // XXX/TODO: generate failed custodial signal for "forwarded
        // over unidirectional link" if the bundle has the retention
        // constraint "custody accepted" and all of the nodes in the
        // minimum reception group of the endpoint selected for
        // forwarding are known to be unable to send bundles back to
        // this node
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    // update statistics
    stats_.delivered_bundles_++;
    
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    /*
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_DELIVERED id:%d (%zu bytes) -> regid %d (%s)",
             bundle->bundleid(), bundle->payload().length(),
             event->registration_->regid(),
             event->registration_->endpoint().c_str());
	s10_bundle(S10_DELIVERED,bundle,event->registration_->endpoint().c_str(),0,0,NULL,NULL);

    /*
     * Generate the delivery status report if requested.
     */
    if (bundle->delivery_rcpt())
    {
        generate_status_report(bundle, BundleStatusReport::STATUS_DELIVERED);
    }

    /*
     * If this is a custodial bundle and it was delivered, we either
     * release custody (if we have it), or send a custody signal to
     * the current custodian indicating that the bundle was
     * successfully delivered, unless there is no current custodian
     * (the eid is still dtn:none).
     */
    if (bundle->custody_requested())
    {
        if (bundle->local_custody()) {
            release_custody(bundle);

        } else if (bundle->custodian().equals(EndpointID::NULL_EID())) {
            log_info("custodial bundle *%p delivered before custody accepted",
                     bundle);

        } else {
            generate_custody_signal(bundle, true,
                                    BundleProtocol::CUSTODY_NO_ADDTL_INFO);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_expired(BundleExpiredEvent* event)
{
    // update statistics
    stats_.expired_bundles_++;
    
    const BundleRef& bundle = event->bundleref_;

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    log_info("BUNDLE_EXPIRED *%p", bundle.object());

    // note that there may or may not still be a pending expiration
    // timer, since this event may be coming from the console, so we
    // just fall through to delete_bundle which will cancel the timer

    delete_bundle(bundle, BundleProtocol::REASON_LIFETIME_EXPIRED);
    
    // fall through to notify the routers
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_send(BundleSendRequest* event)
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
BundleDaemon::handle_bundle_cancel(BundleCancelRequest* event)
{
    BundleRef br = event->bundle_;

    if(!br.object()) {
        log_err("NULL bundle object in BundleCancelRequest");
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    // If the request has a link name, we are just canceling the send on
    // that link.
    if (!event->link_.empty()) {
        LinkRef link = contactmgr_->find_link(event->link_.c_str());
        if (link == NULL) {
            log_err("BUNDLE_CANCEL no link with name %s", event->link_.c_str());
            return;
        }

        log_info("BUNDLE_CANCEL bundle %d on link %s", br->bundleid(),
                event->link_.c_str());
        
        actions_->cancel_bundle(br.object(), link);
    }
    
    // If the request does not have a link name, the bundle itself has been
    // canceled (probably by an application).
    else {
        delete_bundle(br);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    LinkRef link = event->link_;
    
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    log_info("BUNDLE_CANCELLED id:%d -> %s (%s)",
            bundle->bundleid(),
            link->name(),
            link->nexthop());
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
              bundle->bundleid(), link->name());
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed 
    // messages. If an event about a bundle bound for particular link is 
    // posted after  another, which it might contradict, the BundleDaemon 
    // need not reprocess the event. The router (DP) might, however, be 
    // interested in the new status of the send.
    if (blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_cancelled event "
                 "about bundle id:%d -> %s (%s)",
                 bundle->bundleid(),
                 link->name(),
                 link->nexthop());
        return;
    }

    /*
     * The bundle should no longer be on the link queue or on the
     * inflight queue if it was cancelled.
     */
    if (link->queue()->contains(bundle))
    {
        log_warn("cancelled bundle id:%d still on link %s queue",
                 bundle->bundleid(), link->name());
    }

    /*
     * The bundle should no longer be on the link queue or on the
     * inflight queue if it was cancelled.
     */
    if (link->inflight()->contains(bundle))
    {
        log_warn("cancelled bundle id:%d still on link %s inflight list",
                 bundle->bundleid(), link->name());
    }

    /*
     * Update statistics. Note that the link's queued length must
     * always be decremented by the full formatted size of the bundle.
     */
    link->stats()->bundles_cancelled_++;
    
    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
    log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
              bundle->bundleid(), link->name());
    BundleProtocol::delete_blocks(bundle, link);
    blocks = NULL;

    /*
     * Update the forwarding log.
     */
    log_debug("trying to update the forwarding log for "
              "bundle id:%d on link %s to state CANCELLED",
              bundle->bundleid(), link->name());
    bundle->fwdlog()->update(link, ForwardingInfo::CANCELLED);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_inject(BundleInjectRequest* event)
{
    // link isn't used at the moment, so don't bother searching for
    // it.  TODO:  either remove link ID and forward action from
    // RequestInjectBundle, or make link ID optional and send the
    // bundle on the link if specified.
    /*
      LinkRef link = contactmgr_->find_link(event->link_.c_str());
      if (link == NULL) return;
    */

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    EndpointID src(event->src_); 
    EndpointID dest(event->dest_); 
    if ((! src.valid()) || (! dest.valid())) return;
    
    // The bundle's source EID must be either dtn:none or an EID 
    // registered at this node.
    const RegistrationTable* reg_table = 
            BundleDaemon::instance()->reg_table();
    std::string base_reg_str = src.uri().scheme() + "://" + src.uri().host();
    
    if (!reg_table->get(EndpointIDPattern(base_reg_str)) && 
         src != EndpointID::NULL_EID()) {
        log_err("this node is not a member of the injected bundle's source "
                "EID (%s)", src.str().c_str());
        return;
    }
    
    // The new bundle is placed on the pending queue but not
    // in durable storage (no call to BundleActions::inject_bundle)
    Bundle* bundle = new Bundle(params_.injected_bundles_in_memory_ ? 
                                BundlePayload::MEMORY : BundlePayload::DISK);
    
    bundle->mutable_source()->assign(src);
    bundle->mutable_dest()->assign(dest);
    
    if (! bundle->mutable_replyto()->assign(event->replyto_))
        bundle->mutable_replyto()->assign(EndpointID::NULL_EID());

    if (! bundle->mutable_custodian()->assign(event->custodian_))
        bundle->mutable_custodian()->assign(EndpointID::NULL_EID()); 

    // bundle COS defaults to COS_BULK
    bundle->set_priority(event->priority_);

    // bundle expiration on remote dtn nodes
    // defaults to 5 minutes
    if(event->expiration_ == 0)
        bundle->set_expiration(300);
    else
        bundle->set_expiration(event->expiration_);
    
    // set the payload (by hard linking, then removing original)
    bundle->mutable_payload()->
        replace_with_file(event->payload_file_.c_str());
    log_debug("bundle payload size after replace_with_file(): %zd", 
              bundle->payload().length());
    oasys::IO::unlink(event->payload_file_.c_str(), logpath_);

    /*
     * Deliver the bundle to any local registrations that it matches,
     * unless it's generated by the router or is a bundle fragment.
     * Delivery of bundle fragments is deferred until after re-assembly.
     */
    bool is_local = check_local_delivery(bundle, !bundle->is_fragment());
    
    /*
     * Re-assemble bundle fragments that are destined to the local node.
     */
    if (bundle->is_fragment() && is_local) {
        log_debug("deferring delivery of injected bundle *%p "
                  "since bundle is a fragment", bundle);
        fragmentmgr_->process_for_reassembly(bundle);
    }

    // The injected bundle is no longer sent automatically. It is
    // instead added to the pending queue so that it can be resent
    // or sent on multiple links.

    // If add_to_pending returns false, the bundle has already expired
    if (add_to_pending(bundle, 0))
        BundleDaemon::post(new BundleInjectedEvent(bundle, event->request_id_));
    
    ++stats_.injected_bundles_;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_query(BundleQueryRequest*)
{
    BundleDaemon::post_at_head(new BundleReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_report(BundleReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_query(BundleAttributesQueryRequest* request)
{
    BundleRef &br = request->bundle_;
    if (! br.object()) return; // XXX or should it post an empty report?

    log_debug(
        "BundleDaemon::handle_bundle_attributes_query: query %s, bundle *%p",
        request->query_id_.c_str(), br.object());

    // we need to keep a reference to the bundle because otherwise it may
    // be deleted before the event is handled
    BundleDaemon::post(
        new BundleAttributesReportEvent(request->query_id_,
                                        br,
                                        request->attribute_names_,
                                        request->metadata_blocks_));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_report(BundleAttributesReportEvent* event)
{
    (void)event;
    log_debug("BundleDaemon::handle_bundle_attributes_report: query %s",
              event->query_id_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_added(RegistrationAddedEvent* event)
{
    Registration* registration = event->registration_;
    log_info("REGISTRATION_ADDED %d %s",
             registration->regid(), registration->endpoint().c_str());

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (!reg_table_->add(registration,
                         (event->source_ == EVENTSRC_APP) ? true : false))
    {
        log_err("error adding registration %d to table",
                registration->regid());
    }
    
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "BundleDaemon::handle_registration_added");
    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        Bundle* bundle = *iter;

        if (!bundle->is_fragment() &&
            registration->endpoint().match(bundle->dest())) {
            deliver_to_registration(bundle, registration);
        }
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_removed(RegistrationRemovedEvent* event)
{

    Registration* registration = event->registration_;
    log_info("REGISTRATION_REMOVED %d %s",
             registration->regid(), registration->endpoint().c_str());


    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (!reg_table_->del(registration->regid())) {
        log_err("error removing registration %d from table",
                registration->regid());
        return;
    }

    post(new RegistrationDeleteRequest(registration));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_expired(RegistrationExpiredEvent* event)
{
    Registration* registration = event->registration_;

    if (reg_table_->get(registration->regid()) == NULL) {
        // this shouldn't ever happen
        log_err("REGISTRATION_EXPIRED -- dead regid %d", registration->regid());
        return;
    }
    
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    registration->set_expired(true);
    
    if (registration->active()) {
        // if the registration is currently active (i.e. has a
        // binding), we wait for the binding to clear, which will then
        // clean up the registration
        log_info("REGISTRATION_EXPIRED %d -- deferred until binding clears",
                 registration->regid());
    } else {
        // otherwise remove the registration from the table
        log_info("REGISTRATION_EXPIRED %d", registration->regid());
        reg_table_->del(registration->regid());
        post_at_head(new RegistrationDeleteRequest(registration));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_delete(RegistrationDeleteRequest* request)
{
    log_info("REGISTRATION_DELETE %d", request->registration_->regid());

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    delete request->registration_;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_created(LinkCreatedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);

    if (link->isdeleted()) {
        log_warn("BundleDaemon::handle_link_created: "
                 "link %s deleted prior to full creation", link->name());
        event->daemon_only_ = true;
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (link->reincarnated())
    {
    	if (! LinkStore::instance()->update(link.object()))
    	{
    		log_crit("error updating link %s: error in persistent store",
    				 link->name());
			return;
		}
    } else {
    	if (! LinkStore::instance()->add(link.object()))
    	{
    		log_crit("error adding link %s: error in persistent store",
    				 link->name());
			return;
		}
    }

    log_info("LINK_CREATED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_deleted(LinkDeletedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);

    // If link has been used in some forwarding log entry during this run of the
    // daemon or the link is a reincarnation of a link that was extant when a
    // previous run was terminated, then the persistent storage should be left
    // intact (and the link spec will remain in ContactManager::previous_links_
    // so that consistency will be checked).  Otherwise delete the entry in Links.
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (!(link->used_in_fwdlog() || link->reincarnated()))
    {
    	if (! LinkStore::instance()->del(link->name_str()))
    	{
    		log_crit("error deleting link %s: error in persistent store",
    				 link->name());
			return;
		}
    }

    log_info("LINK_DELETED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_available(LinkAvailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(link->isavailable());

    if (link->isdeleted()) {
        log_warn("BundleDaemon::handle_link_available: "
                 "link %s already deleted", link->name());
        event->daemon_only_ = true;
        return;
    }

    log_info("LINK_AVAILABLE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_unavailable(LinkUnavailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isavailable());
    
    log_info("LINK UNAVAILABLE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_state_change_request(LinkStateChangeRequest* request)
{
    LinkRef link = request->link_;
    if (link == NULL) {
        log_warn("LINK_STATE_CHANGE_REQUEST received invalid link");
        return;
    }

    Link::state_t new_state = Link::state_t(request->state_);
    Link::state_t old_state = Link::state_t(request->old_state_);
    int reason = request->reason_;

    if (link->isdeleted() && new_state != Link::CLOSED) {
        log_warn("BundleDaemon::handle_link_state_change_request: "
                 "link %s already deleted; cannot change link state to %s",
                 link->name(), Link::state_to_str(new_state));
        return;
    }
    
    if (link->contact() != request->contact_) {
        log_warn("stale LINK_STATE_CHANGE_REQUEST [%s -> %s] (%s) for "
                 "link *%p: contact %p != current contact %p", 
                 Link::state_to_str(old_state), Link::state_to_str(new_state),
                 ContactEvent::reason_to_str(reason), link.object(),
                 request->contact_.object(), link->contact().object());
        return;
    }

    log_info("LINK_STATE_CHANGE_REQUEST [%s -> %s] (%s) for link *%p",
             Link::state_to_str(old_state), Link::state_to_str(new_state),
             ContactEvent::reason_to_str(reason), link.object());

    //avoid a race condition caused by opening a partially closed link
    oasys::ScopeLock l;
    if (new_state == Link::OPEN)
    {
        l.set_lock(contactmgr_->lock(), "BundleDaemon::handle_link_state_change_request");
    }
    
    switch(new_state) {
    case Link::UNAVAILABLE:
        if (link->state() != Link::AVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state UNAVAILABLE in state %s",
                    link.object(), Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);
        post_at_head(new LinkUnavailableEvent(link,
                     ContactEvent::reason_t(reason)));
        break;

    case Link::AVAILABLE:
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);
            
        } else {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link.object(), Link::state_to_str(link->state()));
            return;
        }

        post_at_head(new LinkAvailableEvent(link,
                     ContactEvent::reason_t(reason)));
        break;
        
    case Link::OPENING:
    case Link::OPEN:
        // force the link to be available, since someone really wants it open
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);
        }
        actions_->open_link(link);
        break;

    case Link::CLOSED:
        // The only case where we should get this event when the link
        // is not actually open is if it's in the process of being
        // opened but the CL can't actually open it.
        if (! link->isopen() && ! link->isopening()) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "setting state CLOSED (%s) in unexpected state %s",
                    link.object(), ContactEvent::reason_to_str(reason),
                    Link::state_to_str(link->state()));
            break;
        }

        // If the link is open (not OPENING), we need a ContactDownEvent
        if (link->isopen()) {
            ASSERT(link->contact() != NULL);
            post_at_head(new ContactDownEvent(link->contact(),
                         ContactEvent::reason_t(reason)));
        }

        // close the link
        actions_->close_link(link);
        
        // now, based on the reason code, update the link availability
        // and set state accordingly
        if (reason == ContactEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            post_at_head(new LinkUnavailableEvent(link,
                         ContactEvent::reason_t(reason)));
        }
    
        break;

    default:
        PANIC("unhandled state %s", Link::state_to_str(new_state));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_create(LinkCreateRequest* request)
{
    //lock the contact manager so no one creates a link before we do
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), "BundleDaemon::handle_link_create");
    //check for an existing link with that name
    LinkRef linkCheck = cm->find_link(request->name_.c_str());
    if(linkCheck != NULL)
    {
    	log_err( "Link already exists with name %s, aborting create", request->name_.c_str());
        request->daemon_only_ = true;
    	return;
    }
  
    std::string nexthop("");

    int argc = request->parameters_.size();
    char* argv[argc];
    AttributeVector::iterator iter;
    int i = 0;
    for (iter = request->parameters_.begin();
         iter != request->parameters_.end();
         iter++)
    {
        if (iter->name() == "nexthop") {
            nexthop = iter->string_val();
        }
        else {
            std::string arg = iter->name() + iter->string_val();
            argv[i] = new char[arg.length()+1];
            memcpy(argv[i], arg.c_str(), arg.length()+1);
            i++;
        }
    }
    argc = i+1;

    const char *invalidp;
    LinkRef link = Link::create_link(request->name_, request->link_type_,
                                     request->cla_, nexthop.c_str(), argc,
                                     (const char**)argv, &invalidp);
    for (i = 0; i < argc; i++) {
        delete argv[i];
    }

    if (link == NULL) {
        log_err("LINK_CREATE %s failed", request->name_.c_str());
        return;
    }

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    if (!contactmgr_->add_new_link(link)) {
        log_err("LINK_CREATE %s failed, already exists",
                request->name_.c_str());
        link->delete_link();
        return;
    }
    log_info("LINK_CREATE %s: *%p", request->name_.c_str(), link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_delete(LinkDeleteRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);

    log_info("LINK_DELETE *%p", link.object());
    if (!link->isdeleted()) {
        contactmgr_->del_link(link);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_reconfigure(LinkReconfigureRequest *request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);

    link->reconfigure_link(request->parameters_);
    log_info("LINK_RECONFIGURE *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_query(LinkQueryRequest*)
{
    BundleDaemon::post_at_head(new LinkReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_report(LinkReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_query(BundleQueuedQueryRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);
    ASSERT(link->clayer() != NULL);

    log_debug("BundleDaemon::handle_bundle_queued_query: "
              "query %s, checking if bundle *%p is queued on link *%p",
              request->query_id_.c_str(),
              request->bundle_.object(), link.object());
    
    bool is_queued = request->bundle_->is_queued_on(link->queue());
    BundleDaemon::post(
        new BundleQueuedReportEvent(request->query_id_, is_queued));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_report(BundleQueuedReportEvent* event)
{
    (void)event;
    log_debug("BundleDaemon::handle_bundle_queued_report: query %s, %s",
              event->query_id_.c_str(),
              (event->is_queued_? "true" : "false"));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_eid_reachable_query(EIDReachableQueryRequest* request)
{
    Interface *iface = request->iface_;
    ASSERT(iface != NULL);
    ASSERT(iface->clayer() != NULL);

    log_debug("BundleDaemon::handle_eid_reachable_query: query %s, "
              "checking if endpoint %s is reachable via interface *%p",
              request->query_id_.c_str(), request->endpoint_.c_str(), iface);

    iface->clayer()->is_eid_reachable(request->query_id_,
                                      iface,
                                      request->endpoint_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_eid_reachable_report(EIDReachableReportEvent* event)
{
    (void)event;
    log_debug("BundleDaemon::handle_eid_reachable_report: query %s, %s",
              event->query_id_.c_str(),
              (event->is_reachable_? "true" : "false"));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attribute_changed(LinkAttributeChangedEvent *event)
{
    LinkRef link = event->link_;

    if (link->isdeleted()) {
        log_debug("BundleDaemon::handle_link_attribute_changed: "
                  "link %s deleted", link->name());
        event->daemon_only_ = true;
        return;
    }

    // Update any state as necessary
    AttributeVector::iterator iter;
    for (iter = event->attributes_.begin();
         iter != event->attributes_.end();
         iter++)
    {
        if (iter->name() == "nexthop") {
            link->set_nexthop(iter->string_val());
        }
        else if (iter->name() == "how_reliable") {
            link->stats()->reliability_ = iter->u_int_val();
        }
        else if (iter->name() == "how_available") {
            link->stats()->availability_ = iter->u_int_val();
        }
    }
    log_info("LINK_ATTRIB_CHANGED *%p", link.object());
}
  
//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attributes_query(LinkAttributesQueryRequest* request)
{
    LinkRef link = request->link_;
    ASSERT(link != NULL);
    ASSERT(link->clayer() != NULL);

    log_debug("BundleDaemon::handle_link_attributes_query: query %s, link *%p",
              request->query_id_.c_str(), link.object());

    link->clayer()->query_link_attributes(request->query_id_,
                                          link,
                                          request->attribute_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_attributes_report(LinkAttributesReportEvent* event)
{
    (void)event;
    log_debug("BundleDaemon::handle_link_attributes_report: query %s",
              event->query_id_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_iface_attributes_query(
                  IfaceAttributesQueryRequest* request)
{
    Interface *iface = request->iface_;
    ASSERT(iface != NULL);
    ASSERT(iface->clayer() != NULL);

    log_debug("BundleDaemon::handle_iface_attributes_query: "
              "query %s, interface *%p", request->query_id_.c_str(), iface);

    iface->clayer()->query_iface_attributes(request->query_id_,
                                            iface,
                                            request->attribute_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_iface_attributes_report(IfaceAttributesReportEvent* event)
{
    (void)event;
    log_debug("BundleDaemon::handle_iface_attributes_report: query %s",
              event->query_id_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_parameters_query(CLAParametersQueryRequest* request)
{
    ASSERT(request->cla_ != NULL);

    log_debug("BundleDaemon::handle_cla_parameters_query: "
              "query %s, convergence layer %s",
              request->query_id_.c_str(), request->cla_->name());

    request->cla_->query_cla_parameters(request->query_id_,
                                        request->parameter_names_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_cla_parameters_report(CLAParametersReportEvent* event)
{
    (void)event;
    log_debug("Bundledaemon::handle_cla_parameters_report: query %s",
              event->query_id_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_up(ContactUpEvent* event)
{
    const ContactRef& contact = event->contact_;
    LinkRef link = contact->link();
    ASSERT(link != NULL);

    if (link->isdeleted()) {
        log_debug("BundleDaemon::handle_contact_up: "
                  "cannot bring contact up on deleted link %s", link->name());
        event->daemon_only_ = true;
        return;
    }

    //ignore stale notifications that an old contact is up
    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::handle_contact_up");
    if (link->contact() != contact)
    {
        log_info("CONTACT_UP *%p (contact %p) being ignored (old contact)",
                 link.object(), contact.object());
        return;
    }
    
    log_info("CONTACT_UP *%p (contact %p)", link.object(), contact.object());
    link->set_state(Link::OPEN);
    link->stats_.contacts_++;
	s10_contact(S10_CONTUP,contact.object(),NULL);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_down(ContactDownEvent* event)
{
    const ContactRef& contact = event->contact_;
    int reason = event->reason_;
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    
    log_info("CONTACT_DOWN *%p (%s) (contact %p)",
             link.object(), ContactEvent::reason_to_str(reason),
             contact.object());

    // update the link stats
    link->stats_.uptime_ += (contact->start_time().elapsed_ms() / 1000);
	s10_contact(S10_CONTDOWN,contact.object(),NULL);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_query(ContactQueryRequest*)
{
    BundleDaemon::post_at_head(new ContactReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_report(ContactReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_reassembly_completed(ReassemblyCompletedEvent* event)
{
    log_info("REASSEMBLY_COMPLETED bundle id %d",
             event->bundle_->bundleid());

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    // remove all the fragments from the pending list
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        delete_bundle(ref);
    }

    // post a new event for the newly reassembled bundle
    post_at_head(new BundleReceivedEvent(event->bundle_.object(),
                                         EVENTSRC_FRAGMENTATION));
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_route_add(RouteAddEvent* event)
{
    log_info("ROUTE_ADD *%p", event->entry_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_del(RouteDelEvent* event)
{
    log_info("ROUTE_DEL %s", event->dest_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_query(RouteQueryRequest*)
{
    BundleDaemon::post_at_head(new RouteReportEvent());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_report(RouteReportEvent*)
{
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_signal(CustodySignalEvent* event)
{
    log_info("CUSTODY_SIGNAL: %s %llu.%llu %s (%s)",
             event->data_.orig_source_eid_.c_str(),
             event->data_.orig_creation_tv_.seconds_,
             event->data_.orig_creation_tv_.seqno_,
             event->data_.succeeded_ ? "succeeded" : "failed",
             CustodySignal::reason_to_str(event->data_.reason_));

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    GbofId gbof_id;
    gbof_id.source_ = event->data_.orig_source_eid_;
    gbof_id.creation_ts_ = event->data_.orig_creation_tv_;
    gbof_id.is_fragment_
        = event->data_.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT;
    gbof_id.frag_length_
        = gbof_id.is_fragment_ ? event->data_.orig_frag_length_ : 0;
    gbof_id.frag_offset_
        = gbof_id.is_fragment_ ? event->data_.orig_frag_offset_ : 0;

    BundleRef orig_bundle =
        custody_bundles_->find(gbof_id);
    
    if (orig_bundle == NULL) {
        log_warn("received custody signal for bundle %s %llu.%llu "
                 "but don't have custody",
                 event->data_.orig_source_eid_.c_str(),
                 event->data_.orig_creation_tv_.seconds_,
                 event->data_.orig_creation_tv_.seqno_);
        return;
    }

    // release custody if either the signal succeded or if it
    // (paradoxically) failed due to duplicate transmission
    bool release = event->data_.succeeded_;
    if ((event->data_.succeeded_ == false) &&
        (event->data_.reason_ == BundleProtocol::CUSTODY_REDUNDANT_RECEPTION))
    {
        log_notice("releasing custody for bundle %s %llu.%llu "
                   "due to redundant reception",
                   event->data_.orig_source_eid_.c_str(),
                   event->data_.orig_creation_tv_.seconds_,
                   event->data_.orig_creation_tv_.seqno_);
        
        release = true;
    }

	s10_bundle(S10_RELCUST,orig_bundle.object(),event->data_.orig_source_eid_.c_str(),0,0,NULL,NULL);
    
    if (release) {
        release_custody(orig_bundle.object());
        try_to_delete(orig_bundle);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    Bundle* bundle = event->bundle_.object();
    LinkRef link   = event->link_;
    ASSERT(link != NULL);
    
    log_info("CUSTODY_TIMEOUT *%p, *%p", bundle, link.object());
    
    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    // remove and delete the expired timer from the bundle
    oasys::ScopeLock l(bundle->lock(), "BundleDaemon::handle_custody_timeout");

    bool found = false;
    CustodyTimer* timer = NULL;
    CustodyTimerVec::iterator iter;
    for (iter = bundle->custody_timers()->begin();
         iter != bundle->custody_timers()->end();
         ++iter)
    {
        timer = *iter;
        if (timer->link_ == link)
        {
            if (timer->pending()) {
                log_err("multiple pending custody timers for link %s",
                        link->nexthop());
                continue;
            }
            
            found = true;
            bundle->custody_timers()->erase(iter);
            break;
        }
    }

    if (!found) {
        log_err("custody timeout for *%p *%p: timer not found in bundle list",
                bundle, link.object());
        return;
    }

    ASSERT(!timer->cancelled());
    
    if (!pending_bundles_->contains(bundle)) {
        log_err("custody timeout for *%p *%p: bundle not in pending list",
                bundle, link.object());
    }

    // modify the TRANSMITTED entry in the forwarding log to indicate
    // that we got a custody timeout. then when the routers go through
    // to figure out whether the bundle needs to be re-sent, the
    // TRANSMITTED entry is no longer in there
    bool ok = bundle->fwdlog()->update(link, ForwardingInfo::CUSTODY_TIMEOUT);
    if (!ok) {
        log_err("custody timeout can't find ForwardingLog entry for link *%p",
                link.object());
    }
    
    delete timer;

    // now fall through to let the router handle the event, typically
    // triggering a retransmission to the link in the event
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_shutdown_request(ShutdownRequest* request)
{
    shutting_down_ = true;

    (void)request;

    log_notice("Received shutdown request");

    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::handle_shutdown");

    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;

    // close any open links
    for (iter = links->begin(); iter != links->end(); ++iter)
    {
        LinkRef link = *iter;
        if (link->isopen()) {
            log_debug("Shutdown: closing link *%p\n", link.object());
            link->close();
        }
    }

    // Shutdown all actively registered convergence layers.
    ConvergenceLayer::shutdown_clayers();

    // call the rtr shutdown procedure
    if (rtr_shutdown_proc_) {
        (*rtr_shutdown_proc_)(rtr_shutdown_data_);
    }

    // call the app shutdown procedure
    if (app_shutdown_proc_) {
        (*app_shutdown_proc_)(app_shutdown_data_);
    }

    // signal to the main loop to bail
    set_should_stop();

    // fall through -- the DTNServer will close and flush all the data
    // stores
#ifdef BSP_ENABLED
    Ciphersuite::shutdown();
    KeyDB::shutdown();
#endif

}
//----------------------------------------------------------------------

void
BundleDaemon::handle_cla_set_params(CLASetParamsRequest* request)
{
    ASSERT(request->cla_ != NULL);
    request->cla_->set_cla_parameters(request->parameters_);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_status_request(StatusRequest* request)
{
    (void)request;
    log_info("Received status request");
}

//----------------------------------------------------------------------
void
BundleDaemon::event_handlers_completed(BundleEvent* event)
{
    log_debug("event handlers completed for (%p) %s", event, event->type_str());
    
    /**
     * Once bundle reception, transmission or delivery has been
     * processed by the router, check to see if it's still needed,
     * otherwise we delete it.
     */
    BundleRef bundle("BundleDaemon::event_handlers_completed");
    if (event->type_ == BUNDLE_RECEIVED) {
        bundle = ((BundleReceivedEvent*)event)->bundleref_;
    } else if (event->type_ == BUNDLE_TRANSMITTED) {
        bundle = ((BundleTransmittedEvent*)event)->bundleref_;
    } else if (event->type_ == BUNDLE_DELIVERED) {
        bundle = ((BundleTransmittedEvent*)event)->bundleref_;
    }

    if (bundle != NULL) {
        try_to_delete(bundle);
    }

    /**
     * Once the bundle expired event has been processed, the bundle
     * shouldn't exist on any more lists.
     */
    if (event->type_ == BUNDLE_EXPIRED) {
        bundle = ((BundleExpiredEvent*)event)->bundleref_.object();
        if (bundle==NULL) {
            log_warn("can't get bundle from event->bundleref_.object()");
            return;
        }
        size_t num_mappings = bundle->num_mappings();
        if (num_mappings != 1) {
            log_warn("expired bundle *%p still has %zu mappings (i.e. not just in ALL_BUNDLES)",
                     bundle.object(), num_mappings);
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    bool ok_to_route = true;

    log_debug("adding bundle *%p to pending list (%d)",
              bundle, add_to_store);
 
    log_info_p("/dtn/bundle/expiration","adding bundle to pending list");
 
    pending_bundles_->push_back(bundle);
    
    if (add_to_store) {
        bundle->set_in_datastore(true);
        actions_->store_add(bundle);
    }

       struct timeval now;

       gettimeofday(&now, 0);

       // schedule the bundle expiration timer
       struct timeval expiration_time;

 
       u_int64_t temp = BundleTimestamp::TIMEVAL_CONVERSION +
               bundle->creation_ts().seconds_ +
               bundle->expiration();

       // The expiration time has to be expressible in a
       // time_t (signed long on Linux)
       if (temp>INT_MAX) {
               log_crit("Expiration time too large.");
               ASSERT(false);
       }
       expiration_time.tv_sec = temp;

       long int when = expiration_time.tv_sec - now.tv_sec;

       expiration_time.tv_usec = now.tv_usec;
 
    //+[AEB] handling
    bool age_block_exists = false;
    
    if ((bundle->recv_blocks()).find_block(BundleProtocol::AGE_BLOCK) != false) {
        age_block_exists = true;
    }

    if(bundle->creation_ts().seconds_ == 0 || age_block_exists == true) {
	if(bundle->creation_ts().seconds_ == 0) {
            log_info_p("/dtn/bundle/expiration", "[AEB]: creation ts is 0");
	} 
        if(age_block_exists) {
            log_info_p("/dtn/bundle/expiration", "[AEB]: AgeBlock exists.");
        }

        expiration_time.tv_sec = 
            BundleTimestamp::TIMEVAL_CONVERSION +
            BundleTimestamp::get_current_time() +
            bundle->expiration() -
            bundle->age() - 
            (bundle->time_aeb().elapsed_ms() / 1000); // this might not be necessary

        expiration_time.tv_usec = now.tv_usec;

        when = expiration_time.tv_sec - now.tv_sec;

        log_info_p("/dtn/bundle/expiration", "[AEB]: expiring in %lu seconds", when);
    }
    //+[AEB] end handling
  
       if (when > 0) {
               log_debug_p("/dtn/bundle/expiration",
                                       "scheduling expiration for bundle id %d at %u.%u "
                                       "(in %lu seconds)",
                                       bundle->bundleid(),
                                       (u_int)expiration_time.tv_sec, (u_int)expiration_time.tv_usec,
                                       when);
       } else {
               log_warn_p("/dtn/bundle/expiration",
                                  "scheduling IMMEDIATE expiration for bundle id %d: "
                                  "[expiration %llu, creation time %llu.%llu, offset %u, now %u.%u]",
                                  bundle->bundleid(), bundle->expiration(),
                                  bundle->creation_ts().seconds_,
                                  bundle->creation_ts().seqno_,
                                  BundleTimestamp::TIMEVAL_CONVERSION,
                                  (u_int)now.tv_sec, (u_int)now.tv_usec);
               expiration_time = now;
               ok_to_route = false;
       }

       bundle->set_expiration_timer(new ExpirationTimer(bundle));
       bundle->expiration_timer()->schedule_at(&expiration_time);

    return ok_to_route;
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_from_pending(const BundleRef& bundle)
{
    log_debug("removing bundle *%p from pending list", bundle.object());

    // first try to cancel the expiration timer if it's still
    // around
    if (bundle->expiration_timer()) {
        log_debug("cancelling expiration timer for bundle id %d",
                  bundle->bundleid());
        
        bool cancelled = bundle->expiration_timer()->cancel();
        if (!cancelled) {
            log_crit("unexpected error cancelling expiration timer "
                     "for bundle *%p", bundle.object());
        }
        
        bundle->expiration_timer()->bundleref_.release();
        bundle->set_expiration_timer(NULL);
    }

    // XXX/demmer the whole BundleDaemon core should be changed to use
    // BundleRefs instead of Bundle*, as should the BundleList API, as
    // should the whole system, really...
    log_debug("pending_bundles size %zd", pending_bundles_->size());
    
    oasys::Time now;
    now.get_time();
    
    bool erased = pending_bundles_->erase(bundle);

    log_debug("BundleDaemon: pending_bundles erasure took %u ms",
              now.elapsed_ms());

    if (!erased) {
        log_err("unexpected error removing bundle from pending list");
    }

    return erased;
}

//----------------------------------------------------------------------
bool
BundleDaemon::try_to_delete(const BundleRef& bundle)
{
    /*
     * Check to see if we should remove the bundle from the system.
     * 
     * If we're not configured for early deletion, this never does
     * anything. Otherwise it relies on the router saying that the
     * bundle can be deleted.
     */

    log_debug("pending_bundles size %zd", pending_bundles_->size());
    if (! bundle->is_queued_on(pending_bundles_))
    {
        if (bundle->expired()) {
            log_debug("try_to_delete(*%p): bundle already expired",
                      bundle.object());
            return false;
        }
        
        log_err("try_to_delete(*%p): bundle not in pending list!",
                bundle.object());
        return false;
    }

    if (!params_.early_deletion_) {
        log_debug("try_to_delete(*%p): not deleting because "
                  "early deletion disabled",
                  bundle.object());
        return false;
    }

    if (! router_->can_delete_bundle(bundle)) {
        log_debug("try_to_delete(*%p): not deleting because "
                  "router wants to keep bundle",
                  bundle.object());
        return false;
    }
    
#ifdef BPQ_ENABLED
    if (bpq_cache()->bundle_in_bpq_cache(bundle.object())) {
    	log_debug("try_to_delete(*%p): not deleting because"
    			  "bundle is in BPQ cache",
    			  bundle.object());
    	return false;
    }
#endif

    return delete_bundle(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_bundle(const BundleRef& bundleref,
                            status_report_reason_t reason)
{
    Bundle* bundle = bundleref.object();
    
    ++stats_.deleted_bundles_;
    
    // send a bundle deletion status report if we have custody or the
    // bundle's deletion status report request flag is set and a reason
    // for deletion is provided
    bool send_status = (bundle->local_custody() ||
                       (bundle->deletion_rcpt() &&
                        reason != BundleProtocol::REASON_NO_ADDTL_INFO));
        
    // check if we have custody, if so, remove it
    if (bundle->local_custody()) {
        release_custody(bundle);
    }

    // XXX/demmer if custody was requested but we didn't take it yet
    // (due to a validation error, space constraints, etc), then we
    // should send a custody failed signal here

    // check if bundle is a fragment, if so, remove any fragmentation state
    if (bundle->is_fragment()) {
        fragmentmgr_->delete_fragment(bundle);
    }

    // notify the router that it's time to delete the bundle
    router_->delete_bundle(bundleref);

#ifdef BPQ_ENABLED
    // Forcibly delete bundle from BPQ cache
    // The cache list will be there even if it is not being used
    bpq_cache()->check_and_remove(bundle);

#endif

    // delete the bundle from the pending list
    log_debug("pending_bundles size %zd", pending_bundles_->size());
    bool erased = true;
    if (bundle->is_queued_on(pending_bundles_)) {
        erased = delete_from_pending(bundleref);
    }

    if (erased && send_status) {
        generate_status_report(bundle, BundleStatusReport::STATUS_DELETED, reason);
    }

    // cancel the bundle on all links where it is queued or in flight
    oasys::Time now;
    now.get_time();
    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::delete_bundle");
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    for (iter = links->begin(); iter != links->end(); ++iter) {
        const LinkRef& link = *iter;
        
        if (link->queue()->contains(bundle) ||
            link->inflight()->contains(bundle))
        {
            actions_->cancel_bundle(bundle, link);
        }
    }

    // cancel the bundle on all API registrations
    RegistrationList matches;
    RegistrationList::iterator reglist_iter;

    reg_table_->get_matching(bundle->dest(), &matches);

    for (reglist_iter = matches.begin(); reglist_iter != matches.end(); ++reglist_iter) {
        Registration* registration = *reglist_iter;
        registration->delete_bundle(bundle);
    }

    // XXX/demmer there may be other lists where the bundle is still
    // referenced so the router needs to be told what to do...
    
    log_debug("BundleDaemon: canceling deleted bundle on all links took %u ms",
               now.elapsed_ms());

    return erased;
}

//----------------------------------------------------------------------
Bundle*
BundleDaemon::find_duplicate(Bundle* b)
{
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "BundleDaemon::find_duplicate");
    log_debug("pending_bundles size %zd", pending_bundles_->size());
    Bundle *found = NULL;
    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        Bundle* b2 = *iter;
        
        if ((b->source().equals(b2->source())) &&
            (b->creation_ts().seconds_ == b2->creation_ts().seconds_) &&
            (b->creation_ts().seqno_   == b2->creation_ts().seqno_) &&
            (b->is_fragment()          == b2->is_fragment()) &&
            (b->frag_offset()          == b2->frag_offset()) &&
            /*(b->orig_length()          == b2->orig_length()) &&*/
            (b->payload().length()     == b2->payload().length()))
        {
            // b is a duplicate of b2
            found = b2;
            /*
             * If we are not suppressing duplicates, we might have custody of
             * one of any number of duplicates, so if this one does not have
             * custody, keep looking until we find one that does have custody
             * or we run out of choices. If we are suppressing duplicates
             * there's no need to keep looking.
             */
            if (params_.suppress_duplicates_ || b2->local_custody()) {
                break;
            }
        }
    }

    return found;
}

//----------------------------------------------------------------------
#ifdef BPQ_ENABLED
bool
BundleDaemon::handle_bpq_block(Bundle* bundle, BundleReceivedEvent* event)
{
    const BlockInfo* block = NULL;
    bool local_bundle = true;

    switch (event->source_) {
    	case EVENTSRC_PEER:
    		if (bundle->recv_blocks().has_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) {
    			block = bundle->recv_blocks().
    			                find_block(BundleProtocol::QUERY_EXTENSION_BLOCK);
    			local_bundle = false;
    		} else {
    			return false;
    		}
    		break;

    	case EVENTSRC_APP:
    		if (bundle->api_blocks()->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) {
				block = bundle->api_blocks()->
    		                	find_block(BundleProtocol::QUERY_EXTENSION_BLOCK);
				local_bundle = true;
    		} else {
    			return false;
    		}
    		break;

    	case EVENTSRC_STORE:
    	case EVENTSRC_FRAGMENTATION:
    		if (bundle->recv_blocks().has_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) {
				block = bundle->recv_blocks().
								find_block(BundleProtocol::QUERY_EXTENSION_BLOCK);
				local_bundle = false;
    		}
    		else if (bundle->api_blocks()->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) {
				block = bundle->api_blocks()->
    		                	find_block(BundleProtocol::QUERY_EXTENSION_BLOCK);
				local_bundle = true;
    		} else {
    			return false;
    		}
    		break;

    	default:
    		log_err_p("/dtn/daemon/bpq", "Handle BPQ Block failed for unknown event source: %s",
    				source_to_str((event_source_t)event->source_));
            NOTREACHED;
            return false;
        }

    /**
     * At this point the BPQ Block has been found in the bundle
     */
    ASSERT ( block != NULL );
    BPQBlock* bpq_block = dynamic_cast<BPQBlock *>(block->locals());

    log_info_p("/dtn/daemon/bpq", "handle_bpq_block: Kind: %d Query: %s",
        (int)  bpq_block->kind(),
        (char*)bpq_block->query_val());

    if (bpq_block->kind() == BPQBlock::KIND_QUERY) {
    	if (bpq_cache()->answer_query(bundle, bpq_block)) {
    		log_info_p("/dtn/daemon/bpq", "Query: %s answered completely",
    				(char*)bpq_block->query_val());
            event->daemon_only_ = true;
        }

    } else if (bpq_block->kind() == BPQBlock::KIND_RESPONSE ||
               bpq_block->kind() == BPQBlock::KIND_PUBLISH) {
    	// don't accept local responses for KIND_RESPONSE
    	// This was originally because of a bug in the APIServer that
    	// didn't send api_blocks in local receives.  Now need to think
    	// if this results in circularity because cache is being searched
    	// Leave the special case for the time being.
        if (!local_bundle || bpq_block->kind() == BPQBlock::KIND_PUBLISH) {


    		if (bpq_cache()->add_response_bundle(bundle, bpq_block) &&
    			event->source_ != EVENTSRC_STORE) {


    			bundle->set_in_datastore(true);
				actions_->store_add(bundle);

				/*
			     * Send a reception receipt if requested within the primary
			     * block and this is a locally generated bundle - special case
			     * as reception reports are not normally generated for bundles
			     * from local apps but this tells the app that the bundle has
			     * been cached.
			     */
			    if (local_bundle && bundle->receive_rcpt()) {
			    	 generate_status_report(bundle, BundleStatusReport::STATUS_RECEIVED,
			                                BundleProtocol::REASON_NO_ADDTL_INFO);
			    }
    		}
    	}

    } else if (bpq_block->kind() == BPQBlock::KIND_RESPONSE_DO_NOT_CACHE_FRAG) {
    	// don't accept local responses
    	if (!local_bundle &&
    		!bundle->is_fragment()	) {

    		if (bpq_cache()->add_response_bundle(bundle, bpq_block) &&
    			event->source_ != EVENTSRC_STORE) {

    			bundle->set_in_datastore(true);
				actions_->store_add(bundle);
    		}
    	}

    } else {
        log_err_p("/dtn/daemon/bpq", "ERROR - BPQ Block: invalid kind %d",
            bpq_block->kind());
        NOTREACHED;
        return false;
    }

    return true;
}
#endif /* BPQ_ENABLED */

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_free(BundleFreeEvent* event)
{
    Bundle* bundle = event->bundle_;
    event->bundle_ = NULL;

    ASSERT(bundle->refcount() == 1);
    ASSERT(all_bundles_->contains(bundle));

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    all_bundles_->erase(bundle);
    
    bundle->lock()->lock("BundleDaemon::handle_bundle_free");

    if (bundle->in_datastore()) {
        log_debug("removing freed bundle (id %d) from data store", bundle->bundleid());
        actions_->store_del(bundle);
    }
    log_debug("deleting freed bundle");

    delete bundle;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(BundleEvent* event)
{
    handle_event(event, true);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(BundleEvent* event, bool closeTransaction)
{
    dispatch_event(event);
    
    if (! event->daemon_only_) {
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(event);
        contactmgr_->handle_event(event);
    }

    event_handlers_completed(event);

    if (closeTransaction) {
        oasys::DurableStore* ds = oasys::DurableStore::instance();
        if ( ds->is_transaction_open() ) {
            log_debug("handle_event closing transaction");
            ds->end_transaction();
        }
    } else {
        log_debug("handle_event NOT closing transaction");
    }

    stats_.events_processed_++;

    if (event->processed_notifier_) {
        event->processed_notifier_->notify();
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::load_registrations()
{
    admin_reg_ = new AdminRegistration();
    {
        RegistrationAddedEvent e(admin_reg_, EVENTSRC_ADMIN);
        handle_event(&e);
    }

    EndpointID ping_eid(local_eid());
    bool ok = ping_eid.append_service_tag("ping");
    if (!ok) {
        log_crit("local eid (%s) scheme must be able to append service tags",
                 local_eid().c_str());
        exit(1);
    }
    
    ping_reg_ = new PingRegistration(ping_eid);
    {
        RegistrationAddedEvent e(ping_reg_, EVENTSRC_ADMIN);
        handle_event(&e);
    }

    Registration* reg;
    RegistrationStore* reg_store = RegistrationStore::instance();
    RegistrationStore::iterator* iter = reg_store->new_iterator();

    while (iter->next() == 0) {
        reg = reg_store->get(iter->cur_val());
        if (reg == NULL) {
            log_err("error loading registration %d from data store",
                    iter->cur_val());
            continue;
        }
        
        RegistrationAddedEvent e(reg, EVENTSRC_STORE);
        handle_event(&e);
    }

    delete iter;
}

//----------------------------------------------------------------------
void
BundleDaemon::load_previous_links()
{
    Link* link;
    LinkStore* link_store = LinkStore::instance();
    LinkStore::iterator* iter = link_store->new_iterator();
    LinkRef link_ref;

    log_debug("Entering load_previous_links");

    /*!
     * By default this routine is called during the start of the run of the BundleDaemon
     * event loop, but this may need to be preempted if the DTN2 configuration file
     * 'link add' commands
     */
    if (load_previous_links_executed_)
    {
    	log_debug("load_previous_links called again - skipping data store reads");
    	return;
    }

    load_previous_links_executed_ = true;

    while (iter->next() == 0) {
        link = link_store->get(iter->cur_val());
        if (link == NULL) {
            log_err("error loading link %s from data store",
                    iter->cur_val().c_str());
            continue;
         }

        log_debug("Read link %s from data store", iter->cur_val().c_str());
        link_ref = LinkRef(link, "Read from datastore");
        contactmgr_->add_previous_link(link_ref);
    }

    // Check links created in config file have names consistent with previous usage
    contactmgr_->config_links_consistent();

    // If configured, reincarnate other non-OPPORTUNISTIC links recorded from previous
    // runs of the daemon.
    if (params_.recreate_links_on_restart_)
    {
    	contactmgr_->reincarnate_links();
    }

    delete iter;
    log_debug("Exiting load_previous_links");
}
//----------------------------------------------------------------------
void
BundleDaemon::load_bundles()
{
    Bundle* bundle;
    BundleStore* bundle_store = BundleStore::instance();
    BundleStore::iterator* iter = bundle_store->new_iterator();

    log_notice("loading bundles from data store");

    u_int64_t total_size = 0;

    std::vector<Bundle*> doa_bundles;
    
    for (iter->begin(); iter->more(); iter->next()) {
        bundle = bundle_store->get(iter->cur_val());
        
        if (bundle == NULL) {
            log_err("error loading bundle %d from data store",
                    iter->cur_val());
            continue;
        }

        total_size += bundle->durable_size();
        
        // if the bundle payload file is missing, we need to kill the
        // bundle, but we can't do so while holding the durable
        // iterator or it may deadlock, so cleanup is deferred 
        if (bundle->payload().location() != BundlePayload::DISK) {
            log_err("error loading payload for *%p from data store",
                    bundle);
            doa_bundles.push_back(bundle);
            continue;
        }

        BundleProtocol::reload_post_process(bundle);

        //WAS
        //BundleReceivedEvent e(bundle, EVENTSRC_STORE);
        //handle_event(&e);

        // We're going to post this to the event queue, since 
        // the act of determining the registration(s) to which the
        // bundle should be delivered will cause the bundle to be
        // updated in the store (as the PENDING deliveries are
        // marked).
        // Note that since delivery is via the DELIVER_TO_REG
        // event, delivery will happen one event queue loop after
        // the receivedEvent is processed.
        post(new BundleReceivedEvent(bundle, EVENTSRC_STORE));

        // in the constructor, we disabled notifiers on the event
        // queue, so in case loading triggers other events, we just
        // let them queue up and handle them later when we're done
        // loading all the bundles
    }
    
    bundle_store->set_total_size(total_size);

    delete iter;

    // now that the durable iterator is gone, purge the doa bundles
    for (unsigned int i = 0; i < doa_bundles.size(); ++i) {
        actions_->store_del(doa_bundles[i]);
        delete doa_bundles[i];
    }

    log_debug("Done with load_bundles");
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_delivery_events(Bundle* bundle)
{
    oasys::ScopeLock(bundle->fwdlog()->lock(), "generating delivery events");

    ForwardingLog::Log flog = bundle->fwdlog()->log();
    ForwardingLog::Log::const_iterator iter;
    for (iter = flog.begin(); iter != flog.end(); ++iter)
    {
        const ForwardingInfo* info = &(*iter);

        if ( info->regid()!=0 ) {
            if ( info->state()==ForwardingInfo::PENDING_DELIVERY ) {
                BundleDaemon::post(new DeliverBundleToRegEvent(bundle, info->regid()));
            }
        }
    }
}

//----------------------------------------------------------------------
bool
BundleDaemon::DaemonIdleExit::is_idle(const struct timeval& tv)
{
    oasys::Time now(tv.tv_sec, tv.tv_usec);
    u_int elapsed = (now - BundleDaemon::instance()->last_event_).in_milliseconds();

    BundleDaemon* d = BundleDaemon::instance();
    d->logf(oasys::LOG_DEBUG,
            "checking if is_idle -- last event was %u msecs ago",
            elapsed);

    // fudge
    if (elapsed + 500 > interval_ * 1000) {
        d->logf(oasys::LOG_NOTICE,
                "more than %u seconds since last event, "
                "shutting down daemon due to idle timer",
                interval_);
        
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::init_idle_shutdown(int interval)
{
    idle_exit_ = new DaemonIdleExit(interval);
}

//----------------------------------------------------------------------
void
BundleDaemon::run()
{
    static const char* LOOP_LOG = "/dtn/bundle/daemon/loop";
    
    if (! BundleTimestamp::check_local_clock()) {
        exit(1);
    }
    
    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
    router_->initialize();
    
    load_previous_links();
    load_bundles();
    load_registrations();

    BundleEvent* event;

    oasys::TimerSystem* timersys = oasys::TimerSystem::instance();

    last_event_.get_time();
    
    struct pollfd pollfds[2];
    struct pollfd* event_poll = &pollfds[0];
    struct pollfd* timer_poll = &pollfds[1];
    
    event_poll->fd     = eventq_->read_fd();
    event_poll->events = POLLIN;

    timer_poll->fd     = timersys->notifier()->read_fd();
    timer_poll->events = POLLIN;
    
    while (1) {
        if (should_stop()) {
            log_debug("BundleDaemon: stopping");
            break;
        }

        int timeout = timersys->run_expired_timers();

        log_debug_p(LOOP_LOG, 
                    "BundleDaemon: checking eventq_->size() > 0, its size is %zu", 
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
            
            
            log_debug_p(LOOP_LOG, "BundleDaemon: handling event %s",
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

            log_debug_p(LOOP_LOG, "BundleDaemon: deleting event %s",
                        event->type_str());
            // clean up the event
            delete event;
            
            continue; // no reason to poll
        }
        
        pollfds[0].revents = 0;
        pollfds[1].revents = 0;

        log_debug_p(LOOP_LOG, "BundleDaemon: poll_multiple waiting for %d ms", 
                    timeout);
        int cc = oasys::IO::poll_multiple(pollfds, 2, timeout);
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

        // if the timer notifier fired, then someone just scheduled a
        // new timer, so we just continue, which will call
        // run_expired_timers and handle it
        if (timer_poll->revents != 0) {
            log_debug_p(LOOP_LOG, "poll returned new timers to handle");
            timersys->notifier()->clear();
        }
    }
}

} // namespace dtn
