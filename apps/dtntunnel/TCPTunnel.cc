/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/io/NetUtils.h>
#include "DTNTunnel.h"
#include "TCPTunnel.h"

namespace dtntunnel {

//----------------------------------------------------------------------
TCPTunnel::TCPTunnel()
    : IPTunnel("TCPTunnel", "/dtntunnel/tcp")
{
}

//----------------------------------------------------------------------
void
TCPTunnel::add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                        in_addr_t remote_addr, u_int16_t remote_port)
{
    new Listener(this, listen_addr, listen_port,
                 remote_addr, remote_port);
}

//----------------------------------------------------------------------
void
TCPTunnel::new_connection(Connection* c)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::new_connection");
    
    ConnTable::iterator i;
    ConnKey key(c->client_addr_,
                c->client_port_,
                c->remote_addr_,
                c->remote_port_);
    
    i = connections_.find(key);
    
    if (i != connections_.end()) {
        log_err("got duplicate connection %s:%d -> %s:%d",
                intoa(c->client_addr_),
                c->client_port_,
                intoa(c->remote_addr_),
                c->remote_port_);
        return;
    }

    connections_.insert(ConnTable::value_type(key, c));
}

//----------------------------------------------------------------------
void
TCPTunnel::kill_connection(Connection* c)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::kill_connection");
    
    ConnTable::iterator i;
    ConnKey key(c->client_addr_,
                c->client_port_,
                c->remote_addr_,
                c->remote_port_);
    
    i = connections_.find(key);

    if (i == connections_.end()) {
        log_err("can't find connection to kill %s:%d -> %s:%d",
                intoa(c->client_addr_),
                c->client_port_,
                intoa(c->remote_addr_),
                c->remote_port_);
        return;
    }

    connections_.erase(i);
}

//----------------------------------------------------------------------
void
TCPTunnel::handle_bundle(dtn::APIBundle* bundle)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::handle_bundle");

    log_debug("handle_bundle got %d byte bundle", bundle->payload_.len());
    
    DTNTunnel::BundleHeader hdr;
    memcpy(&hdr, bundle->payload_.buf(), sizeof(hdr));
    hdr.client_port_ = htons(hdr.client_port_);
    hdr.remote_port_ = htons(hdr.remote_port_);

    Connection* conn;
    ConnTable::iterator i;
    ConnKey key(hdr.client_addr_,
                hdr.client_port_,
                hdr.remote_addr_,
                hdr.remote_port_);
    
    i = connections_.find(key);

    if (i == connections_.end()) {
        log_debug("new connection %s:%d -> %s:%d",
                  intoa(hdr.client_addr_),
                  hdr.client_port_,
                  intoa(hdr.remote_addr_),
                  hdr.remote_port_);
        
        conn = new Connection(this, &bundle->spec_.source,
                              hdr.client_addr_, hdr.client_port_,
                              hdr.remote_addr_, hdr.remote_port_);
        conn->start();
        connections_.insert(ConnTable::value_type(key, conn));

    } else {
        conn = i->second;
        ASSERT(conn != NULL);
    }

    conn->handle_bundle(bundle);
    return;
}

//----------------------------------------------------------------------
TCPTunnel::Listener::Listener(TCPTunnel* t,
                              in_addr_t listen_addr, u_int16_t listen_port,
                              in_addr_t remote_addr, u_int16_t remote_port)
    : TCPServerThread("TCPTunnel::Listener",
                      "/dtntunnel/tcp/listener",
                      Thread::DELETE_ON_EXIT),
      tcptun_(t),
      listen_addr_(listen_addr),
      listen_port_(listen_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    bind_listen_start(listen_addr, listen_port);
}

//----------------------------------------------------------------------
void
TCPTunnel::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    Connection* c = new Connection(tcptun_, DTNTunnel::instance()->dest_eid(),
                                   fd, addr, port, remote_addr_, remote_port_);
    tcptun_->new_connection(c);
    c->start();
}

//----------------------------------------------------------------------
TCPTunnel::Connection::Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                                  in_addr_t client_addr, u_int16_t client_port,
                                  in_addr_t remote_addr, u_int16_t remote_port)
    : Thread("TCPTunnel::Connection", Thread::DELETE_ON_EXIT),
      Logger("TCPTunnel::Connection", "/dtntunnel/tcp/conn"),
      tcptun_(t),
      sock_(logpath_),
      queue_(logpath_),
      client_addr_(client_addr),
      client_port_(client_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    dtn_copy_eid(&dest_eid_, dest_eid);
}

