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

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <iostream>
#include <map>
#include <sys/ioctl.h>
#include <tcl.h>
#include <string.h>
#include <netinet/in.h>

#include "ExternalRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "contacts/ContactManager.h"
#include "reg/RegistrationTable.h"

#include <oasys/io/UDPClient.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/io/IO.h>

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

    if (ExternalRouter::client_validation) {
        oasys::StringBuffer alert_with_ns(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<dtn eid=\"%s\" hello_interval=\"%i\" alert=\"justBooted\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            "xsi:noNamespaceSchemaLocation=\"file://%s\"/>",
            BundleDaemon::instance()->local_eid().c_str(),
            ExternalRouter::hello_interval,
            ExternalRouter::schema.c_str());

        srv_->eventq->push_back(
            new std::string(alert_with_ns.c_str()));
    } else {
        oasys::StringBuffer alert(
            "<dtn eid=\"%s\" hello_interval=\"%i\" alert=\"justBooted\"/>",
            BundleDaemon::instance()->local_eid().c_str(),
            ExternalRouter::hello_interval);

        srv_->eventq->push_back(
            new std::string(alert.c_str()));
    }

    hello_->schedule_in(ExternalRouter::hello_interval * 1000);
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

// Builds a static route XML report
void
ExternalRouter::build_route_report(oasys::SerializeAction *a)
{
    oasys::ScopeLock l(route_table_->lock(),
        "ExternalRouter::build_route_report");

    const RouteEntryVec *re = route_table_->route_table();
    RouteEntryVec::const_iterator i;

    a->begin_action();

    for(i = re->begin(); i != re->end(); ++i) {
        a->process("route_entry", *i);
    }

    a->end_action();
}

// Serialize events and UDP multicast to external routers
void
ExternalRouter::handle_event(BundleEvent *event)
{
    static oasys::StringBuffer front_matter(
        "<dtn eid=\"%s\">",
        BundleDaemon::instance()->local_eid().c_str());

    static oasys::StringBuffer front_matter_with_ns(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><dtn eid=\"%s\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xsi:noNamespaceSchemaLocation=\"file://%s\">",
        BundleDaemon::instance()->local_eid().c_str(),
        ExternalRouter::schema.c_str());

    // Preprocess events based upon type
    if ((event->type_ == BUNDLE_DELIVERED) ||
        (event->type_ == LINK_CREATE) ||
        (event->type_ == REASSEMBLY_COMPLETED)) {
        // Filter out events not supported by the external router interface
        return;
    } else if (event->type_ == BUNDLE_RECEIVED) {
        // Filter out notification of bundles already
        // delivered to registrations
        BundleReceivedEvent *bre = dynamic_cast< BundleReceivedEvent * >(event);
        if (bre->bundleref_->owner_ == "daemon") return;
    } else if (event->type_ == ROUTE_ADD) {
        // Add this route to our static routing table
        RouteAddEvent *rae = dynamic_cast< RouteAddEvent * >(event);
        route_table_->add_entry(rae->entry_);
    } else if (event->type_ == ROUTE_DEL) {
        // Delete this route from our static routing table
        RouteDelEvent *rae = dynamic_cast< RouteDelEvent * >(event);
        route_table_->del_entries(rae->dest_);
    } else if (event->type_ == BUNDLE_TRANSMITTED) {
        // Do not pass transmit events for deleted contacts
        BundleTransmittedEvent *event = dynamic_cast< BundleTransmittedEvent * >(event);
        if(event->contact_ == NULL)
	{
	    return;
	}
    } else if (event->type_ == BUNDLE_TRANSMIT_FAILED) {
        // Do not pass transmit failed events for deleted contacts
        BundleTransmitFailedEvent *event = dynamic_cast< BundleTransmitFailedEvent * >(event);
        if(event->contact_ == NULL)
	{
	    return;
	}
    }

    // serialize the event
    oasys::StringBuffer event_buf;

    if (ExternalRouter::client_validation)
        event_buf.append(front_matter_with_ns.c_str());
    else
        event_buf.append(front_matter.c_str());

    oasys::XMLMarshal event_a(event_buf.expandable_buf(),
        event_to_str(event->type_, true));

    if (event->type_ == ROUTE_REPORT)
        // handle route report serialization locally
        build_route_report(&event_a);
    else
        event_a.action(event);

    event_buf.append("</dtn>");

    // give the serialized event to the ModuleServer thread
    log_debug("ExternalRouter::handle_event (%s) sending:\n %s\n",
        event_to_str(event->type_), event_buf.c_str());
    srv_->eventq->push_back(new std::string(event_buf.c_str()));
}

