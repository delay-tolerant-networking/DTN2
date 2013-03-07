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
#include "UDPTunnel.h"

namespace dtntunnel {

//----------------------------------------------------------------------
UDPTunnel::UDPTunnel()
    : IPTunnel("UDPTunnel", "/dtntunnel/udp"),
      next_connection_id_(0),
      listener_(NULL)
{
}

//----------------------------------------------------------------------
void
UDPTunnel::add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                        in_addr_t remote_addr, u_int16_t remote_port)
{
    if (NULL != listener_) {
        log_crit("attempt was made to add listener after already instantiated");
    } else {
        listener_ = new Listener(this, listen_addr, listen_port,
                                 remote_addr, remote_port);
    }
}

//----------------------------------------------------------------------
u_int32_t
UDPTunnel::next_connection_id()
{
    oasys::ScopeLock l(&lock_, "UDPTunnel::next_connection_id");
    return ++next_connection_id_;
}

//----------------------------------------------------------------------
void
UDPTunnel::kill_connection(Connection* c)
{
    oasys::ScopeLock l(&lock_, "UDPTunnel::kill_connection");
    
    ConnTable::iterator i;
    ConnKey key(c->dest_eid_,
                c->client_addr_,
                c->client_port_,
                c->remote_addr_,
                c->remote_port_,
                c->connection_id_);
    
    i = connections_.find(key);

    if (i == connections_.end()) {
        log_err("can't find connection *%p in table", c);
        return;
    }

    // there's a chance that the connection was replaced by a
    // restarted one, in which case we leave the existing one in the
    // table and don't want to blow it away
    if (i->second == c) {
        connections_.erase(i);
    } else {
        log_notice("not erasing connection in table since already replaced");
    }

}

//----------------------------------------------------------------------
void
UDPTunnel::handle_bundle(dtn::APIBundle* bundle)
{
    oasys::ScopeLock l(&lock_, "UDPTunnel::handle_bundle");

    DTNTunnel::BundleHeader hdr;
    memcpy(&hdr, bundle->payload_.buf(), sizeof(hdr));
    hdr.connection_id_ = ntohl(hdr.connection_id_);
    hdr.seqno_ = ntohl(hdr.seqno_);
    hdr.client_port_ = ntohs(hdr.client_port_);
    hdr.remote_port_ = ntohs(hdr.remote_port_);

    log_debug("handle_bundle got %zu byte bundle from %s for %s:%d -> %s:%d (id %u seqno %u)",
              bundle->payload_.len(),
              bundle->spec_.source.uri,
              intoa(hdr.client_addr_),
              hdr.client_port_,
              intoa(hdr.remote_addr_),
              hdr.remote_port_,
              hdr.connection_id_,
              hdr.seqno_);


    if (NULL != listener_) {
        listener_->handle_bundle(bundle);
        return;
    }

    
    Connection* conn = NULL;
    ConnTable::iterator i;
    ConnKey key(bundle->spec_.source,
                hdr.client_addr_,
                hdr.client_port_,
                hdr.remote_addr_,
                hdr.remote_port_,
                hdr.connection_id_);
    
    i = connections_.find(key);
    
    if (i == connections_.end()) {
        log_debug("got bundle with seqno %u but no connection... "
                 "creating new connection for client %s:%d",
                 hdr.seqno_, intoa(hdr.client_addr_), hdr.client_port_);

        conn = new Connection(this, &bundle->spec_.source,
                              hdr.client_addr_, hdr.client_port_,
                              hdr.remote_addr_, hdr.remote_port_,
                              hdr.connection_id_);
                              // next_connection_id()); - listener now assigns an ID

        log_info("new connection *%p", conn);
        conn->start();
        connections_[key] = conn;

        // wait for new connection to establish a connection
        //
    } else {
        conn = i->second;
    }

    ASSERT(conn != NULL);
    conn->handle_bundle(bundle);
}

