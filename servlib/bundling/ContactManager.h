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
#ifndef _CONTACT_MANAGER_H_
#define _CONTACT_MANAGER_H_

#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

class BundleTuple;
class Contact;
class ConvergenceLayer;
class CLInfo;
class Link;
class LinkSet;
class Peer;
class PeerSet;

/**
 * A contact manager singleton class.
 * Maintains topological information regarding available
 * links, peers, contacts.
 */
class ContactManager : public oasys::Logger {
public:
    /**
     * Constructor / Destructor
     */
    ContactManager();
    virtual ~ContactManager() {}
    
    /**
     * Dump a string representation of the info inside contact manager.
     */
    void dump(oasys::StringBuffer* buf) const;
    
    /**********************************************
     *
     * Peer set accessor functions
     *
     *********************************************/
    
    /**
     * Add a peer
     */
    void add_peer(Peer* peer);
    
    /**
     * Delete a peer
     */
    void delete_peer(Peer* peer);
    
    /**
     * Check if contact manager already has this peer
     */
    bool has_peer(Peer* peer);

    /**
     * Finds peer corresponding to this next hop address
     */
    Peer* find_peer(const char* address);
    
    /**
     * Return the list of peers 
     */
    PeerSet* peers() { return peers_; }
    
    /**********************************************
     *
     * Link set accessor functions
     *
     *********************************************/
    /**
     * Add a link
     */
    void add_link(Link* link);
    
    /**
     * Delete a link
     */
    void delete_link(Link* link);
    
    /**
     * Check if contact manager already has this link
     */
    bool has_link(Link* link);

    /**
     * Finds link corresponding to this name
     */
    Link* find_link(const char* name);

    /**
     * Return the list of links 
     */
    LinkSet* links() { return links_; }

    /**********************************************
     *
     * Link state manipulation routines
     *
     *********************************************/
    /**
     * Open the given link.
     */
    void open_link(Link* link);
    
    /**
     * Close the given link.
     */
    void close_link(Link* link);

    /**********************************************
     *
     * Opportunistic contact routines
     *
     *********************************************/

    /**
     * Notification from the convergence layer that a new contact has
     * come knocking. Find the appropriate Link / Contact and return
     * the new contact, notifying the router as necessary.
     */
    Contact* new_opportunistic_contact(ConvergenceLayer* cl,
                                       CLInfo* clinfo,
                                       const char* nexthop);

protected:
    /**
     * Helper routine to find or create an opportunistic link.
     */
    Link* find_opportunistic_link(ConvergenceLayer* cl,
                                  const char* nexthop);
    
    
    PeerSet* peers_;			///< Set of all peers
    LinkSet* links_;			///< Set of all links
    int opportunistic_cnt_;		///< Counter for opportunistic links
};


} // namespace dtn

#endif /* _CONTACT_MANAGER_H_ */
