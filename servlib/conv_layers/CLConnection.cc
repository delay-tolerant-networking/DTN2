/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/util/OptParser.h>

#include "CLConnection.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundlePayload.h"

namespace dtn {

//----------------------------------------------------------------------
CLConnection::CLConnection(const char* classname,
                           const char* logpath,
                           ConvergenceLayer* cl,
                           LinkParams* params)
    : Thread(classname),
      Logger(classname, logpath),
      contact_(classname),
      cmdqueue_(logpath),
      cl_(cl),
      params_(params),
      num_pollfds_(0),
      poll_timeout_(-1),
      contact_broken_(false)
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

    // the contact_ field is set by open_contact when we're initiating
    // a new connection. otherwise, we're being started in response to
    // an underlying CL connection and so we need to accept it
    if (contact_ != NULL) {
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

        // check if we need to unblock the link by clearing the BUSY state
        if (contact_ != NULL &&
            contact_->link()->state() == Link::BUSY &&
            cmdqueue_.size() < params_->busy_queue_depth_)
        {
            BundleDaemon::post(
                new LinkStateChangeRequest(contact_->link(),
                                           Link::AVAILABLE,
                                           ContactEvent::UNBLOCKED));
        }
        
        // send any data there is to send
        send_pending_data();

        // now we poll() to wait for a new command (indicated by the
        // notifier on the command queue), data arriving from the
        // remote side, or write-readiness on the socket indicating
        // that we can send more data.
        for (int i = 0; i < num_pollfds_ + 1; ++i) {
            pollfds_[i].revents = 0;
        }
        int cc = oasys::IO::poll_multiple(pollfds_, num_pollfds_ + 1,
                                          poll_timeout_, NULL, logpath_);

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
CLConnection::contact_up()
{
    log_debug("contact_up");
    ASSERT(contact_ != NULL);
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

        size_t sent_bytes  = inflight->sent_data_.num_contiguous();
        size_t acked_bytes = inflight->ack_data_.num_contiguous();
        
        if ((! params->reactive_frag_enabled_) ||
            (sent_bytes == 0) ||
            (contact_->link()->is_reliable() && acked_bytes == 0))
        {
            BundleDaemon::post(
                new BundleTransmitFailedEvent(inflight->bundle_.object(),
                                              contact_));
            
        } else {
            // XXX/demmer the reactive fragmentation code needs to be
            // fixed to include the header bytes
            sent_bytes  -= inflight->header_block_length_;
            if (acked_bytes > inflight->header_block_length_) {
                acked_bytes -= inflight->header_block_length_;
            } else {
                acked_bytes = 0;
            }
            
            BundleDaemon::post(
                new BundleTransmittedEvent(inflight->bundle_.object(),
                                           contact_,
                                           sent_bytes, acked_bytes));
        }

        inflight_.pop_front();
    }

    // ditto for the incoming queue, posting received events for all
    // in-progress bundles (if reactive fragmentation is enabled)
    while (! incoming_.empty()) {
        IncomingBundle* incoming = incoming_.front();
        
        if (params->reactive_frag_enabled_ &&
            (incoming->total_rcvd_length_ > incoming->header_block_length_))
        {
            size_t bytes_rcvd = incoming->total_rcvd_length_ -
                                incoming->header_block_length_;
            
            log_debug("partial arrival of bundle (got %zu/%zu payload bytes)",
                      bytes_rcvd, incoming->bundle_->payload_.length());

            // XXX/demmer need to fix the fragmentation code to assume the
            // event includes the header bytes as well as the payload.
            
            BundleDaemon::post(
                new BundleReceivedEvent(incoming->bundle_.object(),
                                        EVENTSRC_PEER,
                                        bytes_rcvd));
        }
        
        incoming_.pop_front();
        delete incoming;
    }
        
    // now drain the message queue, posting transmit failed events for
    // any send bundle commands that may be in there (though this is
    // unlikely to happen)
    if (cmdqueue_.size() > 0) {
        log_warn("close_contact: %zu CL commands still in queue: ", cmdqueue_.size());

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

} // namespace dtn
