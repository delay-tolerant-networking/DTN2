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
#  include <config.h>
#endif

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
#include "storage/BundleStore.h"
#include "storage/RegistrationStore.h"

namespace dtn {

template <>
BundleDaemon* oasys::Singleton<BundleDaemon, false>::instance_ = NULL;

bool
BundleDaemon::is_simulator_ = false;

BundleDaemon::Params::Params()
    :  early_deletion_(true),
       accept_custody_(true),
       reactive_frag_enabled_(true),
       retry_reliable_unacked_(true),
       test_permuted_delivery_(false) {}

BundleDaemon::Params BundleDaemon::params_;

bool BundleDaemon::shutting_down_ = false;

//----------------------------------------------------------------------
BundleDaemon::BundleDaemon()
    : BundleEventHandler("BundleDaemon", "/dtn/bundle/daemon"),
      Thread("BundleDaemon", CREATE_JOINABLE)
{
    // default local eid
    local_eid_.assign(EndpointID::NULL_EID());

    memset(&stats_, 0, sizeof(stats_));

    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");

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
    buf->appendf("%zu pending -- "
                 "%zu custody -- "
                 "%u received -- "
                 "%u delivered -- "
                 "%u generated -- "
                 "%u transmitted -- "
                 "%u expired -- "
                 "%u duplicate -- "
                 "%u deleted",
                 pending_bundles()->size(),
                 custody_bundles()->size(),
                 stats_.received_bundles_,
                 stats_.delivered_bundles_,
                 stats_.generated_bundles_,
                 stats_.transmitted_bundles_,
                 stats_.expired_bundles_,
                 stats_.duplicate_bundles_,
                 stats_.deleted_bundles_);
}

//----------------------------------------------------------------------
void
BundleDaemon::get_daemon_stats(oasys::StringBuffer* buf)
{
    buf->appendf("%zu pending_events -- "
                 "%u processed_events",
                 eventq_->size(),
                 stats_.events_processed_);}


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
                                     status_report_flag_t flag,
                                     status_report_reason_t reason)
{
    log_debug("generating return receipt status report, "
              "flag = 0x%x, reason = 0x%x", flag, reason);
    
    Bundle* report = new Bundle();
    BundleStatusReport::create_status_report(report, orig_bundle,
                                             local_eid_, flag, reason);
    
    BundleReceivedEvent e(report, EVENTSRC_ADMIN);
    handle_event(&e);
}

//----------------------------------------------------------------------
void
BundleDaemon::generate_custody_signal(Bundle* bundle, bool succeeded,
                                      custody_signal_reason_t reason)
{
    if (bundle->local_custody_) {
        log_err("send_custody_signal(*%p): already have local custody",
                bundle);
        return;
    }

    if (bundle->custodian_.equals(EndpointID::NULL_EID())) {
        log_err("send_custody_signal(*%p): current custodian is NULL_EID",
                bundle);
        return;
    }
    
    Bundle* signal = new Bundle();
    CustodySignal::create_custody_signal(signal, bundle, local_eid_,
                                         succeeded, reason);
    
    BundleReceivedEvent e(signal, EVENTSRC_ADMIN);
    handle_event(&e);
}

