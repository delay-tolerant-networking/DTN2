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
#ifndef _BUNDLE_CONSUMER_H_
#define _BUNDLE_CONSUMER_H_

#include "BundleTuple.h"
#include <oasys/debug/Log.h>

namespace dtn {

class Bundle;
class BundleList;
class BundleMapping;
class BundleTuple;

/**
 * Base class used for "next hops" in the bundle forwarding logic, i.e
 * either a local registration or a Contact/Link/Peer.
 */
class BundleConsumer : public oasys::Logger {
public:
    typedef enum {
        INVALID = 0,
        LINK = 1,
        PEER,
        CONTACT,
        FUTURE_CONTACT,
        REGISTRATION
    } type_t;

    static const char* type2str(type_t type)
    {
        switch(type) {
        case LINK: 		return "Link";
        case PEER:		return "Peer";
        case CONTACT:		return "Contact";
        case FUTURE_CONTACT:	return "Future Contact";
        case REGISTRATION:	return "Reg";
        default:		return "__INVALID__";
        }
    }
    
    /**
     * Constructor. It is the responsibility of the subclass to
     * allocate the bundle_list_ if the consumer does any queuing.
     */
    BundleConsumer(const char* dest_str, bool is_local, type_t type);

    /**
     * Destructor
     */
    virtual ~BundleConsumer() {}

    /**
     * Add the bundle to the queue.
     */
    virtual void enqueue_bundle(Bundle* bundle, const BundleMapping* mapping);

    /**
     * Attempt to remove the given bundle from the queue.
     *
     * @return true if the bundle was dequeued, false if not. If
     * mappingp is non-null, return the old mapping as well.
     */
    virtual bool dequeue_bundle(Bundle* bundle, BundleMapping** mappingp);

    /**
     * Check if the given bundle is already queued on this consumer.
     */
    virtual bool is_queued(Bundle* bundle);

    /**
     * Each BundleConsumer has a next hop destination address (either
     * the registration endpoint or the next-hop link address).
     */
    const char* dest_str() { return dest_str_.c_str(); }

    /**
     * Is the consumer a local registration or a peer.
     */
    bool is_local() { return is_local_; }

    /**
     * The type of the consumer (link, peer, contact, registration).
     */
    type_t type() { return type_; }

    /**
     * Stringified type.
     */
    const char* type_str() { return type_str_; }

    /**
     * Accessor for the list of bundles in this consumer
     * This can be null, unless it is initalized by the
     * specific instance of the bundle consumer
     */
    BundleList* bundle_list() { return bundle_list_; }
    
protected:
    std::string dest_str_;
    bool is_local_;
    type_t type_;
    const char* type_str_;
    BundleList* bundle_list_;

private:
    /**
     * Default constructor should not be used.
     */
    BundleConsumer();
};

} // namespace dtn

#endif /* _BUNDLE_CONSUMER_H_ */
