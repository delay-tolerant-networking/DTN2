/*
 *    Copyright 2006 Intel Corporation
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


#include <oasys/util/OptParser.h>

#include "CLConnection.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundlePayload.h"
#include "contacts/ContactManager.h"

namespace dtn {

//----------------------------------------------------------------------
CLConnection::CLConnection(const char*       classname,
                           const char*       logpath,
                           ConvergenceLayer* cl,
                           LinkParams*       params,
                           bool              active_connector)
    : Thread(classname),
      Logger(classname, logpath),
      contact_(classname),
      contact_up_(false),
      cmdqueue_(logpath),
      cl_(cl),
      params_(params),
      active_connector_(active_connector),
      num_pollfds_(0),
      poll_timeout_(-1),
      contact_broken_(false),
      num_pending_(0)
{
    sendbuf_.reserve(params_->sendbuf_len_);
    recvbuf_.reserve(params_->recvbuf_len_);
}

//----------------------------------------------------------------------
CLConnection::~CLConnection()
{
}

//----------------------------------------------------------------------
void
CLConnection::run()
{
    struct pollfd* cmdqueue_poll;

    initialize_pollfds();

    cmdqueue_poll         = &pollfds_[num_pollfds_];
    cmdqueue_poll->fd     = cmdqueue_.read_fd();
    cmdqueue_poll->events = POLLIN;

    // based on the parameter passed to the constructor, we either
    // initiate a connection or accept one, then move on to the main
    // run() loop. it is the responsibility of the underlying CL to
    // make sure that a contact_ structure is found / created
    if (active_connector_) {
        connect();
    } else {
        accept();
    }

    while (true) {
        if (contact_broken_) {
            log_debug("contact_broken set, exiting main loop");
            return;
        }

        // check the comand queue coming in from the bundle daemon
        // if any arrive, we continue to the top of the loop to check
        // contact_broken and then process any other commands before
        // checking for data to/from the remote side
        if (cmdqueue_.size() != 0) {
            process_command();
            continue;
        }

        // send any data there is to send, and if something was sent
        // out, we'll call poll() with a zero timeout so we can read
        // any data there is to consume, then return to send another
        // chunk.
        bool more_to_send = send_pending_data();

        // check again here for contact broken since we don't want to
        // poll if the socket's been closed
        if (contact_broken_) {
            log_debug("contact_broken set, exiting main loop");
            return;
        }
        
        // now we poll() to wait for a new command (indicated by the
        // notifier on the command queue), data arriving from the
        // remote side, or write-readiness on the socket indicating
        // that we can send more data.
        for (int i = 0; i < num_pollfds_ + 1; ++i) {
            pollfds_[i].revents = 0;
        }

        int timeout = more_to_send ? 0 : poll_timeout_;

        log_debug("calling poll on %d fds with timeout %d",
                  num_pollfds_ + 1, timeout);
                                                 
        int cc = oasys::IO::poll_multiple(pollfds_, num_pollfds_ + 1,
                                          timeout, NULL, logpath_);

        if (cc == oasys::IOTIMEOUT)
        {
            handle_poll_timeout();
        }
        else if (cc > 0)
        {
            if (cc == 1 && cmdqueue_poll->revents != 0) {
                continue; // activity on the command queue only
            }
            handle_poll_activity();
        }
        else
        {
            log_err("unexpected return from poll_multiple: %d", cc);
            break_contact(ContactEvent::BROKEN);
            return;
        }
    }
}

//----------------------------------------------------------------------
void
CLConnection::queue_bundle(Bundle* bundle)
{
    /*
     * Called by the main BundleDaemon thread... push a new CLMsg onto
     * the queue and potentially set the BUSY state on the link. Note
     * that it's important to update num_pending before pushing the
     * message onto the queue since the latter might trigger a context
     * switch.
     */
    LinkParams* params = dynamic_cast<LinkParams*>(contact_->link()->cl_info());
    ASSERT(params != NULL);
    
    oasys::atomic_incr(&num_pending_);
    
    if (num_pending_.value >= params->busy_queue_depth_)
    {
        log_debug("%d bundles pending, setting BUSY state",
                  num_pending_.value);
        contact_->link()->set_state(Link::BUSY);
    }
    else
    {
        log_debug("%d bundles pending -- leaving state as-is",
                  num_pending_.value);
    }

    cmdqueue_.push_back(
        CLConnection::CLMsg(CLConnection::CLMSG_SEND_BUNDLE, bundle));
}

