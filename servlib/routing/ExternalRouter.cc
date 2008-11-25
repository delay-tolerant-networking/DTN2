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
#  include <dtn-config.h>
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sstream>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "ExternalRouter.h"
#include "bundling/GbofId.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "bundling/MetadataBlockProcessor.h"
#include "contacts/ContactManager.h"
#include "contacts/NamedAttribute.h"
#include "reg/RegistrationTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include <oasys/io/UDPClient.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/io/IO.h>

#define SEND(event, data) \
    rtrmessage::bpa message; \
    message.event(data); \
    send(message);

#define CATCH(exception) \
    catch (exception &e) { log_warn("%s", e.what()); }

namespace dtn {

using namespace rtrmessage;

ExternalRouter::ExternalRouter()
    : BundleRouter("ExternalRouter", "external")
{
    log_notice("Initializing ExternalRouter");
}

ExternalRouter::~ExternalRouter()
{
    delete srv_;
    delete hello_;
    delete reg_;
    delete route_table_;
}

// Initialize inner classes
void
ExternalRouter::initialize()
{
    // Create the static route table
    route_table_ = new RouteTable("external");

    // Register as a client app with the forwarder
    reg_ = new ERRegistration(this);

    // Create a hello timer
    hello_ = new HelloTimer(this);

    // Register the global shutdown function
    BundleDaemon::instance()->set_rtr_shutdown(
        external_rtr_shutdown, (void *) 0);

    // Start module server thread
    srv_ = new ModuleServer();
    srv_->start();

    bpa message;
    message.alert(dtnStatusType(std::string("justBooted")));
    message.hello_interval(ExternalRouter::hello_interval);
    send(message);
    hello_->schedule_in(ExternalRouter::hello_interval * 1000);
}

void
ExternalRouter::shutdown()
{
    dtnStatusType e(std::string("shuttingDown"));
    SEND(alert, e)
}

// Format the given StringBuffer with static routing info
void
ExternalRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Static route table for %s router(s):\n", name_.c_str());
    route_table_->dump(buf);
}

// Serialize events and UDP multicast to external routers
void
ExternalRouter::handle_event(BundleEvent *event)
{
    dispatch_event(event);
}

void
ExternalRouter::handle_bundle_received(BundleReceivedEvent *event)
{
    bpa::bundle_received_event::type e(
        event->bundleref_.object(),
        event->bundleref_->dest(),
        event->bundleref_->custodian(),
        event->bundleref_->replyto(),
        bundle_ts_to_long(event->bundleref_->extended_id()),
        event->bundleref_->expiration(),
        event->bytes_received_);

    // optional param, so has to be added after constructor call
    e.prevhop(event->bundleref_->prevhop());

    LinkRef null_link("ExternalRouter::handle_bundle_received");
    MetadataVec * gen_meta = event->bundleref_->generated_metadata().
                             find_blocks(null_link);
    unsigned int num_meta_blocks = event->bundleref_->recv_metadata().size() +
                                   ((gen_meta == NULL)? 0 : gen_meta->size());
    e.num_meta_blocks(num_meta_blocks);

    SEND(bundle_received_event, e)
}

void
ExternalRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    if (event->contact_ == NULL) return;

    bpa::data_transmitted_event::type e(
        event->bundleref_.object(),
        bundle_ts_to_long(event->bundleref_->extended_id()),
        event->link_.object()->name_str(),
        event->bytes_sent_,
        event->reliably_sent_);
    SEND(data_transmitted_event, e)
}

void
ExternalRouter::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    bpa::bundle_delivered_event::type e(
        event->bundleref_.object(),
        bundle_ts_to_long(event->bundleref_->extended_id()));
    SEND(bundle_delivered_event, e)
}

void
ExternalRouter::handle_bundle_expired(BundleExpiredEvent* event)
{
    bpa::bundle_expired_event::type e(
        event->bundleref_.object(),
        bundle_ts_to_long(event->bundleref_->extended_id()));
    SEND(bundle_expired_event, e)
}

void
ExternalRouter::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
    bpa::bundle_send_cancelled_event::type e(
        event->bundleref_.object(),
        event->link_.object()->name_str(),
        bundle_ts_to_long(event->bundleref_->extended_id()));
    SEND(bundle_send_cancelled_event, e)
}

void
ExternalRouter::handle_bundle_injected(BundleInjectedEvent* event)
{
    bpa::bundle_injected_event::type e(
        event->request_id_,
        event->bundleref_.object(),
        bundle_ts_to_long(event->bundleref_->extended_id()));
    SEND(bundle_injected_event, e)
}

void
ExternalRouter::handle_contact_up(ContactUpEvent* event)
{
    ASSERT(event->contact_->link() != NULL);
    ASSERT(!event->contact_->link()->isdeleted());
        
    bpa::link_opened_event::type e(
        event->contact_.object(),
        event->contact_->link().object()->name_str());
    SEND(link_opened_event, e)
}

void
ExternalRouter::handle_contact_down(ContactDownEvent* event)
{
    bpa::link_closed_event::type e(
        event->contact_.object(),
        event->contact_->link().object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_closed_event, e)
}

void
ExternalRouter::handle_link_created(LinkCreatedEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_created_event::type e(
        event->link_.object(),
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_created_event, e)
}

void
ExternalRouter::handle_link_deleted(LinkDeletedEvent *event)
{
    ASSERT(event->link_ != NULL);

    bpa::link_deleted_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_deleted_event, e)
}

void
ExternalRouter::handle_link_available(LinkAvailableEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_available_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_available_event, e)
}

void
ExternalRouter::handle_link_unavailable(LinkUnavailableEvent *event)
{
    bpa::link_unavailable_event::type e(
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_unavailable_event, e)
}

