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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/io/NetUtils.h>
#include <oasys/util/Time.h>
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

    connections_[key] = c;
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

    // there's a chance that the connection was replaced by a
    // restarted one, in which case we leave the existing one in the
    // table and don't want to blow it away
    if (i->second == c) {
        connections_.erase(i);
    }
}

//----------------------------------------------------------------------
void
TCPTunnel::handle_bundle(dtn::APIBundle* bundle)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::handle_bundle");

    log_debug("handle_bundle got %zu byte bundle", bundle->payload_.len());
    
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

    if (ntohl(hdr.seqno_) == 0)
    {
        if (i != connections_.end()) {
            log_warn("got bundle with seqno 0 but connection already exists... "
                     "closing and restarting");
            
            // push a NULL bundle onto the existing connection queue
            // to signal that it should shut down
            i->second->queue_.push_back(NULL);
        }

        log_info("new connection %s:%d -> %s:%d",
                 intoa(hdr.client_addr_),
                 hdr.client_port_,
                 intoa(hdr.remote_addr_),
                 hdr.remote_port_);
        
        conn = new Connection(this, &bundle->spec_.source,
                              hdr.client_addr_, hdr.client_port_,
                              hdr.remote_addr_, hdr.remote_port_);
        conn->start();
        connections_[key] = conn;
    }
    else
    {
        // seqno != 0
        if (i == connections_.end()) {
            log_warn("got bundle with seqno %d but no connection, ignoring",
                     ntohl(hdr.seqno_));
            delete bundle;
            return;
        }

        conn = i->second;
    }

    ASSERT(conn != NULL);
    conn->handle_bundle(bundle);
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
    if (bind_listen_start(listen_addr, listen_port) != 0) {
        log_err("can't initialize listener socket, bailing");
        exit(1);
    }
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
      next_seqno_(0),
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
      next_seqno_(0),
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
    u_int32_t send_seqno = 0;
    u_int32_t next_recv_seqno = 0;
    u_int32_t total_sent = 0;
    bool sock_eof = false;
    bool dtn_blocked = false;
    bool first = true;

    // outgoing (tcp -> dtn) / incoming (dtn -> tcp) bundles
    dtn::APIBundle* b_xmit = NULL;
    dtn::APIBundle* b_recv = NULL;

    // time values to implement nagle
    oasys::Time tbegin, tnow;
    ASSERT(tbegin.sec_ == 0);
    
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
            hdr.eof_ = 1;
            memcpy(b->payload_.buf(sizeof(hdr)), &hdr, sizeof(hdr));
            b->payload_.set_len(sizeof(hdr));
            int err;
            if ((err = tunnel->send_bundle(b, &dest_eid_)) != DTN_SUCCESS) {
                log_err("error sending connect reply bundle: %s",
                        dtn_strerror(err));
                tcptun_->kill_connection(this);
                exit(1);
            }
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

        // if the socket already had an eof or if dtn is write
        // blocked, we just poll for activity on the message queue to
        // look for data that needs to be returned out the TCP socket
        int nfds = (sock_eof || dtn_blocked) ? 1 : 2;

        int timeout = -1;
        if (first || dtn_blocked) {
            timeout = 1000; // one second between retries
        } else if (tbegin.sec_ != 0) {
            timeout = tunnel->delay();
        }
        
        log_debug("blocking in poll... (timeout %d)", timeout);
        int nready = oasys::IO::poll_multiple(pollfds, nfds, timeout,
                                              NULL, logpath_);
        if (nready == oasys::IOERROR) {
            log_err("unexpected error in poll: %s", strerror(errno));
            goto done;
        }

        // check if we need to create a new bundle, either because
        // this is the first time through and we'll need to send an
        // initial bundle to create the connection on the remote side,
        // or because there's data on the socket.
        if ((first || sock_poll->revents != 0) && (b_xmit == NULL)) {
            first = false;
            b_xmit = new dtn::APIBundle();
            b_xmit->payload_.reserve(tunnel->max_size());
            hdr.seqno_ = ntohl(send_seqno++);
            memcpy(b_xmit->payload_.buf(), &hdr, sizeof(hdr));
            b_xmit->payload_.set_len(sizeof(hdr));
        }

        // now we check if there really is data on the socket
        if (sock_poll->revents != 0) {
            u_int payload_todo = tunnel->max_size() - b_xmit->payload_.len();

            if (payload_todo != 0) {
                tbegin.get_time();
                
                char* bp = b_xmit->payload_.end();
                int ret = sock_.read(bp, payload_todo);
                if (ret < 0) {
                    log_err("error reading from socket: %s", strerror(errno));
                    delete b_xmit;
                    goto done;
                }
                
                b_xmit->payload_.set_len(b_xmit->payload_.len() + ret);
                
                if (ret == 0) {
                    DTNTunnel::BundleHeader* hdrp =
                        (DTNTunnel::BundleHeader*)b_xmit->payload_.buf();
                    hdrp->eof_ = 1;
                    sock_eof = true;
                }
            }
        }

        // now check if we should send the outgoing bundle
        tnow.get_time();
        if ((b_xmit != NULL) &&
            ((sock_eof == true) ||
             (b_xmit->payload_.len() == tunnel->max_size()) ||
             ((tnow - tbegin).in_milliseconds() >= tunnel->delay())))
        {
            size_t len = b_xmit->payload_.len();
            int err = tunnel->send_bundle(b_xmit, &dest_eid_);
            if (err == DTN_SUCCESS) {
                total_sent += len;
                log_debug("sent %zu byte payload #%u to dtn (%u total)",
                          len, send_seqno, total_sent);
                b_xmit = NULL;
                tbegin.sec_ = 0;
                tbegin.usec_ = 0;
                dtn_blocked = false;
                
            } else if (err == DTN_ENOSPACE) {
                log_debug("no space for %zu byte payload... "
                          "setting dtn_blocked", len);
                dtn_blocked = true;
                continue;
            } else {
                log_err("error sending bundle: %s", dtn_strerror(err));
                exit(1);
            }
        }
        
        // now check for activity on the incoming bundle queue
        if (msg_poll->revents != 0) {
            b_recv = queue_.pop_blocking();

            // if a NULL bundle is put on the queue, then the main
            // thread is signalling that we should abort the
            // connection
            if (b_recv == NULL)
            {
                log_debug("got signal to abort connection");
                goto done;
            }

            DTNTunnel::BundleHeader* recv_hdr =
                (DTNTunnel::BundleHeader*)b_recv->payload_.buf();

            u_int32_t recv_seqno = ntohl(recv_hdr->seqno_);

            // check the seqno match -- reordering should have been
            // handled before the bundle was put on the blocking
            // message queue
            if (recv_seqno != next_recv_seqno) {
                log_err("got out of order bundle: seqno %d, expected %d",
                        recv_seqno, next_recv_seqno);
                delete b_recv;
                goto done;
            }
            ++next_recv_seqno;

            u_int len = b_recv->payload_.len() - sizeof(hdr);

            if (len != 0) {
                int cc = sock_.writeall(b_recv->payload_.buf() + sizeof(hdr), len);
                if (cc != (int)len) {
                    log_err("error writing payload to socket: %s", strerror(errno));
                    delete b_recv;
                    goto done;
                }

                log_debug("sent %d byte payload to client", len);
            }
            

            if (recv_hdr->eof_) {
                log_info("bundle had eof bit set... closing connection");
                sock_.close();
            }
            
            delete b_recv;
        }
    }

 done:
    tcptun_->kill_connection(this);
}