//----------------------------------------------------------------------
void
BundleDaemon::cancel_custody_timers(Bundle* bundle)
{
    oasys::ScopeLock l(&bundle->lock_, "BundleDaemon::cancel_custody_timers");
    
    CustodyTimerVec::iterator iter;
    for (iter =  bundle->custody_timers_.begin();
         iter != bundle->custody_timers_.end();
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
    
    bundle->custody_timers_.clear();
}

//----------------------------------------------------------------------
void
BundleDaemon::accept_custody(Bundle* bundle)
{
    log_info("accept_custody *%p", bundle);
    
    if (bundle->local_custody_) {
        log_err("accept_custody(*%p): already have local custody",
                bundle);
        return;
    }

    if (bundle->custodian_.equals(local_eid_)) {
        log_err("send_custody_signal(*%p): "
                "current custodian is already local_eid",
                bundle);
        return;
    }
    
    // send a custody acceptance signal to the current custodian (if
    // it is someone, and not the null eid)
    if (! bundle->custodian_.equals(EndpointID::NULL_EID())) {
        generate_custody_signal(bundle, true, BundleProtocol::CUSTODY_NO_ADDTL_INFO);
    }

    // now we mark the bundle to indicate that we have custody and add
    // it to the custody bundles list
    bundle->custodian_.assign(local_eid_);
    bundle->local_custody_ = true;
    actions_->store_update(bundle);
    
    custody_bundles_->push_back(bundle);

    // finally, if the bundle requested custody acknowledgements,
    // deliver them now
    if (bundle->custody_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_CUSTODY_ACCEPTED);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::release_custody(Bundle* bundle)
{
    log_info("release_custody *%p", bundle);
    
    if (!bundle->local_custody_) {
        log_err("release_custody(*%p): don't have local custody",
                bundle);
        return;
    }

    cancel_custody_timers(bundle);

    bundle->custodian_.assign(EndpointID::NULL_EID());
    bundle->local_custody_ = false;
    actions_->store_update(bundle);

    custody_bundles_->erase(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      Registration* registration)
{
    // tells routers that this Bundle has been taken care of
    // by the daemon already
    bundle->owner_ = "daemon";

    if (bundle->is_fragment_) {
        log_debug("deferring delivery of bundle *%p to registration %d (%s) "
                  "since bundle is a fragment",
                  bundle, registration->regid(),
                  registration->endpoint().c_str());
        
        fragmentmgr_->process_for_reassembly(bundle);
        return;
    }

    log_debug("delivering bundle *%p to registration %d (%s)",
              bundle, registration->regid(),
              registration->endpoint().c_str());
    
    registration->deliver_bundle(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemon::check_registrations(Bundle* bundle)
{
    int num;
    log_debug("checking for matching registrations for bundle *%p", bundle);

    RegistrationList matches;
    RegistrationList::iterator iter;

    num = reg_table_->get_matching(bundle->dest_, &matches);

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
    if (request->bundle_.object()) {
        log_info("BUNDLE_DELETE: bundle *%p (reason %s)",
                 request->bundle_.object(),
                 BundleStatusReport::reason_to_str(request->reason_));
        delete_bundle(request->bundle_.object(), request->reason_);
    }
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
    Bundle* bundle = event->bundleref_.object();

    // update statistics and store an appropriate event descriptor
    const char* source_str = "";
    switch (event->source_) {
    case EVENTSRC_PEER:
        stats_.received_bundles_++;
        break;
        
    case EVENTSRC_APP:
        stats_.received_bundles_++;
        source_str = " (from app)";
        break;
        
    case EVENTSRC_STORE:
        source_str = " (from data store)";
        break;
        
    case EVENTSRC_ADMIN:
        stats_.generated_bundles_++;
        source_str = " (generated)";
        break;
        
    case EVENTSRC_FRAGMENTATION:
        stats_.generated_bundles_++;
        source_str = " (from fragmentation)";
        break;

    case EVENTSRC_ROUTER:
        stats_.generated_bundles_++;
        source_str = " (from router)";
        break;

    default:
        NOTREACHED;
    }

    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("BUNDLE_RECEIVED%s: (%u bytes recvd)\n",
                    source_str, event->bytes_received_);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_DEBUG, buf.c_str());
    } else {
        log_info("BUNDLE_RECEIVED%s *%p (%u bytes recvd)",
                 source_str, bundle, event->bytes_received_);
    }
    
    // log a warning if the bundle doesn't have any expiration time or
    // has a creation time that's in the future. in either case, we
    // proceed as normal
    if (bundle->expiration_ == 0) {
        log_warn("bundle id %d arrived with zero expiration time",
                 bundle->bundleid_);
    }

    u_int32_t now = BundleTimestamp::get_current_time();
    if ((bundle->creation_ts_.seconds_ > now) &&
        (bundle->creation_ts_.seconds_ - now > 30000))
    {
        log_warn("bundle id %d arrived with creation time in the future "
                 "(%u > %u)",
                 bundle->bundleid_, bundle->creation_ts_.seconds_, now);
    }

    /*
     * If a previous hop block wasn't included, but we know the remote
     * endpoint id of the link where the bundle arrived, assign the
     * prevhop_ field in the bundle so it's available for routing.
     */
    if (bundle->prevhop_.str() == "dtn:none" || bundle->prevhop_.str() == "")
    {
        bundle->prevhop_.assign(event->prevhop_);
    }

    if (bundle->prevhop_ != event->prevhop_)
    {
        log_warn("previous hop mismatch: prevhop header contains '%s' but "
                 "convergence layer indicates prevhop is '%s'",
                 bundle->prevhop_.c_str(),
                 event->prevhop_.c_str());
    }
    
    // validate a bundle, including all bundle blocks, received from a peer
    if (event->source_ == EVENTSRC_PEER) { 
        status_report_reason_t
            reception_reason = BundleProtocol::REASON_NO_ADDTL_INFO,
            deletion_reason = BundleProtocol::REASON_NO_ADDTL_INFO;

        bool accept_bundle = BundleProtocol::validate(bundle,
                                                      &reception_reason,
                                                      &deletion_reason);
        
        /*
         * Send the reception receipt if requested within the primary
         * block or some other error occurs that requires a reception
         * status report but may or may not require deleting the whole
         * bundle.
         */
        if (bundle->receive_rcpt_ ||
            reception_reason != BundleProtocol::REASON_NO_ADDTL_INFO)
        {
            generate_status_report(bundle, BundleProtocol::STATUS_RECEIVED,
                                   reception_reason);
        }

        /*
         * If the bundle is valid, probe the router to see if it wants
         * to accept the bundle.
         */
        if (accept_bundle) {
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
            delete_bundle(bundle, deletion_reason);
            event->daemon_only_ = true;
            return;
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
     * Check if the bundle is a duplicate, i.e. shares a source id,
     * timestamp, and fragmentation information with some other bundle
     * in the system.
     */
    Bundle* duplicate = find_duplicate(bundle);
    if (duplicate != NULL) {
        log_notice("got duplicate bundle: %s -> %s creation %u.%u",
                   bundle->source_.c_str(),
                   bundle->dest_.c_str(),
                   bundle->creation_ts_.seconds_,
                   bundle->creation_ts_.seqno_);

        stats_.duplicate_bundles_++;
        
        if (bundle->custody_requested_ && duplicate->local_custody_)
        {
            generate_custody_signal(bundle, false,
                                    BundleProtocol::CUSTODY_REDUNDANT_RECEPTION);
        }

        // since we don't want the bundle to be processed by the rest
        // of the system, we mark the event as daemon_only (meaning it
        // won't be forwarded to routers) and return, which should
        // eventually remove all references on the bundle and then it
        // will be deleted
        event->daemon_only_ = true;
        return;
    }

    /*
     * Check all BlockProcessors to validate the bundle.
     */
    
    /*
     * Add the bundle to the master pending queue and the data store
     * (unless the bundle was just reread from the data store on startup)
     *
     * Note that if add_to_pending returns false, the bundle has
     * already expired so we immediately return instead of trying to
     * deliver and/or forward the bundle. Otherwise there's a chance
     * that expired bundles will persist in the network.
     */
    bool ok_to_route =
        add_to_pending(bundle, (event->source_ != EVENTSRC_STORE));

    if (!ok_to_route) {
        event->daemon_only_ = true;
        return;
    }
    
    /*
     * If the bundle is a custody bundle and we're configured to take
     * custody, then do so. In case the event was delivered due to a
     * reload from the data store, then if we have local custody, make
     * sure it's added to the custody bundles list.
     */
    if (bundle->custody_requested_ && params_.accept_custody_)
    {
        if (event->source_ != EVENTSRC_STORE) {
            accept_custody(bundle);
        
        } else if (bundle->local_custody_) {
            custody_bundles_->push_back(bundle);
        }
    }
    
    /*
     * Deliver the bundle to any local registrations that it matches
     */
    if (event->source_ != EVENTSRC_ADMIN &&
        event->source_ != EVENTSRC_ROUTER)
    {
        check_registrations(bundle);
    }
    
    /*
     * Finally, bounce out so the router(s) can do something further
     * with the bundle in response to the event.
     */
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
              bundle->bundleid_,link->name());
    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
    // If an event about a bundle bound for particular link is posted after another,
    // which it might contradict, the BundleDaemon need not reprocess the event.
    // The router (DP) might, however, be interested in the new status of the send.
    if(blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_transmit event about "
                 "bundle id:%d -> %s (%s)",
                 bundle->bundleid_,
                 link->name(),
                 link->nexthop());
        return;
    }
    
    /*
     * Update statistics. Note that the link's queued length must
     * always be decremented by the full formatted size of the bundle,
     * yet the transmitted length is only the amount reported by the
     * event.
     */
        
    size_t total_len = BundleProtocol::total_length(blocks);
    
    stats_.transmitted_bundles_++;
    link->stats()->bundles_transmitted_++;
    link->stats()->bundles_queued_--;

    link->stats()->bytes_transmitted_ += event->bytes_sent_;
    link->stats()->bytes_queued_ -= total_len;
    
    log_info("BUNDLE_TRANSMITTED id:%d (%u bytes_sent/%u reliable) -> %s (%s)",
             bundle->bundleid_,
             event->bytes_sent_,
             event->reliably_sent_,
             link->name(),
             link->nexthop());

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
        bundle->fwdlog_.update(link, ForwardingInfo::TRANSMIT_FAILED);
        log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
                  bundle->bundleid_,link->name());
        BundleProtocol::delete_blocks(bundle, link);
        return;
    }

    /*
     * Grab the latest forwarding log state so we can find the custody
     * timer information (if any).
     */
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog_.get_latest_entry(link, &fwdinfo);
    if(!ok)
    {
        oasys::StringBuffer buf;
        bundle->fwdlog_.dump(&buf);
        log_debug("%s",buf.c_str());
    }
    ASSERTF(ok, "no forwarding log entry for transmission");
    ASSERT(fwdinfo.state() == ForwardingInfo::IN_FLIGHT);
    
    /*
     * Update the forwarding log indicating that the bundle is no
     * longer in flight.
     */
    log_debug("updating forwarding log entry on *%p for *%p to TRANSMITTED",
              bundle, link.object());
    bundle->fwdlog_.update(link, ForwardingInfo::TRANSMITTED);
                            
    /*
     * Check for reactive fragmentation. If the bundle was only
     * partially sent, then a new bundle received event for the tail
     * part of the bundle will be processed immediately after this
     * event.
     */
    if (link->reliable_) {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 BundleProtocol::payload_offset(blocks),
                                                 event->reliably_sent_);
    } else {
        fragmentmgr_->try_to_reactively_fragment(bundle,
                                                 BundleProtocol::payload_offset(blocks),
                                                 event->bytes_sent_);
    }

    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
     log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
               bundle->bundleid_,link->name());
    BundleProtocol::delete_blocks(bundle, link);
    blocks = NULL;

    /*
     * Generate the forwarding status report if requested
     */
    if (bundle->forward_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_FORWARDED);
    }
    
    /*
     * Schedule a custody timer if we have custody.
     */
    if (bundle->local_custody_) {
        bundle->custody_timers_.push_back(
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

    // remove the bundle from the link's delayed-send queue
    if (link->queue()->size() != 0) {
        if(link->queue()->erase(bundle,false))
        {
                log_info("removed delayed-send bundle id:%d from link %s queue",
                 bundle->bundleid_,
                 link->name());
        }
    }
    
    /*
     * Check if we should can delete the bundle from the pending list,
     * i.e. we don't have custody and it's not being transmitted
     * anywhere else.
     */
    try_delete_from_pending(bundle);
}

//----------------------------------------------------------------------

/*void
BundleDaemon::handle_bundle_transmit_failed(BundleTransmitFailedEvent* event)
{
    
    // The bundle was delivered to a next-hop contact.
     
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",bundle->bundleid_,link->name());
    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed messages.
    // If an event about a bundle bound for particular link is posted after another,
    // which it might contradict, the BundleDaemon need not reprocess the event.
    // The router (DP) might, however, be interested in the new status of the send.
    if(blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_transmit_failed event about bundle id:%d -> %s (%s)",
             bundle->bundleid_,
             link->name(),
             link->nexthop());
        return;
    }
    
    log_debug("trying to delete xmit blocks for bundle id:%d on link %s",bundle->bundleid_,link->name());
    BundleProtocol::delete_blocks(bundle, link);
    
    log_info("BUNDLE_TRANSMIT_FAILED id:%d -> %s (%s)",
             bundle->bundleid_,
             event->contact_->link()->name(),
             event->contact_->link()->nexthop());
    

    // Update the forwarding log so routers know to try to retransmit
    // on the next contact.
    
    bundle->fwdlog_.update(event->contact_->link(),
                           ForwardingInfo::TRANSMIT_FAILED);

    
    // Fall through to notify the routers
    
}*/

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    // update statistics
    stats_.delivered_bundles_++;
    
    /*
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_DELIVERED id:%d (%zu bytes) -> regid %d (%s)",
             bundle->bundleid_, bundle->payload_.length(),
             event->registration_->regid(),
             event->registration_->endpoint().c_str());

    /*
     * Generate the delivery status report if requested.
     */
    if (bundle->delivery_rcpt_)
    {
        generate_status_report(bundle, BundleProtocol::STATUS_DELIVERED);
    }

    /*
     * If this is a custodial bundle and it was delivered, we either
     * release custody (if we have it), or send a custody signal to
     * the current custodian indicating that the bundle was
     * successfully delivered, unless there is no current custodian
     * (the eid is still dtn:none).
     */
    if (bundle->custody_requested_)
    {
        if (bundle->local_custody_) {
            release_custody(bundle);

        } else if (bundle->custodian_.equals(EndpointID::NULL_EID())) {
            log_info("custodial bundle *%p delivered before custody accepted",
                     bundle);

        } else {
            generate_custody_signal(bundle, true,
                                    BundleProtocol::CUSTODY_NO_ADDTL_INFO);
        }
    }
    
    /*
     * Finally, check if we can and should delete the bundle from the
     * pending list, i.e. we don't have custody and it's not being
     * transmitted anywhere else.
     */
    try_delete_from_pending(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_expired(BundleExpiredEvent* event)
{
    // update statistics
    stats_.expired_bundles_++;
    
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_EXPIRED *%p", bundle);

    // note that there may or may not still be a pending expiration
    // timer, since this event may be coming from the console, so we
    // just fall through to delete_bundle which will cancel the timer

    delete_bundle(bundle, BundleProtocol::REASON_LIFETIME_EXPIRED);
    
    // fall through to notify the routers
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_not_needed(BundleNotNeededEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_info("BUNDLE_NOT_NEEDED *%p", bundle);
    try_delete_from_pending(bundle);
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

    actions_->send_bundle(br.object(), link,
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

    // If the request has a link name, we are just canceling the send on
    // that link.
    if (!event->link_.empty()) {
        LinkRef link = contactmgr_->find_link(event->link_.c_str());
        if (link == NULL) {
            log_err("BUNDLE_CANCEL no link with name %s", event->link_.c_str());
            return;
        }

        log_info("BUNDLE_CANCEL bundle %d on link %s", br->bundleid_,
                event->link_.c_str());
        
        actions_->cancel_bundle(br.object(), link);
    }
    
    // If the request does not have a link name, the bundle itself has been
    // canceled (probably by an application).
    else {
        delete_bundle(br.object());
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
    Bundle* bundle = event->bundleref_.object();

    LinkRef link = event->link_;
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
            bundle->bundleid_,link->name());
    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    
    // Because a CL is running in another thread or process (External CLs),
    // we cannot prevent all redundant transmit/cancel/transmit_failed 
    // messages. If an event about a bundle bound for particular link is 
    // posted after  another, which it might contradict, the BundleDaemon 
    // need not reprocess the event. The router (DP) might, however, be 
    // interested in the new status of the send.
    if(blocks == NULL)
    {
        log_info("received a redundant/conflicting bundle_cancelled event "
                "about bundle id:%d -> %s (%s)",
            bundle->bundleid_,
            link->name(),
            link->nexthop());
        return;
    }
        
    size_t total_len = BundleProtocol::total_length(blocks);
    
    /*
     * Update statistics. Note that the link's queued length must
     * always be decremented by the full formatted size of the bundle.
     */
    link->stats()->bundles_queued_--;
    link->stats()->bytes_queued_ -= total_len;
    
    log_info("BUNDLE_CANCELLED id:%d -> %s (%s)",
            bundle->bundleid_,
            link->name(),
            link->nexthop());
    
    /*
     * Remove the formatted block info from the bundle since we don't
     * need it any more.
     */
    log_debug("trying to delete xmit blocks for bundle id:%d on link %s",
            bundle->bundleid_,link->name());
    BundleProtocol::delete_blocks(bundle, link);
    blocks = NULL;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_inject(BundleInjectRequest* event)
{
    LinkRef link = contactmgr_->find_link(event->link_.c_str());
    if (link == NULL) return;

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
    Bundle *bundle=new Bundle();

    bundle->source_.assign(src);
    bundle->dest_.assign(dest);
    
    if (! bundle->replyto_.assign(event->replyto_))
        bundle->replyto_.assign(EndpointID::NULL_EID());

    if (! bundle->custodian_.assign(event->custodian_))
        bundle->custodian_.assign(EndpointID::NULL_EID()); 

    // bundle COS defaults to COS_BULK
    bundle->priority_ = event->priority_;

    // bundle expiration on remote dtn nodes
    // defaults to 5 minutes
    if(event->expiration_ == 0)
        bundle->expiration_ = 300;
    else
        bundle->expiration_ = event->expiration_;
    

    // set the payload
    bundle->payload_.set_data(event->payload_, event->payload_length_);

    // The injected bundle is no longer sent automatically. It is
    // instead added to the pending queue so that it can be resent
    // or sent on multiple links.

    // If add_to_pending returns false, the bundle has already expired
    if (add_to_pending(bundle, 0))
        BundleDaemon::post(new BundleInjectedEvent(bundle, event->request_id_));
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
                                        request->extension_blocks_));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_attributes_report(BundleAttributesReportEvent* event)
{
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

        if (registration->endpoint().match(bundle->dest_))
        {
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
    
    if (!reg_table_->del(registration->regid())) {
        log_err("error removing registration %d from table",
                registration->regid());
        return;
    }

    delete registration;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_expired(RegistrationExpiredEvent* event)
{
    Registration* registration = reg_table_->get(event->regid_);

    if (registration == NULL) {
        log_err("REGISTRATION_EXPIRED -- dead regid %d", event->regid_);
        return;
    }
    
    registration->set_expired(true);
    
    if (registration->active()) {
        // if the registration is currently active (i.e. has a
        // binding), we wait for the binding to clear, which will then
        // clean up the registration
        log_info("REGISTRATION_EXPIRED %d -- deferred until binding clears",
                 event->regid_);
    } else {
        // otherwise remove the registration from the table
        log_info("REGISTRATION_EXPIRED %d", event->regid_);
        reg_table_->del(registration->regid());
        delete registration;
    }
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

    log_info("LINK_CREATED *%p", link.object());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_deleted(LinkDeletedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(link->isdeleted());

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
BundleDaemon::handle_link_busy(LinkBusyEvent* event)
{
    LinkRef link = event->link_;
    if (link->isbusy())
    {    
        log_info("LINK BUSY *%p", link.object());
    }
    else
    {
        log_info("STALE LINK BUSY NOTIFICATION FOR LINK *%p -- not telling the router or the contact manager", link.object());
        event->daemon_only_ = true;
    }
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
            
        } else if (link->state() == Link::BUSY &&
                   reason        == ContactEvent::UNBLOCKED) {
            ASSERT(link->contact() != NULL);
            link->set_state(Link::OPEN);
            
        } else if (link->state() == Link::OPEN &&
                   reason        == ContactEvent::UNBLOCKED) {
            // a CL might send multiple requests to go from
            // BUSY->AVAILABLE, so we can safely ignore this
            
        } else {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link.object(), Link::state_to_str(link->state()));
            return;
        }

        post_at_head(new LinkAvailableEvent(link,
                     ContactEvent::reason_t(reason)));
        break;
        
    case Link::BUSY:
        log_err("LINK_STATE_CHANGE_REQUEST can't be used for state %s",
                Link::state_to_str(new_state));
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
    
    bool is_queued = link->clayer()->is_queued(link, request->bundle_.object());
    BundleDaemon::post(
        new BundleQueuedReportEvent(request->query_id_, is_queued));
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_queued_report(BundleQueuedReportEvent* event)
{
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

        // If the convergence layer doesn't retain bundles between link up/down
        // events, then we need to copy the delayed-send queue to it from the link
    if (!(link->clayer()->has_persistent_link_queues())
            && link->queue()->size() != 0) {
        log_info("sending %zu delayed-send bundles from link %s to clayer %s",
                 link->queue()->size(),
                 link->name(),
                 link->clayer()->name());

        oasys::ScopeLock l(link->queue()->lock(),
                           "BundleDaemon::handle_contact_up");
        BundleList::iterator i;
        for (i = link->queue()->begin(); i != link->queue()->end(); ++i) {
            link->clayer()->send_bundle(contact, *i);
        }
    }
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

    // we don't need to do anything here since we just generated this
    // event in response to a link state change request
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
             event->bundle_->bundleid_);

    // remove all the fragments from the pending list
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        try_delete_from_pending(ref.object());
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
    log_info("CUSTODY_SIGNAL: %s %u.%u %s (%s)",
             event->data_.orig_source_eid_.c_str(),
             event->data_.orig_creation_tv_.seconds_,
             event->data_.orig_creation_tv_.seqno_,
             event->data_.succeeded_ ? "succeeded" : "failed",
             CustodySignal::reason_to_str(event->data_.reason_));

    BundleRef orig_bundle =
        custody_bundles_->find(event->data_.orig_source_eid_,
                               event->data_.orig_creation_tv_);
    
    if (orig_bundle == NULL) {
        log_warn("received custody signal for bundle %s %u.%u "
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
        log_notice("releasing custody for bundle %s %u.%u "
                   "due to redundant reception",
                   event->data_.orig_source_eid_.c_str(),
                   event->data_.orig_creation_tv_.seconds_,
                   event->data_.orig_creation_tv_.seqno_);
        
        release = true;
    }
    
    if (release) {
        release_custody(orig_bundle.object());
        try_delete_from_pending(orig_bundle.object());
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
    
    // remove and delete the expired timer from the bundle
    oasys::ScopeLock l(&bundle->lock_, "BundleDaemon::handle_custody_timeout");

    bool found = false;
    CustodyTimer* timer = NULL;
    CustodyTimerVec::iterator iter;
    for (iter = bundle->custody_timers_.begin();
         iter != bundle->custody_timers_.end();
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
            bundle->custody_timers_.erase(iter);
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
    bool ok = bundle->fwdlog_.update(link, ForwardingInfo::CUSTODY_TIMEOUT);
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
bool
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    log_debug("adding bundle *%p to pending list", bundle);
    
    pending_bundles_->push_back(bundle);
    
    if (add_to_store) {
        bundle->in_datastore_ = true;
        actions_->store_add(bundle);
    }

    struct timeval now;
    gettimeofday(&now, 0);
    
    // schedule the bundle expiration timer
    struct timeval expiration_time;
    expiration_time.tv_sec =
        BundleTimestamp::TIMEVAL_CONVERSION +
        bundle->creation_ts_.seconds_ + 
        bundle->expiration_;
    
    expiration_time.tv_usec = now.tv_usec;
    
    long int when = expiration_time.tv_sec - now.tv_sec;

    bool ok_to_route = true;
    
    if (when > 0) {
        log_debug_p("/dtn/bundle/expiration",
                    "scheduling expiration for bundle id %d at %u.%u "
                    "(in %lu seconds)",
                    bundle->bundleid_,
                    (u_int)expiration_time.tv_sec, (u_int)expiration_time.tv_usec,
                    when);
    } else {
        log_warn_p("/dtn/bundle/expiration",
                   "scheduling IMMEDIATE expiration for bundle id %d: "
                   "[expiration %u, creation time %u.%u, offset %u, now %u.%u]",
                   bundle->bundleid_, bundle->expiration_,
                   (u_int)bundle->creation_ts_.seconds_,
                   (u_int)bundle->creation_ts_.seqno_,
                   BundleTimestamp::TIMEVAL_CONVERSION,
                   (u_int)now.tv_sec, (u_int)now.tv_usec);
        expiration_time = now;
        ok_to_route = false;
    }

    bundle->expiration_timer_ = new ExpirationTimer(bundle);
    bundle->expiration_timer_->schedule_at(&expiration_time);

    return ok_to_route;
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_from_pending(Bundle* bundle)
{
    log_debug("removing bundle *%p from pending list", bundle);

    // first try to cancel the expiration timer if it's still
    // around
    if (bundle->expiration_timer_) {
        log_debug("cancelling expiration timer for bundle id %d",
                  bundle->bundleid_);
        
        bool cancelled = bundle->expiration_timer_->cancel();
        if (!cancelled) {
           log_crit("unexpected error cancelling expiration timer "
                     "for bundle *%p", bundle);
        }
        
        bundle->expiration_timer_->bundleref_.release();
        bundle->expiration_timer_ = NULL;
    }

    // XXX/demmer the whole BundleDaemon core should be changed to use
    // BundleRefs instead of Bundle*, as should the BundleList API, as
    // should the whole system, really...
    bool erased = pending_bundles_->erase(bundle);

    if (!erased) {
        log_err("unexpected error removing bundle from pending list");
    }

    return erased;
}

//----------------------------------------------------------------------
bool
BundleDaemon::try_delete_from_pending(Bundle* bundle)
{
    /*
     * Check to see if we should remove the bundle from the pending
     * list, after which all references to the bundle should be
     * cleaned up and the bundle will be deleted from the system.
     *
     * We do this only if:
     *
     * 1) We're configured for early deletion
     * 2) The bundle isn't queued on any lists other than the pending
     *    list. This covers the case where we have custody, since the
     *    bundle will be on the custody_bundles list as well as allowing
     *    routers to hold onto bundles by just putting them on a list.
     * 3) The bundle isn't currently in flight, as recorded
     *    in the forwarding log.
     */

    if (! bundle->is_queued_on(pending_bundles_))
    {
        if (bundle->expiration_timer_ == NULL) {
            log_debug("try_delete_from_pending(*%p): bundle already expired",
                      bundle);
            return false;
        }
        
        log_err("try_delete_from_pending(*%p): bundle not in pending list!",
                bundle);
        return false;
    }

    if (!params_.early_deletion_) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "early deletion disabled",
                  bundle);
        return false;
    }

    size_t num_mappings = bundle->num_mappings();
    if (num_mappings != 1) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "bundle has %zu list mappings",
                  bundle, num_mappings);
        return false;
    }
    
    size_t num_in_flight = bundle->fwdlog_.get_count(ForwardingInfo::IN_FLIGHT);
    if (num_in_flight > 0) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "bundle in flight on %zu links",
                  bundle, num_in_flight);
        return false;
    }

    size_t num_pending =
        bundle->fwdlog_.get_count(ForwardingInfo::TRANSMIT_PENDING);
    if (num_pending > 0) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "bundle transmission pending on %zu links",
                  bundle, num_pending);
        return false;
    }
    
    return delete_from_pending(bundle);
}

//----------------------------------------------------------------------
bool
BundleDaemon::delete_bundle(Bundle* bundle, status_report_reason_t reason)
{
    ++stats_.deleted_bundles_;
    
    // send a bundle deletion status report if we have custody or the
    // bundle's deletion status report request flag is set and a reason
    // for deletion is provided
    bool send_status = (bundle->local_custody_ ||
                       (bundle->deletion_rcpt_ &&
                        reason != BundleProtocol::REASON_NO_ADDTL_INFO));
        
    // check if we have custody, if so, remove it
    if (bundle->local_custody_) {
        release_custody(bundle);
    }

    // XXX/demmer if custody was requested but we didn't take it yet
    // (due to a validation error, space constraints, etc), then we
    // should send a custody failed signal here

    // check if bundle is a fragment, if so, remove any fragmentation state
    if (bundle->is_fragment_) {
        fragmentmgr_->delete_fragment(bundle);
    }

    // delete the bundle from the pending list
    bool erased = true;
    if (bundle->is_queued_on(pending_bundles_)) {
        erased = delete_from_pending(bundle);
    }

    if (erased && send_status) {
        generate_status_report(bundle, BundleProtocol::STATUS_DELETED, reason);
    }

    // We have a choice to track what bundles are active on what links
    // and then cancel only on appropriate links, or just cancel on every link.
    // Since canceling a send is not likely the common case, we will cancel
    // the bundle on all links for now.
    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::delete_bundle");
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    for (iter = links->begin(); iter != links->end(); ++iter) {
        actions_->cancel_bundle(bundle, (*iter));
    }
    

    return erased;
}

//----------------------------------------------------------------------
Bundle*
BundleDaemon::find_duplicate(Bundle* b)
{
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "BundleDaemon::find_duplicate");
    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        Bundle* b2 = *iter;
        
        if ((b->source_.equals(b2->source_)) &&
            (b->creation_ts_.seconds_ == b2->creation_ts_.seconds_) &&
            (b->creation_ts_.seqno_   == b2->creation_ts_.seqno_) &&
            (b->is_fragment_          == b2->is_fragment_) &&
            (b->frag_offset_          == b2->frag_offset_) &&
            (b->orig_length_          == b2->orig_length_) &&
            (b->payload_.length()     == b2->payload_.length()))
        {
            return b2;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_free(BundleFreeEvent* event)
{
    Bundle* bundle = event->bundle_;
    event->bundle_ = NULL;
    ASSERT(bundle->refcount() == 0);
    
    bundle->lock_.lock("BundleDaemon::handle_bundle_free");

    if (bundle->in_datastore_) {
        log_debug("removing freed bundle from data store");
        actions_->store_del(bundle);
    }
    log_debug("deleting freed bundle");
    delete bundle;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_event(BundleEvent* event)
{
    dispatch_event(event);
    
    if (! event->daemon_only_) {
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(event);
        contactmgr_->handle_event(event);
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
BundleDaemon::load_bundles()
{
    Bundle* bundle;
    BundleStore* bundle_store = BundleStore::instance();
    BundleStore::iterator* iter = bundle_store->new_iterator();

    log_notice("loading bundles from data store");

    u_int64_t total_size = 0;
    
    for (iter->begin(); iter->more(); iter->next()) {
        bundle = bundle_store->get(iter->cur_val());
        
        if (bundle == NULL) {
            log_err("error loading bundle %d from data store",
                    iter->cur_val());
            continue;
        }

        total_size += bundle->durable_size();
        
        BundleReceivedEvent e(bundle, EVENTSRC_STORE);
        handle_event(&e);

        // in the constructor, we disabled notifiers on the event
        // queue, so in case loading triggers other events, we just
        // let them queue up and handle them later when we're done
        // loading all the bundles
    }
    
    bundle_store->set_total_size(total_size);

    delete iter;
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
    
    load_registrations();
    load_bundles();

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
        
                log_debug_p(LOOP_LOG, "BundleDaemon: handling event %s",
                        event->type_str());
            // handle the event
            handle_event(event);

            int elapsed = now.elapsed_ms();
            if (elapsed > 2000) {
                log_warn_p(LOOP_LOG, "event %s took %d ms to process",
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
