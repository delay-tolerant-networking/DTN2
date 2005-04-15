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

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "NeighborhoodRouter.h"
#include <stdlib.h>

namespace dtn {

NeighborhoodRouter::NeighborhoodRouter()
{
    Logger::set_logpath("/route/dynamic");
    log_info("Initializing NeighborhoodRouter");
}


/**
 * Default event handler when a new link is created
 */
void
NeighborhoodRouter::handle_link_created(LinkCreatedEvent* event)
{
    log_info("LINK_CREATED *%p adding route", event->link_);

    // XXX/jakob - this is pretty nasty really. I believe the bundles://<region> syntax needs to go very soon.
   
    char tuplestring[100];
    sprintf(tuplestring, "bundles://internet/%s",event->link_->nexthop());

    // By default, we add a route for all the next hops we have around. 
    RouteEntry* entry = new RouteEntry(BundleTuplePattern(tuplestring), 
                                       event->link_, 
                                       FORWARD_REASSEMBLE);    
    add_route(entry);

}

} // namespace dtn
