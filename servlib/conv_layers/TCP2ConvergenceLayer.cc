/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/poll.h>
#include <stdlib.h>

#include <oasys/io/NetUtils.h>
#include <oasys/util/OptParser.h>

#include "TCP2ConvergenceLayer.h"
#include "IPConvergenceLayerUtils.h"

namespace dtn {

TCP2ConvergenceLayer::TCPLinkParams
    TCP2ConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
TCP2ConvergenceLayer::TCPLinkParams::TCPLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults),
      local_addr_(INADDR_ANY),
      remote_addr_(INADDR_NONE),
      remote_port_(TCPCL_DEFAULT_PORT)
{
}

//----------------------------------------------------------------------
TCP2ConvergenceLayer::TCP2ConvergenceLayer()
    : StreamConvergenceLayer("TCP2ConvergenceLayer", "tcp2", TCPCL_VERSION)
{
}

//----------------------------------------------------------------------
ConnectionConvergenceLayer::LinkParams*
TCP2ConvergenceLayer::new_link_params()
{
    return new TCPLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
TCP2ConvergenceLayer::parse_link_params(LinkParams* lparams,
                                        int argc, const char** argv,
                                        const char** invalidp)
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;
    
    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;
    
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }
    
    // continue up to parse the parent class
    return StreamConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    StreamConvergenceLayer::dump_link(link, buf);
    
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("local_addr: %s\n", intoa(params->local_addr_));
}

//----------------------------------------------------------------------
bool
TCP2ConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                        const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
TCP2ConvergenceLayer::parse_nexthop(Link* link, LinkParams* lparams)
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(lparams);
    ASSERT(params != NULL);

    if (! IPConvergenceLayerUtils::parse_nexthop(link->nexthop(),
                                                 &params->remote_addr_,
                                                 &params->remote_port_)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }
    
    if (params->remote_addr_ == INADDR_ANY ||
        params->remote_addr_ == INADDR_NONE)
    {
        log_err("invalid host in next hop address '%s'", link->nexthop());
        return false;
    }
    
    // make sure the port was specified
    if (params->remote_port_ == 0) {
        log_err("port not specified in next hop address '%s'",
                link->nexthop());
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
CLConnection*
TCP2ConvergenceLayer::new_connection(LinkParams* p)
{
    TCPLinkParams* params = dynamic_cast<TCPLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::dump_params(TCPLinkParams* params)
{
    oasys::StringBuffer buf;

    buf.appendf("busy_queue_depth_ %d ", params->busy_queue_depth_);
    buf.appendf("reactive_frag_enabled_ %d ", params->reactive_frag_enabled_);
    buf.appendf("sendbuf_len_ %d ", params->sendbuf_len_);
    buf.appendf("recvbuf_len_ %d ", params->recvbuf_len_);
    buf.appendf("keepalive_interval_ %d ", params->keepalive_interval_);
    buf.appendf("data_timeout_ %d ", params->data_timeout_);

    log_info("%s: %s", __FUNCTION__, buf.c_str() );
}

//----------------------------------------------------------------------
bool
TCP2ConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    in_addr_t local_addr = INADDR_ANY;
    u_int16_t local_port = TCPCL_DEFAULT_PORT;

    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("local_addr", &local_addr));
    p.addopt(new oasys::UInt16Opt("local_port", &local_port));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }
    
    // check that the local interface / port are valid
    if (local_addr == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }

    if (local_port == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this);
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
TCP2ConvergenceLayer::interface_down(Interface* iface)
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

    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::dump_interface(Interface* iface,
                                     oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(listener->local_addr()), listener->local_port());
}

//----------------------------------------------------------------------
TCP2ConvergenceLayer::Listener::Listener(TCP2ConvergenceLayer* cl)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/tcp/listener")), 
      TCPServerThread("TCP2ConvergenceLayer::Listener",
                      "/dtn/cl/tcp/listener"),
      cl_(cl)
{
    logfd_  = false;
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);
    
    Connection* conn =
        new Connection(cl_, &TCP2ConvergenceLayer::default_link_params_,
                       fd, addr, port);
    conn->start();
}