//----------------------------------------------------------------------
UDPTunnel::Connection::Connection(UDPTunnel* t, dtn_endpoint_id_t* dest_eid,
                                  in_addr_t client_addr, u_int16_t client_port,
                                  in_addr_t remote_addr, u_int16_t remote_port,
                                  u_int32_t connection_id)
    : Thread("UDPTunnel::Connection", Thread::DELETE_ON_EXIT),
      Logger("UDPTunnel::Connection", "/dtntunnel/udp/conn"),
      udptun_(t),
      sock_("/dtntunnel/udp/conn/sock"),
      queue_("/dtntunnel/udp/conn"),
      next_seqno_(0),
      client_addr_(client_addr),
      client_port_(client_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port),
      connection_id_(connection_id)
{
    dtn_copy_eid(&dest_eid_, dest_eid);
    reorder_udp_ = DTNTunnel::instance()->reorder_udp();
}

//----------------------------------------------------------------------
UDPTunnel::Connection::~Connection()
{
    dtn::APIBundle* b;
    while(queue_.try_pop(&b)) {
        delete b;
    }
}

//----------------------------------------------------------------------
int
UDPTunnel::Connection::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "[%s %s:%d -> %s:%d (id %u)",
                    dest_eid_.uri,
                    intoa(client_addr_),
                    client_port_,
                    intoa(remote_addr_),
                    remote_port_,
                    connection_id_);
}

//----------------------------------------------------------------------
void
UDPTunnel::Connection::run()
{
    DTNTunnel* tunnel = DTNTunnel::instance();
    u_int32_t send_seqno = 0;
    u_int32_t next_recv_seqno = 0;

    // XXX/dz in iperf test with 10 parallel connections, using timeout=0 
    // doubles the time needed for the server to finish
    int timeout = 1;
    if (tunnel->delay_set()) timeout = tunnel->delay();

    reorder_udp_ = tunnel->reorder_udp();

    // outgoing (udp -> dtn) / incoming (dtn -> udp) bundles
    dtn::APIBundle* b_xmit = NULL;
    dtn::APIBundle* b_recv = NULL;

    // time values to implement nagle
    oasys::Time tbegin, tnow;
    ASSERT(tbegin.sec_ == 0);
    
    // header for outgoing bundles
    DTNTunnel::BundleHeader hdr;
    hdr.eof_           = 0;
    hdr.protocol_      = IPPROTO_UDP;
    hdr.connection_id_ = htonl(connection_id_);
    hdr.seqno_         = 0;
    hdr.client_addr_   = client_addr_;
    hdr.client_port_   = htons(client_port_);
    hdr.remote_addr_   = remote_addr_;
    hdr.remote_port_   = htons(remote_port_);
    
    sock_.init_socket();

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
        //int nfds = (sock_eof || dtn_blocked) ? 1 : 2;
        int nfds = 2;

        int nready = oasys::IO::poll_multiple(pollfds, nfds, timeout,
                                              NULL, logpath_);
        if (nready == oasys::IOERROR) {
            log_err("unexpected error in poll: %s", strerror(errno));
            goto done;
        }

        // now we check if there really is data on the socket
        if (sock_poll->revents != 0) {
            int ret = sock_.recv(recv_buf_, sizeof(recv_buf_), 0);
            if (ret < 0) {
                log_err("error reading from socket (for client %s:%d): %s", 
                        intoa(client_addr_), client_port_,
                        strerror(errno));
                delete b_xmit;
                goto done;
            }
                
            hdr.seqno_ = ntohl(send_seqno++);

            dtn::APIBundle* b = new dtn::APIBundle();
            char* bp = b->payload_.buf(sizeof(hdr) + ret);
            memcpy(bp, &hdr, sizeof(hdr));
            memcpy(bp + sizeof(hdr), recv_buf_, ret);
            b->payload_.set_len(sizeof(hdr) + ret);
       
            int status =  tunnel->send_bundle(b, &dest_eid_);
            if (status != DTN_SUCCESS) {
                //exit(1);
                log_crit("error sending bundle - status: %d", status);
            }

            delete b;
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
            if ( reorder_udp_ ) {
                if (recv_seqno != next_recv_seqno) {
                    log_err("got out of order bundle: seqno %d, expected %d",
                            recv_seqno, next_recv_seqno);
                    delete b_recv;
                    goto done;
                }
            }
            ++next_recv_seqno;

            u_int len = b_recv->payload_.len() - sizeof(hdr);

            if (len != 0) {
                DTNTunnel::BundleHeader hdr;
                memcpy(&hdr, b_recv->payload_.buf(), sizeof(hdr));
                hdr.remote_port_ = htons(hdr.remote_port_);

                char* bp = b_recv->payload_.buf() + sizeof(hdr);
                int  len = b_recv->payload_.len() - sizeof(hdr);
    
                int cc = sock_.sendto(bp, len,
                                       0, hdr.remote_addr_, hdr.remote_port_);
                if (cc != len) {
                    log_err("error sending packet to %s:%d for client %s:%d: %s",
                            intoa(hdr.remote_addr_), hdr.remote_port_, 
                            intoa(client_addr_), client_port_,
                            strerror(errno));
                } else {
                    log_debug("sent %zu byte payload to %s:%d for client %s:%d",
                              len,
                              intoa(hdr.remote_addr_), hdr.remote_port_,
                              intoa(client_addr_), client_port_);
                }
            
                if (cc != len) {
                  udptun_->kill_connection(this);
                }
            }

            delete b_recv;
        }
    }

 done:
    udptun_->kill_connection(this);
}