//----------------------------------------------------------------------
void
CLConnection::process_command()
{
    CLMsg msg;
    bool ok = cmdqueue_.try_pop(&msg);
    ASSERT(ok); // shouldn't be called if the queue is empty
    
    switch(msg.type_) {
    case CLMSG_SEND_BUNDLE:
        log_debug("processing CLMSG_SEND_BUNDLE");
        handle_send_bundle(msg.bundle_.object());
        break;
        
    case CLMSG_CANCEL_BUNDLE:
        log_debug("processing CLMSG_CANCEL_BUNDLE");
        handle_cancel_bundle(msg.bundle_.object());
        break;
        
    case CLMSG_BREAK_CONTACT:
        log_debug("processing CLMSG_BREAK_CONTACT");
        break_contact(ContactEvent::USER);
        break;
    default:
        PANIC("invalid CLMsg typecode %d", msg.type_);
    }
}

//----------------------------------------------------------------------
void
CLConnection::check_unblock_link()
{
    /*
     * Check if we need to unblock the link by clearing the BUSY
     * state. Note that we only do this when the link first goes back
     * below the threshold since otherwise we'll flood the router with
     * incorrect BUSY->AVAILABLE events.
     */
    LinkParams* params = dynamic_cast<LinkParams*>(contact_->link()->cl_info());
    ASSERT(params != NULL);

    oasys::atomic_decr(&num_pending_);
    ASSERT((int)num_pending_.value >= 0);

    if (contact_->link()->state() == Link::BUSY)
    {
        if (num_pending_.value == (params->busy_queue_depth_ - 1))
        {
            log_debug("%d bundles pending, clearing BUSY state", num_pending_.value);

            // XXX/demmer post the AVAILABLE event at the head of the
            // event queue, since we want it to be processed quickly
            // in case there's a backlog of events. this whole issue
            // of backpressure needs to be worked through in a much
            // better way.
            BundleDaemon::post_at_head(
                new LinkStateChangeRequest(contact_->link(),
                                           Link::AVAILABLE,
                                           ContactEvent::UNBLOCKED));
        }
        else
        {
            log_debug("%d bundles pending, leaving state as-is",
                      num_pending_.value);
        }
    }
}

//----------------------------------------------------------------------
void
CLConnection::contact_up()
{
    log_debug("contact_up");
    ASSERT(contact_ != NULL);
    
    ASSERT(!contact_up_);
    contact_up_ = true;
    
    BundleDaemon::post(new ContactUpEvent(contact_));
}

//----------------------------------------------------------------------
void
CLConnection::break_contact(ContactEvent::reason_t reason)
{
    log_debug("break_contact: %s", ContactEvent::reason_to_str(reason));

    if (reason != ContactEvent::BROKEN) {
        disconnect();
    }

    contact_broken_ = true;
    
    // if the connection isn't being closed by the user, we need to
    // notify the daemon that either the contact ended or the link
    // became unavailable before a contact began.
    //
    // we need to check that there is in fact a contact, since a
    // connection may be accepted and then break before establishing a
    // contact
    if ((reason != ContactEvent::USER) && (contact_ != NULL)) {
        BundleDaemon::post(
            new LinkStateChangeRequest(contact_->link(),
                                       Link::CLOSED,
                                       reason));
    }
}