//----------------------------------------------------------------------
void
TCPTunnel::Connection::handle_bundle(dtn::APIBundle* bundle)
{
    DTNTunnel::BundleHeader* hdr =
        (DTNTunnel::BundleHeader*)bundle->payload_.buf();
    
    u_int32_t recv_seqno = ntohl(hdr->seqno_);

    // if the seqno is in the past, but isn't 0 to mark that it starts
    // a new connection (which is handled above), then it's a
    // duplicate delivery so just ignore it
    if (recv_seqno < next_seqno_)
    {
        log_warn("got seqno %u, but already delivered up to %u: "
                 "ignoring bundle",
                 recv_seqno, next_seqno_);
        delete bundle;
        return;
    }
    
    // otherwise, if it's not the next one expected, put it on the
    // queue and wait for the one that's missing
    else if (recv_seqno != next_seqno_)
    {
        log_debug("got out of order bundle: expected seqno %d, got %d",
                  next_seqno_, recv_seqno);
        
        reorder_table_[recv_seqno] = bundle;
        return;
    }

    // deliver the one that just arrived
    log_debug("delivering %zu byte bundle with seqno %d",
              bundle->payload_.len(), recv_seqno);
    queue_.push_back(bundle);
    next_seqno_++;
    
    // once we get one that's in order, that might let us transfer
    // more bundles out of the reorder table and into the queue
    ReorderTable::iterator iter;
    while (1) {
        iter = reorder_table_.find(next_seqno_);
        if (iter == reorder_table_.end()) {
            break;
        }

        bundle = iter->second;
        log_debug("delivering %zu byte bundle with seqno %d (from reorder table)",
                  bundle->payload_.len(), next_seqno_);
        
        reorder_table_.erase(iter);
        next_seqno_++;
        
        queue_.push_back(bundle);
    }
}

} // namespace dtntunnel

