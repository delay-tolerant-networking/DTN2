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
#include "conv_layers/ConvergenceLayer.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"

namespace dtn {

/**
 * Constructor
 */
Contact::Contact(Link* link)
    : QueueConsumer(link->nexthop(), false, CONTACT), link_(link)
{
    logpathf("/contact/%s", link->nexthop());

    // XXX/jakob - can we change this to use the same bundlelist as the link?

    bundle_list_ = new BlockingBundleList(logpath_);
    cl_info_ = NULL;
    
    log_info("new contact *%p", this);
}

Contact::~Contact()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;

    ASSERT(cl_info_ == NULL);
}

void
Contact::consume_bundle(Bundle* bundle)
{
    // Add it to the queue (default behavior as defined by queue
    // consumer)
    QueueConsumer::consume_bundle(bundle);

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