void
ExternalRouter::handle_link_attribute_changed(LinkAttributeChangedEvent *event)
{
    ASSERT(event->link_ != NULL);
    ASSERT(!event->link_->isdeleted());
        
    bpa::link_attribute_changed_event::type e(
        event->link_.object(),
        event->link_.object()->name_str(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(link_attribute_changed_event, e)
}

void
ExternalRouter::handle_new_eid_reachable(NewEIDReachableEvent* event)
{
    bpa::eid_reachable_event::type e(
        event->endpoint_,
        event->iface_->name());
    SEND(eid_reachable_event, e)
}

void
ExternalRouter::handle_contact_attribute_changed(ContactAttributeChangedEvent *event)
{
    ASSERT(event->contact_->link() != NULL);
    ASSERT(!event->contact_->link()->isdeleted());
        
    // @@@ is this right? contact_eid same as link's remote_eid?
    bpa::contact_attribute_changed_event::type e(
        eidType(event->contact_->link()->remote_eid().str()), 
        event->contact_.object(),
        contactReasonType(reason_to_str(event->reason_)));
    SEND(contact_attribute_changed_event, e)
}

// void
// ExternalRouter::handle_link_busy(LinkBusyEvent *event)
// {
//     bpa::link_busy_event::type e(
//         event->link_.object());
//     SEND(link_busy_event, e)
// }

void
ExternalRouter::handle_registration_added(RegistrationAddedEvent* event)
{
    bpa::registration_added_event::type e(
        event->registration_,
        (xml_schema::string)source_to_str((event_source_t)event->source_));
    SEND(registration_added_event, e)
}

void
ExternalRouter::handle_registration_removed(RegistrationRemovedEvent* event)
{
    bpa::registration_removed_event::type e(
        event->registration_);
    SEND(registration_removed_event, e)
}

void
ExternalRouter::handle_registration_expired(RegistrationExpiredEvent* event)
{
    bpa::registration_expired_event::type e(
        event->registration_->regid());
    SEND(registration_expired_event, e)
}

void
ExternalRouter::handle_route_add(RouteAddEvent* event)
{
    // update our own static route table first
    route_table_->add_entry(event->entry_);

    bpa::route_add_event::type e(
        event->entry_);
    SEND(route_add_event, e)
}

void
ExternalRouter::handle_route_del(RouteDelEvent* event)
{
    // update our own static route table first
    route_table_->del_entries(event->dest_);

    bpa::route_delete_event::type e(
        eidType(event->dest_.str()));
    SEND(route_delete_event, e)
}

void
ExternalRouter::handle_custody_signal(CustodySignalEvent* event)
{
    custodySignalType attr(
        event->data_.admin_type_,
        event->data_.admin_flags_,
        event->data_.succeeded_,
        event->data_.reason_,
        event->data_.orig_frag_offset_,
        event->data_.orig_frag_length_,
        event->data_.custody_signal_tv_.seconds_,
        event->data_.custody_signal_tv_.seqno_,
        event->data_.orig_creation_tv_.seconds_,
        event->data_.orig_creation_tv_.seqno_);

    // In order to provide the correct local_id we have to go find the
    // bundle in our system that has this GBOF-ID and that we are custodian
    // of. There should only be one such bundle.

    GbofId gbof_id;
    gbof_id.source_ = event->data_.orig_source_eid_;
    gbof_id.creation_ts_ = event->data_.orig_creation_tv_;
    gbof_id.is_fragment_
        = event->data_.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT;
    gbof_id.frag_length_
        = gbof_id.is_fragment_ ? event->data_.orig_frag_length_ : 0;
    gbof_id.frag_offset_
        = gbof_id.is_fragment_ ? event->data_.orig_frag_offset_ : 0;

    BundleDaemon *bd = BundleDaemon::instance();
    BundleRef br = bd->custody_bundles()->find(gbof_id);
    if (!br.object()) {
        // We don't seem to currently have custody of this bundle
        return;
    }

    bpa::custody_signal_event::type e(
        event->data_,
        attr,
        bundle_ts_to_long(br->extended_id()));

    SEND(custody_signal_event, e)
}

void
ExternalRouter::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    bpa::custody_timeout_event::type e(
        event->bundle_.object(),
        bundle_ts_to_long(event->bundle_->extended_id()));
    SEND(custody_timeout_event, e)
}

void
ExternalRouter::handle_link_report(LinkReportEvent *event)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator i = links->begin();
    LinkSet::const_iterator end = links->end();

    link_report report;
    link_report::link::container c;

    for(; i != end; ++i)
        c.push_back(link_report::link::type((*i).object()));

    report.link(c);
    SEND(link_report, report)
}

void
ExternalRouter::handle_link_attributes_report(LinkAttributesReportEvent *event)
{
    AttributeVector::const_iterator iter = event->attributes_.begin();
    AttributeVector::const_iterator end = event->attributes_.end();

    link_attributes_report::report_params::container c;

    for(; iter != end; ++iter) {
      c.push_back( key_value_pair(*iter) );
    }

    link_attributes_report e(event->query_id_);
    e.report_params(c);
    SEND(link_attributes_report, e)
}

void
ExternalRouter::handle_contact_report(ContactReportEvent* event)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->contactmgr()->lock(),
        "ExternalRouter::handle_event");

    (void) event;
    
    const LinkSet *links = bd->contactmgr()->links();
    LinkSet::const_iterator i = links->begin();
    LinkSet::const_iterator end = links->end();

    contact_report report;
    contact_report::contact::container c;
    
    for(; i != end; ++i) {
        if ((*i)->contact() != NULL) {
            c.push_back((*i)->contact().object());
        }
    }

    report.contact(c);
    SEND(contact_report, report)
}

