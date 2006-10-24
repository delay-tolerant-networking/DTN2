/*
 *    Copyright 2006 Intel Corporation
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

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/AnnounceBundle.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "ProphetRouter.h"

namespace dtn {

ProphetRouter::ProphetRouter()
    : TableBasedRouter("ProphetRouter", "prophet")
{
    log_info("Initializing ProphetRouter");
}

void
ProphetRouter::handle_link_created(LinkCreatedEvent* event)
{
    ASSERT(event->link_->remote_eid().equals(EndpointID::NULL_EID()) == false);
    TableBasedRouter::handle_link_created(event);
}

void
ProphetRouter::handle_contact_down(ContactDownEvent* event)
{
    route_table_->del_entries_for_nexthop(event->contact_->link());

    TableBasedRouter::handle_contact_down(event);
}

} // namespace dtn