//----------------------------------------------------------------------
TCP2ConvergenceLayer::Connection::Connection(TCP2ConvergenceLayer* cl,
                                             TCPLinkParams* params)
    : StreamConvergenceLayer::Connection("TCP2ConvergenceLayer::Connection",
                                         cl->logpath(), cl, params)
{
    logpathf("%s/conn/%s:%d", cl->logpath(),
             intoa(params->remote_addr_), params->remote_port_);

    // set up the base class' nexthop parameter
    oasys::StringBuffer nexthop("%s:%d",
                                intoa(params->remote_addr_),
                                params->remote_port_);
    set_nexthop(nexthop.c_str());
    
    // the actual socket
    sock_ = new oasys::TCPClient(logpath_);

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);

    // cache the remote addr and port in the fields in the socket
    // since we want to actually connect to the peer side from the
    // Connection thread, not from the caller thread
    sock_->set_remote_addr(params->remote_addr_);
    sock_->set_remote_port(params->remote_port_);

    // if the parameters specify a local address, do the bind here --
    // however if it fails, we can't really do anything about it, so
    // just log and go on
    if (params->local_addr_ != INADDR_ANY)
    {
        if (sock_->bind(params->local_addr_, 0) != 0) {
            log_err("error binding to %s: %s",
                    intoa(params->local_addr_),
                    strerror(errno));
        }
    }
}

//----------------------------------------------------------------------
TCP2ConvergenceLayer::Connection::Connection(TCP2ConvergenceLayer* cl,
                                             TCPLinkParams* params,
                                             int fd,
                                             in_addr_t remote_addr,
                                             u_int16_t remote_port)
    : StreamConvergenceLayer::Connection("TCP2ConvergenceLayer::Connection",
                                         cl->logpath(), cl, params)
{
    logpathf("%s/conn/%s:%d", cl->logpath(), intoa(remote_addr), remote_port);
    
    // set up the base class' nexthop parameter
    oasys::StringBuffer nexthop("%s:%d", intoa(remote_addr), remote_port);
    set_nexthop(nexthop.c_str());
    
    sock_ = new oasys::TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);
}

//----------------------------------------------------------------------
TCP2ConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::initialize_pollfds()
{
    if (sock_->state() == oasys::IPSocket::INIT) {
        sock_->init_socket();
    }
    
    sock_pollfd_  = &pollfds_[0];
    num_pollfds_  = 1;
    poll_timeout_ = params_->data_timeout_;
    
    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::connect()
{
    // start a connection to the other side... in most cases, this
    // returns EINPROGRESS, in which case we wait for a call to
    // handle_poll_activity
    log_debug("connect: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    ASSERT(contact_ != NULL);
    ASSERT(contact_->link()->isopening());
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    int ret = sock_->connect(sock_->remote_addr(), sock_->remote_port());

    if (ret == 0) {
        log_debug("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

        initiate_contact();
        
    } else if (ret == -1 && errno == EINPROGRESS) {
        log_debug("connect: EINPROGRESS returned, waiting for write ready");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("connection attempt to %s:%d failed... %s",
                 intoa(sock_->remote_addr()), sock_->remote_port(),
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::accept()
{
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);
    
    log_debug("accept: got connection from %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());
    initiate_contact();
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::disconnect()
{
    // do not log an error when we fail to write, since the
    // SHUTDOWN is basically advisory. Expected errors here
    // include a short write due to socket already closed,
    // or maybe EAGAIN due to socket not ready for write.
    char typecode = SHUTDOWN;
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->write(&typecode, 1);
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::handle_poll_activity()
{
    if (sock_pollfd_->revents & POLLHUP) {
        log_info("remote socket closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    if (sock_pollfd_->revents & POLLERR) {
        log_info("error condition on remote socket -- returned POLLERR");
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    // first check for write readiness, meaning either we're getting a
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked
    if (sock_pollfd_->revents & POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;
            
        if (sock_->state() != oasys::IPSocket::ESTABLISHED) {
            int result = sock_->async_connect_result();
            if (result == 0) {
                log_debug("delayed_connect to %s:%d succeeded",
                          intoa(sock_->remote_addr()), sock_->remote_port());
                initiate_contact();
                
            } else {
                log_info("connection attempt to %s:%d failed... %s",
                         intoa(sock_->remote_addr()), sock_->remote_port(),
                         strerror(errno));
                break_contact(ContactEvent::BROKEN);
            }

            return;
        }
        
        send_data();
    }

    // finally, check for incoming data
    if (sock_pollfd_->revents & POLLIN) {
        recv_data();
        process_data();
    }
}

//----------------------------------------------------------------------
void
TCP2ConvergenceLayer::Connection::send_data()
{
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
TCP2ConvergenceLayer::Connection::recv_data()
{
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
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    recvbuf_.fill(cc);
}

} // namespace dtn
