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

#include <config.h>
#ifdef OASYS_BLUETOOTH_ENABLED

#include <oasys/bluez/Bluetooth.h>
#include <oasys/bluez/BluetoothSDP.h>
#include <oasys/util/Random.h>
#include <oasys/util/OptParser.h>
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "Bluetooth2ConvergenceLayer.h"

namespace dtn {

Bluetooth2ConvergenceLayer::BluetoothLinkParams
    Bluetooth2ConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::BluetoothLinkParams::BluetoothLinkParams(
                                                    bool init_defaults )
    : StreamLinkParams(init_defaults),
      local_addr_(*(BDADDR_ANY)),
      remote_addr_(*(BDADDR_ANY)),
      channel_(BTCL_DEFAULT_CHANNEL)
{
}

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::Bluetooth2ConvergenceLayer()
    : StreamConvergenceLayer("Bluetooth2ConvergenceLayer",
                             "bt2",BTCL_VERSION)
{
}

//----------------------------------------------------------------------
ConnectionConvergenceLayer::LinkParams*
Bluetooth2ConvergenceLayer::new_link_params()
{
    return new BluetoothLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
Bluetooth2ConvergenceLayer::parse_link_params(LinkParams* lparams,
                                              int argc, const char** argv,
                                              const char** invalidp)
{

    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;

    p.addopt(new oasys::BdAddrOpt("local_addr",&params->local_addr_));
    p.addopt(new oasys::BdAddrOpt("remote_addr",&params->remote_addr_));
    p.addopt(new oasys::UInt8Opt("channel",&params->channel_));

    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }

    argc -= count;

    // validate the local address
    if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
        // try again by reading address
        oasys::Bluetooth::hci_get_bdaddr(&params->local_addr_);
        if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
            log_err("cannot find local Bluetooth adapter address");
            return false;
        }
    }

    // continue up to parse the parent class
    return StreamConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    StreamConvergenceLayer::dump_link(link,buf);
    BluetoothLinkParams* params =
        dynamic_cast<BluetoothLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    buf->appendf("local_addr: %s\n", bd2str(params->local_addr_));
    buf->appendf("remote_addr: %s\n", bd2str(params->remote_addr_));
    buf->appendf("channel: %u\n",params->channel_);
}

//----------------------------------------------------------------------
bool
Bluetooth2ConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                              const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
Bluetooth2ConvergenceLayer::parse_nexthop(Link* link, LinkParams* lparams)
{
    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(lparams);
    ASSERT(params != NULL);

    std::string tmp;
    bdaddr_t ba;
    const char* p = link->nexthop();
    const char* str = p;
    int numcolons = 5; // expecting 12 hex digits, 5 colons total

    while (numcolons > 0) {
        p = strchr(p+1, ':');
        if (p != NULL) {
            numcolons--;
        } else {
            log_warn("bad format for remote Bluetooth address: '%s'", str);
            return false;
        }
    }
    tmp.assign(str, p - str + 3);
    oasys::Bluetooth::strtoba(tmp.c_str(),&ba);

    bacpy(&params->remote_addr_,&ba);
    return true;
}

//----------------------------------------------------------------------
CLConnection*
Bluetooth2ConvergenceLayer::new_connection(LinkParams* p) 
{
    BluetoothLinkParams *params = dynamic_cast<BluetoothLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
bool
Bluetooth2ConvergenceLayer::interface_up(Interface* iface,
                                         int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    bdaddr_t local_addr;
    u_int8_t channel = BTCL_DEFAULT_CHANNEL;
    u_int neighbor_poll_interval = 0; // defaults to off

    memset(&local_addr,0,sizeof(bdaddr_t));

    oasys::OptParser p;
    p.addopt(new oasys::UInt8Opt("channel",&channel));
    p.addopt(new oasys::UIntOpt("neighbor_poll_interval",
                                &neighbor_poll_interval));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // read adapter address from default adapter (hci0)
    oasys::Bluetooth::hci_get_bdaddr(&local_addr);
    if (bacmp(&local_addr,BDADDR_ANY) == 0) {
        log_err("invalid local address setting of BDADDR_ANY");
        return false;
    }

    if (channel < 1 || channel > 30) {
        log_err("invalid channel setting of %d",channel);
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());

    int ret = listener->bind(local_addr, channel);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);

        ret = listener->bind(local_addr, channel);
    }

    if (ret != 0) {
        return false; // error already logged
    }

    // start listening and then start the thread to loop calling accept()
    listener->listen();
    listener->start();

    // scan for neighbors
    if (neighbor_poll_interval > 0) {
        log_debug("Starting up neighbor polling with interval=%d",
                  neighbor_poll_interval);
        listener->nd_ = new NeighborDiscovery(this,
                                              neighbor_poll_interval);
        listener->nd_->logpathf("%s/neighbordiscovery",logpath_);
        listener->nd_->start();
    }

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);

    return true;
}