//----------------------------------------------------------------------
TCPTunnel::Connection::Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                                  int fd,
                                  in_addr_t client_addr, u_int16_t client_port,
                                  in_addr_t remote_addr, u_int16_t remote_port)
    : Thread("TCPTunnel::Connection", Thread::DELETE_ON_EXIT),
      Logger("TCPTunnel::Connection", "/dtntunnel/tcp/conn"),
      tcptun_(t),
      sock_(fd, client_addr, client_port, logpath_),
      queue_(logpath_),
      client_addr_(client_addr),
      client_port_(client_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    dtn_copy_eid(&dest_eid_, dest_eid);
}

//----------------------------------------------------------------------
TCPTunnel::Connection::~Connection()
{
    dtn::APIBundle* b;
    while(queue_.try_pop(&b)) {
        delete b;
    }
}

//----------------------------------------------------------------------
void
TCPTunnel::Connection::run()
{
    DTNTunnel* tunnel = DTNTunnel::instance();
    u_int32_t seqno = 0;

    // header for outgoing bundles
    DTNTunnel::BundleHeader hdr;
    hdr.protocol_    = IPPROTO_TCP;
    hdr.seqno_       = 0;
    hdr.client_addr_ = client_addr_;
    hdr.client_port_ = htons(client_port_);
    hdr.remote_addr_ = remote_addr_;
    hdr.remote_port_ = htons(remote_port_);
    
    if (sock_.state() != oasys::IPSocket::ESTABLISHED) {
        int err = sock_.connect(remote_addr_, remote_port_);
        if (err != 0) {
            log_err("error connecting to %s:%d",
                    intoa(remote_addr_), remote_port_);

            // send an empty bundle back
            dtn::APIBundle* b = new dtn::APIBundle();
            memcpy(b->payload_.buf(sizeof(hdr)), &hdr, sizeof(hdr));
            b->payload_.set_len(sizeof(hdr));
            tunnel->send_bundle(b, &dest_eid_);
            goto done;
        }
    }

    while (1) {
        struct pollfd pollfds[2];

        struct pollfd* msg_poll  = &pollfds[0];
        msg_poll->fd             = queue_.read_fd();
        msg_poll->events         = POLLIN;
        msg_poll->revents        = 0;
    
        struct pollfd* sock_poll = &pollfds[1];
        sock_poll->fd            = sock_.fd();
        sock_poll->events        = POLLIN | POLLERR;
        sock_poll->revents       = 0;

        log_debug("blocking in poll...");
        int nready = oasys::IO::poll_multiple(pollfds, 2, -1, NULL, logpath_);
        if (nready <= 0) {
            log_err("unexpected error in poll: %s", strerror(errno));
            goto done;
        }

        // check first for activity on the socket
        if (sock_poll->revents != 0) {
            dtn::APIBundle* b = new dtn::APIBundle();

            u_int payload_len = tunnel->max_size();
            char* bp = b->payload_.buf(sizeof(hdr) + payload_len);
            int ret = sock_.read(bp + sizeof(hdr), payload_len);
            if (ret < 0) {
                log_err("error reading from socket: %s", strerror(errno));
                delete b;
                goto done;
            }

            hdr.seqno_ = ntohl(seqno++);
            b->payload_.set_len(sizeof(hdr) + ret);
            memcpy(b->payload_.buf(), &hdr, sizeof(hdr));
            tunnel->send_bundle(b, &dest_eid_);
            log_info("sent %d byte payload to dtn", ret);

            if (ret == 0) {
                goto done;
            }
        }
        
        // now check for activity on the incoming bundle queue
        if (msg_poll->revents != 0) {
            dtn::APIBundle* b = queue_.pop_blocking();
            ASSERT(b);

            // XXX/demmer check seqno

            u_int len = b->payload_.len() - sizeof(hdr);

            if (len == 0) {
                log_info("got zero byte payload... closing connection");
                sock_.close();
                delete b;
                goto done;
            }
            
            int cc = sock_.writeall(b->payload_.buf() + sizeof(hdr), len);
            if (cc != (int)len) {
                log_err("error writing payload to socket: %s", strerror(errno));
                delete b;
                goto done;
            }

            log_info("sent %d byte payload to client", len);
            
            delete b;
        }
    }

 done:
    tcptun_->kill_connection(this);
}

//----------------------------------------------------------------------
void
TCPTunnel::Connection::handle_bundle(dtn::APIBundle* bundle)
{
    queue_.push_back(bundle);
}

} // namespace dtntunnel

