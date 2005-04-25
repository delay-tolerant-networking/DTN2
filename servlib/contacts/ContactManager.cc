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

#include "AddressFamily.h"
#include "ContactManager.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleTuple.h"
#include "Contact.h"
#include "Link.h"
#include "Peer.h"

namespace dtn {

/**
 * Constructor / Destructor
 */
ContactManager::ContactManager()
    : Logger("/contact_manager"), opportunistic_cnt_(0)
{
    peers_ = new PeerSet();
    links_ = new LinkSet();
}

/**********************************************
 *
 * Peer set accessor functions
 *
 *********************************************/
void
ContactManager::add_peer(Peer *peer)
{
    log_debug("adding peer %s", peer->address());
    peers_->insert(peer);
}

void
ContactManager::delete_peer(Peer *peer)
{
    log_debug("deleting peer %s", peer->address());
    if (!has_peer(peer)) {
        log_err("Error in deleting peer from contact manager -- "
                "Peer %s does not exist", peer->address());
    } else {
        peers_->erase(peer);
    }
}

bool
ContactManager::has_peer(Peer *peer)
{
    PeerSet::iterator iter = peers_->find(peer);
    if (iter == peers_->end())
        return false;
    return true;
}

/**
 * Finds peer with a given next hop admin address
 */
Peer*
ContactManager::find_peer(const char* address)
{
    bool valid;
    AddressFamily* af = AddressFamilyTable::instance()->lookup(address, &valid);
    if (!af || !valid) {
        log_err("trying to find an invalid peer admin address '%s'", address);
        return NULL;
    }
    
    PeerSet::iterator iter;
    Peer* peer = NULL;
    ASSERT(peers_ != NULL);

    for (iter = peers_->begin(); iter != peers_->end(); ++iter)
    {
        peer = *iter;
        if (!strcmp(peer->address(), address)) {
            return peer;
        }
    }

    return NULL;
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
    
    Peer* peer = find_peer(link->nexthop());
    if (peer == NULL) {
        peer = new Peer(link->nexthop());
        add_peer(peer);
    }
    link->peer_ = peer;
    peer->add_link(link);
    
    BundleDaemon::post(new LinkCreatedEvent(link));

    // ONDEMAND links are assumed to be available
    if (link->type() == Link::ONDEMAND) {
        link->set_link_available();
    }

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
 * Open the given link.
 */
void
ContactManager::open_link(Link* link)
{
    if (!link->isopen()) {
        link->open();
    }
}

/**
 * Close the given link.
 */
void
ContactManager::close_link(Link* link)
{
    if (link->isopen()) {
        link->close();
    }
}

/**
 * Helper routine to find or create an idle opportunistic link.
 */
Link*
ContactManager::get_opportunistic_link(ConvergenceLayer* cl,
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
        
    if (!link) {
        log_crit("unexpected error creating opportunistic link!!");
        return NULL;
    }

    // this posts a LinkCreatedEvent, adds peers and does all kinds of nice stuff
    add_link(link);

    return link;
}

/**
 * Notification from the convergence layer that a new contact has
 * come knocking. Find the appropriate Link / Contact and return
 * the new contact, notifying the router as necessary.
 */
Contact*
ContactManager::new_opportunistic_contact(ConvergenceLayer* cl,
                                          CLInfo* clinfo,
                                          const char* nexthop)
{
    Link* link = get_opportunistic_link(cl, nexthop);
    if (!link) // no link means it couldn't be created
        return NULL;

    if(!link->isopen()) {
        // notify the router that the link is ready
        link->set_link_available();
        
        // now open the link
        link->open();
        
        // store the given cl info in the contact
        Contact* contact = link->contact();
        ASSERT(contact);
        contact->set_cl_info(clinfo);
        return contact;
    }
    else {
        log_debug("Contact to %s already established, ignoring...",nexthop);
        return link->contact();
    }
}
    
/**
 * Dump the contact manager info
 */
void
ContactManager::dump(oasys::StringBuffer* buf) const
{
    buf->append("contact manager info:\n");

    buf->append("Links:\n");
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        buf->appendf("\t link (type %s): (name) %s -> (peer) %s - (status) %s, %s\n",
                     link->type_str(),
                     link->name(),
                     link->dest_str(),
                     link->isavailable() ? "avail" : "not-avail",
                     link->isopen() ? "open" : "closed");
    }
}



} // namespace dtn