void
ExternalRouter::handle_bundle_report(BundleReportEvent *event)
{
    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->pending_bundles()->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
    const BundleList *bundles = bd->pending_bundles();
    BundleList::iterator i = bundles->begin();
    BundleList::iterator end = bundles->end();

    bundle_report report;
    bundle_report::bundle::container c;

    for(; i != end; ++i)
        c.push_back(bundle_report::bundle::type(*i));

    report.bundle(c);
    SEND(bundle_report, report)
}

void
ExternalRouter::handle_bundle_attributes_report(BundleAttributesReportEvent *event)
{
    BundleRef br = event->bundle_;

    bundleAttributesReportType response;

    AttributeNameVector::iterator i = event->attribute_names_.begin();
    AttributeNameVector::iterator end = event->attribute_names_.end();

    for (; i != end; ++i) {
        const std::string& name = i->name();

        if (name == "bundleid")
            response.bundleid( br->bundleid() );
        else if (name == "is_admin")
            response.is_admin( br->is_admin() );
        else if (name == "do_not_fragment")
            response.do_not_fragment( br->do_not_fragment() );
        else if (name == "priority")
            response.priority(
                bundlePriorityType(lowercase(br->prioritytoa(br->priority()))) );
        else if (name == "custody_requested")
            response.custody_requested( br->custody_requested() );
        else if (name == "local_custody")
            response.local_custody( br->local_custody() );
        else if (name == "singleton_dest")
            response.singleton_dest( br->singleton_dest() );
        else if (name == "custody_rcpt")
            response.custody_rcpt( br->custody_rcpt() );
        else if (name == "receive_rcpt")
            response.receive_rcpt( br->receive_rcpt() );
        else if (name == "forward_rcpt")
            response.forward_rcpt( br->forward_rcpt() );
        else if (name == "delivery_rcpt")
            response.delivery_rcpt( br->delivery_rcpt() );
        else if (name == "deletion_rcpt")
            response.deletion_rcpt( br->deletion_rcpt() );
        else if (name == "app_acked_rcpt")
            response.app_acked_rcpt( br->app_acked_rcpt() );
        else if (name == "expiration")
            response.expiration( br->expiration() );
        else if (name == "orig_length")
            response.orig_length( br->orig_length() );
        else if (name == "owner")
            response.owner( br->owner() );
        else if (name == "location")
            response.location(
                bundleLocationType(
                    bundleType::location_to_str(br->payload().location())) );
        else if (name == "dest")
            response.dest( br->dest() );
        else if (name == "custodian")
            response.custodian( br->custodian() );
        else if (name == "replyto")
            response.replyto( br->replyto() );
        else if (name == "prevhop")
            response.prevhop( br->prevhop() );
        else if (name == "payload_file") {
            response.payload_file( br->payload().filename() );
        }
    }

    if (event->metadata_blocks_.size() > 0) {

        // We don't want to send duplicate blocks, so keep track of which
        // blocks have been added to the outgoing list with this.
        std::vector<unsigned int> added_blocks;

        // Iterate through the requests.
        MetaBlockRequestVector::iterator requested;
        for (requested = event->metadata_blocks_.begin(); 
             requested != event->metadata_blocks_.end();
             ++requested) {

            for (unsigned int i = 0; i < 2; ++i) {
                MetadataVec * block_vec = NULL;
                if (i == 0) {
                    block_vec = br->mutable_recv_metadata();
                    ASSERT(block_vec != NULL);
                } else if (i == 1) {
                    LinkRef null_link("ExternalRouter::"
                                      "handle_bundle_attributes_report");
                    block_vec = br->generated_metadata().find_blocks(null_link);
                    if (block_vec == NULL) {
                        continue;
                    }
                } else {
                    ASSERT(block_vec != NULL);
                }

                MetadataVec::iterator block_i;
                for (block_i  = block_vec->begin();
                     block_i != block_vec->end();
                     ++block_i) {

                    MetadataBlockRef block = *block_i;

                    // Skip this block if we already added it.
                    bool added = false;
                    for (unsigned int i = 0; i < added_blocks.size(); ++i) {
                        if (added_blocks[i] == block->id()) {
                            added = true;
                            break;
                        }
                    }
                    if (added) {
                        continue;
                    }

                    switch (requested->query_type()) {
                    // Match the block's identifier (index).
                    case MetadataBlockRequest::QueryByIdentifier:
                        if (requested->query_value() != block->id())
                            continue;
                        break;

                    // Match the block's type.
                    case MetadataBlockRequest::QueryByType:
                        if (block->ontology() != requested->query_value())
                            continue;
                        break;

                    // All blocks were requested.
                    case MetadataBlockRequest::QueryAll:
                        break;
                    }

                    // Note that we have added this block.
                    added_blocks.push_back(block->id());

                    // Add the block to the list.
                    response.meta_blocks().push_back(
                        rtrmessage::metadataBlockType(
                            block->id(), false, block->ontology(),
                            xml_schema::base64_binary(
                                    block->metadata(),
                                    block->metadata_len())));
                }
            }
        }
    }

    bundle_attributes_report e(event->query_id_, response);
    SEND(bundle_attributes_report, e)
}

void
ExternalRouter::handle_route_report(RouteReportEvent* event)
{
    oasys::ScopeLock l(route_table_->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    const RouteEntryVec *re = route_table_->route_table();
    RouteEntryVec::const_iterator i = re->begin();
    RouteEntryVec::const_iterator end = re->end();

    route_report report;
    route_report::route_entry::container c;

    for(; i != end; ++i)
        c.push_back(route_report::route_entry::type(*i));

    report.route_entry(c);
    SEND(route_report, report)
}

void
ExternalRouter::send(bpa &message)
{
    xercesc::MemBufFormatTarget buf;
    xml_schema::namespace_infomap map;

    message.eid(BundleDaemon::instance()->local_eid().c_str());

    if (ExternalRouter::client_validation)
        map[""].schema = ExternalRouter::schema.c_str();

    try {
        bpa_(buf, message, map, "UTF-8",
             xml_schema::flags::dont_initialize);
        srv_->eventq->push_back(new std::string((char *)buf.getRawBuffer()));
    }
    catch (xml_schema::serialization &e) {
        const xml_schema::errors &elist = e.errors();
        xml_schema::errors::const_iterator i = elist.begin();
        xml_schema::errors::const_iterator end = elist.end();

        for (; i < end; ++i) {
            std::cout << (*i).message() << std::endl;
        }
    }
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::no_namespace_mapping)
    CATCH(xml_schema::no_prefix_mapping)
    CATCH(xml_schema::xsi_already_in_use)
}

