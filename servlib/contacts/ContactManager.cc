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

#include <oasys/util/StringBuffer.h>

#include "ContactManager.h"
#include "Contact.h"
#include "Link.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "conv_layers/ConvergenceLayer.h"

namespace dtn {

//----------------------------------------------------------------------
ContactManager::ContactManager()
    : BundleEventHandler("ContactManager", "/dtn/contact/manager"),
      opportunistic_cnt_(0)
{
    links_ = new LinkSet();
}

//----------------------------------------------------------------------
ContactManager::~ContactManager()
{
}

//----------------------------------------------------------------------
void
ContactManager::add_link(Link* link)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    log_debug("adding link %s", link->name());
    links_->insert(link);
    
    BundleDaemon::post(new LinkCreatedEvent(link));
}

//----------------------------------------------------------------------
void
ContactManager::del_link(Link *link)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    log_debug("deleting link %s", link->name());
    if (!has_link(link)) {
        log_err("Error in del_link: link %s does not exist \n",
                link->name());
        return;
    }

    links_->erase(link);
    BundleDaemon::post(new LinkDeletedEvent(link));
}

//----------------------------------------------------------------------
bool
ContactManager::has_link(Link *link)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

//----------------------------------------------------------------------
Link*
ContactManager::find_link(const char* name)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        if (strcasecmp(link->name(), name) == 0)
            return link;
    }
    return NULL;
}