//----------------------------------------------------------------------
void
UDPTunnel::Connection::handle_bundle(dtn::APIBundle* bundle)
{
    DTNTunnel::BundleHeader* hdr =
        (DTNTunnel::BundleHeader*)bundle->payload_.buf();
    
    u_int32_t recv_seqno = ntohl(hdr->seqno_);

    if ( reorder_udp_ ) {
        // if the seqno is in the past, then it's a duplicate delivery so
        // just ignore it
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
            log_debug("got out of order bundle (client: %s:%d): expected seqno %d, got %d",
                     intoa(client_addr_), client_port_,
                     next_seqno_, recv_seqno);
            
            reorder_table_[recv_seqno] = bundle;
            return;
        }
    }

    // deliver the one that just arrived
    log_debug("delivering %zu byte bundle with seqno %d for client %s:%d",
              bundle->payload_.len(), recv_seqno,
              intoa(client_addr_), client_port_);
    queue_.push_back(bundle);
    next_seqno_++;


    if ( reorder_udp_ ) {
        // once we get one that's in order, that might let us transfer
        // more bundles out of the reorder table and into the queue
        ReorderTable::iterator iter;
        while (1) {
            iter = reorder_table_.find(next_seqno_);
            if (iter == reorder_table_.end()) {
                break;
            }

            bundle = iter->second;
            log_debug("delivering %zu byte bundle with seqno %d for client %s:%d (from reorder table)",
                     bundle->payload_.len(), next_seqno_,
                     intoa(client_addr_), client_port_);
        
            reorder_table_.erase(iter);
            next_seqno_++;
        
            queue_.push_back(bundle);
        }
    }
}

//----------------------------------------------------------------------
UDPTunnel::Listener::Listener(UDPTunnel* t,
                              in_addr_t listen_addr, u_int16_t listen_port,
                              in_addr_t remote_addr, u_int16_t remote_port)
    : Thread("UDPTunnel::Listener", DELETE_ON_EXIT),
      Logger("UDPTunnel::Listener", "/dtntunnel/udp/listener"), 
      sock_("/dtntunnel/udp/listener/sock"),
      udptun_(t),
      listen_addr_(listen_addr),
      listen_port_(listen_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    start();
}

