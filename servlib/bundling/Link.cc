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
Link::create_link(std::string name, link_type_t type,
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
Link::Link(std::string name, link_type_t type,
           ConvergenceLayer* cl, const char* nexthop)
    :  BundleConsumer(nexthop, false, LINK),
       type_(type), nexthop_(nexthop), name_(name), avail_(false),
       closing_(false), clayer_(cl)
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

    // By default link does not have an associated contact or any cl
    // info, but all links get a bundle list
    contact_ = NULL ;
    cl_info_ = NULL;
    peer_ = NULL;
    bundle_list_ = new BundleList(logpath_);

    log_info("new link *%p", this);
}

Link::~Link()
{
    ASSERT(!isopen());
    
    if (bundle_list_ != NULL) {
        ASSERT(bundle_list_->size() == 0);
        delete bundle_list_;
    }

    if (cl_info_ != NULL) {
        delete cl_info_;
        cl_info_ = NULL;
    }
}

/**
 * Open a channel to the link for bundle transmission.
 * Relevant if Link is ONDEMAND or OPPORTUNISTIC
 */
void
Link::open()
{
    log_debug("Link::open");
    
    ASSERT(isavailable());
    ASSERT(!isopen());
    ASSERT(!closing_);
    
    if (!isopen()) {
        contact_ = new Contact(this);

        if (type_ == ONDEMAND) {
            clayer()->open_contact(contact_);

        } else if (type_ == OPPORTUNISTIC) {
            // the CL will take care of adding the contact
            
        } else {
            PANIC("Link::open not implemented for %s links",
                  link_type_to_str(type_));
        }
        
    } else {
        log_warn("Trying to open an already open link %s",name());
    }
}
    
/**
 * Close the currently active contact on the link.
 * This is typically called in the following scenario's
 *
 * By BundleRouter:  when it receives contact_down event
 *                :  when it thinks it has sent all messages for
 *                   an on demand link and it no longer needs it
 *                :  when link becomes unavailable because link is
 *                   scheduled or because link was opportunistic
 *
 * In general link close() is called as a reaction to the fact that
 * link is no more available.
 *
 */
void
Link::close()
{
    log_debug("Link::close");
    
    // Ensure that link is open
    ASSERT(isopen());
    ASSERT(contact_ != NULL); // same thing

    // Assert contact bundle list is empty
    ASSERT(contact_->bundle_list()->size() == 0);

    // Set the closing bit on the link. This is needed to inform the
    // convergence layer that it shouldn't post a ContactDownEvent
    // that would reference the about-to-be-deleted contact.
    ASSERT(!closing_);
    closing_ = true;
    
    // Close the contact
    clayer()->close_contact(contact_);

    // Make sure the convergence layer cleaned up its state
    ASSERT(contact_->cl_info() == NULL);

    // Clean it up
    delete contact_;
    
    // Actually nullify the contact.
    // This is important because link == Open iff contact_ == NULL
    contact_ = NULL;

    // Clear out the closing bit
    closing_ = false;

    // Unless this is an ondemand link, once it's closed, it's no
    // longer available.
    if (type_ != ONDEMAND)
        set_link_unavailable();

    log_debug("Link::close complete");
}

/**
 * Formatting
 */
int
Link::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "%s: %s",name(),link_type_to_str(type_));
}

/**
 * Overrided default queuing behavior to handle ONDEMAND links
 * For other types of links invoke default behavior
 */
void
Link::enqueue_bundle(Bundle* bundle, const BundleMapping* mapping)
{
   /*
     * If the link is open, messages are always queued on the contact
     * queue, if not, put them on the link queue.
     */
    if (isopen()) {
        log_debug("Link %s is open, so queueing it on contact queue",name());
        contact_->enqueue_bundle(bundle, mapping);
        
    } else {
        log_debug("Link %s is closed, so queueing it on link queue",name());
        BundleConsumer::enqueue_bundle(bundle, mapping);
    }
}

/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
Link::is_queued(Bundle* bundle)
{
    if (isopen()) {
        return contact_->is_queued(bundle);
    } else {
        return BundleConsumer::is_queued(bundle);
    }
}


/**
 * Set the state of the link to be available
 */
void
Link::set_link_available()
{
    ASSERT(!isavailable());
    avail_ = true ;
    // Post a link available event
    BundleDaemon::post(new LinkAvailableEvent(this));
}

/**
 * Set the state of the link to be unavailable
 */
void
Link::set_link_unavailable()
{
    ASSERT(isavailable());
    avail_ = false;
    // Post a link unavailable event
    BundleDaemon::post(new LinkUnavailableEvent(this));
}

/**
 * Finds, how many messages are queued to go through
 * this link (potentially)
 */
size_t
Link::size()
{
    size_t retval =0;
    retval += bundle_list_->size();

    if ((type_ == ONDEMAND) && isopen()) {
        /*
         * If link is open, there should not be queued messages
         * on link queue. 
         * There may be some race conditions and ASSERT may be
         * too strong a requirement.
         */
        ASSERT(retval == 0);
        retval += contact_->bundle_list()->size();
    }

    /*
     * Peer queue may have some messages too
     * Assume, router will move from peer queue to link queue when
     * it receives a link available message
    */
    // retval += peer()->bundle_list()->size();

    // TODO, for scheduled links check on queues of future contacts
    return retval;
}

} // namespace dtn