//----------------------------------------------------------------------
Link*
ContactManager::find_link_to(const char* next_hop, const char* clayer)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        if ((strcasecmp(link->nexthop(), next_hop) == 0) &&
            (clayer == NULL ||
             (strcasecmp(link->clayer()->name(), clayer) == 0)))
        {
            return link;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
const LinkSet*
ContactManager::links()
{
    ASSERTF(lock_.is_locked_by_me(),
            "ContactManager::links must be called while holding lock");
    return links_;
}

//----------------------------------------------------------------------
void
ContactManager::LinkAvailabilityTimer::timeout(const struct timeval& now)
{
    (void)now;
    cm_->reopen_link(link_);
    delete this;
}

//----------------------------------------------------------------------
void
ContactManager::reopen_link(Link* link)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    log_debug("reopen link %s", link->name());
    
    if (link->state() == Link::UNAVAILABLE) {
        BundleDaemon::post_at_head(
            new LinkStateChangeRequest(link, Link::OPEN,
                                       ContactEvent::RECONNECT));
    } else {
        // state race (possibly due to user action)
        log_err("availability timer fired for link %s but state is %s",
                link->name(), Link::state_to_str(link->state()));
    }

    availability_timers_.erase(link);
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_available(LinkAvailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    AvailabilityTimerMap::iterator iter;
    iter = availability_timers_.find(event->link_);
    if (iter == availability_timers_.end()) {
        return; // no timer for this link
    }
    
    LinkAvailabilityTimer* timer = iter->second;
    availability_timers_.erase(event->link_);
    
    // try to cancel the timer and rely on the timer system to clean
    // it up once it bubbles to the top of the queue... if there's a
    // race and the timer is in the process of firing, it should clean
    // itself up in the timeout handler.
    if (! timer->cancel()) {
        log_warn("can't cancel availability timer: race condition ");
    }
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_unavailable(LinkUnavailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    // don't do anything for links that aren't ondemand or alwayson
    if (event->link_->type() != Link::ONDEMAND &&
        event->link_->type() != Link::ALWAYSON)
    {
        log_debug("ignoring link unavailable for link of type %s",
                  event->link_->type_str());
        return;
    }
    
    // or if the link wasn't broken but instead was closed by user
    // action or by going idle
    if (event->reason_ != ContactEvent::BROKEN) {
        log_debug("ignoring link unavailable for unavailable due to %s",
                  event->reason_to_str(event->reason_));
        return;
    }
    
    // adjust the retry interval in the link to handle backoff in case
    // it continuously fails to open, then schedule the timer.
    // the retry interval is reset in the link open event handler
    Link* link = event->link_;
    int timeout = link->retry_interval_;
    
    link->retry_interval_ *= 2;
    if (link->retry_interval_ > link->params().max_retry_interval_) {
        link->retry_interval_ = link->params().max_retry_interval_;
    }

    LinkAvailabilityTimer* timer = new LinkAvailabilityTimer(this, link);

    AvailabilityTimerMap::value_type val(link, timer);
    if (availability_timers_.insert(val).second == false) {
        log_err("error inserting timer for link %s into table!",
                link->name());
        delete timer;
        return;
    }

    log_debug("scheduling availability timer in %d seconds for link %s",
              timeout, link->name());
    timer->schedule_in(timeout * 1000);
}


//----------------------------------------------------------------------
void
ContactManager::handle_contact_up(ContactUpEvent* event)
{
    Link* link = event->contact_->link();
    if (link->type() == Link::ONDEMAND ||
        link->type() == Link::ALWAYSON)
    {
        log_debug("resetting retry interval for link %s: %d -> %d",
                  link->name(),
                  link->retry_interval_,
                  link->params().min_retry_interval_);
        link->retry_interval_ = link->params().min_retry_interval_;
    }
}

//----------------------------------------------------------------------
Link*
ContactManager::get_opportunistic_link(ConvergenceLayer* cl,
				       CLInfo* clinfo,
				       const char* nexthop,
				       EndpointID* remote_eid)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    LinkSet::iterator iter;
    Link* link = NULL;

    // first look through the list of links for an idle opportunistic
    // link to this next hop
    log_debug("looking for OPPORTUNISTIC link to %s", nexthop);
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        // find_link is case independent, better make this one the same, no?
        if ( (strcasecmp(link->nexthop(), nexthop) == 0) &&   
             (link->type() == Link::OPPORTUNISTIC)   &&
             (link->clayer() == cl) ) {
            if(! link->isopen()) {
                log_debug("found match: link %s", link->name());
                return link;
            }
            else {
                log_debug("found match: link %s, but it's already open! "
                          "Returning it anyway.", link->name());
                return link;
            }
        }
    }

    log_debug("no match, creating new link to %s", nexthop);

    // find a unique link name
    char name[64];
    do {
        snprintf(name, sizeof(name), "opportunistic-%d",
                 opportunistic_cnt_);
        opportunistic_cnt_++;
        link = find_link(name);
    } while (link != NULL);
        
    link = Link::create_link(name, Link::OPPORTUNISTIC, cl, nexthop, 0, NULL);
    if ( remote_eid != NULL )
        link->set_remote_eid (*remote_eid);
        
    if (!link) {
        log_crit("unexpected error creating opportunistic link!!");
        return NULL;
    }

    link->set_cl_info(clinfo);

    // add the link to the set and post a link created event
    add_link(link);

    return link;
}

//----------------------------------------------------------------------
Link*
ContactManager::new_opportunistic_link(ConvergenceLayer* cl,
                                       CLInfo* clinfo,
                                       const char* nexthop,
                                       EndpointID* remote_eid)
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    Link* link = get_opportunistic_link(cl, clinfo, nexthop, remote_eid);
    if (!link) // no link means it couldn't be created
        return NULL;

    if (link->isopen()) {
        log_debug("Contact to %s already established, ignoring...", nexthop);
        return link;
    }

    if (remote_eid != NULL) {
        link->set_state(Link::AVAILABLE);
    }

    // notify the daemon that the link is available
    BundleDaemon::post(new LinkAvailableEvent(link, ContactEvent::NO_INFO));
    return link;
}
    
//----------------------------------------------------------------------
void
ContactManager::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, "ContactManager");
    
    buf->append("Links:\n");
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        buf->appendf("*%p\n", link);
    }
}

} // namespace dtn
