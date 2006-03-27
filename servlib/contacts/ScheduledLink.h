/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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
#ifndef _SCHEDULED_LINK_H_
#define _SCHEDULED_LINK_H_

#include "Link.h"
#include <set>

namespace dtn {

/**
 * Abstraction for a SCHEDULED link.
 *
 * Scheduled links have a list  of future contacts.
 *
*/

class FutureContact;

class ScheduledLink : public Link {
public:
    typedef std::set<FutureContact*> FutureContactSet;

    /**
     * Constructor / Destructor
     */
    ScheduledLink(std::string name, ConvergenceLayer* cl,
                  const char* nexthop);
    
    virtual ~ScheduledLink();
    
    /**
     * Return the list of future contacts that exist on the link
     */
    FutureContactSet* future_contacts() { return fcts_; }

    // Add a future contact
    void add_fc(FutureContact* fc) ;

    // Remove a future contact
    void delete_fc(FutureContact* fc) ;

    // Convert a future contact to an active contact
    void convert_fc(FutureContact* fc) ;

    // Return list of all future contacts
    FutureContactSet* future_contacts_list() { return fcts_; }
    
protected:
    FutureContactSet*  fcts_;
};

/**
 * Abstract base class for FutureContact
 * Relevant only for scheduled links.
 */
class FutureContact {
public:
    /**
     * Constructor / Destructor
     */
    FutureContact()
        : start_(0), duration_(0)
    {
    }
    
    /// Time at which contact starts, 0 value means not defined
    time_t start_;
    
    /// Duration for this future contact, 0 value means not defined
    time_t duration_;
};


} // namespace dtn

#endif /* _SCHEDULED_LINK_H_ */
