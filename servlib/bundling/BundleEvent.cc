/*
 * License Agreement
 * 
 * NOTICE
 * This software (or technical data) was produced for the U. S.
 * Government under contract W15P7T-05-C-F600, and is
 * subject to the Rights in Data-General Clause 52.227-14 (JUNE 1987)
 * 
 * Copyright (C) 2006. The MITRE Corporation (http://www.mitre.org/).
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The US Government will not be charged any license fee and/or
 * royalties related to this software.
 * 
 * * Neither name of The MITRE Corporation; nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include <routing/RouteTable.h>
#include <contacts/Contact.h>
#include <contacts/ContactManager.h>
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
    a->process("link", link_);
}

void
BundleTransmitFailedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("bundle", bundleref_.object());
    a->process("contact", contact_.object());
    a->process("link", link_);
}

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
BundleSendRequest::serialize(oasys::SerializeAction* a)
{
    a->process("bundleid", &bundleid_);
    a->process("link", &link_);
    a->process("fwd_action", &action_);
}

void
BundleCancelRequest::serialize(oasys::SerializeAction* a)
{
    a->process("bundleid", &bundleid_);
    a->process("link", &link_);
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
    a->process("payload", &payload_);
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
LinkCreatedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_);
}

void
LinkDeletedEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_);
}

void
LinkAvailableEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_);
}

void
LinkUnavailableEvent::serialize(oasys::SerializeAction *a)
{
    a->process("reason", &reason_);
    a->process("link", link_);
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
        a->process("link", *j);
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

        if (link_) {
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
    a->process("link", link_);
}

} // namespace dtn
