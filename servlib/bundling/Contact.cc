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

#include "Contact.h"
#include "Bundle.h"
#include "BundleList.h"
#include "BundleMapping.h"
#include "conv_layers/ConvergenceLayer.h"
#include "BundleForwarder.h"
#include "BundleEvent.h"

namespace dtn {

/**
 * Constructor
 */
Contact::Contact(Link* link)
    : BundleConsumer(link->dest_tuple(), false, "Contact"), link_(link)
{
    logpathf("/contact/%s", link->tuple().c_str());

    bundle_list_ = new BundleList(logpath_);
    contact_info_ = NULL;
    clayer()->open_contact(this);
    
    // Post a contact Available event
    BundleForwarder::post(new ContactUpEvent(this));
    log_info("new contact *%p", this);
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

void
Contact::enqueue_bundle(Bundle* bundle, const BundleMapping* mapping)
{
    // Add it to the queue (default behavior as defined by bundle
    // consumer)
    BundleConsumer::enqueue_bundle(bundle,mapping);

    // and kick the convergence layer
    clayer()->send_bundles(this);
}

/**
 * Formatting...XXX Make it better
 */
int
Contact::format(char* buf, size_t sz)
{
    return link_->format(buf,sz);
}

} // namespace dtn
