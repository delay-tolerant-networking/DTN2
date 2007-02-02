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

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <memory>
#include <iostream>
#include <map>
#include <sys/ioctl.h>
#include <string.h>
#include <netinet/in.h>
#include <sstream>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "ExternalRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "contacts/ContactManager.h"
#include "reg/RegistrationTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include <oasys/io/UDPClient.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/io/IO.h>

#define SEND(event, data) \
    rtrmessage::dtn message; \
    message.event(data); \
    send(message);

#define CATCH(exception) \
    catch (exception &e) { log_warn(e.what()); }

namespace dtn {

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

    rtrmessage::dtn message;
    message.alert(rtrmessage::dtnStatusType(std::string("justBooted")));
    message.hello_interval(ExternalRouter::hello_interval);
    send(message);
    hello_->schedule_in(ExternalRouter::hello_interval * 1000);
}

void
ExternalRouter::shutdown()
{
    rtrmessage::dtnStatusType e(std::string("shuttingDown"));
    SEND(alert, e)
}

// Format the given StringBuffer with static routing info
void
ExternalRouter::get_routing_state(oasys::StringBuffer* buf)
{
    EndpointIDVector long_eids;
    buf->appendf("Static route table for %s router(s):\n", name_.c_str());
    route_table_->dump(buf, &long_eids);

    if (long_eids.size() > 0) {
        buf->appendf("\nLong Endpoint IDs referenced above:\n");
        for (u_int i = 0; i < long_eids.size(); ++i) {
            buf->appendf("\t[%d]: %s\n", i, long_eids[i].c_str());
        }
        buf->appendf("\n");
    }
    
    buf->append("\nClass of Service (COS) bits:\n"
                "\tB: Bulk  N: Normal  E: Expedited\n\n");
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
    // filter out bundles already delivered
    if (event->bundleref_->owner_ == "daemon") return;

    rtrmessage::dtn::bundle_received_event::type e(
        event->bundleref_.object(),
        (xml_schema::string)source_to_str((event_source_t)event->source_),
        event->bytes_received_);
    SEND(bundle_received_event, e)
}

void
ExternalRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    if (event->contact_ == NULL) return;

    rtrmessage::dtn::bundle_transmitted_event::type e(
        event->bundleref_.object(),
        event->contact_.object(),
        event->link_.object(),
        event->bytes_sent_,
        event->reliably_sent_);
    SEND(bundle_transmitted_event, e)
}

void
ExternalRouter::handle_bundle_transmit_failed(BundleTransmitFailedEvent* event)
{
    if (event->contact_ == NULL) return;

    rtrmessage::dtn::bundle_transmit_failed_event::type e(
        event->bundleref_.object(),
        event->contact_.object(),
        event->link_.object());
    SEND(bundle_transmit_failed_event, e)
}

void
ExternalRouter::handle_bundle_expired(BundleExpiredEvent* event)
{
    rtrmessage::dtn::bundle_expired_event::type e(
        event->bundleref_.object());
    SEND(bundle_expired_event, e)
}

void
ExternalRouter::handle_contact_up(ContactUpEvent* event)
{
    rtrmessage::dtn::contact_up_event::type e(
        event->contact_.object());
    SEND(contact_up_event, e)
}

void
ExternalRouter::handle_contact_down(ContactDownEvent* event)
{
    rtrmessage::dtn::contact_down_event::type e(
        event->contact_.object(),
        rtrmessage::contactReasonType(reason_to_str(event->reason_)));
    SEND(contact_down_event, e)
}

void
ExternalRouter::handle_link_created(LinkCreatedEvent *event)
{
    rtrmessage::dtn::link_created_event::type e(
        event->link_.object(),
        rtrmessage::contactReasonType(reason_to_str(event->reason_)));
    SEND(link_created_event, e)
}

void
ExternalRouter::handle_link_deleted(LinkDeletedEvent *event)
{
    rtrmessage::dtn::link_deleted_event::type e(
        event->link_.object(),
        rtrmessage::contactReasonType(reason_to_str(event->reason_)));
    SEND(link_deleted_event, e)
}