const char *
ExternalRouter::reason_to_str(int reason)
{
    switch(reason) {
        case ContactEvent::NO_INFO:     return "no_info";
        case ContactEvent::USER:        return "user";
        case ContactEvent::SHUTDOWN:    return "shutdown";
        case ContactEvent::BROKEN:      return "broken";
        case ContactEvent::CL_ERROR:    return "cl_error";
        case ContactEvent::CL_VERSION:  return "cl_version";
        case ContactEvent::RECONNECT:   return "reconnect";
        case ContactEvent::IDLE:        return "idle";
        case ContactEvent::TIMEOUT:     return "timeout";
        default: return "";
    }
}

ExternalRouter::ModuleServer::ModuleServer()
    : IOHandlerBase(new oasys::Notifier("/router/external/moduleserver")),
      Thread("/router/external/moduleserver"),
      parser_(new oasys::XercesXMLUnmarshal(
                  ExternalRouter::server_validation,
                  ExternalRouter::schema.c_str())),
      lock_(new oasys::SpinLock())
{
    set_logpath("/router/external/moduleserver");

    // router interface and external routers must be able to bind
    // to the same port
    if (fd() == -1) {
        init_socket();
    }
    const int on = 1;
    if (setsockopt(fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        log_err("ExternalRouter::ModuleServer::ModuleServer():  "
                "Failed to set SO_REUSEADDR:  %s", strerror(errno));
    bind(htonl(INADDR_ALLRTRS_GROUP), ExternalRouter::server_port);

    // join the "all routers" multicast group
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl(INADDR_ALLRTRS_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_LOOPBACK);
    if (setsockopt(fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0)
        log_err("ExternalRouter::ModuleServer::ModuleServer():  "
                "Failed to join multicast group:  %s", strerror(errno));

    // source messages from the loopback interface
    in_addr src_if;
    src_if.s_addr = htonl(INADDR_LOOPBACK);
    if (setsockopt(fd(), IPPROTO_IP, IP_MULTICAST_IF,
                   &src_if, sizeof(src_if)) < 0)
        log_err("ExternalRouter::ModuleServer::ModuleServer():  "
                "Failed to set IP_MULTICAST_IF:  %s", strerror(errno));

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    set_logfd(false);

    eventq = new oasys::MsgQueue< std::string * >(logpath_, lock_);
}

ExternalRouter::ModuleServer::~ModuleServer()
{
    // free all pending events
    std::string *event;
    while (eventq->try_pop(&event))
        delete event;

    delete eventq;
}

// ModuleServer main loop
void
ExternalRouter::ModuleServer::run() 
{
    // block on input from the socket and
    // on input from the bundle event list
    struct pollfd pollfds[2];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = fd();
    sock_poll->events = POLLIN;
    sock_poll->revents = 0;

    while (1) {
        if (should_stop()) return;

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 2, -1,
            get_notifier());

        if (ret == oasys::IOINTR) {
            log_debug("module server interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_debug("module server error");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            std::string *event;
            if (eventq->try_pop(&event)) {
                ASSERT(event != NULL)
                sendto(const_cast< char * >(event->c_str()),
                    event->size(), 0,
                    htonl(INADDR_ALLRTRS_GROUP),
                    ExternalRouter::server_port);
                delete event;
            }
        }

        if (sock_poll->revents & POLLIN) {
            char buf[MAX_UDP_PACKET];
            in_addr_t raddr;
            u_int16_t rport;
            int bytes;

            bytes = recvfrom(buf, MAX_UDP_PACKET, 0, &raddr, &rport);
            buf[bytes] = '\0';

            process_action(buf);
        }
    }
}

// Handle a message from an external router
void
ExternalRouter::ModuleServer::process_action(const char *payload)
{
    // clear any error condition before next parse
    parser_->reset_error();
    
    // parse the xml payload received
    const xercesc::DOMDocument *doc = parser_->doc(payload);
    
    // was the message valid?
    if (parser_->error()) {
        log_debug("received invalid message");
        return;
    }

    std::auto_ptr<bpa> instance;

    try {
        instance = bpa_(*doc);
    }
    CATCH(xml_schema::expected_element)
    CATCH(xml_schema::unexpected_element)
    CATCH(xml_schema::expected_attribute)
    CATCH(xml_schema::unexpected_enumerator)
    CATCH(xml_schema::no_type_info)
    CATCH(xml_schema::not_derived)

    // Check that we have an instance object to work with
    if (instance.get() == 0)
        return;

    // @@@ Need to add:
    //      broadcast_send_bundle_request

    // Examine message contents
    if (instance->send_bundle_request().present()) {
        log_debug("posting BundleSendRequest");
        send_bundle_request& in_request = instance->send_bundle_request().get();

        gbofIdType id = in_request.gbof_id();
        BundleTimestamp local_id;
        local_id.seconds_ = in_request.local_id() >> 32;
        local_id.seqno_ = in_request.local_id() & 0xffffffff;
        std::string link = in_request.link_id();
        int action = convert_fwd_action(in_request.fwd_action());

        GbofId gbof_id;
        gbof_id.source_ = EndpointID( id.source().uri() );
        gbof_id.creation_ts_.seconds_ = id.creation_ts() >> 32;
        gbof_id.creation_ts_.seqno_ = id.creation_ts() & 0xffffffff;
        gbof_id.is_fragment_ = id.is_fragment();
        gbof_id.frag_length_ = id.frag_length();
        gbof_id.frag_offset_ = id.frag_offset();

        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
        BundleRef br = bd->pending_bundles()->find(gbof_id, local_id);
        if (br.object()) {
            BundleSendRequest *request = new BundleSendRequest(br, link, action);
    
            // @@@ need to handle optional params frag_size, frag_offset
            
            LinkRef link_ref = bd->contactmgr()->find_link(link.c_str());
            if (link_ref == NULL) {
                if (in_request.metadata_block().size() > 0) {
                    log_err("link %s does not exist; failed to "
                            "modify/generate metadata for send bundle request",
                            link.c_str());
                }
    
            } else if (in_request.metadata_block().size() > 0) {
    
                typedef ::xsd::cxx::tree::sequence<rtrmessage::metadataBlockType>
                        MetaBlockSequence;
                
                MetadataBlockProcessor* meta_processor = 
                        dynamic_cast<MetadataBlockProcessor*>(
                            BundleProtocol::find_processor(
                            BundleProtocol::METADATA_BLOCK));
                ASSERT(meta_processor != NULL);
                
                oasys::ScopeLock bundle_lock(br->lock(), "ExternalRouter");
                
                MetaBlockSequence::const_iterator block_i;
                for (block_i = in_request.metadata_block().begin();
                     block_i != in_request.metadata_block().end();
                     ++block_i) {
    
                    if (!block_i->generated()) {
    
                        MetadataBlockRef existing("ExternalRouter metadata block search");
                        for (unsigned int i = 0;
                             i < br->recv_metadata().size(); ++i) {
                            if (br->recv_metadata()[i]->id() ==
                                block_i->identifier()) {
                                existing = br->recv_metadata()[i];
                                break;
                            }
                        }
    
                        if (existing != NULL) {
                            // Lock the block so nobody tries to read it while
                            // we are changing it.
                            oasys::ScopeLock metadata_lock(existing->lock(),
                                                           "ExternalRouter");
                        
                            // If the new block size is zero, it is being removed
                            // for this particular link.
                            if (block_i->contents().size() == 0) {
                                existing->remove_outgoing_metadata(link_ref);                        
                                log_info("Removing metadata block %u from bundle "
                                         "%u on link %s", block_i->identifier(),
                                         br->bundleid(), link.c_str());
                            }
                        
                            // Otherwise, if the new block size is non-zero, it
                            // it is being modified for this link.
                            else {
                                log_info("Modifying metadata block %u on bundle "
                                         "%u on link %s", block_i->identifier(),
                                         br->bundleid(), link.c_str());
                                existing->modify_outgoing_metadata(
                                              link_ref,
                                              (u_char*)block_i->contents().data(),
                                              block_i->contents().size());
                            }
                            continue;
                        }
    
                        ASSERT(existing == NULL);
    
                        LinkRef null_link("ExternalRouter::process_action");
                        MetadataVec * nulldata = br->generated_metadata().
                                                         find_blocks(null_link);
                        if (nulldata != NULL) {
                            for (unsigned int i = 0; i < nulldata->size(); ++i) {
                                if ((*nulldata)[i]->id() == block_i->identifier()) {
                                    existing = (*nulldata)[i];
                                    break;
                                }
    	                }
                        }
    
                        if (existing != NULL) {
                            MetadataVec * link_vec = br->generated_metadata().
                                                     find_blocks(link_ref);
                            if (link_vec == NULL) {
                                link_vec = br->mutable_generated_metadata()->
                                           create_blocks(link_ref);
                            }
                            ASSERT(link_vec != NULL);
    
                            ASSERT(existing->ontology() == block_i->type());
    
                            MetadataBlock * meta_block =
                                new MetadataBlock(
                                        existing->id(),
                                        block_i->type(),
                                        (u_char *)block_i->contents().data(),
                                        block_i->contents().size());
                            meta_block->set_flags(existing->flags());
    
    			link_vec->push_back(meta_block);
    
                            log_info("Adding a metadata block to bundle %u on "
                                     "link %s", br->bundleid(), link.c_str());
                            continue;
                        }
    
                        log_err("bundle %u does not have a block %u",
                                br->bundleid(), block_i->identifier());
    
                    } else {
                        ASSERT(block_i->generated());
    
                        MetadataVec* vec = 
                            br->generated_metadata().find_blocks(link_ref);
                        if (vec == NULL)
                            vec = br->mutable_generated_metadata()->create_blocks(link_ref);
                        
                        MetadataBlock* meta_block = new MetadataBlock(
                                block_i->type(),
                                (u_char*)block_i->contents().data(),
                                block_i->contents().size());
                        
                        vec->push_back(meta_block);
                        log_info("Adding an metadata block to bundle %u on "
                                 "link %s", br->bundleid(), link.c_str());
                    }
                 }
            }
            
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to send nonexistent bundle %s",
                     gbof_id.str().c_str());
        }
    }

    if (instance->open_link_request().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkStateChangeRequest");

        std::string lstr =
            instance->open_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::OPENING,
                                           ContactEvent::NO_INFO));
        } else {
            log_warn("attempt to open link %s that doesn't exist!",
                     lstr.c_str());
        }
    }

    if (instance->close_link_request().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkStateChangeRequest");

        std::string lstr =
            instance->close_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::CLOSED,
                                           ContactEvent::NO_INFO));
        } else {
            log_warn("attempt to close link %s that doesn't exist!",
                     lstr.c_str());
        }
    }

    if (instance->add_link_request().present()) {
        log_debug("posting LinkCreateRequest");

        rtrmessage::add_link_request request
            = instance->add_link_request().get();
        std::string clayer = request.clayer();
        ConvergenceLayer *cl = ConvergenceLayer::find_clayer(clayer.c_str());
        if (!cl) {
            log_warn("attempt to create link using non-existent CLA %s",
                     clayer.c_str());
        }
        else {
            std::string name = request.link_id();
            Link::link_type_t type = convert_link_type( request.link_type() );
            std::string eid = request.remote_eid().uri();

            AttributeVector params;

            if (request.link_config_params().present()) {
                linkConfigType::cl_params::container
                    c = request.link_config_params().get().cl_params();
                linkConfigType::cl_params::container::iterator iter;

                for (iter = c.begin(); iter < c.end(); iter++) {
                    if (iter->bool_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->bool_value()));
                    else if (iter->u_int_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->u_int_value()));
                    else if (iter->int_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->int_value()));
                    else if (iter->str_value().present())
                        params.push_back(NamedAttribute(iter->name(), 
                                                        iter->str_value()));
                    else
                        log_warn("unknown value type in key-value pair");
                }
            }

            BundleDaemon::post(
                new LinkCreateRequest(name, type, eid, cl, params));
        }
    }

    if (instance->delete_link_request().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkDeleteRequest");

        std::string lstr = instance->delete_link_request().get().link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
            BundleDaemon::post(new LinkDeleteRequest(link));
        } else {
            log_warn("attempt to delete link %s that doesn't exist!",
                     lstr.c_str());
        }
    }

    if (instance->reconfigure_link_request().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkReconfigureRequest");

        rtrmessage::reconfigure_link_request request
            = instance->reconfigure_link_request().get();
        std::string lstr = request.link_id();
        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        rtrmessage::linkConfigType request_params=request.link_config_params();

        if (link.object() != 0) {
            AttributeVector params;
            if (request_params.is_usable().present()) {
                params.push_back(NamedAttribute("is_usable", request_params.is_usable().get()));
            }
            if (request_params.reactive_frag_enabled().present()) {
                // xlate between router.xsd/clevent.xsd
                params.push_back(NamedAttribute("reactive_fragment", request_params.reactive_frag_enabled().get()));
            }
            if (request_params.nexthop().present()) {
                params.push_back(NamedAttribute("nexthop", request_params.nexthop().get()));
            }
            // Following are DTN2 parameters not listed in the DP interface
            if (request_params.min_retry_interval().present()) {
                params.push_back(
                    NamedAttribute("min_retry_interval",
                                   request_params.min_retry_interval().get()));
            }
            if (request_params.max_retry_interval().present()) {
                params.push_back(
                    NamedAttribute("max_retry_interval",
                                   request_params.max_retry_interval().get()));
            }
            if (request_params.idle_close_time().present()) {
                params.push_back(
                    NamedAttribute("idle_close_time",
                                   request_params.idle_close_time().get()));
            }

            linkConfigType::cl_params::container
                c = request_params.cl_params();
            linkConfigType::cl_params::container::iterator iter;

            for (iter = c.begin(); iter < c.end(); iter++) {
              if (iter->bool_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->bool_value()));
              else if (iter->u_int_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->u_int_value()));
              else if (iter->int_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->int_value()));
              else if (iter->str_value().present())
                params.push_back(NamedAttribute(iter->name(), 
                                                iter->str_value()));
              else
                log_warn("unknown value type in key-value pair");
            }

            BundleDaemon::post(new LinkReconfigureRequest(link, params));
        } else {
            log_warn("attempt to reconfigure link %s that doesn't exist!",
                     lstr.c_str());
        }
    }

    if (instance->inject_bundle_request().present()) {
        log_debug("posting BundleInjectRequest");

        inject_bundle_request fields = instance->inject_bundle_request().get();

        //XXX Where did the other BundleInjectRequest constructor go?
        BundleInjectRequest *request = new BundleInjectRequest();

        request->src_ = fields.source().uri();
        request->dest_ = fields.dest().uri();
        request->link_ = fields.link_id();
        request->request_id_ = fields.request_id();
        
        request->payload_file_ = fields.payload_file();

        if(fields.replyto().present())
            request->replyto_ = fields.replyto().get().uri();
        else
            request->replyto_ = "";

        if(fields.custodian().present())
            request->custodian_ = fields.custodian().get().uri();
        else
            request->custodian_ = "";

        if(fields.priority().present())
            request->priority_ = convert_priority( fields.priority().get() );
        else
            request->priority_ = Bundle::COS_BULK; // default

        if(fields.expiration().present())
            request->expiration_ = fields.expiration().get();
        else
            request->expiration_ = 0; // default will be used

        BundleDaemon::post(request);
    }

    if (instance->cancel_bundle_request().present()) {
        log_debug("posting BundleCancelRequest");

        gbofIdType id =
            instance->cancel_bundle_request().get().gbof_id();
        BundleTimestamp local_id;
        local_id.seconds_ =
            instance->cancel_bundle_request().get().local_id() >> 32;
        local_id.seqno_ =
            instance->cancel_bundle_request().get().local_id() & 0xffffffff;
        std::string link =
            instance->cancel_bundle_request().get().link_id();

        GbofId gbof_id;
        gbof_id.source_ = EndpointID( id.source().uri() );
        gbof_id.creation_ts_.seconds_ = id.creation_ts() >> 32;
        gbof_id.creation_ts_.seqno_ = id.creation_ts() & 0xffffffff;
        gbof_id.is_fragment_ = id.is_fragment();
        gbof_id.frag_length_ = id.frag_length();
        gbof_id.frag_offset_ = id.frag_offset();

        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
        BundleRef br = bd->pending_bundles()->find(gbof_id, local_id);
        if (br.object()) {
            BundleCancelRequest *request = new BundleCancelRequest(br, link);
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to cancel send of nonexistent bundle %s",
                     gbof_id.str().c_str());
        }
    }

    if (instance->delete_bundle_request().present()) {
        log_debug("posting BundleDeleteRequest");

        gbofIdType id =
            instance->delete_bundle_request().get().gbof_id();

        GbofId gbof_id;
        gbof_id.source_ = EndpointID( id.source().uri() );
        gbof_id.creation_ts_.seconds_ = id.creation_ts() >> 32;
        gbof_id.creation_ts_.seqno_ = id.creation_ts() & 0xffffffff;
        gbof_id.is_fragment_ = id.is_fragment();
        gbof_id.frag_length_ = id.frag_length();
        gbof_id.frag_offset_ = id.frag_offset();
        BundleTimestamp local_id;
        local_id.seconds_ =
            instance->delete_bundle_request().get().local_id() >> 32;
        local_id.seqno_ =
            instance->delete_bundle_request().get().local_id() & 0xffffffff;

        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
        BundleRef br = bd->pending_bundles()->find(gbof_id, local_id);
        if (br.object()) {
            BundleDeleteRequest *request =
                new BundleDeleteRequest(br,
                                        BundleProtocol::REASON_NO_ADDTL_INFO);
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to delete nonexistent bundle %s",
                     gbof_id.str().c_str());
        }
    }

    if (instance->set_cl_params_request().present()) {
        log_debug("posting CLASetParamsRequest");

        std::string clayer = instance->set_cl_params_request().get().clayer();
        ConvergenceLayer *cl = ConvergenceLayer::find_clayer(clayer.c_str());
        if (!cl) {
            log_warn("attempt to set parameters for non-existent CLA %s",
                     clayer.c_str());
        }
        else {
            AttributeVector params;

            set_cl_params_request::cl_params::container
                c = instance->set_cl_params_request().get().cl_params();
            set_cl_params_request::cl_params::container::iterator iter;

            for (iter = c.begin(); iter < c.end(); iter++) {
                if (iter->bool_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->bool_value()));
                else if (iter->u_int_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->u_int_value()));
                else if (iter->int_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->int_value()));
                else if (iter->str_value().present())
                    params.push_back(NamedAttribute(iter->name(), 
                                                    iter->str_value()));
                else
                    log_warn("unknown value type in key-value pair");
            }

            BundleDaemon::post(new CLASetParamsRequest(cl, params));
        }
    }

    if (instance->bundle_attributes_query().present()) {
        log_debug("posting BundleAttributesQueryRequest");
        bundle_attributes_query query =
            instance->bundle_attributes_query().get();
        std::string query_id = query.query_id();
        gbofIdType id = query.gbof_id();

        GbofId gbof_id;
        gbof_id.source_ = EndpointID( id.source().uri() );
        gbof_id.creation_ts_.seconds_ = id.creation_ts() >> 32;
        gbof_id.creation_ts_.seqno_ = id.creation_ts() & 0xffffffff;
        gbof_id.is_fragment_ = id.is_fragment();
        gbof_id.frag_length_ = id.frag_length();
        gbof_id.frag_offset_ = id.frag_offset();
        BundleTimestamp local_id;
        local_id.seconds_ = query.local_id() >> 32;
        local_id.seqno_ = query.local_id() & 0xffffffff;

        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
        BundleRef br = bd->pending_bundles()->find(gbof_id, local_id);

        // XXX note, if we want to send a report even when the bundle does
        // not exist (instead of ignoring the request), we have to not test
        // for the existence of the object here (it is tested again in
        // BundleDaemon::handle_bundle_attributes_query)
        if (br.object()) {
            AttributeNameVector attribute_names;
            MetaBlockRequestVector metadata_blocks;
            bundle_attributes_query::query_params::container
                c = query.query_params();
            bundle_attributes_query::query_params::container::iterator iter;
    
            for (iter = c.begin(); iter != c.end(); iter++) {
                bundleAttributesQueryType& q = *iter;
    
                if (q.query().present()) {
                    attribute_names.push_back( AttributeName(q.query().get()) );
                }
                if (q.meta_blocks().present()) {
                    bundleMetaBlockQueryType& block = q.meta_blocks().get();
                    int query_value = -1;
                    MetadataBlockRequest::QueryType query_type;
                    
                    if (block.identifier().present()) {
                        query_value = block.identifier().get();
                        query_type = MetadataBlockRequest::QueryByIdentifier;
                    }
                    
                    else if (block.type().present()) {
                        query_value = block.type().get();
                        query_type = MetadataBlockRequest::QueryByType;
                    }
                    
                    if (query_value < 0)
                        query_type = MetadataBlockRequest::QueryAll;
                    
                    metadata_blocks.push_back(
                        MetadataBlockRequest(query_type, query_value) );
                }
            }
    
            BundleAttributesQueryRequest* request
                = new BundleAttributesQueryRequest(query_id, br, attribute_names);
    
            if (metadata_blocks.size() > 0) {
                request->metadata_blocks_ = metadata_blocks;
            }
    
            BundleDaemon::post(request);
        }
        else {
            log_warn("attempt to query nonexistent bundle %s",
                     gbof_id.str().c_str());
        }
    }

    if (instance->link_query().present()) {
        log_debug("posting LinkQueryRequest");
        BundleDaemon::post(new LinkQueryRequest());
    }

    if (instance->link_attributes_query().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkAttributesQueryRequest");

        link_attributes_query query = instance->link_attributes_query().get();
        std::string query_id = query.query_id();
        std::string lstr = query.link_id();

        LinkRef link = bd->contactmgr()->find_link(lstr.c_str());

        if (link.object() != 0) {
           AttributeNameVector attribute_names;

           link_attributes_query::query_params::container 
             c = query.query_params();
           link_attributes_query::query_params::container::iterator iter;

           for (iter = c.begin(); iter < c.end(); iter++) {
             attribute_names.push_back( AttributeName(*iter) );
           }

          BundleDaemon::post(new LinkAttributesQueryRequest(query_id, 
                                                            link, 
                                                            attribute_names));
        } else {
            log_warn("attempt to query attributes of link %s that doesn't exist!",
                     lstr.c_str());
        }
    }

    if (instance->bundle_query().present()) {
        log_debug("posting BundleQueryRequest");
        BundleDaemon::post(new BundleQueryRequest());
    }

    if (instance->contact_query().present()) {
        log_debug("posting ContactQueryRequest");
        BundleDaemon::post(new ContactQueryRequest());
    }

    if (instance->route_query().present()) {
        log_debug("posting RouteQueryRequest");
        BundleDaemon::post(new RouteQueryRequest());
    }

    /* This is needed for delivering to LB applications */
    if (instance->deliver_bundle_to_app_request().present()) {
        log_debug("posting DeliverBundleToAppRequest");
        deliver_bundle_to_app_request& in_request = 
            instance->deliver_bundle_to_app_request().get();

        eidType reg = in_request.endpoint();
        EndpointID reg_eid;
        reg_eid.assign(reg.uri());

        gbofIdType id = in_request.gbof_id();
        GbofId gbof_id;
        gbof_id.source_ = EndpointID( id.source().uri() );
        gbof_id.creation_ts_.seconds_ = id.creation_ts() >> 32;
        gbof_id.creation_ts_.seqno_ = id.creation_ts() & 0xffffffff;
        gbof_id.is_fragment_ = id.is_fragment();
        gbof_id.frag_length_ = id.frag_length();
        gbof_id.frag_offset_ = id.frag_offset();
        BundleTimestamp local_id;
        local_id.seconds_ = in_request.local_id() >> 32;
        local_id.seqno_ = in_request.local_id() & 0xffffffff;

        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("pending_bundles size %zu", bd->pending_bundles()->size());
        BundleRef br = bd->pending_bundles()->find(gbof_id, local_id);

        if (br.object()) {
            bd->check_and_deliver_to_registrations(br.object(), reg_eid);
        }
        else {
            log_warn("attempt to deliver nonexistent bundle %s to app %s",
                     gbof_id.str().c_str(), reg_eid.c_str());
        }
    }
}