//----------------------------------------------------------------------
bool
Bluetooth2ConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);

    listener->set_should_stop();

    listener->interrupt_from_io();

    while (! listener->is_stopped()) {
        oasys::Thread::yield();
    }

    NeighborDiscovery* nd = listener->nd_;

    if (nd != NULL) {
        nd->set_should_stop();
        nd->interrupt();
        while (! nd->is_stopped()) {
            oasys::Thread::yield();
        }
        delete nd;
        listener->nd_ = NULL; // not that it matters ... ?
    }

    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::dump_interface(Interface* iface,
                                           oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);

    bdaddr_t addr;
    listener->local_addr(addr);
    buf->appendf("\tlocal_addr: %s channel: %u\n",
                 bd2str(addr), listener->channel());
}

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::Listener::Listener(Bluetooth2ConvergenceLayer* cl)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/bt2/listener")),
      RFCOMMServerThread("/dtn/cl/bt2/listener",oasys::Thread::INTERRUPTABLE),
      cl_(cl), nd_(NULL)
{
    logfd_ = false;
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Listener::accepted(int fd, bdaddr_t addr,
                                               u_int8_t channel)
{
    log_debug("new connection from %s on channel %u", bd2str(addr),channel);
    Connection *conn =
        new Connection(cl_, &Bluetooth2ConvergenceLayer::default_link_params_,
                       fd, addr, channel);
    conn->start();
}

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::Connection::Connection(
        Bluetooth2ConvergenceLayer* cl, BluetoothLinkParams* params)
    : StreamConvergenceLayer::Connection(
            "Bluetooth2ConvergenceLayer::Connection",
            cl->logpath(), cl, params, true /* call connect() */)
{
    logpathf("%s/conn/%s",cl->logpath(),bd2str(params->remote_addr_));

    // set the nexthop parameter for the base class
    set_nexthop(bd2str(params->remote_addr_));

    // Bluetooth socket based on RFCOMM profile stream semantics
    sock_ = new oasys::RFCOMMClient(logpath_);
    sock_->set_local_addr(params->local_addr_);
    sock_->logpathf("%s/sock",logpath_);
    sock_->set_logfd(false);
    sock_->set_remote_addr(params->remote_addr_);
    sock_->set_channel(params->channel_);
    sock_->init_socket(); // make sure sock_::fd_ exists for set_nonblocking
    // sock_->set_nonblocking(true); // defer until after connect()
}

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::Connection::Connection(
                                Bluetooth2ConvergenceLayer* cl,
                                BluetoothLinkParams* params,
                                int fd, bdaddr_t addr, u_int8_t channel)
    : StreamConvergenceLayer::Connection(
         "Bluetooth2ConvergenceLayer::Connection", cl->logpath(), cl, params,
         false /* call accept() */)
{
    // set the nexthop parameter for the base class
    set_nexthop(bd2str(addr));
    ::bacpy(&params->remote_addr_,&addr);

    logpathf("%s/conn/%s-%d",cl->logpath(),bd2str(addr),channel);

    // set nexthop parameter for base class
    set_nexthop(bd2str(addr));

    sock_ = new oasys::RFCOMMClient(fd, addr, channel, logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);
}

//----------------------------------------------------------------------
Bluetooth2ConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;

    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;

    if (sock_pollfd_->fd == -1) {
        log_err("initialize_pollfds was given a bad socket descriptor");
        break_contact(ContactEvent::BROKEN);
    }

    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(params_);
    ASSERT(params != NULL);

    poll_timeout_ = params->data_timeout_;

    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::connect()
{
    bdaddr_t addr;
    sock_->remote_addr(addr);
//    sock_->set_channel(params_->channel_);
    log_debug("connect: connecting to %s-%d",bd2str(addr),sock_->channel());

    ASSERT(active_connector_);
    ASSERT(contact_ == NULL || contact_->link()->isopening());

    ASSERT(sock_->state() != oasys::BluetoothSocket::ESTABLISHED);

    int ret = sock_->connect();

    if (ret == 0) {
        log_debug("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::BluetoothSocket::ESTABLISHED);

        sock_->set_nonblocking(true);
        initiate_contact();

//    } else if (ret == -1 && errno == EINPROGRESS) {
//        log_debug("connect: EINPROGRESS returned, waiting for write ready");
//        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("failed to connect to %s: %s",bd2str(addr),strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::accept()
{
    bdaddr_t addr;
    memset(&addr,0,sizeof(bdaddr_t));
    ASSERT(sock_->state() == oasys::BluetoothSocket::ESTABLISHED);
    sock_->remote_addr(addr);
    log_debug("accept: got connection from %s",bd2str(addr));
    initiate_contact();
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::disconnect()
{
    if (sock_->state() != oasys::BluetoothSocket::CLOSED) {
        // we can only send a shutdown byte if we're not in the middle
        // of sending a segment, otherwise the shutdown byte could be
        // interpreted as a part of the payload
        if (send_segment_todo_ == 0) {
            log_debug("disconnect: trying to send shutdown byte");
            char typecode = SHUTDOWN;
            sock_->write(&typecode, 1);
        }

        sock_->close();
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::handle_poll_activity()
{
    if ((sock_pollfd_->revents & POLLNVAL) == POLLNVAL) {
        log_info("invalid file descriptor -- returned POLLNVAL");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if ((sock_pollfd_->revents & POLLHUP) == POLLHUP) {
        log_info("remote socket closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if ((sock_pollfd_->revents & POLLERR) == POLLERR) {
        log_info("error condition on remote socket -- returned POLLERR");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // first check for write readiness, meaning either we're getting a 
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked
    if ((sock_pollfd_->revents & POLLOUT) == POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;

//        if (sock_->state() == oasys::BluetoothSocket::CONNECTING) {
//            bdaddr_t addr;
//            sock_->remote_addr(addr);
//            int result = sock_->async_connect_result();
//            if (result == 0) {
//                log_debug("delayed connect() to %s succeeded",bd2str(addr));
//                initiate_contact();

//            } else {
//                log_info("connection attempt to %s failed ... %s",
//                         bd2str(addr),strerror(errno));
//                break_contact(ContactEvent::BROKEN);
//            }

//            return;
//        }

        send_data();
    }

    // check for incoming data
    if ((sock_pollfd_->revents & POLLIN) == POLLIN) {
        recv_data();
        process_data();
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::send_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    if (params_->test_write_delay_ != 0) {
        log_debug("send_data: sleeping for test_write_delay msecs %u",
                  params_->test_write_delay_);

        usleep(params_->test_write_delay_ * 1000);
    }

    log_debug("send_data: trying to drain %zu bytes from send buffer...",
              sendbuf_.fullbytes());
    ASSERT(sendbuf_.fullbytes() > 0);
    int cc = sock_->write(sendbuf_.start(), sendbuf_.fullbytes());
    if (cc > 0) {
        log_debug("send_data: wrote %d/%zu bytes from send buffer",
                  cc, sendbuf_.fullbytes());
        sendbuf_.consume(cc);

        if (sendbuf_.fullbytes() != 0) {
            log_debug("send_data: incomplete write, setting POLLOUT bit");
            sock_pollfd_->events |= POLLOUT;
        } else {
            if (sock_pollfd_->events & POLLOUT) {
                log_debug("send_data: drained buffer, clearing POLLOUT bit");
                sock_pollfd_->events &= ~POLLOUT;
            }
        }

    } else if (errno == EWOULDBLOCK) {
        log_debug("send_data: write returned EWOULDBLOCK, setting POLLOUT bit");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("send_data: remote connection unexpectedly closed: %s",
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::Connection::recv_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    // this shouldn't ever happen
    if (recvbuf_.tailbytes() == 0) {
        log_err("no space in receive buffer to accept data!!!");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if (params_->test_read_delay_ != 0) {
        log_debug("recv_data: sleeping for test_read_delay msecs %u",
                  params_->test_read_delay_);
        usleep(params_->test_read_delay_ * 1000);
    }

    log_debug("recv_data: draining up to %zu bytes into recv buffer...",
              recvbuf_.tailbytes());
    int cc = sock_->read(recvbuf_.end(), recvbuf_.tailbytes());
    if (cc < 1) {
        // why does RFCOMM return POLLIN when it doesn't have anything!??
        if (errno != EAGAIN) {
            log_info("remote connection unexpectedly closed: %s (%d)",
                     strerror(errno),errno);
            break_contact(ContactEvent::BROKEN);
            ASSERTF(0,"EAGAIN bug on BT2CL");
        }
        return;
    }

    log_debug("recv_data: read %d bytes, rcvbuf has %zu bytes",
              cc, recvbuf_.fullbytes());
    recvbuf_.fill(cc);
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::NeighborDiscovery::initiate_contact(bdaddr_t remote)
{
    // First let's find out what we know about this remote addr
    std::string nexthop = bd2str(remote);

    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    Link* link = cm->find_link_to(cl_, nexthop);

    if (link == NULL) {
        Bluetooth2ConvergenceLayer::BluetoothLinkParams* params =
            dynamic_cast<Bluetooth2ConvergenceLayer::BluetoothLinkParams*>(
                cl_->new_link_params());
        bacpy(&params->remote_addr_,&remote);
        CLConnection* conn = cl_->new_connection(params);
        conn->start(); // good morning starshine, the earth says, hello!
        // contact/link setup happens elsewhere in the CL

        // back off for a while
        sleep(100);
    } else {
        // nothing else to do
        log_debug("found %s running DTN, already linked via %s",
                  nexthop.c_str(), link->name());
    }
}

//----------------------------------------------------------------------
void
Bluetooth2ConvergenceLayer::NeighborDiscovery::run()
{
    if (poll_interval_ == 0) {
        return;
    }

    // register DTN service with local SDP daemon
    oasys::BluetoothServiceRegistration sdp_reg("dtnd",
                           BundleDaemon::instance()->local_eid().c_str());
    if (sdp_reg.success() == false) {
        log_err("SDP registration failed");
        return;
    }

    // loop forever (until interrupted)
    while (true) {

        // randomize the sleep time:
        // the point is that two nodes with zero prior knowledge of each other
        // need to be able to discover each other in a reasonably short time.
        // if common practice is that all set their poll_interval to 1 or 30
        // or x then there's the chance of synchronization or syncopation such
        // that discovery doesn't happen.
        int sleep_time = oasys::Random::rand(poll_interval_);

        log_debug("sleep_time %d",sleep_time);
        sleep(sleep_time);

        // initiate inquiry on local Bluetooth controller
        int nr = inquire(); // blocks until inquiry process completes

        if (should_stop()) break;

        if (nr < 0) {
            log_debug("no Bluetooth devices in range");
            continue;
        }

        // enumerate any remote Bluetooth adapters in range
        bdaddr_t addr;
        while (next(addr) != -1) {

            // query SDP daemon on remote host for DTN's registration
            oasys::BluetoothServiceDiscoveryClient sdpclient;
            if (sdpclient.is_dtn_router(addr)) {
                log_info("Discovered DTN router %s at %s channel %d\n",
                         sdpclient.remote_eid().c_str(),
                         bd2str(addr),sdpclient.channel()); 
                initiate_contact(addr);
            }
            if (should_stop()) break;
        }
        if (should_stop()) break;

        // flush results of previous inquiry
        reset();
    }

    // interrupted! unregister
    log_info("Bluetooth inquiry interrupted by user");
}

} // dtn

#endif // OASYS_BLUETOOTH_ENABLED
