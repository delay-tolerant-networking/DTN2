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

class Bundle;
class BundleList;
class BundleMapping;
class BundleTuple;

/**
 * Base class used for "next hops" in the bundle forwarding logic, i.e
 * either a local registration or a Contact/Link/Peer.
 */
class BundleConsumer : public Logger {
public:
    /**
     * Constructor. It is the responsibility of the subclass to
     * allocate the bundle_list_ if the consumer does any queuing.
     */
    BundleConsumer(const BundleTuple* dest_tuple, bool is_local, const char* type_str);

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
     * Each BundleConsumer has a tuple (either the registration
     * endpoint or the next-hop address).
     */
    const BundleTuple* dest_tuple() { return dest_tuple_; }

    /**
     * Is the consumer a local registration or a peer.
     */
    bool is_local() { return is_local_; }

    /**
     * Type of the bundle consumer. Link or Peer or Contact or Reg
     */
    const char* type_str() { return type_str_; }

    /**
     * Accessor for the list of bundles in this consumer
     * This can be null, unless it is initalized by the
     * specific instance of the bundle consumer
     */
    BundleList* bundle_list() { return bundle_list_; }
    
protected:
    const BundleTuple* dest_tuple_;
    bool is_local_;
    const char* type_str_;
    BundleList* bundle_list_;

private:
    /**
     * Default constructor should not be used.
     */
    BundleConsumer();
};

#endif /* _BUNDLE_CONSUMER_H_ */
