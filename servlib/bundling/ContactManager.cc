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
#include "BundleTuple.h"
#include "Contact.h"
#include "Link.h"
#include "Peer.h"

/**
 * Define the singleton contact manager instance
 */
ContactManager* ContactManager::instance_; 

/**
 * Constructor / Destructor
 */
ContactManager::ContactManager()
    : Logger("/contact_manager")
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
    peers_->insert(peer);
}

void
ContactManager::delete_peer(Peer *peer)
{
    if (!has_peer(peer)) {
        log_err("Error in deleting peer from contact manager -- "
                "Peer %s does not exist", peer->name());
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
 * Finds peer with a given bundletuple pattern
 */
Peer*
ContactManager::find_peer(const BundleTuple& tuple)
{
    if (!tuple.valid()) {
        log_err("Trying to find an malformed peer %s", tuple.c_str());
        return NULL;
    }
    
    PeerSet::iterator iter;
    Peer* peer = NULL;
    ASSERT(peers_ != NULL);

    for (iter = peers_->begin(); iter != peers_->end(); ++iter)
    {
        peer = *iter;
        if (peer->tuple().equals(tuple)) {
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
ContactManager::add_link(Link *link)
{
    links_->insert(link);
}

void
ContactManager::delete_link(Link *link)
{
    if (!has_link(link)) {
        log_err("Error in deleting link from contact manager.\
                 Link %s does not exist \n",link->name());
    } else {
        links_->erase(link);
    }
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
 * Finds link with a given bundletuple.
 */
Link*
ContactManager::find_link(const char* name)
{
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        if (strcasecmp(link->name(), name) == 0) return link;
    }
    return NULL;
}

/**
 * Dump the contact manager info
 */
void
ContactManager::dump(StringBuffer* buf) const
{
    buf->append("contact manager info:\n");

    buf->append("Links::\n");
    LinkSet::iterator iter;
    Link* link = NULL;
    for (iter = links_->begin(); iter != links_->end(); ++iter)
    {
        link = *iter;
        buf->appendf("\t link (type %s): (name) %s -> (peer) %s - (status) %s, %s",
                     link->type_str(),
                     link->name(),
//                     link->clayer()->proto(),
                     link->dest_tuple()->c_str(),
                     link->isavailable() ? "avail" : "not-avail",
                     link->isopen() ? "open" : "closed");
    }
}


