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
#include "Link.h"
#include "conv_layers/ConvergenceLayer.h"
#include "ContactManager.h"
#include "OndemandLink.h"
#include "ScheduledLink.h"
#include "OpportunisticLink.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"

namespace dtn {

/**
 * Static constructor to create different type of links
 */
Link*
Link::create_link(const std::string& name, link_type_t type,
                  ConvergenceLayer* cl, const char* nexthop,
                  int argc, const char* argv[])
{
    Link* link;
    switch(type) {
    case ONDEMAND: 	link = new OndemandLink(name, cl, nexthop); break;
    case SCHEDULED: 	link = new ScheduledLink(name, cl, nexthop); break;
    case OPPORTUNISTIC: link = new OpportunisticLink(name, cl, nexthop); break;
    default: 		PANIC("bogus link_type_t");
    }

    // Notify the convergence layer, which parses options
    // XXX/demmer how to deal with options for scheduled links?
    ASSERT(link->clayer_);
    if (!link->clayer_->init_link(link, argc, argv)) {
        delete link;
        return NULL;
    }

    return link;
}

/**
 * Constructor
 */
Link::Link(const std::string& name, link_type_t type,
           ConvergenceLayer* cl, const char* nexthop)
    :  BundleConsumer(nexthop, false, LINK),
       type_(type), state_(UNAVAILABLE),
       nexthop_(nexthop), name_(name), contact_(NULL),
       clayer_(cl), cl_info_(NULL)
{
    ASSERT(clayer_);
    logpathf("/link/%s", name_.c_str());

    // pretty up the BundleConsumer type str
    switch(type) {
    case ONDEMAND: 	type_str_ = "Ondemand Link"; break;
    case SCHEDULED: 	type_str_ = "Scheduled Link"; break;
    case OPPORTUNISTIC: type_str_ = "Opportunistic Link"; break;
    default: 		PANIC("bogus link_type_t");
    }

    log_info("new link *%p", this);
}

Link::~Link()
{
    /*
     * Once they're created, links are never actually deleted.
     * However, if there's a misconfiguration, then init_link may
     * delete the link, so we don't want to PANIC here.
     *
     * Note also that the destructor of the class is protected so
     * we're (relatively) sure this constraint does hold.
     */
    ASSERT(!isopen());
    
    if (cl_info_ != NULL) {
        delete cl_info_;
        cl_info_ = NULL;
    }
}

/**
 * Sets the state of the link. Performs various assertions to
 * ensure the state transitions are legal.
 */
void
Link::set_state(state_t new_state)
{
    log_debug("set_state %s -> %s",
              state_to_str(state_), state_to_str(new_state));

    switch(new_state) {
    case UNAVAILABLE:
        break; // any old state is valid

    case AVAILABLE:
        ASSERT(state_ == CLOSING || state_ == UNAVAILABLE);
        break;

    case OPENING:
        ASSERT(state_ == AVAILABLE);
        break;
        
    case OPEN:
        ASSERT(state_ == OPENING || state_ == BUSY);
        break;

    case BUSY:
        ASSERT(state_ == OPEN);
        break;
    
    case CLOSING:
        ASSERT(state_ == OPENING || state_ == OPEN || state_ == BUSY);
        break;

    default:
        NOTREACHED;
    }

    state_ = new_state;
}

/**
 * Open the link.
 */
void
Link::open()
{
    log_debug("Link::open");

    if (state_ != AVAILABLE) {
        log_crit("Link::open in state %s: expected state AVAILABLE",
                 state_to_str(state_));
        return;
    }

    set_state(OPENING);

    // create a new contact and kick the convergence layer. once it
    // has established a session however it needs to, it will set the
    // Link state to OPEN and post a ContactUpEvent
    ASSERT(contact_ == NULL);
    contact_ = new Contact(this);
    clayer()->open_contact(contact_); 
}
    
/**
 * Close the link.
 */
void
Link::close()
{
    log_debug("Link::close");

    // This should only be called when the state has been set to
    // CLOSING by the daemon's handler for the link state change
    // request
    if (state_ != CLOSING) {
        log_err("Link::close in state %s: expected state CLOSING",
                state_to_str(state_));
        return;
    }

    // Kick the convergence layer to close the contact and make sure
    // it cleaned up its state
    clayer()->close_contact(contact_);
    ASSERT(contact_->cl_info() == NULL);
    
    // Clean it up
    delete contact_;
    contact_ = NULL;

    log_debug("Link::close complete");
}

/**
 * Formatting
 */
int
Link::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "%s %s -> %s (%s)",
                    type_str_, name(), nexthop(),
                    state_to_str(state_));
}


void
Link::consume_bundle(Bundle* bundle)
{
    // BundleActions should make sure that we're only called on to
    // send a bundle if we can
    if (state_ != OPEN) {
        PANIC("Link::consume_bundle in state %s", state_to_str(state_));
    }

    clayer()->send_bundle(contact_, bundle);
}

/**
 * Attempt to remove the given bundle from the queue.
 *
 * @return true if the bundle was dequeued, false if not.
 */
bool
Link::cancel_bundle(Bundle* bundle)
{
    return clayer()->cancel_bundle(contact_, bundle);
}
    
/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
Link::is_queued(Bundle* bundle)
{
    return clayer()->is_queued(contact_, bundle);
}

} // namespace dtn
