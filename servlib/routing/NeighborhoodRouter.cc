/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "contacts/Contact.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "NeighborhoodRouter.h"

namespace dtn {

NeighborhoodRouter::NeighborhoodRouter()
    : TableBasedRouter("NeighborhoodRouter", "neighborhood")
{
    log_info("Initializing NeighborhoodRouter");
}

void
NeighborhoodRouter::handle_contact_up(ContactUpEvent* event)
{
    LinkRef link = event->contact_->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    TableBasedRouter::handle_contact_up(event);
    
    log_info("Contact Up: *%p adding route", link.object());

    char eidstring[255];
    sprintf(eidstring, "dtn://%s", link->nexthop());

    // By default, we add a route for all the next hops we have around. 
    RouteEntry* entry = new RouteEntry(EndpointIDPattern(eidstring), link);
    entry->set_action(ForwardingInfo::FORWARD_ACTION);
    add_route(entry);
}

void
NeighborhoodRouter::handle_contact_down(ContactDownEvent* event)
{
    route_table_->del_entries_for_nexthop(event->contact_->link());

    TableBasedRouter::handle_contact_down(event);
}

} // namespace dtn
