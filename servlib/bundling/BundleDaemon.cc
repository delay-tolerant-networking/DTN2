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

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleStatusReport.h"
#include "CustodySignal.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"

namespace dtn {

BundleDaemon* BundleDaemon::instance_ = NULL;
BundleDaemon::Params BundleDaemon::params_;

//----------------------------------------------------------------------
BundleDaemon::BundleDaemon()
    : BundleEventHandler("/bundle/daemon"),
      Thread("BundleDaemon", CREATE_JOINABLE)
{
    // default local eid
    // XXX/demmer fixme
    local_eid_.assign("dtn:localhost");

    bundles_received_ = 0;
    bundles_delivered_ = 0;
    bundles_generated_ = 0;
    bundles_transmitted_ = 0;
    bundles_expired_ = 0;

    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();
    reg_table_ = new RegistrationTable();

    router_ = 0;

    app_shutdown_proc_ = NULL;
    app_shutdown_data_ = NULL;
}

//----------------------------------------------------------------------
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>(logpath_);
}

//----------------------------------------------------------------------
void
BundleDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

//----------------------------------------------------------------------
bool
BundleDaemon::post_and_wait(BundleEvent* event,
                            oasys::Notifier* notifier,
                            int timeout)
{
    ASSERT(instance()->started());
    
    ASSERT(event->processed_notifier_ == NULL);
    event->processed_notifier_ = notifier;
    post(event);
    return notifier->wait(NULL, timeout);
}

