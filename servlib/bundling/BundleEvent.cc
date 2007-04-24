/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iostream>
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include <routing/RouteTable.h>
#include <contacts/Contact.h>
#include <contacts/ContactManager.h>
#include <contacts/Interface.h>
#include <reg/Registration.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

void
BundleReceivedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("source", &source_);
    a->process("bytes_received", &bytes_received_);
    a->process("bundle", bundleref_.object());
}

void
BundleTransmittedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bytes_sent", &bytes_sent_);
    a->process("reliably_sent", &reliably_sent_);
    a->process("bundle", bundleref_.object());
    a->process("contact", contact_.object());
    a->process("link", link_.object());
}

/*void
BundleTransmitFailedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundleref_.object());
    a->process("contact", contact_.object());
    a->process("link", link_.object());
}*/

void
BundleDeliveryEvent::serialize(oasys::SerializeAction *a)
{
    a->process("source", &source_);
    a->process("bundle", bundleref_.object());
}

void
BundleExpiredEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundleref_.object());
}

void
BundleNotNeededEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundleref_.object());
}

void
BundleSendCancelledEvent::serialize(oasys::SerializeAction* a)
{
    a->process("bundle", bundleref_.object());
    a->process("link", link_.object());
}

void
BundleInjectRequest::serialize(oasys::SerializeAction* a)
{
    a->process("source", &src_);
    a->process("dest", &dest_);
    a->process("replyto", &replyto_);
    a->process("custodian", &custodian_);
    a->process("link", &link_);
    a->process("fwd_action", &action_);
    a->process("priority", &priority_);
    a->process("expiration", &expiration_);
    u_int64_t length = payload_length_;
    a->process("payload_length", &length);
    a->process("request_id", &request_id_);
}

void
BundleInjectedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundleref_.object());
    a->process("request_id", &request_id_);
}

void
BundleAcceptRequest::serialize(oasys::SerializeAction *a)
{
    // XXX/demmer this request needs a better API since we can't get
    // the result out using the current interface
    a->process("source", &source_);
    a->process("bundle", bundle_.object());
}

void
BundleReportEvent::serialize(oasys::SerializeAction *a)
{
    BundleDaemon *bd = BundleDaemon::instance();

    // add pending bundles
    const BundleList *bundles = bd->pending_bundles();
    BundleList::const_iterator j;

    oasys::ScopeLock l(bd->pending_bundles()->lock(),
        "BundleEvent::BundleReportEvent");

    for(j = bundles->begin(); j != bundles->end(); ++j)
        a->process("bundle", *j);

    l.unlock();

    // add custody bundles
    oasys::ScopeLock m(bd->custody_bundles()->lock(),
        "BundleEvent::BundleReportEvent");

    bundles = bd->custody_bundles();
    BundleList::const_iterator k;

    for(k = bundles->begin(); k != bundles->end(); ++k) {
        a->process("bundle", *k);
    }
}

void
ContactUpEvent::serialize(oasys::SerializeAction *a)
{
    a->process("contact", contact_.object());
}

void
ContactDownEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("contact", contact_.object());
}

void
ContactReportEvent::serialize(oasys::SerializeAction *a)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "BundleEvent::ContactReportEvent");

    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator j;

    for(j = links->begin(); j != links->end(); ++j) {
        if ((*j)->contact() != NULL) {
            a->process("contact", (*j)->contact().object());
        }
    }
}

void
ContactAttributeChangedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("contact", contact_.object());
}

void
LinkCreatedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_.object());
}

void
LinkDeletedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_.object());
}

void
LinkAvailableEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_.object());
}

void
LinkUnavailableEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_.object());
}

void
LinkAttributeChangedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_.object());
}

void
LinkBusyEvent::serialize(oasys::SerializeAction *a)
{
    a->process("link", link_.object());
}

void
LinkReportEvent::serialize(oasys::SerializeAction *a)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "BundleEvent::LinkReportEvent");

    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator j;

    for(j = links->begin(); j != links->end(); ++j) {
        a->process("link", (*j).object());
    }
}

void
LinkStateChangeRequest::serialize(oasys::SerializeAction* a)
{
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        std::string link;
        a->process("link", &link);

        BundleDaemon *bd = BundleDaemon::instance();

        link_ = bd->contactmgr()->find_link(link.c_str());
        if (link_ != NULL) {
            contact_   = link_->contact();
            old_state_ = link_->state();
        }
    }
}

void
RegistrationAddedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("registration", registration_);
    a->process("source", &source_);
}

void
RegistrationRemovedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("registration", registration_);
}

void
RegistrationExpiredEvent::serialize(oasys::SerializeAction *a)
{
    a->process("regid", &regid_);
}

void
RouteAddEvent::serialize(oasys::SerializeAction *a)
{
    a->process("route_entry", entry_);
}

void
RouteDelEvent::serialize(oasys::SerializeAction *a)
{
    a->process("dest", const_cast< std::string * >(&dest_.str()));
}

void
CustodySignalEvent::serialize(oasys::SerializeAction *a)
{
    a->process("admin_type", &data_.admin_type_);
    a->process("admin_flags", &data_.admin_flags_);
    a->process("succeeded", &data_.succeeded_);
    a->process("reason", &data_.reason_);
    a->process("orig_frag_offset", &data_.orig_frag_offset_);
    a->process("orig_frag_length", &data_.orig_frag_length_);
    a->process("custody_signal_seconds", &data_.custody_signal_tv_.seconds_);
    a->process("custody_signal_seqno", &data_.custody_signal_tv_.seqno_);
    a->process("orig_creation_seconds", &data_.orig_creation_tv_.seconds_);
    a->process("orig_creation_seqno", &data_.orig_creation_tv_.seqno_);
    a->process("orig_source_eid",
         const_cast< std::string * >(&data_.orig_source_eid_.str()));
}

void
CustodyTimeoutEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundle_.object());
    a->process("link", link_.object());
}

void
NewEIDReachableEvent::serialize(oasys::SerializeAction *a)
{
    std::string name = iface_->name();

    a->process("eid", &endpoint_);
    a->process("interfaceName", &name);
}

} // namespace dtn
