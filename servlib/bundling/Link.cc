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
#include "BundleForwarder.h"
#include "BundleEvent.h"

namespace dtn {

/**
 * Static constructor to create different type of links
 */
Link*
Link::create_link(std::string name, link_type_t type,
                             const char* conv_layer,
                        const BundleTuple& tuple)
{
    switch(type) {
    case ONDEMAND: 	return new OndemandLink(name,conv_layer,tuple);
    case SCHEDULED: 	return new ScheduledLink(name,conv_layer,tuple);
    case OPPORTUNISTIC: return new OpportunisticLink(name,conv_layer,tuple);
    default: 		PANIC("bogus link_type_t");
    }
}

/**
 * Constructor
 */
Link::Link(std::string name, link_type_t type, const char* conv_layer,
           const BundleTuple& tuple)
    :  BundleConsumer(&tuple_, false, "Link"),
       type_(type),   tuple_(tuple),  name_(name), avail_(false)
{

    logpathf("/link/%s",name_.c_str());

    // Find convergence layer
    clayer_ = ConvergenceLayer::find_clayer(conv_layer);
    if (!clayer_) {
        PANIC("can't find convergence layer for %s", tuple.admin().c_str());
        // XXX/demmer need better error handling
    }
    
    // Contact manager will create a peer if peer does not exist
    if (!tuple.valid()) {
        PANIC("malformed peer %s",tuple.c_str());
    }
    peer_ = ContactManager::instance()->find_peer(tuple);
    if (peer_ == NULL) {
        peer_ = new Peer(tuple);
        ContactManager::instance()->add_peer(peer_);
    }
    peer_->add_link(this);
     
    // By default link does not have an associated contact
    contact_ = NULL ;

    /*
     * Note that bundle list is not initialized, for ondemand links it
     * must be null
     * XXX, Sushant check this
     */
    bundle_list_ = NULL;
    link_info_ = NULL;
    
    log_info("new link *%p", this);

    // Add the link to contact manager
    ContactManager::instance()->add_link(this);

    bundle_list_ = new BundleList(logpath_);

    // Post a link created event
    BundleForwarder::post(new LinkCreatedEvent(this));

    // If link is ONDEMAND set it to be available
    if (type_ == ONDEMAND)
        set_link_available();
}
    

Link::~Link()
{
    if (bundle_list_ != NULL) {
        ASSERT(bundle_list_->size() == 0);
        delete bundle_list_;
    }
// Ensure that link delete event is posted somewhere
}

/**
 * Open a channel to the link for bundle transmission.
 * Relevant only if Link is ONDEMAND
 * Move this to subclasses ? XXX/Sushant
 *
 */
void
Link::open()
{
    ASSERT(isavailable());
    log_debug("Link::open");
    ASSERT(!isopen());
    if (!isopen()) {
        contact_ = new Contact(this);
    } else {
        log_warn("Trying to open an already open link %s",name());
    }
}
    
/**
 * Close the currently active contact on the link.
 * This is typically called in the following scenario's
 *
 * By BundleRouter:  when it receives contact_broken event
 *                :  when it thinks it has sent all messages for
 *                   an on demand link and it no longer needs it
 *                :  when link becomes unavailable because link is
 *                   scheduled or because link was opportunistic
 *
 * In general link close() is called as a reaction to the fact that
 * link is no more available
 *
 */
void
Link::close()
{
    // Ensure that link is open
    ASSERT(contact_ != NULL);

    // Assert contact bundle list is empty
    ASSERT(contact_->bundle_list()->size()==0);
    
    //Close the contact
    clayer()->close_contact(contact_);
    ASSERT(contact_->contact_info() == NULL) ; 

    // Actually nullify the contact.
    // This is important because link == Open iff contact_ == NULL
    contact_ = NULL;

    log_debug("Link::close");
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
        contact_->enqueue_bundle(bundle,mapping);
    } else {
        log_debug("Link %s is closed, so queueing it on link queue",name());
        BundleConsumer::enqueue_bundle(bundle,mapping);
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
    BundleForwarder::post(new LinkAvailableEvent(this));
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
    BundleForwarder::post(new LinkUnavailableEvent(this));
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
    if (isopen()) {
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
