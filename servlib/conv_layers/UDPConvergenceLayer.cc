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

#include <sys/poll.h>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/URL.h>

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
    oasys::Intoa local_addr(local_addr_);
    const char *local_addr_str = local_addr.buf();

    oasys::Intoa remote_addr(remote_addr_);
    const char *remote_addr_str = remote_addr.buf();

    a->process("local_addr",
        (u_char *) local_addr_str, strlen(local_addr_str));
    a->process("remote_addr",
        (u_char *) remote_addr_str, strlen(remote_addr_str));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
}

//----------------------------------------------------------------------
UDPConvergenceLayer::UDPConvergenceLayer()
    : IPConvergenceLayer("UDPConvergenceLayer", "udp")
{
    defaults_.local_addr_		= INADDR_ANY;
    defaults_.local_port_		= 5000;
    defaults_.remote_addr_		= INADDR_NONE;
    defaults_.remote_port_		= 0;
    defaults_.rate_			= 0; // unlimited
    defaults_.bucket_depth_		= 0; // default
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
UDPConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // validate the link next hop address
    if (! IPConvergenceLayer::parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("invalid host in next hop address '%s'", link->nexthop());
        return false;
    }
    
    // make sure the port was specified
    if (port == 0) {
        log_err("port not specified in next hop address '%s'",
                link->nexthop());
        return false;
    }

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
UDPConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
}

//----------------------------------------------------------------------
bool
UDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    Link* link = contact->link();
    log_debug("opening contact for link *%p", link);
    
    // parse out the address / port from the nexthop address. note
    // that these should have been validated in init_link() above, so
    // we ASSERT as such
    bool valid = parse_nexthop(link->nexthop(), &addr, &port);
    ASSERT(valid == true);
    ASSERT(addr != INADDR_NONE && addr != INADDR_ANY);
    ASSERT(port != 0);
    
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
UDPConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    ASSERT(contact == sender->contact_);

    bool ok = sender->send_bundle(bundle); // consumes bundle reference

    if (ok) {
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact,
                                       bundle->payload_.length(),
                                       0));
    } else {
        BundleDaemon::post(
            new BundleTransmitFailedEvent(bundle, contact));
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
    Bundle* bundle = NULL;       
    int header_len;
    
    // parse the headers into a new bundle. this sets the payload_len
    // appropriately in the new bundle and returns the number of bytes
    // consumed for the bundle headers
    bundle = new Bundle();
    header_len = BundleProtocol::parse_header_blocks(bundle, bp, len);

    if (header_len < 0) {
        log_err("process_data: invalid or too short bundle header");
        delete bundle;
        return;
    }
    
    size_t payload_len = bundle->payload_.length();

    if (len != header_len + payload_len) {
        log_err("process_data: error in bundle lengths: "
                "bundle_length %zu, header_length %d, payload_length %zu",
                len, header_len, payload_len);
        delete bundle;
        return;
    }

    // store the payload and notify the daemon
    bundle->payload_.set_data(bp + header_len, payload_len);
    
    log_debug("process_data: new bundle id %d arrival, payload length %zu",
	      bundle->bundleid_, bundle->payload_.length());
    
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, payload_len));
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
        
        log_debug("initialized rate controller: rate %u depth %u",
                  rate_socket_.bucket()->rate(),
                  rate_socket_.bucket()->depth());
    }

    return true;
}
    
//----------------------------------------------------------------------
bool 
UDPConvergenceLayer::Sender::send_bundle(Bundle* bundle)
{
    int header_len;

    size_t formatted_len = BundleProtocol::formatted_length(bundle);
    if (formatted_len > UDPConvergenceLayer::MAX_BUNDLE_LEN) {
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, UDPConvergenceLayer::MAX_BUNDLE_LEN);
        return false;
    }
        
    size_t payload_len = bundle->payload_.length();

    // stuff in the bundle headers
    header_len =
        BundleProtocol::format_header_blocks(bundle, buf_, sizeof(buf_));
    if (header_len < 0) {
        log_err("send_bundle: bundle header too big for buffer (len %zu)",
                sizeof(buf_));
        return false;
    }

    // check that the payload isn't too big (it should have been
    // fragmented by the higher layers)
    if (payload_len > (sizeof(buf_) - header_len)) {
        log_err("send_bundle: bundle payload + headers (length %zu) too big",
                header_len + payload_len);
        return false;
    }

    // read the payload data into the buffer
    bundle->payload_.read_data(0, payload_len, &buf_[header_len],
                               BundlePayload::FORCE_COPY);

    // write it out the socket and make sure we wrote it all
    int cc = socket_.write((char*)buf_, header_len + payload_len);
    if (cc == (int)(header_len + payload_len)) {
        log_info("send_bundle: successfully sent bundle length %d", cc);
        return true;
    } else {
        
        log_err("send_bundle: error sending bundle (wrote %d/%zu): %s",
                cc, (header_len + payload_len), strerror(errno));
        return false;
    }
}

} // namespace dtn