void
ExternalRouter::handle_link_available(LinkAvailableEvent *event)
{
    rtrmessage::dtn::link_available_event::type e(
        event->link_.object(),
        rtrmessage::contactReasonType(reason_to_str(event->reason_)));
    SEND(link_available_event, e)
}

void
ExternalRouter::handle_link_unavailable(LinkUnavailableEvent *event)
{
    rtrmessage::dtn::link_unavailable_event::type e(
        event->link_.object(),
        rtrmessage::contactReasonType(reason_to_str(event->reason_)));
    SEND(link_unavailable_event, e)
}

void
ExternalRouter::handle_link_busy(LinkBusyEvent *event)
{
    rtrmessage::dtn::link_busy_event::type e(
        event->link_.object());
    SEND(link_busy_event, e)
}

void
ExternalRouter::handle_registration_added(RegistrationAddedEvent* event)
{
    rtrmessage::dtn::registration_added_event::type e(
        event->registration_,
        (xml_schema::string)source_to_str((event_source_t)event->source_));
    SEND(registration_added_event, e)
}

void
ExternalRouter::handle_registration_removed(RegistrationRemovedEvent* event)
{
    rtrmessage::dtn::registration_removed_event::type e(
        event->registration_);
    SEND(registration_removed_event, e)
}

void
ExternalRouter::handle_registration_expired(RegistrationExpiredEvent* event)
{
    rtrmessage::dtn::registration_expired_event::type e(
        event->regid_);
    SEND(registration_expired_event, e)
}

void
ExternalRouter::handle_route_add(RouteAddEvent* event)
{
    // update our own static route table first
    route_table_->add_entry(event->entry_);

    rtrmessage::dtn::route_add_event::type e(
        event->entry_);
    SEND(route_add_event, e)
}

void
ExternalRouter::handle_route_del(RouteDelEvent* event)
{
    // update our own static route table first
    route_table_->del_entries(event->dest_);

    rtrmessage::dtn::route_delete_event::type e(
        rtrmessage::eidType(event->dest_.str()));
    SEND(route_delete_event, e)
}

void
ExternalRouter::handle_custody_signal(CustodySignalEvent* event)
{
    rtrmessage::dtn::custody_signal_event::type e(
        rtrmessage::eidType(event->data_.orig_source_eid_.str()),
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
    SEND(custody_signal_event, e)
}

void
ExternalRouter::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    rtrmessage::dtn::custody_timeout_event::type e(
        event->bundle_.object(),
        event->link_.object());
    SEND(custody_timeout_event, e)
}

