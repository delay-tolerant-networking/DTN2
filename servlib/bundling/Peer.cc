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

#include "Peer.h"
#include "Bundle.h"
#include "BundleList.h"

/**
 * Constructor / Destructor
 */
Peer::Peer(const BundleTuple& tuple)
    : BundleConsumer(&tuple_, false, "Peer"), tuple_(tuple)
{
    ASSERT(tuple.valid());
    logpathf("/peer/%s",tuple.c_str());
    bundle_list_ = new BundleList(logpath_);
    log_debug("new peer *%p", this);
    links_ = new LinkSet();
}

Peer::~Peer()
{
    ASSERT(bundle_list_->size() == 0);
    delete bundle_list_;
}

int
Peer::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "Peer %.*s",
                    (int)tuple().length(), tuple().data());
}

bool
Peer::has_link(Link *link)
{
    LinkSet::iterator iter = links_->find(link);
    if (iter == links_->end())
        return false;
    return true;
}

void
Peer::add_link(Link *link)
{
    links_->insert(link);
}

void
Peer::delete_link(Link *link)
{
    if (!has_link(link)) {
        log_err("Error in deleting link from peer -- "
                "link %s does not exist on peer %s",
                link->name(), name());
    } else {
        links_->erase(link);
    }
}
