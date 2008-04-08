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
    delete links_;
}

//----------------------------------------------------------------------
bool
ContactManager::add_new_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::add_new_link");

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    log_debug("adding NEW link %s", link->name());
    if (has_link(link->name())) {
        return false;
    }
    links_->insert(LinkRef(link.object(), "ContactManager"));

    if (!link->is_create_pending()) {
        log_debug("posting LinkCreatedEvent");
        BundleDaemon::post(new LinkCreatedEvent(link));
    }

    return true;
}

//----------------------------------------------------------------------
void
ContactManager::del_link(const LinkRef& link, bool wait,
                         ContactEvent::reason_t reason)
{
    oasys::ScopeLock l(&lock_, "ContactManager::del_link");
    ASSERT(link != NULL);

    if (!has_link(link)) {
        log_err("ContactManager::del_link: link %s does not exist",
                link->name());
        return;
    }
    ASSERT(!link->isdeleted());

    log_debug("ContactManager::del_link: deleting link %s", link->name());

    if (!wait)
        link->delete_link();

    // Close the link if it is open or in the process of being opened.
    if (link->isopen() || link->isopening()) {
        BundleDaemon::instance()->post(
            new LinkStateChangeRequest(link, Link::CLOSED, reason));
    }

    // Cancel the link's availability timer (if one exists).
    AvailabilityTimerMap::iterator iter = availability_timers_.find(link);
    if (iter != availability_timers_.end()) {
        LinkAvailabilityTimer* timer = iter->second;
        availability_timers_.erase(link);

        // Attempt to cancel the timer, relying on the timer system to clean
        // up the timer state once it bubbles to the top of the timer queue.
        // If the timer is in the process of firing (i.e., race condition),
        // the timer should clean itself up in the timeout handler.
        if (!timer->cancel()) {
            log_warn("ContactManager::del_link: "
                     "failed to cancel availability timer -- race condition");
        }
    }

    links_->erase(link);
    
    if (wait) {
        l.unlock();
        // If some parent calling del_link already locked the Contact Manager,
        // the lock will remain locked, and an event ahead of the
        // LinkDeletedEvent may wait for the lock, causing deadlock
        ASSERT(!lock()->is_locked_by_me());
        oasys::Notifier notifier("ContactManager::del_link");
        BundleDaemon::post_and_wait(new LinkDeletedEvent(link), &notifier);
        link->delete_link();
    } else {
        BundleDaemon::post(new LinkDeletedEvent(link));
    }
}

//----------------------------------------------------------------------
bool
ContactManager::has_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::has_link");
    ASSERT(link != NULL);
    
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

//----------------------------------------------------------------------
bool
ContactManager::has_link(const char* name)
{
    oasys::ScopeLock l(&lock_, "ContactManager::has_link");
    ASSERT(link != NULL);
    
    LinkSet::iterator iter;
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        if (strcasecmp((*iter)->name(), name) == 0)
            return true;
    }
    return false;
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_link(const char* name)
{
    oasys::ScopeLock l(&lock_, "ContactManager::find_link");
    
    LinkSet::iterator iter;
    LinkRef link("ContactManager::find_link: return value");
    
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        if (strcasecmp((*iter)->name(), name) == 0) {
            link = *iter;
            ASSERT(!link->isdeleted());
            return link;
        }
    }
    return link;
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
ContactManager::reopen_link(const LinkRef& link)
{
    oasys::ScopeLock l(&lock_, "ContactManager::reopen_link");
    ASSERT(link != NULL);

    log_debug("reopen link %s", link->name());

    availability_timers_.erase(link);

    if (!has_link(link)) {
        log_warn("ContactManager::reopen_link: "
                 "link %s does not exist", link->name());
        return;
    }
    ASSERT(!link->isdeleted());
    
    if (link->state() == Link::UNAVAILABLE) {
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::OPEN,
                                       ContactEvent::RECONNECT));
    } else {
        // state race (possibly due to user action)
        log_err("availability timer fired for link %s but state is %s",
                link->name(), Link::state_to_str(link->state()));
    }
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_created(LinkCreatedEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_created");

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_created: "
                "link %s is being deleted", link->name());
        return;
    }
        
    if (!has_link(link)) {
        log_err("ContactManager::handle_link_created: "
                "link %s does not exist", link->name());
        return;
    }

    // Post initial state events; MOVED from Link::create_link().
    link->set_initial_state();
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_available(LinkAvailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_available");

    LinkRef link = event->link_;
    ASSERT(link != NULL);
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_available: "
                "link %s is being deleted", link->name());
        return;
    }

    if (!has_link(link)) {
        log_warn("ContactManager::handle_link_available: "
                 "link %s does not exist", link->name());
        return;
    }

    AvailabilityTimerMap::iterator iter;
    iter = availability_timers_.find(link);
    if (iter == availability_timers_.end()) {
        return; // no timer for this link
    }

    LinkAvailabilityTimer* timer = iter->second;
    availability_timers_.erase(link);

    // try to cancel the timer and rely on the timer system to clean
    // it up once it bubbles to the top of the queue... if there's a
    // race and the timer is in the process of firing, it should clean
    // itself up in the timeout handler.
    if (!timer->cancel()) {
        log_warn("ContactManager::handle_link_available: "
                 "can't cancel availability timer: race condition");
    }
}

