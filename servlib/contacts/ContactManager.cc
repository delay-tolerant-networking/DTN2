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

namespace dtn {

/**
 * Constructor / Destructor
 */
ContactManager::ContactManager()
    : Logger("/contact_manager"), opportunistic_cnt_(0)
{
    links_ = new LinkSet();
}

/**********************************************
 *
 * Link set accessor functions
 *
 *********************************************/
void
ContactManager::add_link(Link* link)
{
    log_debug("adding link %s", link->name());
    links_->insert(link);
    
    BundleDaemon::post(new LinkCreatedEvent(link));
}

void
ContactManager::del_link(Link *link)
{
    log_debug("deleting link %s", link->name());
    if (!has_link(link)) {
        log_err("Error in del_link: link %s does not exist \n",
                link->name());
        return;
    }

    links_->erase(link);
    BundleDaemon::post(new LinkDeletedEvent(link));
}

bool
ContactManager::has_link(Link *link)
{
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

/**
 * Finds link with a given name
 */
Link*
ContactManager::find_link(const char* name)
{
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

/**
 * Finds link to a given next_hop
 */
Link*
ContactManager::find_link_to(const char* next_hop)
{
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        if (strcasecmp(link->nexthop(), next_hop) == 0)
            return link;
    }
    return NULL;
}

/**
 * Helper routine to find or create an idle opportunistic link.
 */
Link*
ContactManager::get_opportunistic_link(ConvergenceLayer* cl,
				       CLInfo* clinfo,
				       const char* nexthop)
{
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
                log_debug("found match: link %s, but it's already open! Returning it anyway.", link->name());
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
    link->set_cl_info(clinfo);
        
    if (!link) {
        log_crit("unexpected error creating opportunistic link!!");
        return NULL;
    }

    // add the link to the set and post a link created event
    add_link(link);

    return link;
}

/**
 * Notification from a convergence layer that a new contact has come
 * knocking.
 *
 * Find the appropriate Link, store the given convergence layer state
 * in the link and post a new LinkAvailableEvent along which will kick
 * the daemon.
 */
Link*
ContactManager::new_opportunistic_link(ConvergenceLayer* cl,
                                       CLInfo* clinfo,
                                       const char* nexthop)
{
    Link* link = get_opportunistic_link(cl, clinfo, nexthop);
    if (!link) // no link means it couldn't be created
        return NULL;

    if (link->isopen()) {
        log_debug("Contact to %s already established, ignoring...",nexthop);
        return link;
    }

    // notify the daemon that the link is available
    BundleDaemon::post(new LinkAvailableEvent(link));
    return link;
}
    
/**
 * Dump the contact manager info
 */
void
ContactManager::dump(oasys::StringBuffer* buf) const
{
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