void
ExternalRouter::handle_link_report(LinkReportEvent *event)
{
    typedef rtrmessage::link_report link_report;

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
ExternalRouter::handle_contact_report(ContactReportEvent* event)
{
    typedef rtrmessage::contact_report contact_report;

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
    typedef rtrmessage::bundle_report bundle_report;

    BundleDaemon *bd = BundleDaemon::instance();
    oasys::ScopeLock l(bd->pending_bundles()->lock(),
        "ExternalRouter::handle_event");

    (void) event;

    const BundleList *bundles = bd->pending_bundles();
    BundleList::const_iterator i = bundles->begin();
    BundleList::const_iterator end = bundles->end();

    bundle_report report;
    bundle_report::bundle::container c;

    for(; i != end; ++i)
        c.push_back(bundle_report::bundle::type(*i));

    report.bundle(c);
    SEND(bundle_report, report)
}

void
ExternalRouter::handle_route_report(RouteReportEvent* event)
{
    typedef rtrmessage::route_report route_report;

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
ExternalRouter::send(rtrmessage::dtn &message)
{
    xercesc::MemBufFormatTarget buf;
    xml_schema::namespace_infomap map;

    message.eid(BundleDaemon::instance()->local_eid().c_str());

    if (ExternalRouter::client_validation)
        map[""].schema = ExternalRouter::schema.c_str();

    try {
        dtn_(buf, message, map, "UTF-8",
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
        case ContactEvent::UNBLOCKED:   return "unblocked";
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
    // router interface and external routers must be able to bind
    // to the same port
    const int on = 1;
    setsockopt(fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bind(htonl(INADDR_ANY), ExternalRouter::server_port);

    // join the "all routers" multicast group
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl(INADDR_ALLRTRS_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    // source messages from the loopback interface
    in_addr src_if;
    src_if.s_addr = htonl(INADDR_LOOPBACK);
    setsockopt(fd(), IPPROTO_IP, IP_MULTICAST_IF, &src_if, sizeof(src_if));

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

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = fd();
    sock_poll->events = POLLIN;

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

    std::auto_ptr<rtrmessage::dtn> instance;

    try {
        instance = rtrmessage::dtn_(*doc);
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

    // Examine message contents
    if (instance->send_bundle_request().present()) {
        log_debug("posting BundleSendRequest");

        u_int32_t bundleid =
            instance->send_bundle_request().get().bundleid();
        std::string link =
            instance->send_bundle_request().get().link();
        int action =
            instance->send_bundle_request().get().fwd_action();

        //XXX Where did the other BundleSendRequest consructor go?
        BundleSendRequest *request = new BundleSendRequest();
        request->bundleid_ = bundleid;
        request->link_ = link;
        request->action_ = action;
        BundleDaemon::post(request);
    }

    if (instance->open_link_request().present()) {
        BundleDaemon *bd = BundleDaemon::instance();
        log_debug("posting LinkStateChangeRequest");

        std::string lstr =
            instance->open_link_request().get().link();
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

    if (instance->inject_bundle_request().present()) {
        log_debug("posting BundleInjectRequest");

        std::string src =
            instance->inject_bundle_request().get().source();
        std::string dest =
            instance->inject_bundle_request().get().dest();
        std::string link =
            instance->inject_bundle_request().get().link();
        xml_schema::base64_binary payload =
            instance->inject_bundle_request().get().payload();

        //XXX Where did the other BundleInjectRequest constructor go?
        BundleInjectRequest *request = new BundleInjectRequest();
        request->src_ = src;
        request->dest_ = dest;
        request->link_ = link;
        request->payload_.assign(payload.data());
        BundleDaemon::post(request);
    }

    if (instance->cancel_bundle_request().present()) {
        log_debug("posting BundleCancelRequest");

        u_int32_t bundleid =
            instance->cancel_bundle_request().get().bundleid();
        std::string link =
            instance->cancel_bundle_request().get().link();

        BundleCancelRequest *request = new BundleCancelRequest();
        request->bundleid_ = bundleid;
        request->link_.assign(link);
        BundleDaemon::post(request);
    }

    if (instance->link_query().present()) {
        log_debug("posting LinkQueryRequest");
        BundleDaemon::post(new LinkQueryRequest());
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
    rtrmessage::dtn message;
    message.hello_interval(ExternalRouter::hello_interval);
    router_->send(message);
    schedule_in(ExternalRouter::hello_interval * 1000);
}

ExternalRouter::ERRegistration::ERRegistration(ExternalRouter *router)
    : Registration(Registration::EXTERNALROUTER_REGID,
                    EndpointID(BundleDaemon::instance()->local_eid().str() +
                        EXTERNAL_ROUTER_SERVICE_TAG),
                    Registration::DEFER, 0),
      router_(router)
{
    logpathf("/reg/admin");

    BundleDaemon::post(new RegistrationAddedEvent(this, EVENTSRC_ADMIN));
}

// deliver a bundle to external routers
void
ExternalRouter::ERRegistration::deliver_bundle(Bundle *bundle)
{
    rtrmessage::bundle_delivery_event e(
        bundle, (xml_schema::string)source_to_str(EVENTSRC_PEER));

    // add the payload
    int len = bundle->payload_.length();
    u_char buf[len];
    bundle->payload_.read_data(0, len, buf); 
    e.bundle().payload(xml_schema::base64_binary(buf, len));

    rtrmessage::dtn message;
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
u_int16_t ExternalRouter::hello_interval    =  30;
std::string ExternalRouter::schema          = "/etc/router.xsd";
bool ExternalRouter::server_validation      = true;
bool ExternalRouter::client_validation      = false;

} // namespace dtn
#endif // XERCES_C_ENABLED
