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

#include <sys/poll.h>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "UDPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

struct UDPConvergenceLayer::Params UDPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
}

//----------------------------------------------------------------------
UDPConvergenceLayer::UDPConvergenceLayer()
    : IPConvergenceLayer("UDPConvergenceLayer", "udp")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = UDPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.bucket_depth_             = 0; // default
}

//----------------------------------------------------------------------
CLInfo*
UDPConvergenceLayer::new_link_params()
{
    return new UDPConvergenceLayer::Params(UDPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&UDPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::UIntOpt("rate", &params->rate_));
    p.addopt(new oasys::UIntOpt("bucket_depth_", &params->bucket_depth_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
};

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = UDPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }
    
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params);
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
    
    // start the thread which automatically listens for data
    receiver->start();
    
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt_from_io();
    
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;
    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Receiver*)iface->cl_info())->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected\n");
    }
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &addr, &port);

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);
    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(params);
    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("UDPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
    buf->appendf("rate: %u\n", params->rate_);
    buf->appendf("bucket_depth: %u\n", params->bucket_depth_);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    log_debug("UDPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());
    
    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("can't lookup hostname in next hop address '%s'",
                link->nexthop());
        return false;
    }

    // if the port wasn't specified, use the default
    if (port == 0) {
        port = UDPCL_DEFAULT_PORT;
    }

    Params* params = (Params*)link->cl_info();
    
    // create a new sender structure
    Sender* sender = new Sender(link->contact());

    if (!sender->init(params, addr, port)) {
        log_err("error initializing contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        delete sender;
        return false;
    }
        
    contact->set_cl_info(sender);
    BundleDaemon::post(new ContactUpEvent(link->contact()));
    
    // XXX/demmer should this assert that there's nothing on the link
    // queue??
    
    return true;
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {
        delete sender;
        contact->set_cl_info(NULL);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    const ContactRef& contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    ASSERT(contact == sender->contact_);

    int len = sender->send_bundle(bundle);

    if (len > 0) {
        link->del_from_queue(bundle, len);
        link->add_to_inflight(bundle, len);
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle.object(), contact, link, len, 0));
    }
}

//----------------------------------------------------------------------
UDPConvergenceLayer::Receiver::Receiver(UDPConvergenceLayer::Params* params)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/udp/receiver")),
      UDPClient("/dtn/cl/udp/receiver"),
      Thread("UDPConvergenceLayer::Receiver")
{
    logfd_  = false;
    params_ = *params;
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    // the payload should contain a full bundle
    Bundle* bundle = new Bundle();
    
    bool complete = false;
    int cc = BundleProtocol::consume(bundle, bp, len, &complete);

    if (cc < 0) {
        log_err("process_data: bundle protocol error");
        delete bundle;
        return;
    }

    if (!complete) {
        log_err("process_data: incomplete bundle");
        delete bundle;
        return;
    }
    
    log_debug("process_data: new bundle id %d arrival, length %zu (payload %zu)",
              bundle->bundleid(), len, bundle->payload().length());
    
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, EndpointID::NULL_EID()));
}

//----------------------------------------------------------------------
void
UDPConvergenceLayer::Receiver::run()
{
    int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char buf[MAX_UDP_PACKET];

    while (1) {
        if (should_stop())
            break;
        
        ret = recvfrom((char*)buf, MAX_UDP_PACKET, 0, &addr, &port);
        if (ret <= 0) {   
            if (errno == EINTR) {
                continue;
            }
            log_err("error in recvfrom(): %d %s",
                    errno, strerror(errno));
            close();
            break;
        }
        
        log_debug("got %d byte packet from %s:%d",
                  ret, intoa(addr), port);               
        process_data(buf, ret);
    }
}

//----------------------------------------------------------------------
UDPConvergenceLayer::Sender::Sender(const ContactRef& contact)
    : Logger("UDPConvergenceLayer::Sender",
             "/dtn/cl/udp/sender/%p", this),
      socket_(logpath_),
      rate_socket_(logpath_, 0, 0),
      contact_(contact.object(), "UDPCovergenceLayer::Sender")
{
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::Sender::init(Params* params,
                                  in_addr_t addr, u_int16_t port)
    
{
    log_debug("initializing sender");

    params_ = params;
    
    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);

    if (params->local_addr_ != INADDR_NONE || params->local_port_ != 0)
    {
        if (socket_.bind(params->local_addr_, params->local_port_) != 0) {
            log_err("error binding to %s:%d: %s",
                    intoa(params->local_addr_), params->local_port_,
                    strerror(errno));
            return false;
        }
    }
    
    if (socket_.connect(addr, port) != 0) {
        log_err("error issuing udp connect to %s:%d: %s",
                intoa(addr), port, strerror(errno));
        return false;
    }

    if (params->rate_ != 0) {
        rate_socket_.bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_.bucket()->set_depth(params->bucket_depth_);
        }
        
        log_debug("initialized rate controller: rate %llu depth %llu",
                  U64FMT(rate_socket_.bucket()->rate()),
                  U64FMT(rate_socket_.bucket()->depth()));
    }

    return true;
}
    
//----------------------------------------------------------------------
int
UDPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle)
{
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);

    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, UDPConvergenceLayer::MAX_BUNDLE_LEN);
        return -1;
    }
        
    // write it out the socket and make sure we wrote it all
    int cc = socket_.write((char*)buf_, total_len);
    if (cc == (int)total_len) {
        log_info("send_bundle: successfully sent bundle length %d", cc);
        return total_len;
    } else {
        log_err("send_bundle: error sending bundle (wrote %d/%zu): %s",
                cc, total_len, strerror(errno));
        return -1;
    }
}

} // namespace dtn
