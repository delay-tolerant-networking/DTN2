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

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Formatter.h>
#include <oasys/util/Ref.h>
#include <oasys/util/RefCountedObject.h>

namespace dtn {

class Bundle;
class BundleList;
class ConvergenceLayer;
class CLInfo;
class Link;

/**
 * Encapsulation of an active connection to a next-hop DTN contact.
 * This is basically a repository for any state about the contact
 * opportunity including start time, estimations for bandwidth or
 * latency, etc.
 *
 * It also contains the CLInfo slot for the convergence layer to put
 * any state associated with the active connection.
 *
 * Since the contact object may be used by multiple threads in the
 * case of a connection-oriented convergence layer, and because the
 * object is intended to be deleted when the contact opportunity ends,
 * all object instances are reference counted and will be deleted when
 * the last reference is removed.
 */
class Contact : public oasys::RefCountedObject,
                public oasys::Logger
{
public:
    /**
     * Constructor
     */
    Contact(Link* link);

private:
    /**
     * Destructor -- private since the class is reference counted and
     * therefore is never explicitly deleted.
     */ 
    virtual ~Contact();
    friend class oasys::RefCountedObject;

public:
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
     * Virtual from formatter
     */
    int format(char* buf, size_t sz) const;

    /// Time when the contact begin
    struct timeval start_time_;

    /// Contact duration (0 if unknown)
    u_int32_t duration_ms_;

    /// Approximate bandwidth
    u_int32_t bps_;
    
    /// Approximate latency
    u_int32_t latency_ms_;

protected:
    Link* link_ ; 	///< Pointer to parent link on which this
    			///  contact exists
    
    CLInfo* cl_info_;	///< convergence layer specific info
};

/**
 * Typedef for a reference on a contact.
 */
typedef oasys::Ref<Contact> ContactRef;

} // namespace dtn

#endif /* _CONTACT_H_ */
