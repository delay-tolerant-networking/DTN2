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
#ifndef _PEER_H_
#define _PEER_H_

#include <set>

#include <oasys/debug/Formatter.h>
#include <oasys/debug/Debug.h>
#include <oasys/util/StringBuffer.h>
#include "BundleConsumer.h"
#include "BundleTuple.h"

namespace dtn {

class Link;
class LinkSet;
class Peer;

/**
 * Set of peers
 */
class PeerSet : public ::std::set<Peer*> {};

/**
 * Encapsulation of a next-hop DTN node. The object
 * contains a list of incoming links and a list of bundles
 * that are destined to it.
 * Used by the routing logic to store bundles going towards
 * a common destination but yet to be assigned to a particular
 * link.
 */
class Peer : public oasys::Formatter,  public BundleConsumer {
public:
    /**
     * Constructor / Destructor
     */
    Peer(const char* address);
    virtual ~Peer();

    /**
     * Add a link
     */
    void add_link(Link* link);
    
    /**
     * Delete a link
     */
    void del_link(Link* link);
    
    /**
     * Check if the peer already has this link
     */
    bool has_link(Link* link); 
    
    /**
     * Return the list of links that lead to the peer
     */
    LinkSet* links() { return links_; }
    
    /**
     * Accessor to address
     */
    const char* address() { return address_.c_str(); }
    
    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);
        
    /**
     * Add the bundle to the peer. Searches through the list of links
     * for an open one, and if it finds one, then the bundle is queued
     * on the link instead. Otherwise, it remains on the peer's queue
     * awaiting some link to open up.
     *
     * XXX/demmer think this through a little better?
     */
    void enqueue_bundle(Bundle* bundle, const BundleMapping* mapping);

protected:
    LinkSet* links_;		///< List of links that lead to this peer
    std::string address_;	///< Identity of peer
};

} // namespace dtn

#endif /* _PEER_H_ */