Link::link_type_t
ExternalRouter::ModuleServer::convert_link_type(rtrmessage::linkTypeType type)
{
    if(type==linkTypeType(linkTypeType::alwayson)){
        return Link::ALWAYSON;
    }
    else if(type==linkTypeType(linkTypeType::ondemand)){
        return Link::ONDEMAND;
    }
    else if(type==linkTypeType(linkTypeType::scheduled)){
        return Link::SCHEDULED;
    }
    else if(type==linkTypeType(linkTypeType::opportunistic)){
        return Link::OPPORTUNISTIC;
    }

    return Link::LINK_INVALID;
}

ForwardingInfo::action_t
ExternalRouter::ModuleServer::convert_fwd_action(rtrmessage::bundleForwardActionType action)
{
    if(action==bundleForwardActionType(bundleForwardActionType::forward)){
        return ForwardingInfo::FORWARD_ACTION;
    }
    else if(action== bundleForwardActionType(bundleForwardActionType::copy)){
        return ForwardingInfo::COPY_ACTION;
    }

    return ForwardingInfo::INVALID_ACTION;
}

Bundle::priority_values_t
ExternalRouter::ModuleServer::convert_priority(rtrmessage::bundlePriorityType priority)
{
    if(priority == bundlePriorityType(bundlePriorityType::bulk)){
        return Bundle::COS_BULK;
    }
    else if(priority == bundlePriorityType(bundlePriorityType::normal)){
        return Bundle::COS_NORMAL;
    }
    else if(priority == bundlePriorityType(bundlePriorityType::expedited)){
        return Bundle::COS_EXPEDITED;
    }

    return Bundle::COS_INVALID;
}