//----------------------------------------------------------------------
void
BundleDaemon::post_event(BundleEvent* event)
{
    log_debug("posting event (%p) with type %s", event, event->type_str());
    eventq_->push(event);
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
BundleDaemon::get_statistics(oasys::StringBuffer* buf)
{
    buf->appendf("%u pending -- "
                 "%u custody -- "
                 "%u received -- "
                 "%u delivered -- "
                 "%u generated -- "
                 "%u transmitted -- "
                 "%u expired",
                 (u_int)pending_bundles()->size(),
                 (u_int)custody_bundles()->size(),
                 bundles_received_,
                 bundles_delivered_,
                 bundles_generated_,
                 bundles_transmitted_,
                 bundles_expired_);
}

//----------------------------------------------------------------------
void
BundleDaemon::reset_statistics()
{
    bundles_received_    = 0;
    bundles_delivered_   = 0;
    bundles_generated_   = 0;
    bundles_transmitted_ = 0;
    bundles_expired_     = 0;
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
    
    BundleReceivedEvent e(report, EVENTSRC_ADMIN, report->payload_.length());
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
    
    BundleReceivedEvent e(signal, EVENTSRC_ADMIN, signal->payload_.length());
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
        // XXX/demmer fix this race
        if (!(*iter)->cancel()) {
            log_err("custody timer cancel race!!");
            continue;
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

    // tells routers that this Bundle has been taken care of
    // by the daemon already
    if (num > 0) {
        bundle->owner_ = "daemon";
    }

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        Registration* registration = *iter;
        deliver_to_registration(bundle, registration);
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();

    // update statistics and store an appropriate event descriptor
    const char* source_str = "";
    if (event->source_ == EVENTSRC_ADMIN) {
        bundles_generated_++;
        source_str = " (generated)";
    } else if (event->source_ == EVENTSRC_STORE) {
        source_str = " (from data store)";
    } else {
        bundles_received_++;
    }
    
    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("BUNDLE_RECEIVED%s: (%u bytes recvd)\n",
                    source_str, (u_int)event->bytes_received_);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_DEBUG, buf.c_str());
    } else {
        log_info("BUNDLE_RECEIVED%s *%p (%u bytes recvd)",
                 source_str, bundle, (u_int)event->bytes_received_);
    }
    
    // log a warning if the bundle doesn't have any expiration time or
    // has a creation time in the future. in either case, we proceed as
    // normal, though we know that the expiration timer will
    // immediately fire
    if (bundle->expiration_ == 0) {
        log_warn("bundle id %d arrived with zero expiration time",
                 bundle->bundleid_);
    }

    struct timeval now;
    gettimeofday(&now, 0);
    if (TIMEVAL_GT(bundle->creation_ts_, now) &&
        TIMEVAL_DIFF_MSEC(bundle->creation_ts_, now) > 30000)
    {
        log_warn("bundle id %d arrived with creation time in the future "
                 "(%u.%u > %u.%u)",
                 bundle->bundleid_,
                 (u_int)bundle->creation_ts_.tv_sec,
                 (u_int)bundle->creation_ts_.tv_usec,
                 (u_int)now.tv_sec, (u_int)now.tv_usec);
    }

    /*
     * Send the reception receipt 
     */
    if (bundle->receive_rcpt_ && (event->source_ != EVENTSRC_STORE)) {
        generate_status_report(bundle, BundleProtocol::STATUS_RECEIVED);
    }

    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_debug("partial bundle, making reactive fragment of %u bytes",
                  (u_int)event->bytes_received_);

        fragmentmgr_->convert_to_fragment(bundle, event->bytes_received_);
    }

    /*
     * Add the bundle to the master pending queue and the data store
     * (unless the bundle was just reread from the data store on startup)
     */
    add_to_pending(bundle, (event->source_ != EVENTSRC_STORE));

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
    check_registrations(bundle);
    
    /*
     * Finally, bounce out so the router(s) can do something further
     * with the bundle in response to the event.
     */
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    // update statistics
    bundles_transmitted_++;
    
    /*
     * The bundle was delivered to a next-hop contact.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_TRANSMITTED id:%d (%u bytes) %s -> %s (%s)",
             bundle->bundleid_, (u_int)event->bytes_sent_,
             event->acked_ ? "ACKED" : "UNACKED",
             event->contact_->link()->name(),
             event->contact_->nexthop());

    /*
     * Update the forwarding log
     */
    bundle->fwdlog_.update(event->contact_->link(), ForwardingInfo::SENT);
                            
    /*
     * Grab the updated forwarding log information.
     */
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog_.get_latest_entry(event->contact_->link(), &fwdinfo);
    ASSERTF(ok, "no forwarding log entry for transmission");
    ASSERT(fwdinfo.state_ == ForwardingInfo::SENT);
    
    /*
     * Check for reactive fragmentation. The unsent portion (if any)
     * will show up as a new bundle received event.
     */
    fragmentmgr_->reactively_fragment(bundle, event->bytes_sent_);
    
    if (bundle->forward_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_FORWARDED);
    }

    if (bundle->local_custody_) {
        bundle->custody_timers_.push_back(
            new CustodyTimer(fwdinfo.timestamp_,
                             fwdinfo.custody_timer_,
                             bundle, event->contact_->link()));
        
        // XXX/TODO: generate failed custodial signal for "forwarded
        // over unidirectional link" if the bundle has the retention
        // constraint "custody accepted" and all of the nodes in the
        // minimum reception group of the endpoint selected for
        // forwarding are known to be unable to send bundles back to
        // this node
    }

    /*
     * Check if we should can delete the bundle from the pending list,
     * i.e. we don't have custody and it's not being transmitted
     * anywhere else.
     */
    try_delete_from_pending(bundle);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    // update statistics
    bundles_delivered_++;
    
    /*
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_DELIVERED id:%d (%u bytes) -> regid %d (%s)",
             bundle->bundleid_, (u_int)bundle->payload_.length(),
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
            log_info("custodial bundle *%p delivered before custody accepted", bundle);

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
    bundles_expired_++;
    
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_EXPIRED *%p", bundle);

    ASSERT(bundle->expiration_timer_ == NULL);

    // cancel any pending custody timers for the bundle
    cancel_custody_timers(bundle);

    // check that the bundle is on the pending list and then remove it
    // if it is. if it's not, then there was a race between the
    // expiration timer and a prior call to delete_from_pending. check
    // that the bundle isn't on any other lists (which it shouldn't
    // be), and return
    if (pending_bundles_->contains(bundle)) {
        delete_from_pending(bundle, BundleProtocol::REASON_LIFETIME_EXPIRED);

    } else {
        if (bundle->num_mappings() == 0) {
            log_debug("expired bundle *%p already removed from pending "
                      "list and all other lists", bundle);
            return;

        } else {
            log_err("expired bundle *%p not on pending list but still "
                    "queued on other lists!!", bundle);

            // fall through to remove other mappings
        }
    }

    // XXX/demmer should try to cancel transmission on any links where
    // the bundle is active
    
    // fall through to notify the routers
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_added(RegistrationAddedEvent* event)
{
    Registration* registration = event->registration_;
    log_info("REGISTRATION_ADDED %d %s",
             registration->regid(), registration->endpoint().c_str());

    if (!reg_table_->add(registration)) {
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
    }

    delete registration;
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_registration_expired(RegistrationExpiredEvent* event)
{
    
    Registration* registration = reg_table_->get(event->regid_);
    if (registration == NULL) {
        log_warn("REGISTRATION_EXPIRED -- dead regid %d", event->regid_);
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
BundleDaemon::handle_link_available(LinkAvailableEvent* event)
{
    Link* link = event->link_;
    ASSERT(link->isavailable());

    log_info("LINK_AVAILABLE *%p", link);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_unavailable(LinkUnavailableEvent* event)
{
    Link* link = event->link_;
    ASSERT(!link->isavailable());
    
    log_info("LINK UNAVAILABLE *%p", link);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_link_state_change_request(LinkStateChangeRequest* request)
{
    Link* link = request->link_;
    Link::state_t new_state = request->state_;
    ContactEvent::reason_t reason = request->reason_;
    
    log_info("LINK_STATE_CHANGE_REQUEST *%p [%s -> %s] (%s)", link,
             Link::state_to_str(link->state()),
             Link::state_to_str(new_state),
             ContactEvent::reason_to_str(reason));

    switch(new_state) {
    case Link::UNAVAILABLE:
        if (link->state() != Link::AVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state UNAVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);
        post(new LinkUnavailableEvent(link, reason));
        break;

    case Link::AVAILABLE:
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);

        } else if (link->state() == Link::BUSY) {
            link->set_state(Link::OPEN);

        } else if (link->state() == Link::OPEN) {
            // a CL might send multiple requests to go from
            // BUSY->OPEN, so we can safely ignore this
            
        } else {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        
        post(new LinkAvailableEvent(link, reason));
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

    case Link::CLOSING:
        if (link->isclosing()) {
            log_warn("link close request for *%p (%s) already in closing state",
                     link, ContactEvent::reason_to_str(reason));
            break;
        }
        
        if (link->isopen()) {
            Contact* contact = link->contact();
            ASSERT(contact);

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        
        } else {
            // The only case where we should get this event when the link
            // is not actually open is if it's in the process of being
            // opened but the CL can't actually open it. Check this and
            // fall through to close the link and deliver a
            // LinkUnavailableEvent
            if (! link->isopening()) {
                log_err("LINK_CLOSE_REQUEST *%p (%s) in unexpected state %s",
                        link, ContactEvent::reason_to_str(reason),
                        link->state_to_str(link->state()));
            }

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        }
    
        // if this request is user generated, we need to inform the
        // routers that the contact is going down
        if (reason == ContactEvent::USER) {
            ContactDownEvent e(link->contact(), reason);
            handle_event(&e);
        }
        
        // now actually close the link
        link->close();

        // now, based on the reason code, update the link availability
        // and set state accordingly
        if (reason == ContactEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            post(new LinkUnavailableEvent(link, reason));
        }

        break;

    default:
        PANIC("unhandled state %s", Link::state_to_str(new_state));
    }
}
  
//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_up(ContactUpEvent* event)
{
    Contact* contact = event->contact_;
    log_info("CONTACT_UP *%p", contact);
    
    Link* link = event->contact_->link();
    link->set_state(Link::OPEN);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_contact_down(ContactDownEvent* event)
{
    Contact* contact = event->contact_;
    Link* link = event->contact_->link();
    ContactEvent::reason_t reason = event->reason_;
    
    log_info("CONTACT_DOWN *%p (%s)",
             contact, ContactEvent::reason_to_str(reason));
    
    // based on the reason code, update the link availability
    // and set state accordingly
    if (reason == ContactEvent::IDLE) {
        post(new LinkStateChangeRequest(link, Link::CLOSING, reason));
    } else {
        link->set_state(Link::UNAVAILABLE);
        post(new LinkUnavailableEvent(link, reason));
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_reassembly_completed(ReassemblyCompletedEvent* event)
{
    log_info("REASSEMBLY_COMPLETED bundle id %d",
             event->bundle_->bundleid_);
    
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        // remove the fragment from the pending list
        delete_from_pending(ref.object(),
                            BundleProtocol::REASON_NO_ADDTL_INFO);
    }

    // post a new event for the newly reassembled event
    post(new BundleReceivedEvent(ref.object(), EVENTSRC_FRAGMENTATION,
                                 ref->payload_.length()));
}


//----------------------------------------------------------------------
void
BundleDaemon::handle_route_add(RouteAddEvent* event)
{
    oasys::StringBuffer buf;
    event->entry_->dump(&buf);
    log_info("ROUTE_ADD %s", buf.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_route_del(RouteDelEvent* event)
{
    log_info("ROUTE_DEL %s", event->dest_.c_str());
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_signal(CustodySignalEvent* event)
{
    log_info("CUSTODY_SIGNAL: %s %u.%u %s",
             event->data_.orig_source_eid_.c_str(),
             (u_int)event->data_.orig_creation_tv_.tv_sec,
             (u_int)event->data_.orig_creation_tv_.tv_usec,
             event->data_.succeeded_ ? "succeeded" : "failed");

    BundleRef orig_bundle =
        custody_bundles_->find(event->data_.orig_source_eid_,
                               event->data_.orig_creation_tv_);
    
    if (orig_bundle == NULL) {
        log_warn("received custody signal for bundle %s %u.%u "
                 "but don't have custody",
                 event->data_.orig_source_eid_.c_str(),
                 (u_int)event->data_.orig_creation_tv_.tv_sec,
                 (u_int)event->data_.orig_creation_tv_.tv_usec);
        return;
    }
    
    if (event->data_.succeeded_) {
        release_custody(orig_bundle.object());
        try_delete_from_pending(orig_bundle.object());
    }
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    Bundle* bundle = event->bundle_.object();
    Link*   link   = event->link_;
    
    log_info("CUSTODY_TIMEOUT *%p, *%p", bundle, link);
    
    // remove and delete the expired timer from the bundle
    oasys::ScopeLock l(&bundle->lock_, "BundleDaemon::handle_custody_timeout");

    bool found = false;
    CustodyTimer* timer;
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
                bundle, link);
        return;
    }

    // XXX/demmer fix this race
    if (timer->cancelled()) {
        log_err("custody timer for *%p *%p: timer was cancelled after it fired",
                bundle, link);
    }
    
    if (!pending_bundles_->contains(bundle)) {
        log_err("custody timeout for *%p *%p: bundle not in pending list",
                bundle, link);
    }

    // add an entry to the forwarding log to indicate that we got the
    // custody failure signal. this simplifies the task of routers, as
    // the most recent entry in the log will not be SENT, so the
    // router will know to retransmit the bundle.
    bundle->fwdlog_.add_entry(link, FORWARD_INVALID,
                              ForwardingInfo::CUSTODY_TIMEOUT,
                              CustodyTimerSpec::defaults_);

    delete timer;

    // now fall through to let the router handle the event, typically
    // triggering a retransmission to the link in the event
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_shutdown_request(ShutdownRequest* request)
{
    log_notice("Received shutdown request");

    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::handle_shutdown");
    
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    Link* link = NULL;

    // close any open links
    for (iter = links->begin(); iter != links->end(); ++iter)
    {
        link = *iter;
        if (link->isopen()) {
            log_debug("Shutdown: closing link *%p\n", link);
            if ((link->state() != Link::CLOSING)) {
                link->set_state(Link::CLOSING);
            }
            link->close();
        }
    }

    // XXX/todo: cleanly sync the various data stores

    if (app_shutdown_proc_) {
        (*app_shutdown_proc_)(app_shutdown_data_);
    }

    // signal to the main loop to bail
    set_should_stop();
}

//----------------------------------------------------------------------
void
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    log_debug("adding bundle *%p to pending list", bundle);
    
    pending_bundles_->push_back(bundle);
    
    if (add_to_store) {
        actions_->store_add(bundle);
    }

    // schedule the bundle expiration timer
    struct timeval expiration_time = bundle->creation_ts_;
    expiration_time.tv_sec += bundle->expiration_;

    struct timeval now;
    gettimeofday(&now, 0);
    long int when = TIMEVAL_DIFF_MSEC(expiration_time, now);

    if (when > 0) {
        log_debug("scheduling expiration for bundle id %d at %u.%u (in %lu msec)",
                  bundle->bundleid_,
                  (u_int)expiration_time.tv_sec, (u_int)expiration_time.tv_usec, when);
    } else {
        log_warn("scheduling IMMEDIATE expiration for bundle id %d: "
                 "[expiration %u, creation time %u.%u, now %u.%u]",
                 bundle->bundleid_, bundle->expiration_,
                 (u_int)bundle->creation_ts_.tv_sec,
                 (u_int)bundle->creation_ts_.tv_usec,
                 (u_int)now.tv_sec, (u_int)now.tv_usec);
    }

    bundle->expiration_timer_ = new ExpirationTimer(bundle);
    bundle->expiration_timer_->schedule_at(&expiration_time);
}

//----------------------------------------------------------------------
void
BundleDaemon::delete_from_pending(Bundle* bundle,
                                  status_report_reason_t reason)
{
    log_debug("removing bundle *%p from pending list", bundle);

    // first try to cancel the expiration timer if it's still
    // around
    if (bundle->expiration_timer_) {
        log_debug("cancelling expiration timer for bundle id %d",
                  bundle->bundleid_);
        
        bool cancelled = bundle->expiration_timer_->cancel();
        if (cancelled) {
            bundle->expiration_timer_->bundleref_.release();
            bundle->expiration_timer_ = NULL;
        } else {
            // if the timer is no longer in the pending queue (i.e. it
            // can't be cancelled), then we just hit a race where the
            // timer is about to fire, which is ok since
            // handle_bundle_expired takes care of this correctly
            
            // XXX/demmer this would be fixed if the daemon thread
            // handled timers
            bundle->expiration_timer_ = NULL;
        }
    }

    if (pending_bundles_->erase(bundle)) {
        if (bundle->deletion_rcpt_ &&
            (reason != BundleProtocol::REASON_NO_ADDTL_INFO))
        {
            generate_status_report(bundle,
                                   BundleProtocol::STATUS_DELETED,
                                   reason);
        }
    } else {
        log_err("unexpected error removing bundle from pending list");
    }
}

//----------------------------------------------------------------------
void
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
     *    bundle will be on the custody_bundles list
     * 3) The bundle isn't currently in flight, as recorded
     *    in the forwarding log.
     *
     * This allows a router (or the custody system) to maintain a
     * retention constraint by putting the bundle on a list, and
     * thereby adding a mapping.
     */

    if (! bundle->is_queued_on(pending_bundles_)) {
        log_err("try_delete_from_pending(*%p): bundle not in pending list!",
                bundle);
        return;
    }

    if (!params_.early_deletion_) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "early deletion disabled",
                  bundle);
        return;
    }

    size_t num_mappings = bundle->num_mappings();
    if (num_mappings != 1) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "bundle has %u mappings",
                  bundle, (u_int)num_mappings);
        return;
    }
    
    size_t num_in_flight = bundle->fwdlog_.get_count(ForwardingInfo::IN_FLIGHT);
    if (num_in_flight > 0) {
        log_debug("try_delete_from_pending(*%p): not deleting because "
                  "bundle in flight on %u links",
                  bundle, (u_int)num_in_flight);
        return;
    }

    delete_from_pending(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
}

//----------------------------------------------------------------------
void
BundleDaemon::handle_bundle_free(BundleFreeEvent* event)
{
    Bundle* bundle = event->bundle_;
    event->bundle_ = NULL;
    ASSERT(bundle->refcount() == 0);
    
    bundle->lock_.lock("BundleDaemon::handle_bundle_free");
    
    actions_->store_del(bundle);
    
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
}

//----------------------------------------------------------------------
void
BundleDaemon::run()
{
    BundleEvent* event;

    while (1) {
        if (should_stop()) {
            break;
        }
        
        // grab an event off the queue, blocking until we get one
        event = eventq_->pop_blocking();
        ASSERT(event);

        // handle the event
        handle_event(event);
        
        // clean up the event
        delete event;
    }
}


} // namespace dtn