//----------------------------------------------------------------------
void
UDPTunnel::Listener::run()
{
    DTNTunnel* tunnel = DTNTunnel::instance();
    int ret = sock_.bind(listen_addr_, listen_port_);
    if (ret != 0) {
        log_err("can't bind to %s:%u", intoa(listen_addr_), listen_port_);
        return; // die
    }

    DTNTunnel::BundleHeader hdr;
    hdr.protocol_         = IPPROTO_UDP;
    hdr.connection_id_    = 0;
    hdr.seqno_            = 0;
    hdr.remote_addr_      = remote_addr_;
    hdr.remote_port_      = htons(remote_port_);
   
    while (1) {
        int len = sock_.recvfrom(recv_buf_, sizeof(recv_buf_), 0, &hdr.client_addr_, &hdr.client_port_);

        if (len <= 0) {
            log_err("error reading from socket: %s", strerror(errno));
            return; // die
        }
        

        log_debug("got %d byte packet from <udp src: %s:%d>", len, intoa(hdr.client_addr_), hdr.client_port_);

        hdr.seqno_ = next_seqno(hdr.client_addr_, hdr.client_port_, &hdr.connection_id_);
        hdr.seqno_ = ntohl(hdr.seqno_);
        hdr.client_port_ = htons(hdr.client_port_);
        hdr.connection_id_ = ntohl(hdr.connection_id_);

        dtn::APIBundle* b = new dtn::APIBundle();
        char* bp = b->payload_.buf(sizeof(hdr) + len);
        memcpy(bp, &hdr, sizeof(hdr));
        memcpy(bp + sizeof(hdr), recv_buf_, len);
        b->payload_.set_len(sizeof(hdr) + len);
        
        if (tunnel->send_bundle(b, tunnel->dest_eid()) != DTN_SUCCESS)
            exit(1);

        delete b;
    }
}

//----------------------------------------------------------------------
u_int32_t 
UDPTunnel::Listener::next_seqno(in_addr_t client_addr, u_int16_t client_port, 
                                u_int32_t* connection_id)
{
    LConn* lconn;

    LConnTable::iterator i;
    LConnKey key(client_addr, client_port);
    
    i = lconnections_.find(key);
    
    if (i == lconnections_.end()) {
        log_debug("listener detected new connection (%s:%d)... "
                 "creating new entry to track sequence number",
                 intoa(client_addr), client_port);

        lconn = new struct LConn();
        lconn->connection_id_ = udptun_->next_connection_id();
        lconn->seqno_ = 0;

        lconnections_[key] = lconn;
    } else {
        lconn = i->second;
    }

    ASSERT(lconn != NULL);

    *connection_id = lconn->connection_id_;    
    u_int32_t result = lconn->seqno_++;

    //dz debug - uncomment to test out of order packets (after the first one)
    // note that adjusting the seqno will result in lines being reversed
    //result += (result % 2 == 1) ? 1 : (result > 0) ? -1 : 0;

    return result;
}

//----------------------------------------------------------------------
void
UDPTunnel::Listener::handle_bundle(dtn::APIBundle* bundle)
{
    DTNTunnel::BundleHeader hdr;
    memcpy(&hdr, bundle->payload_.buf(), sizeof(hdr));
    hdr.client_port_ = htons(hdr.client_port_);

    char* bp = bundle->payload_.buf() + sizeof(hdr);
    int  len = bundle->payload_.len() - sizeof(hdr);

    // transmit packet to the socket that initiated the connection
    int cc = sock_.sendto(bp, len,
                           0, hdr.client_addr_, hdr.client_port_);
    if (cc != len) {
        log_err("error sending packet to %s:%d: %s",
                intoa(hdr.client_addr_), hdr.client_port_, strerror(errno));
    } else {
        log_debug("sent %zu byte packet to %s:%d",
                  bundle->payload_.len() - sizeof(hdr),
                  intoa(hdr.client_addr_), hdr.client_port_);
    }

    delete bundle;
}

} // namespace dtntunnel