ExternalRouter::HelloTimer::HelloTimer(ExternalRouter *router)
    : router_(router)
{
}

ExternalRouter::HelloTimer::~HelloTimer()
{
    cancel();
}

// Timeout callback for the hello timer
void
ExternalRouter::HelloTimer::timeout(const struct timeval &)
{
    bpa message;
    message.hello_interval(ExternalRouter::hello_interval);
    router_->send(message);
    schedule_in(ExternalRouter::hello_interval * 1000);
}

ExternalRouter::ERRegistration::ERRegistration(ExternalRouter *router)
    : Registration(Registration::EXTERNALROUTER_REGID,
                    EndpointID(BundleDaemon::instance()->local_eid().str() +
                        EXTERNAL_ROUTER_SERVICE_TAG),
                   Registration::DEFER, 0, 0),
      router_(router)
{
    logpathf("/reg/admin");
    
    BundleDaemon::post(new RegistrationAddedEvent(this, EVENTSRC_ADMIN));
}

// deliver a bundle to external routers
void
ExternalRouter::ERRegistration::deliver_bundle(Bundle *bundle)
{
    bundle_delivery_event e(bundle, bundle,
                            bundle_ts_to_long(bundle->extended_id()));

    e.bundle().payload_file( bundle->payload().filename() );

    bpa message;
    message.bundle_delivery_event(e);
    router_->send(message);

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

// Global shutdown callback function
void external_rtr_shutdown(void *)
{
    BundleDaemon::instance()->router()->shutdown();
}

// Initialize ExternalRouter parameters
u_int16_t ExternalRouter::server_port       = 8001;
u_int16_t ExternalRouter::hello_interval    = 30;
std::string ExternalRouter::schema          = INSTALL_SYSCONFDIR "/router.xsd";
bool ExternalRouter::server_validation      = true;
bool ExternalRouter::client_validation      = false;

} // namespace dtn
#endif // XERCES_C_ENABLED && EXTERNAL_DP_ENABLED
