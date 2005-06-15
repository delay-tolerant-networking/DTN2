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
#ifndef _CONTACT_H_
#define _CONTACT_H_

#include <oasys/debug/Debug.h>
#include <oasys/debug/Formatter.h>

#include "BundleConsumer.h"
#include "BundleTuple.h"
#include "Link.h"

namespace dtn {

class Bundle;
class BundleList;
class ConvergenceLayer;
class CLInfo;

/**
 * Encapsulation of a connection to a next-hop DTN contact. The object
 * contains a list of bundles that are destined for it, as well as a
 * slot to store any convergence layer specific attributes.
 */
class Contact : public oasys::Formatter, public QueueConsumer {
public:
    /**
     * Constructor / Destructor
     */
    Contact(Link* link);
    virtual ~Contact();
 
    /**
     * Consume the bundle and send it out.
     */
    void consume_bundle(Bundle* bundle);
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer() { return link_->clayer(); }

    /**
     * Store the convergence layer state associated with the contact.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }
    
    /**
     * Accessor to the convergence layer info.
     */
    CLInfo* cl_info() { return cl_info_; }
    
    /**
     * Accessor to the link
     */
    Link* link() { return link_; }

    /**
     * Accessor to next hop admin string.
     */
    const char* nexthop() { return link_->nexthop() ; }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);

    /**
     * Return the bundle list (a blocking one in fact).
     * XXX/demmer get rid of me
     */
    BlockingBundleList* xxx_bundle_list()
    {
        return (BlockingBundleList*)bundle_list_;
    }
    
    
protected:
    Link* link_ ; 	///< Pointer to parent link on which this
    			///  contact exists
    CLInfo* cl_info_;	///< convergence layer specific info
};

} // namespace dtn

#endif /* _CONTACT_H_ */