//----------------------------------------------------------------------
void
ContactManager::handle_link_unavailable(LinkUnavailableEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_link_unavailable");

    LinkRef link = event->link_;
    ASSERT(link != NULL);

    if (!has_link(link)) {
        log_warn("ContactManager::handle_link_unavailable: "
                 "link %s does not exist", link->name());
        return;
    }
    
    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_link_unavailable: "
                "link %s is being deleted", link->name());
        return;
    }

    // don't do anything for links that aren't ondemand or alwayson
    if (link->type() != Link::ONDEMAND && link->type() != Link::ALWAYSON) {
        log_debug("ContactManager::handle_link_unavailable: "
                  "ignoring link unavailable for link of type %s",
                  link->type_str());
        return;
    }
    
    // or if the link wasn't broken but instead was closed by user
    // action or by going idle
    if (event->reason_ == ContactEvent::USER ||
        event->reason_ == ContactEvent::IDLE)
    {
        log_debug("ContactManager::handle_link_unavailable: "
                  "ignoring link unavailable due to %s",
                  event->reason_to_str(event->reason_));
        return;
    }
    
    // adjust the retry interval in the link to handle backoff in case
    // it continuously fails to open, then schedule the timer. note
    // that if this is the first time the link is opened, the
    // retry_interval will be initialized to zero so we set it to the
    // minimum here. the retry interval is reset in the link open
    // event handler.
    if (link->retry_interval_ == 0) {
        link->retry_interval_ = link->params().min_retry_interval_;
    }

    int timeout = link->retry_interval_;
    link->retry_interval_ *= 2;
    if (link->retry_interval_ > link->params().max_retry_interval_) {
        link->retry_interval_ = link->params().max_retry_interval_;
    }

    LinkAvailabilityTimer* timer = new LinkAvailabilityTimer(this, link);

    AvailabilityTimerMap::value_type val(link, timer);
    if (availability_timers_.insert(val).second == false) {
        log_err("ContactManager::handle_link_unavailable: "
                "error inserting timer for link %s into table!", link->name());
        delete timer;
        return;
    }

    log_debug("link %s unavailable (%s): scheduling retry timer in %d seconds",
              link->name(), event->reason_to_str(event->reason_), timeout);
    timer->schedule_in(timeout * 1000);
}

//----------------------------------------------------------------------
void
ContactManager::handle_contact_up(ContactUpEvent* event)
{
    oasys::ScopeLock l(&lock_, "ContactManager::handle_contact_up");

    LinkRef link = event->contact_->link();
    ASSERT(link != NULL);

    if(link->isdeleted())
    {
        log_warn("ContactManager::handle_contact_up: "
                 "link %s is being deleted, not marking its contact up", link->name());
        return;
    }

    if (!has_link(link)) {
        log_warn("ContactManager::handle_contact_up: "
                 "link %s does not exist", link->name());
        return;
    }

    if (link->type() == Link::ONDEMAND || link->type() == Link::ALWAYSON) {
        log_debug("ContactManager::handle_contact_up: "
                  "resetting retry interval for link %s: %d -> %d",
                  link->name(),
                  link->retry_interval_,
                  link->params().min_retry_interval_);
        link->retry_interval_ = link->params().min_retry_interval_;
    }
}

