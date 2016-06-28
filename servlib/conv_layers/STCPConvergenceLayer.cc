 /*    Copyright 2004-2006 Intel Corporation
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

#include "STCPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

struct STCPConvergenceLayer::Params STCPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
    a->process("rate", &rate_);
    a->process("poll_timeout", &poll_timeout_);
    a->process("keepalive_interval", &keepalive_interval_);
}

//----------------------------------------------------------------------
STCPConvergenceLayer::STCPConvergenceLayer()
    : IPConvergenceLayer("STCPConvergenceLayer", "stcp")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = STCPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.poll_timeout_             = 5000; // default 5 seconds 
    defaults_.keepalive_interval_       = 5;    // default 5 seconds 
    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;
}

//----------------------------------------------------------------------
CLInfo*
STCPConvergenceLayer::new_link_params()
{
    return new STCPConvergenceLayer::Params(STCPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&STCPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::UIntOpt("rate", &params->rate_));
    p.addopt(new oasys::IntOpt("poll_timeout", &params->poll_timeout_));
    p.addopt(new oasys::IntOpt("keepalive_interval", &params->keepalive_interval_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
};

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    in_addr_t local_addr = INADDR_ANY;
    u_int16_t local_port = STCPCL_DEFAULT_PORT;
    
    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &local_addr));
    p.addopt(new oasys::UInt16Opt("local_port", &local_port));
     
    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'", invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (local_addr == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (local_port == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }
    log_debug("interface_up:  Creating Listener on %s:%d",
                 intoa(local_addr), local_port);
    // create a new server socket for the requested interface
    Listener * listener = new Listener(this, local_addr, local_port);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
   
    int ret = listener->bind(local_addr, local_port);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);
    
        ret = listener->bind(local_addr, local_port);
    }
   
    if (ret != 0) {
        return false; // error already logged
    }
  
    // start listening and then start the thread to loop calling accept()
    listener->listen();
    listener->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);
    return true;
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself

    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
     
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(listener->local_addr()), listener->local_port());
 
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::init_link(const LinkRef& link,
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

    next_hop_addr_  = addr;
    next_hop_port_  = port;
    next_hop_flags_ = MSG_DONTWAIT;

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

    if (link->params().mtu_ > MAX_STCP_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_STCP_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("STCPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
    if (params->rate_ != 0) {
        buf->appendf("rate: %u\n", params->rate_);
    }
    buf->appendf("poll_timeout: %u\n", params->poll_timeout_);
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[]) 
{
    (void) link;

    // grab the contact structure
    const ContactRef& contact = link->contact();

    if (contact != NULL) {
        // grab the sender structure
        Sender* sender = (Sender*) contact->cl_info();

        if (NULL != sender) {
            // grab the params structure
            Params * params = (Params*)link->cl_info();
            const char* invalid;
            if (! parse_params(params, argc, argv, &invalid)) {
                log_err("reconfigure_link: invalid parameter %s", invalid);
                return false;
            }
       
            log_err("reconfigure_link: calling reconfigure rate=%d", params->rate_);
            sender->reconfigure();
        }
    }

    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                         AttributeVector& params) 
{
    (void) link;

    // grab the contact structure
    const ContactRef& contact = link->contact();
    if (contact != NULL) {
        Sender* sender = (Sender *) contact->cl_info();
        // inform the sender of the reconfigure
        if (NULL != sender) {
            Params * sender_params = (Params*)link->cl_info();
            AttributeVector::iterator iter;
            for (iter = params.begin(); iter != params.end(); iter++) {
                if (iter->name().compare("rate") == 0) { 
                    sender_params->rate_ = iter->u_int_val();
                    log_debug("reconfigure_link - new rate = %"PRIu32, sender_params->rate_);
                    //dz debug
                    log_always("reconfigure_link - new rate = %"PRIu32, sender_params->rate_);
                }
                // any others to change on the fly through the External Router interface?
        
                else {
                    log_crit("reconfigure_link - unknown parameter: %s", iter->name().c_str());
                }
            }       
            sender->reconfigure();
        }
    }
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    log_debug("STCPConvergenceLayer::open_contact: "
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
        port = STCPCL_DEFAULT_PORT;
    }

    next_hop_addr_  = addr;
    next_hop_port_  = port;
    next_hop_flags_ = MSG_DONTWAIT;

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
    
    sender->start();
 
    // XXX/demmer should this assert that there's nothing on the link
    // queue??
    return true;
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {
        sender->set_should_stop();
        sleep(1);
        delete sender;
        contact->set_cl_info(NULL);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) bundle;  // not used since we are grabbing from the queue
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    log_debug("bundle_queued called..."); 
     
    const ContactRef& contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                   contact.object());
        return;
    }
    BundleRef lbundle = link->queue()->front();
    ASSERT(contact == sender->contact_);
     
    int len = sender->send_bundle(lbundle);
     
    if (len > 0) {
        link->del_from_queue(lbundle, len);
        link->add_to_inflight(lbundle, len);
        BundleDaemon::post(
            new BundleTransmittedEvent(lbundle.object(), contact, link, len, 0));
    }

    return;
}
//----------------------------------------------------------------------
void
STCPConvergenceLayer::handle_bundle(Bundle* bundle)
{
    (void) bundle;


    ///  this routine would pull apart the header and create a new connection
    ///  based the remote address and port found in the header...


}
//----------------------------------------------------------------------
STCPConvergenceLayer::Listener::Listener(STCPConvergenceLayer* cl, in_addr_t addr, u_int16_t port)
    : TCPServerThread("STCPConvergenceLaer::Listener",
                      "/dtn/cl/stcp/listener"),
    cl_(cl)
{
    logfd_       = false;
    listen_addr_ = addr;
    listen_port_ = port;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    Connection* conn = new Connection(cl_, fd, addr, port);
    log_debug("accepted .... starting connection from %s:%d", intoa(addr), port);
    conn->start();
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Listener::force_close_socket(int fd)
{
    log_debug("Listener::force_close_socket...");
    struct linger l;
    int off = 0;
    l.l_onoff = 1;
    l.l_linger = 0;
    int err;
     
    err = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
    if (err) {
        log_err("setsockopt(SO_LINGER) error: %d, %s", err, strerror(errno));
    }
    err = setsockopt(fd, SOL_TCP, TCP_NODELAY, &off, sizeof(off));
    if (err) {
        log_err("setsockopt(TCP_NODELAY) error: %d, %s", err, strerror(errno));
    }
    ::write(fd, "", 1);
    ::close(fd);
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::process_data(u_char* bp, size_t len)
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
    
    log_debug("process_data: new bundle id %"PRIbid" arrival, length %zu (payload %zu)",
              bundle->bundleid(), len, bundle->payload().length());
    
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, EndpointID::NULL_EID()));
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Connection::Connection(STCPConvergenceLayer* cl,
                                             int fd, in_addr_t client_add,
                                             u_int16_t client_port) 
            : TCPClient(fd, client_add, client_port),
              Thread("STCPReader") 
{
    (void) cl;
    client_addr_ = client_add;
    client_port_ = client_port;
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Connection::~Connection()
{

}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Connection::run()
{
    int ret;
    in_addr_t addr;
    u_int16_t port;

    int all_data     = 0;
    int data_ptr     = 0;
    int data_to_read = 0;

    union US {
       u_char char_size[4];
       int    int_size;
    };

    US union_size;

    oasys::ScratchBuffer<u_char*, 0> buffer;

    while (1) {

        if (should_stop())
            break;
        
        data_to_read = 4;       
        //
        // get the size off the wire and then read the data
        //
        data_ptr     = 0;
        while (data_to_read > 0)        
        {
            ret = read((char *) &(union_size.char_size[data_ptr]), data_to_read);
            if (ret <= 0) {   
                if (errno == EINTR) {
                    continue;
                }
                close();
                set_should_stop();
                break;
            }
            data_ptr     += ret;
            data_to_read -= ret;
            if (should_stop())
                break;
        }

        if (should_stop())
            break;

        data_ptr = 0;
        all_data = data_to_read = ntohl(union_size.int_size);
        if (all_data == 0) {
            continue;
        }
        buffer.clear();
        buffer.reserve(data_to_read); // reserve storage

        while (data_to_read > 0) {
            ret = read((char*) buffer.buf()+data_ptr, data_to_read);
            if (ret <= 0) {   
                if (errno == EINTR) {
                    continue;
                }
                close();
                set_should_stop();
                break;
            }
            buffer.incr_len(ret);
            data_to_read -= ret;
            data_ptr     += ret;
            if (should_stop())
                break;
        } 
        log_debug("got %d byte packet from %s:%d",
                  all_data, intoa(addr), port);               
        process_data(buffer.buf(),all_data);
    }
}

//----------------------------------------------------------------------
STCPConvergenceLayer::Sender::Sender(const ContactRef& contact)
    : Logger("STCPConvergenceLayer::Sender",
             "/dtn/cl/stcp/sender/%p", this),
              Thread("STCPReader"),
      bucket_(0),
      contact_broken_(false),
      contact_(contact.object(), "STCPCovergenceLayer::Sender")
{
}

STCPConvergenceLayer::Sender::~Sender()
{
    if (params_->rate_ > 0)
    {
        delete bucket_;
    }
}

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::Sender::reconfigure()
{
    // NOTE: params_ have already been updated with the new values by the parent
    log_debug("reconfigure: called %pX\n",params_);

    if (params_ == NULL) return false;

    if (params_->rate_ != 0) {
       
        if (NULL == bucket_) {
            bucket_ = new oasys::TokenBucket(logpath_, 65535 * 8, params_->rate_);
        }

        // allow updating the rate and the bucket depth on the fly but not the bucket type
        // >> set rate to zero and then reconfigure the bucket type to change it
        log_debug("reconfigure: setting new bucket rate controller values: rate %d",
                  params_->rate_);

        bucket_->set_rate(params_->rate_);

        log_debug("reconfigure: new rate controller values: rate %llu",
                  U64FMT(bucket_->rate()));
    } else {
        if (NULL != bucket_) {
            oasys::ScopeLock l(&(write_lock_), "STCPConvergenceLayer::Sender::reconfigure");
            delete bucket_;
            bucket_ = NULL;
        }
    }

    return true;
}

//----------------------------------------------------------------------
void 
STCPConvergenceLayer::Sender::run()
{
    int keepalive;
    initialize_pollfds();

    while (true) {

        if (should_stop()) {
            log_debug("Sender:run should stop is set");
            break;
        }

        if (contact_broken_) {
            log_debug("Sender:run contact broken, exiting main loop");
            return;
        }
 
        int cc = oasys::IO::poll_multiple(pollfds_, num_pollfds_, // try without adding 1 ... + 1,
                                          poll_timeout_, NULL, logpath_);
   
        if (cc == oasys::IOTIMEOUT) { 
            sleep(1);
        } else if(cc > 0){
            if (handle_poll_activity()) {
                while (!contact_broken_) {
                    sleep(1);  // sleep a second and send keep alive if time to.
                    if (should_stop()) {
                        return;
                    } else {
                       keepalive ++;
                       if (keepalive >= params_->keepalive_interval_) {
                           if (send_keepalive(&keepalive) < 0) {
                                // had an issue writing to the socket.
                                break_contact();
                                return; 
                           }
                       }
                    }
                }
            } else {
                if (errno != EINTR && errno != EINPROGRESS) {
                    log_debug("unexpected return from poll_multiple: %d err:%s", cc, strerror(errno));
                    // break_contact();
                }
            }
        } else {
            if (errno != EINTR && errno != EINPROGRESS) {
                log_debug("unexpected return from poll_multiple: %d err: %s", cc, strerror(errno));
                break_contact();
                return;
            }
        }

        if (errno == EINPROGRESS) {
            sock_pollfd_->events |= POLLOUT;
        }

        sleep(1);
    }
}
//----------------------------------------------------------------------
int
STCPConvergenceLayer::Sender::send_keepalive(int * keepalive)
{
    int data_ptr = 0;
    *keepalive = 0; // write zero to the socket
    int data_to_send = (int) sizeof(*keepalive);

    oasys::ScopeLock l(&(write_lock_), "STCPConvergenceLayer::Sender::send_keepalive");
    while (data_to_send > 0) {
        int cc = socket_->write((char *) keepalive+data_ptr, data_to_send);
        if (cc == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        } else {
            data_to_send -= cc;
            data_ptr     += cc;
        }
    }
    return 0;
}
//----------------------------------------------------------------------
void
STCPConvergenceLayer::Sender::initialize_pollfds()
{
    log_debug("Sender::initialize_pollfds");

    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;
     
    sock_pollfd_->fd     = socket_->fd();
    sock_pollfd_->events = POLLOUT;
     
    poll_timeout_ = params_->poll_timeout_;
}

//----------------------------------------------------------------------
void
STCPConvergenceLayer::Sender::break_contact() 
{
    contact_broken_ = true; 
        
    // if the connection isn't being closed by the user, we need to 
    // notify the daemon that either the contact ended or the link
    // became unavailable before a contact began.
    //
    // we need to check that there is in fact a contact, since a 
    // connecti be accepted and then break before establishing a
    // contact 
    if (contact_ != NULL) {
        BundleDaemon::post(
            new LinkStateChangeRequest(contact_->link(),
                                       Link::CLOSED,
                                       ContactEvent::BROKEN));
    }
}


//----------------------------------------------------------------------
bool
STCPConvergenceLayer::Sender::handle_poll_activity()
{
    log_debug("Sender::handle_poll_activity");
    if (sock_pollfd_->revents & POLLHUP) {
        log_info("remote socket closed connection -- returned POLLHUP");
        break_contact();
        return false;
    }
    
    if (sock_pollfd_->revents & POLLERR) {
        log_info("error condition on remote socket -- returned POLLERR");
        break_contact();
        return false;
    }
    
    // first check for write readiness, meaning either we're getting a
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked 
    if (sock_pollfd_->revents & POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;
        
        if (socket_->state() == oasys::IPSocket::ESTABLISHED) {
            contact_broken_ = false;
            log_crit("STCPConv.. Adding ContactUpEvent ");
            BundleDaemon::post(new ContactUpEvent(contact_));
            return !contact_broken_;
        } else if (socket_->state() == oasys::IPSocket::CONNECTING) {
            int result = socket_->async_connect_result();
            if (result == 0) { 
                log_debug("delayed_connect to %s:%d succeeded",
                          intoa(socket_->remote_addr()), socket_->remote_port());
                contact_broken_ = false;
                log_crit("STCPConv.. Adding ContactUpEvent ");
                BundleDaemon::post(new ContactUpEvent(contact_));

            } else {
                log_info("connection attempt to %s:%d failed... %s",
                         intoa(socket_->remote_addr()), socket_->remote_port(),
                         strerror(errno));
                break_contact();
            }
            return !contact_broken_;
        } else {
           log_debug("socket_state = %08X", socket_->state());
        }
    }
    return false;
}    

//----------------------------------------------------------------------
bool
STCPConvergenceLayer::Sender::init(Params* params,
                                  in_addr_t addr, u_int16_t port)
    
{
    int errno_out;

    log_debug("Sender::init called....");

    logpathf("%s/conn/%p", logpath_, this);

    params_ = params;

    socket_ = new oasys::TCPClient(logpath_);
    socket_->logpathf("%s/sock", logpath_);
    socket_->set_logfd(false);
    socket_->init_socket();

    if (params->local_addr_ != INADDR_NONE || params->local_port_ != 0)
    {
         log_debug("Sender::init binding to %s:%d",
                  intoa(params->local_addr_), params->local_port_);
         if (socket_->bind(params->local_addr_, params->local_port_) != 0) {
             log_err("error Connecting to %s:%d: %s",
                  intoa(params->local_addr_), params->local_port_,
                  strerror(errno));
              return false;
         }
    }
    //
    // if(socket_->connect(addr,port) != 0) {
    //
    if(socket_->timeout_connect(addr,port, 2000, &errno_out) != 0) {
        if (errno != EINPROGRESS) { // poller will handle connection...
            log_debug("Sender::init connection error %s",strerror(errno));
            return false;
        }
        sleep(1);
    }

    if (params_->rate_ > 0 && bucket_ == 0)
    {
        bucket_ = new oasys::TokenBucket(logpath_,(u_int32_t) 65535 * 8, (u_int32_t) (params_->rate_));
        bucket_->set_rate(params_->rate_);
    }

    return true;
}
    
//----------------------------------------------------------------------
int
STCPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle)
{
    int           cc            = 0;
    int           data_ptr      = 0;
    unsigned int  data_to_send  = 0;
    unsigned int  data_to_write = 0;
    BlockInfoVec* blocks        = bundle->xmit_blocks()->find_blocks(contact_->link());
    size_t total_len            = 0;
    size_t total_data           = (size_t) BundleProtocol::total_length(blocks);

    ASSERT(blocks != NULL);
    
    bool complete = false;

    oasys::ScratchBuffer<u_char*, 0> read_buffer;

    read_buffer.clear();
    read_buffer.reserve(4096);

    oasys::ScopeLock l(&(write_lock_), "STCPConvergenceLayer::Sender::send_bundle");

    // write it out the socket and make sure we wrote it all
    data_to_send = 4;
    data_to_write = htonl(total_data);

    log_debug("send_bundled : after produce bundle total_length=%d",(int) total_data);
   
    while (data_to_send > 0) {
        cc = socket_->write((char *) &data_to_write,data_to_send);
        if (cc == -1) {
            log_debug("send_bundled : error writing Size %d (%s)", (int) total_len, strerror(errno));
            return -1;
        }
        data_to_send -= cc;
    }

    data_ptr     = 0;

    while (!complete)
    {
        total_len = BundleProtocol::produce(bundle.object(), blocks,
                                            read_buffer.buf(), data_ptr, 
                                            (size_t) 4096,
                                            &complete);

        data_to_send = total_len;

        while (data_to_send > 0) {

            if (params_->rate_ > 0) {
                cc = rate_write((char *) read_buffer.buf(),total_len);
            } else {
                cc = socket_->write((char *) read_buffer.buf(),total_len);
            }
            if (cc == -1) {
                log_debug("send_bundled : error writing data %d (%s)", (int) total_len, strerror(errno));
                return -1;
            }
            log_debug("send_bundled : wrote %d  bytes", (int) cc);
            data_to_send -= cc;
            data_ptr     += cc;
        }

        read_buffer.clear();

        if (data_ptr == (int)total_data) {
            if (complete) {
                log_info("send_bundle: successfully sent bundle (id:%"PRIbid") length %d", 
                         bundle.object()->bundleid(), cc);
                return total_data;
            } else {
                break;
            }
        }
    }
    
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, STCPConvergenceLayer::MAX_STCP_LEN);
        return -1;
    }
    return -1;
}

//----------------------------------------------------------------------
int
STCPConvergenceLayer::Sender::rate_write(const char* bp, size_t len)
{
    ASSERT(socket_ != NULL);

    bool waiting = true;

    if (bucket_->rate() != 0) {
        while (waiting) {
            bool can_send = bucket_->try_to_drain(len * 8);
            if (!can_send) {
                usleep(1);
            } else {
                log_debug("%llu tokens sufficient for %zu byte packet",
                          U64FMT(bucket_->tokens()), len);
                waiting = false; /// force loop to end
            }
        }
    }
      
    return socket_->write(bp, len);
}

} // namespace dtn