//----------------------------------------------------------------------
void
CLConnection::close_contact()
{
    LinkParams* params = dynamic_cast<LinkParams*>(contact_->link()->cl_info());
    ASSERT(params != NULL);
    
    // drain the inflight queue, posting transmitted or transmit
    // failed events
    while (! inflight_.empty()) {
        InFlightBundle* inflight = inflight_.front();

        // make sure the payload file is closed
        ASSERT(inflight->bundle_.object() != NULL);
        inflight->bundle_->payload_.close_file();

        u_int32_t sent_bytes  = inflight->sent_data_.num_contiguous();
        u_int32_t acked_bytes = inflight->ack_data_.num_contiguous();
        
        if ((! params->reactive_frag_enabled_) ||
            (sent_bytes == 0) ||
            (contact_->link()->is_reliable() && acked_bytes == 0))
        {
            log_debug("posting transmission failed event "
                      "(reactive fragmentation %s, %s link, acked_bytes %u)",
                      params->reactive_frag_enabled_ ? "enabled" : "disabled",
                      contact_->link()->is_reliable() ? "reliable" : "unreliable",
                      acked_bytes);
            
            BundleDaemon::post(
                new BundleTransmitFailedEvent(inflight->bundle_.object(),
                                              contact_));
            
        } else {
            BundleDaemon::post(
                new BundleTransmittedEvent(inflight->bundle_.object(),
                                           contact_, sent_bytes, acked_bytes));
        }

        inflight_.pop_front();
    }

    // check the tail of the incoming queue to see if there's a
    // partially-received bundle that we need to post a received event
    // for (if reactive fragmentation is enabled)
    if (! incoming_.empty()) {
        IncomingBundle* incoming = incoming_.back();
        if(!incoming->rcvd_data_.empty())
        {  
            size_t rcvd_len = incoming->rcvd_data_.last() + 1;

            size_t header_block_length =
                BundleProtocol::payload_offset(&incoming->bundle_->recv_blocks_);
        
            if ((incoming->total_length_ == 0) && 
                params->reactive_frag_enabled_ &&
                (rcvd_len > header_block_length))
            {
                log_debug("partial arrival of bundle: "
                          "got %zu bytes [hdr %zu payload %zu]",
                          rcvd_len, header_block_length,
                          incoming->bundle_->payload_.length());
             
                // make sure the payload file is closed
                ASSERT(incoming->bundle_.object() != NULL);
                incoming->bundle_->payload_.close_file();
            
                BundleDaemon::post(
                    new BundleReceivedEvent(incoming->bundle_.object(),
                                            EVENTSRC_PEER, rcvd_len,
                                            contact_.object()));
            }
        }
    }

    // now drain the incoming queue
    while (!incoming_.empty()) {
        IncomingBundle* incoming = incoming_.back();
        incoming_.pop_back();
        delete incoming;
    }
    
    // finally, drain the message queue, posting transmit failed
    // events for any send bundle commands that may be in there
    // (though this is unlikely to happen)
    if (cmdqueue_.size() > 0) {
        log_warn("close_contact: %zu CL commands still in queue: ",
                 cmdqueue_.size());
        
        while (cmdqueue_.size() != 0) {
            CLMsg msg;
            bool ok = cmdqueue_.try_pop(&msg);
            ASSERT(ok);

            log_warn("close_contact: %s still in queue", clmsg_to_str(msg.type_));
            
            if (msg.type_ == CLMSG_SEND_BUNDLE) {
                BundleDaemon::post(
                    new BundleTransmitFailedEvent(msg.bundle_.object(),
                                                  contact_));
            }
        }
    }
}

//----------------------------------------------------------------------
void
CLConnection::find_contact(const EndpointID& peer_eid)
{
    /*
     * Now we may need to find or create an appropriate opportunistic
     * link for the connection.
     *
     * First, we check if there's an idle (i.e. UNAVAILABLE) link to
     * the remote eid. We explicitly ignore the nexthop address, since
     * that can change (due to things like TCP/UDP port number
     * assignment), but we pass in the remote eid to match for a link.
     *
     * If we can't find one, then we create a new opportunistic link
     * for the connection.
     */
    if (contact_ == NULL) {

        ASSERT(nexthop_ != ""); // the derived class must have set the
                                // nexthop in the constructor

        ContactManager* cm = BundleDaemon::instance()->contactmgr();
        oasys::ScopeLock l(cm->lock(), "CLConnection::find_contact");

        Link* link = cm->find_link_to(cl_, "", peer_eid,
                                      Link::OPPORTUNISTIC,
                                      Link::AVAILABLE | Link::UNAVAILABLE);

        // XXX/demmer remove check for no contact
        if (link != NULL && (link->contact() == NULL)) {
            link->set_nexthop(nexthop_);
            log_debug("found idle opportunistic link *%p", link);
            
        } else {
            if (link != NULL) {
                log_warn("in-use opportunistic link *%p returned from "
                         "ContactManager::find_link_to", link);
            }
            
            link = cm->new_opportunistic_link(cl_,
                                              nexthop_.c_str(),
                                              peer_eid);
            log_debug("created new opportunistic link *%p", link);
        }
        
        ASSERT(! link->isopen());

        contact_ = new Contact(link);
        contact_->set_cl_info(this);
        link->set_contact(contact_.object());

        /*
         * Now that the connection is established, we swing the
         * params_ pointer to those of the link, since there's a
         * chance they've been modified by the user in the past.
         */
        LinkParams* lparams = dynamic_cast<LinkParams*>(link->cl_info());
        ASSERT(lparams != NULL);
        params_ = lparams;
    }
}


} // namespace dtn