//----------------------------------------------------------------------
LinkRef
ContactManager::find_link_to(ConvergenceLayer* cl,
                             const std::string& nexthop,
                             const EndpointID& remote_eid,
                             Link::link_type_t type,
                             u_int states)
{
    oasys::ScopeLock l(&lock_, "ContactManager::find_link_to");
    
    LinkSet::iterator iter;
    LinkRef link("ContactManager::find_link_to: return value");
    
    log_debug("find_link_to: cl %s nexthop %s remote_eid %s "
              "type %s states 0x%x",
              cl ? cl->name() : "ANY",
              nexthop.c_str(), remote_eid.c_str(),
              type == Link::LINK_INVALID ? "ANY" : Link::link_type_to_str(type),
              states);

    // make sure some sane criteria was specified
    ASSERT((cl != NULL) ||
           (nexthop != "") ||
           (remote_eid != EndpointID::NULL_EID()) ||
           (type != Link::LINK_INVALID));
    
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        if ( ((type == Link::LINK_INVALID) || (type == (*iter)->type())) &&
             ((cl == NULL) || ((*iter)->clayer() == cl)) &&
             ((nexthop == "") || (nexthop == (*iter)->nexthop())) &&
             ((remote_eid == EndpointID::NULL_EID()) ||
              (remote_eid == (*iter)->remote_eid())) &&
             ((states & (*iter)->state()) != 0) )
        {
            link = *iter;
            log_debug("ContactManager::find_link_to: "
                      "matched link *%p", link.object());
            ASSERT(!link->isdeleted());
            return link;
        }
    }

    log_debug("ContactManager::find_link_to: no match");
    return link;
}

//----------------------------------------------------------------------
LinkRef
ContactManager::new_opportunistic_link(ConvergenceLayer* cl,
                                       const std::string& nexthop,
                                       const EndpointID& remote_eid,
                                       const std::string* link_name)
{
    log_debug("new_opportunistic_link: cl %s nexthop %s remote_eid %s",
              cl->name(), nexthop.c_str(), remote_eid.c_str());
    
    oasys::ScopeLock l(&lock_, "ContactManager::new_opportunistic_link");

    // find a unique link name
    char name[64];
    
    if (link_name) {
        strncpy(name, link_name->c_str(), sizeof(name));
        
        while (find_link(name) != NULL) {
            snprintf(name, sizeof(name), "%s-%d",
                     link_name->c_str(), opportunistic_cnt_);
                     
            opportunistic_cnt_++;
        }
    }
    
    else {
        do {
            snprintf(name, sizeof(name), "link-%d",
                    opportunistic_cnt_); 
            opportunistic_cnt_++;
        } while (find_link(name) != NULL);
    }
        
    LinkRef link = Link::create_link(name, Link::OPPORTUNISTIC, cl,
                                     nexthop.c_str(), 0, NULL);
    if (link == NULL) {
        log_warn("ContactManager::new_opportunistic_link: "
                 "unexpected error creating opportunistic link");
        return link;
    }

    LinkRef new_link(link.object(),
                     "ContactManager::new_opportunistic_link: return value");
    
    new_link->set_remote_eid(remote_eid);

    if (!add_new_link(new_link)) {
        new_link->delete_link();
        log_err("ContactManager::new_opportunistic_link: "
                 "failed to add new opportunistic link %s", new_link->name());
        new_link = NULL;
    }
    
    return new_link;
}
    
//----------------------------------------------------------------------
void
ContactManager::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, "ContactManager::dump");
    
    buf->append("Links:\n");
    LinkSet::iterator iter;
    for (iter = links_->begin(); iter != links_->end(); ++iter) {
        buf->appendf("*%p\n", (*iter).object());
    }
}

} // namespace dtn