void
ExternalRouter::shutdown()
{
    // Send an alert that we're going away
    if (ExternalRouter::client_validation) {
        oasys::StringBuffer alert_with_ns(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<dtn eid=\"%s\" alert=\"shuttingDown\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            "xsi:noNamespaceSchemaLocation=\"file://%s\"/>",
            BundleDaemon::instance()->local_eid().c_str(),
            ExternalRouter::schema.c_str());

        srv_->eventq->push_back(
            new std::string(alert_with_ns.c_str()));
    } else {
        oasys::StringBuffer alert(
            "<dtn eid=\"%s\" alert=\"shuttingDown\"/>",
            BundleDaemon::instance()->local_eid().c_str());

        srv_->eventq->push_back(
            new std::string(alert.c_str()));
    }
}

ExternalRouter::ModuleServer::ModuleServer()
    : IOHandlerBase(new oasys::Notifier("/router/external/moduleserver")),
      Thread("/router/external/moduleserver"),
      parser_(ExternalRouter::server_validation,
              ExternalRouter::schema.c_str()),
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
    parser_.reset_error();

    // parse the xml payload received
    const char *event_tag = parser_.parse(payload);

    // was the message valid?
    if (parser_.error()) {
        log_debug("received invalid message");
        return;
    }

    // traverse the XML document, instantiating events
    do {
        BundleEvent *action = instantiate(event_tag);
    
        if (! action) continue;

        // unmarshal the action
        parser_.action(action);
    
        // place the action on the global event queue
        BundleDaemon::post(action);
    } while ((event_tag = parser_.parse()) != 0);
}

// Instantiate BundleEvents recognized by the
// external router interface
BundleEvent *
ExternalRouter::ModuleServer::instantiate(const char *event_tag) {
    std::string action(event_tag);

    if (action.find("send_bundle_request") != std::string::npos)
        return new BundleSendRequest();
    else if (action.find("cancel_bundle_request") != std::string::npos)
        return new BundleCancelRequest();
    else if (action.find("inject_bundle_request") != std::string::npos)
        return new BundleInjectRequest();
    else if (action.find("open_link_request") != std::string::npos)
        return new LinkStateChangeRequest(oasys::Builder::builder(),
                                          Link::OPENING, ContactEvent::NO_INFO);
    else if (action.find("create_link_request") != std::string::npos)
        return new LinkCreateRequest();
    else if (action.find("link_query") != std::string::npos)
        return new LinkQueryRequest();
    else if (action.find("contact_query") != std::string::npos)
        return new ContactQueryRequest();
    else if (action.find("bundle_query") != std::string::npos)
        return new BundleQueryRequest();
    else if (action.find("route_query") != std::string::npos)
        return new RouteQueryRequest();

    return 0;
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
    static oasys::StringBuffer hello(
        "<dtn eid=\"%s\" hello_interval=\"%i\"/>",
        BundleDaemon::instance()->local_eid().c_str(),
        ExternalRouter::hello_interval);

    static oasys::StringBuffer hello_with_ns(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<dtn eid=\"%s\" hello_interval=\"%i\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xsi:noNamespaceSchemaLocation=\"file://%s\"/>",
        BundleDaemon::instance()->local_eid().c_str(),
        ExternalRouter::hello_interval,
        ExternalRouter::schema.c_str());

    // throw the hello message onto the ModuleServer queue
    if (ExternalRouter::client_validation)
        router_->srv_->eventq->push_back(new std::string(hello_with_ns.c_str()));
    else
        router_->srv_->eventq->push_back(new std::string(hello.c_str()));

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
    BundleDeliveryEvent *delivery =
        new BundleDeliveryEvent(bundle, EVENTSRC_PEER);

    router_->handle_event(delivery);
    delete delivery;

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

// Global shutdown callback function
void external_rtr_shutdown(void *)
{
    BundleDaemon::instance()->router()->shutdown();
}

// Initialize ExternalRouter parameters
u_int16_t ExternalRouter::server_port = 8001;
u_int16_t ExternalRouter::hello_interval = 30;
std::string ExternalRouter::schema = "/etc/dtn.router.xsd";
bool ExternalRouter::server_validation = true;
bool ExternalRouter::client_validation = false;

} // namespace dtn
#endif // XERCES_C_ENABLED
