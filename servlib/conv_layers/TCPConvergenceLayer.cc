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

#ifndef MIN
# define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif

#include <sys/poll.h>
#include <stdlib.h>

#include <oasys/thread/Notifier.h>
#include <oasys/io/IO.h>
#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/URL.h>

#include "TCPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/FragmentManager.h"
#include "bundling/SDNV.h"
#include "contacts/ContactManager.h"
#include "contacts/OndemandLink.h"
#include "routing/BundleRouter.h"

namespace dtn {

struct TCPConvergenceLayer::Params TCPConvergenceLayer::defaults_;


/******************************************************************************
 *
 * TCPConvergenceLayer
 *
 *****************************************************************************/
TCPConvergenceLayer::TCPConvergenceLayer()
    : IPConvergenceLayer("/cl/tcp")
{
    // set defaults here, then let ../cmd/ParamCommand.cc (as well as
    // the link specific options) handle changing them
    defaults_.local_addr_		= INADDR_ANY;
    defaults_.local_port_		= 5000;
    defaults_.remote_addr_		= INADDR_NONE;
    defaults_.remote_port_		= 0;
    defaults_.pipeline_			= true;
    defaults_.busy_queue_depth_		= 10;
    defaults_.bundle_ack_enabled_ 	= true;
    defaults_.reactive_frag_enabled_ 	= true;
    defaults_.receiver_connect_		= false;
    defaults_.partial_ack_len_		= 1024;
    defaults_.writebuf_len_ 		= 32768;
    defaults_.readbuf_len_ 		= 32768;
    defaults_.keepalive_interval_	= 10;
    defaults_.retry_interval_		= 5;
    defaults_.min_retry_interval_	= 5;
    defaults_.max_retry_interval_	= 10 * 60;
    defaults_.rtt_timeout_		= 30000;  // msecs
    defaults_.test_fragment_size_	= -1;
}

/**
 * Parse variable args into a parameter structure.
 */
bool
TCPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::BoolOpt("bundle_ack_enabled",
                                &params->bundle_ack_enabled_));
    p.addopt(new oasys::BoolOpt("reactive_frag_enabled",
                                &params->reactive_frag_enabled_));
    p.addopt(new oasys::BoolOpt("receiver_connect",
                                &params->receiver_connect_));
    p.addopt(new oasys::UIntOpt("partial_ack_len", &params->partial_ack_len_));
    p.addopt(new oasys::UIntOpt("writebuf_len", &params->writebuf_len_));
    p.addopt(new oasys::UIntOpt("readbuf_len", &params->readbuf_len_));
    p.addopt(new oasys::UIntOpt("keepalive_interval",
                                &params->keepalive_interval_));
    p.addopt(new oasys::UIntOpt("min_retry_interval",
                                &params->min_retry_interval_));
    p.addopt(new oasys::UIntOpt("max_retry_interval",
                                &params->max_retry_interval_));
    p.addopt(new oasys::UIntOpt("rtt_timeout", &params->rtt_timeout_));
    p.addopt(new oasys::IntOpt("test_fragment_size",
                               &params->test_fragment_size_));
    
    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
}

/**
 * Bring up a new interface.
 */
bool
TCPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());

    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = TCPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    // now, special case the receiver_connect option which means we
    // create a Connection, not a Listener
    if (params.receiver_connect_) {
        if (params.remote_addr_ == INADDR_NONE) {
            log_err("receiver_connect option requires remote_addr setting");
            return false;
        }

        if (params.remote_port_ == 0) {
            log_err("receiver_connect option requires remote_port setting");
            return false;
        }

        log_debug("new receiver_connect interface -- "
                  "starting connection thread");
        Connection* conn =
            new Connection(this, params.remote_addr_, params.remote_port_,
                           Connection::RECEIVER, &params);
        conn->start();
        
        return true;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this, &params);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    int ret = listener->bind(params.local_addr_, params.local_port_);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);
        
        ret = listener->bind(params.local_addr_, params.local_port_);
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

/**
 * Bring down the interface.
 */
bool
TCPConvergenceLayer::interface_down(Interface* iface)
{
    // XXX/demmer fixme for receiver connect
    
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* listener = static_cast<Listener*>(iface->cl_info());
    listener->set_should_stop();

    listener->interrupt_from_io();
    
    while (! listener->is_stopped()) {
        oasys::Thread::yield();
    }

    delete listener;
    return true;
}

/**
 * Dump out CL specific interface information.
 */
void
TCPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Listener*)iface->cl_info())->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
}

/**
 * Create any CL-specific components of the Link.
 *
 * This parses and validates the parameters and stores them in the
 * CLInfo slot in the Link class.
 */
bool
TCPConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
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
    // them in the link's cl info slot.
    Params* params = new Params(defaults_);
    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    // check that the local interface is valid
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of INADDR_NONE");
        return false;
    }
    
    if (params->receiver_connect_ && (link->type() != Link::OPPORTUNISTIC)) {
        log_err("receiver_connect option can only be used with "
                "opportunistic links");
        return false;
    }

    // copy the retry parameters from the link itself (we need a copy
    // for ourselves to support receiver connect)
    params->retry_interval_     = link->retry_interval_;
    params->min_retry_interval_ = link->min_retry_interval_;
    params->max_retry_interval_ = link->max_retry_interval_;
    
    link->set_cl_info(params);

    return true;
}

/**
 * Dump out CL specific link information.
 */
void
TCPConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    // XXX/demmer more parameters
}

/**
 * Open the connection to the given contact and prepare for
 * bundles to be transmitted.
 */
bool
TCPConvergenceLayer::open_contact(Contact* contact)
{
    in_addr_t addr;
    u_int16_t port = 0;
    
    log_debug("opening contact *%p", contact);

    Link* link = contact->link();

    // parse out the address / port from the nexthop address. note
    // that these should have been validated in init_link() above, so
    // we ASSERT as such
    bool valid = parse_nexthop(link->nexthop(), &addr, &port);
    ASSERT(valid == true);
    ASSERT(addr != INADDR_NONE && addr != INADDR_ANY);
    ASSERT(port != 0);

    Params* params = (Params*)link->cl_info();
    
    // create a new connection for the contact
    Connection* conn = new Connection(this, addr, port,
                                      Connection::SENDER, params);
    conn->set_contact(contact);
    conn->start();

    return true;
}

/**
 * Close the connnection to the contact.
 */
bool
TCPConvergenceLayer::close_contact(Contact* contact)
{
    Connection* conn = (Connection*)contact->cl_info();

    log_info("close_contact *%p", contact);

    if (conn) {
        if (!conn->is_stopped() && !conn->should_stop()) {
            log_debug("interrupting connection thread");
            conn->set_should_stop();
            conn->interrupt_from_io();
            oasys::Thread::yield();
        }

        // the connection thread will delete itself when it
        // terminates, however, it first clears the cl_info slot in
        // the Contact class which is our indication that it exited,
        // allowing us to exit
        
        while (contact->cl_info() != NULL) {
            log_debug("waiting for connection thread to stop...");
            usleep(100000);
            oasys::Thread::yield();
        }
        
        log_debug("connection thread stopped...");
    }
    
    return true;
}

/**
 * Send a bundle to the contact. Mark the link as busy and queue
 * the bundle on the Connection's bundle queue.
 */
void
TCPConvergenceLayer::send_bundle(Contact* contact, Bundle* bundle)
{
    log_debug("send_bundle *%p to *%p", bundle, contact);

    Connection* conn = (Connection*)contact->cl_info();

    // set the busy state to apply bundle backpressure
    if ((conn->queue_->size() + 1) >= conn->params_.busy_queue_depth_) {
        contact->link()->set_state(Link::BUSY);
    }
    
    ASSERT(conn);
    conn->queue_->push_back(bundle);

}


/******************************************************************************
 *
 * TCPConvergenceLayer::Listener
 *
 *****************************************************************************/
TCPConvergenceLayer::Listener::Listener(TCPConvergenceLayer* cl,
                                        Params* params)
    : IOHandlerBase(new oasys::Notifier("/cl/tcp/listener")), 
      TCPServerThread("/cl/tcp/listener"),
      cl_(cl), params_(*params)
{
    logfd_  = false;
}

void
TCPConvergenceLayer::Listener::accepted(int fd,
                                        in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);

    Connection* conn = new Connection(cl_, fd, addr, port, &params_);
    conn->start();
}


/******************************************************************************
 *
 * TCPConvergenceLayer::Connection
 *
 *****************************************************************************/

/**
 * Constructor for the active connection side of a connection.
 * Note that this may be used both for the actual data sender
 * or for the data receiver side when used with the
 * receiver_connect option.
 */
TCPConvergenceLayer::Connection::Connection(TCPConvergenceLayer* cl,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port,
                                            direction_t direction,
                                            Params* params)

    : Thread("TCPConvergenceLayer::Connection"), 
      params_(*params), cl_(cl), initiate_(true),
      direction_(direction), contact_(NULL)
{
    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);

    // create the blocking queue for bundles to be sent on (if we're
    // the sender)
    if (direction == SENDER) {
        queue_ = new BlockingBundleList(logpath_);
    } else {
        queue_ = NULL;
    }

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    // the actual socket
    sock_ = new oasys::TCPClient();

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);

    // cache the remote addr and port in the fields in the socket
    // since we want to actually connect to the peer side from the
    // Connection thread, not from the caller thread
    sock_->set_remote_addr(remote_addr);
    sock_->set_remote_port(remote_port);
    
    // set the notifier to be able to interrupt IO
    sock_->set_notifier(new oasys::Notifier(logpath_));

    // if the parameters specify a local address, do the bind here --
    // however if it fails, we can't really do anything about it, so
    // just log and go on
    if (params->local_addr_ != INADDR_ANY)
    {
        if (sock_->bind(params->local_addr_, params->local_port_) != 0) {
            log_err("error binding to %s:%d: %s",
                    intoa(params->local_addr_), params->local_port_,
                    strerror(errno));
        }
    }
}    

/**
 * Constructor for the passive accept side of a connection.
 */
TCPConvergenceLayer::Connection::Connection(TCPConvergenceLayer* cl,
                                            int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port,
                                            Params* params)

    : Thread("TCPConvergenceLayer::Connection"),
      params_(*params), cl_(cl), initiate_(false),
      direction_(UNKNOWN), contact_(NULL)
{
    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);

    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    // create an empty queue (for now)
    ASSERT(!params->receiver_connect_);
    queue_ = NULL;
    
    sock_ = new oasys::TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
}

TCPConvergenceLayer::Connection::~Connection()
{
    if (queue_)
        delete queue_;
    
    delete sock_;
}


/**
 * The main loop for a connection. Based on the initial construction
 * state, it either initiates a connection to the other side, or
 * accepts one, then calls one of send_loop or recv_loop.
 *
 * The whole thing is wrapped in a big loop so the active side of the
 * connection is restarted if it's supposed to be, e.g. for links that
 * are configured as ALWAYSON but end up breaking the connection.
 */
void
TCPConvergenceLayer::Connection::run()
{
    // XXX/demmer much of this could be abstracted into a generic CL
    // ConnectionThread class, assuming we had another
    // connection-oriented CL that we wanted to support (e.g. SCTP)

    while (1) {
        log_debug("connection main loop starting up...");
        
        if (initiate_)
        {
            if (! connect()) {
                log_debug("connection failed");
                break_contact(ContactEvent::BROKEN);
                goto broken;
            }

            // if we successfully connected, reset the retry timer to
            // the minimum interval
            params_.retry_interval_ = params_.min_retry_interval_;

            // if the link isn't currently in OPENING state, then we
            // need to put it there since send_loop will post a
            // ContactUpEvent
            if (contact_ && (contact_->link()->state() != Link::OPENING)) {
                contact_->link()->set_state(Link::OPENING);
            }
            
        } else {
            /*
             * If accepting a connection failed, we always return
             * which triggers the thread to be deleted and therefore
             * cleans up our state.
             */
            if (! accept()) {
                log_debug("accept failed");
                return; // trigger a deletion
            }
        }

        if (direction_ == SENDER) {
            send_loop();

        } else if (direction_ == RECEIVER) {
            recv_loop();
        
        } else {
            PANIC("direction_ not set by connect() or accept()");
        }

 broken:
        // use the thread's should_stop() indicator (set by
        // break_contact() as well as the interruption routines) to
        // determine when we should exit, either because we were
        // interrupted by a user action (i.e. link close), we're the
        // passive acceptor, or because an ondemand link is idle
        if (should_stop())
            return;

        // otherwise, we should really be the initiator, or else
        // something wierd happened
        if (!initiate_) {
            log_err("passive side exited loop but didn't set should_stop!");
            return;
        }
        
        // sleep for the appropriate retry amount (by waiting to see
        // if we're interrupted) and try to re-establish the
        // connection
        ASSERT(sock_->get_notifier());
        log_info("waiting for %d seconds before retrying connection",
                 params_.retry_interval_);
        if (sock_->get_notifier()->wait(NULL, params_.retry_interval_ * 1000)) {
            log_info("socket interrupted from retry sleep -- exiting thread");
            return;
        }

        // double the retry timer up to the max for the next time around
        params_.retry_interval_ = params_.retry_interval_ * 2;
        if (params_.retry_interval_ > params_.max_retry_interval_) {
            params_.retry_interval_ = params_.max_retry_interval_;
        }
    }

    log_debug("connection thread main loop complete -- thread exiting");
}


/**
 * Send one bundle over the wire.
 *
 * Return true if the bundle was sent successfully, false if not.
 */
bool
TCPConvergenceLayer::Connection::send_bundle(Bundle* bundle)
{
    int header_len;
    size_t sdnv_len;
    size_t tcphdr_len;
    u_char tcphdr_buf[32];
    size_t block_len;
    size_t payload_len = bundle->payload_.length();
    size_t payload_offset = 0;
    const u_char* payload_data;

    // first put the bundle on the inflight_ queue
    inflight_.push_back(InFlightBundle(bundle));

    // Each bundle is preceded by a BUNDLE_DATA typecode and then a
    // BundleDataHeader that is variable-length since it includes an
    // SDNV for the total bundle length.
    //
    // So, first calculate the length of the bundle headers while we
    // put it into the send buffer, then fill in the small CL buffer

 retry_headers:
    header_len =
        BundleProtocol::format_headers(bundle, sndbuf_.buf(),
                                       sndbuf_.buf_len());
    if (header_len < 0) {
        log_debug("send_bundle: bundle header too big for buffer len %d -- "
                  "doubling size and retrying", (u_int)sndbuf_.buf_len());
        sndbuf_.reserve(sndbuf_.buf_len() * 2);
        goto retry_headers;
    }
    
    sdnv_len = SDNV::encoding_len(header_len + payload_len);
    tcphdr_len = 1 + sizeof(BundleDataHeader) + sdnv_len;

    ASSERT(sizeof(tcphdr_buf) >= tcphdr_len);
    
    // Now fill in the type code and the bundle data header (with the
    // sdnv for the total bundle length)
    tcphdr_buf[0] = BUNDLE_DATA;
    BundleDataHeader* dh = (BundleDataHeader*)(&tcphdr_buf[1]);
    memcpy(&dh->bundle_id, &bundle->bundleid_, sizeof(bundle->bundleid_));
    SDNV::encode(header_len + payload_len, &dh->total_length[0], sdnv_len);

    // Build up a two element iovec
    struct iovec iov[2];
    iov[0].iov_base = (char*)tcphdr_buf;
    iov[0].iov_len  = tcphdr_len;

    iov[1].iov_base = (char*)sndbuf_.buf();
    iov[1].iov_len  = header_len;
    
    // send off the preamble and the headers
    log_debug("send_bundle: sending bundle id %d "
              "tcpcl hdr len: %u, bundle header len: %u payload len: %u",
              bundle->bundleid_, (u_int)tcphdr_len, (u_int)header_len,
              (u_int)payload_len);

    int cc = sock_->writevall(iov, 2);

    if (cc == oasys::IOINTR) {
        log_info("send_bundle: interrupted while sending bundle header");
        break_contact(ContactEvent::USER);
        return false;
    
    } else if (cc != (int)tcphdr_len + header_len) {
        if (cc == 0) {
            log_err("send_bundle: remote side closed connection");
        } else {
            log_err("send_bundle: "
                    "error sending bundle header (wrote %d/%u): %s",
                    cc, (u_int)(tcphdr_len + header_len), strerror(errno));
        }

        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    // now loop through the the payload, sending blocks of data and
    // checking for incoming acks along the way
    while (payload_len > 0) {
        if (payload_len <= params_.writebuf_len_) {
            block_len = payload_len;
        } else {
            block_len = params_.writebuf_len_;
        }

        // grab the next payload chunk
        payload_data =
            bundle->payload_.read_data(payload_offset,
                                       block_len,
                                       sndbuf_.buf(block_len),
                                       BundlePayload::KEEP_FILE_OPEN);
        
        log_debug("send_bundle: sending %u byte block %p",
                  (u_int)block_len, payload_data);
        
        cc = sock_->writeall((char*)payload_data, block_len);

        if (cc == oasys::IOINTR) {
            log_info("send_bundle: interrupted while sending bundle block");
            break_contact(ContactEvent::USER);
            bundle->payload_.close_file();
            return false;
        
        } else if (cc != (int)block_len) {
            if (cc == 0) {
                log_err("send_bundle: remote side closed connection");
            } else {
                log_err("send_bundle: "
                        "error sending bundle block (wrote %d/%u): %s",
                        cc, (u_int)block_len, strerror(errno));
            }

            break_contact(ContactEvent::BROKEN);
            bundle->payload_.close_file();
            return false;
        }

        // update payload_offset and payload_len
        payload_offset += block_len;
        payload_len    -= block_len;
        
        // call poll() to check for any pending ack replies on the
        // socket with a timeout of zero, indicating that we don't
        // want to block
        int revents;
        cc = sock_->poll_sockfd(POLLIN, &revents, 0);

        if (cc == 1) {
            log_debug("send_bundle: data available on the socket");
            if (! handle_reply()) {
                return false;
            }
            
        } else if (cc == 0 || cc == oasys::IOTIMEOUT) {
            // nothing to do

        } else if (cc == oasys::IOINTR) {
            log_info("send_bundle: interrupted while checking for acks");
            break_contact(ContactEvent::USER);
            bundle->payload_.close_file();
            return false;

        } else {
            log_err("send_bundle: unexpected error while checking for acks");
            break_contact(ContactEvent::BROKEN);
            bundle->payload_.close_file();
            return false;
        }
    }

    // if we got here, we sent the whole bundle successfully, so if
    // we're not using acks, post an event for the router. if we are
    // using acks, the event is posted in handle_ack
    if (! params_.bundle_ack_enabled_) {
        inflight_.pop_front();
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_, payload_len, false));
    }

    bundle->payload_.close_file();
    return true;
}


/**
 * Receive one bundle over the wire. Note that immediately before this
 * call, we have just consumed the one byte BUNDLE_DATA typecode off
 * the wire.
 */
bool
TCPConvergenceLayer::Connection::recv_bundle()
{
    int cc;
    Bundle* bundle = new Bundle();
    BundleDataHeader datahdr;
    int header_len;
    u_int64_t total_len;
    int    sdnv_len = 0;
    size_t rcvd_len = 0;
    size_t acked_len = 0;
    size_t payload_len = 0;

    bool valid = false;
    bool recvok = false;

    log_debug("recv_bundle: consuming bundle headers...");

    // if we don't yet have enough bundle data, so read in as much as
    // we can, first making sure we have enough space in the buffer
    // for at least the bundle data header and the SDNV
    if (rcvbuf_.fullbytes() < sizeof(BundleDataHeader)) {
 incomplete_tcp_header:
        rcvbuf_.reserve(sizeof(BundleDataHeader) + 10);
        cc = sock_->timeout_read(rcvbuf_.end(), rcvbuf_.tailbytes(),
                                 params_.rtt_timeout_);
        if (cc < 0) {
            log_err("recv_bundle: error reading bundle headers: %s",
                    strerror(errno));
            delete bundle;
            return false;
        }

        log_debug("recv_bundle: read %d bytes...", cc);
        rcvbuf_.fill(cc);
        note_data_rcvd();
    }
        
    // parse out the BundleDataHeader
    if (rcvbuf_.fullbytes() < sizeof(BundleDataHeader)) {
        log_err("recv_bundle: read too short to encode data header");
        goto incomplete_tcp_header;
    }

    // copy out the data header but don't advance the buffer (yet)
    memcpy(&datahdr, rcvbuf_.start(), sizeof(BundleDataHeader));
    
    // parse out the SDNV that encodes the whole bundle length
    sdnv_len = SDNV::decode((u_char*)rcvbuf_.start() + sizeof(BundleDataHeader),
                            rcvbuf_.fullbytes() - sizeof(BundleDataHeader),
                            &total_len);
    if (sdnv_len < 0) {
        log_err("recv_bundle: read too short to encode SDNV");
        goto incomplete_tcp_header;
    }

    // got the full tcp header, so skip that much in the buffer
    rcvbuf_.consume(sizeof(BundleDataHeader) + sdnv_len);

 incomplete_bundle_header:
    // now try to parse the headers into the new bundle, which may
    // require reading in more data and possibly increasing the size
    // of the stream buffer
    header_len = BundleProtocol::parse_headers(bundle,
                                               (u_char*)rcvbuf_.start(),
                                               rcvbuf_.fullbytes());
    
    if (header_len < 0) {
        if (rcvbuf_.tailbytes() == 0) {
            rcvbuf_.reserve(rcvbuf_.size() * 2);
        }
        cc = sock_->timeout_read(rcvbuf_.end(), rcvbuf_.tailbytes(),
                                 params_.rtt_timeout_);
        if (cc < 0) {
            log_err("recv_bundle: error reading bundle headers: %s",
                    strerror(errno));
            delete bundle;
            return false;
        }
        rcvbuf_.fill(cc);
        note_data_rcvd();
        goto incomplete_bundle_header;
    }

    log_debug("recv_bundle: got valid bundle header -- "
              "sender bundle id %d, header_length %u, total_length %llu",
              datahdr.bundle_id, (u_int)header_len, total_len);
    rcvbuf_.consume(header_len);

    // all lengths have been parsed, so we can do some length
    // validation checks
    payload_len = bundle->payload_.length();
    if (total_len != header_len + payload_len) {
        log_err("recv_bundle: bundle length mismatch -- "
                "total_len %llu, header_len %u, payload_len %u",
                total_len, (u_int)header_len, (u_int)payload_len);
        delete bundle;
        return false;
    }

    // so far so good, now loop until we've read the whole payload
    // done with the rest. note that all reads have a timeout. note
    // also that we may have some payload data in the buffer
    // initially, so we check for that before trying to read more
    do {
        if (rcvbuf_.fullbytes() == 0) {
            // read a chunk of data
            ASSERT(rcvbuf_.data() == rcvbuf_.end());
            
            cc = sock_->timeout_read(rcvbuf_.data(),
                                     rcvbuf_.tailbytes(),
                                     params_.rtt_timeout_);
            if (cc == oasys::IOTIMEOUT) {
                log_warn("recv_bundle: timeout reading bundle data block");
                goto done;
            } else if (cc == oasys::IOEOF) {
                log_info("recv_bundle: eof reading bundle data block");
                goto done;
            } else if (cc < 0) {
                log_warn("recv_bundle: error reading bundle data block: %s",
                         strerror(errno));
                goto done;
            }
            rcvbuf_.fill(cc);
            note_data_rcvd();
        }
        
        log_debug("recv_bundle: got %u byte chunk, rcvd_len %u",
                  (u_int)rcvbuf_.fullbytes(), (u_int)rcvd_len);
        
        // append the chunk of data up to the maximum size of the
        // bundle (which may be empty) and update the amount received
        cc = std::min(rcvbuf_.fullbytes(), payload_len - rcvd_len);
        if (cc != 0) {
            bundle->payload_.append_data((u_char*)rcvbuf_.start(), cc);
            rcvd_len += cc;
            rcvbuf_.consume(cc);
        }
        
        // at this point, we can make at least a valid bundle fragment
        // from what we've gotten thus far (assuming reactive
        // fragmentation is enabled)
        valid = true;

        // check if we've read enough to send an ack
        if (rcvd_len - acked_len > params_.partial_ack_len_) {
            log_debug("recv_bundle: "
                      "got %u bytes acked %u, sending partial ack",
                      (u_int)rcvd_len, (u_int)acked_len);
            
            if (! send_ack(datahdr.bundle_id, rcvd_len)) {
                log_err("recv_bundle: error sending ack");
                goto done;
            }
            acked_len = rcvd_len;
        }
        
    } while (rcvd_len < payload_len);

    // if the sender requested a bundle ack and we haven't yet sent
    // one for the whole bundle in the partial ack check above, send
    // one now
    if (params_.bundle_ack_enabled_ && 
        (payload_len == 0 || (acked_len != rcvd_len)))
    {
        if (! send_ack(datahdr.bundle_id, payload_len)) {
            log_err("recv_bundle: error sending ack");
            goto done;
        }
    }
    
    recvok = true;

 done:
    bundle->payload_.close_file();
    
    if ((!valid) || (!recvok && !params_.reactive_frag_enabled_)) {
        // the bundle isn't valid or we didn't get the whole thing and
        // reactive fragmentation isn't enabled so just return
        if (bundle)
            delete bundle;
        
        return false;
    }

    log_debug("recv_bundle: "
              "new bundle id %d arrival, payload length %u (rcvd %u)",
              bundle->bundleid_, (u_int)payload_len, (u_int)rcvd_len);
    
    // inform the daemon that we got a valid bundle, though it may not
    // be complete (as indicated by passing the rcvd_len)
    ASSERT(rcvd_len <= bundle->payload_.length());
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, rcvd_len));
    
    return true;
}

/**
 * Send an ack message.
 */
bool
TCPConvergenceLayer::Connection::send_ack(u_int32_t bundle_id,
                                          size_t acked_len)
{
    char typecode = BUNDLE_ACK;
    
    BundleAckHeader ackhdr;
    ackhdr.bundle_id = bundle_id;
    ackhdr.acked_length = htonl(acked_len);

    struct iovec iov[2];
    iov[0].iov_base = &typecode;
    iov[0].iov_len  = 1;
    iov[1].iov_base = (char*)&ackhdr;
    iov[1].iov_len  = sizeof(BundleAckHeader);
    
    int total = 1 + sizeof(BundleAckHeader);
    int cc = sock_->writevall(iov, 2);
    if (cc != total) {
        log_err("recv_bundle: error sending ack (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    return true;
}


/**
 * Send a keepalive byte.
 */
bool
TCPConvergenceLayer::Connection::send_keepalive()
{
    char typecode = KEEPALIVE;
    int ret = sock_->write(&typecode, 1);

    if (ret == oasys::IOINTR) { 
        log_info("connection interrupted");
        break_contact(ContactEvent::USER);
        return false;
    }

    if (ret != 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    ::gettimeofday(&keepalive_sent_, 0);
    
    return true;
}



/**
 * Note that we observed some data inbound on this connection.
 */
void
TCPConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting receipt of data");
    ::gettimeofday(&data_rcvd_, 0);
}


/**
 * Handle an incoming ack from the receive buffer.
 */
int
TCPConvergenceLayer::Connection::handle_ack()
{
    if (inflight_.empty()) {
        log_err("handle_ack: received ack but no bundles in flight!");
 protocol_error:
        break_contact(ContactEvent::BROKEN);
        return EINVAL;
    }
    InFlightBundle* ifbundle = &inflight_.front();

    // now see if we got a complete ack header
    if (rcvbuf_.fullbytes() < (1 + sizeof(BundleAckHeader))) {
        log_debug("handle_ack: not enough space in buffer (got %u, need %u...",
                  (u_int)rcvbuf_.fullbytes(), (u_int)sizeof(BundleAckHeader));
        return ENOMEM;
    }

    // if we do, copy it out, after skipping the typecode
    BundleAckHeader ackhdr;
    memcpy(&ackhdr, rcvbuf_.start() + 1, sizeof(BundleAckHeader));
    rcvbuf_.consume(1 + sizeof(BundleAckHeader));

    Bundle* bundle = ifbundle->bundle_.object();
    size_t new_acked_len = ntohl(ackhdr.acked_length);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("handle_ack: got ack length %d for bundle id %d length %d",
              (u_int)new_acked_len, ackhdr.bundle_id, (u_int)payload_len);
    
    if (ackhdr.bundle_id != bundle->bundleid_) {
        log_err("handle_ack: error: bundle id mismatch %d != %d",
                ackhdr.bundle_id, bundle->bundleid_);
        goto protocol_error;
    }

    if (new_acked_len < ifbundle->acked_len_ || new_acked_len > payload_len)
    {
        log_err("handle_ack: invalid acked length %u (acked %u, bundle %u)",
                (u_int)new_acked_len, (u_int)ifbundle->acked_len_,
                (u_int)payload_len);
        goto protocol_error;
    }

    // check if we're done with the bundle and if we need to unblock
    // the link (assuming we're not doing pipelining)
    if (new_acked_len == payload_len) {
        inflight_.pop_front();
        
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_, payload_len, true));
        
        if ((!params_.pipeline_) && (contact_->link()->state() == Link::BUSY)) {
            BundleDaemon::post(
                new LinkStateChangeRequest(contact_->link(), Link::AVAILABLE,
                                           ContactEvent::NO_INFO));
        }

    } else {
        ifbundle->acked_len_ = new_acked_len;
    }

    return 0;
}

/**
 * Helper function to format and send a contact header.
 */
bool
TCPConvergenceLayer::Connection::send_contact_header()
{
    ContactHeader contacthdr;
    contacthdr.magic = htonl(MAGIC);
    contacthdr.version = TCPCL_VERSION;
    
    contacthdr.flags = 0;
    if (params_.bundle_ack_enabled_)
        contacthdr.flags |= BUNDLE_ACK_ENABLED;

    if (params_.reactive_frag_enabled_)
        contacthdr.flags |= REACTIVE_FRAG_ENABLED;
    
    if (params_.receiver_connect_)
        contacthdr.flags |= RECEIVER_CONNECT;
    
    contacthdr.partial_ack_len 	  = htons(params_.partial_ack_len_);
    contacthdr.keepalive_interval = htons(params_.keepalive_interval_);
    contacthdr.xx__unused         = 0;
    
    int cc = sock_->writeall((char*)&contacthdr, sizeof(ContactHeader));
    if (cc != sizeof(ContactHeader)) {
        log_err("error writing contact header (wrote %d/%u): %s",
                cc, (u_int)sizeof(ContactHeader), strerror(errno));
        return false;
    }

    return true;
}


/**
 * Helper function to receive a contact header (and potentially the
 * local identifier), verifying the version and magic bits and doing
 * parameter negotiation.
 */
bool
TCPConvergenceLayer::Connection::recv_contact_header(int timeout)
{
    ContactHeader contacthdr;
    
    ASSERT(timeout != 0);
    int cc = sock_->timeout_readall((char*)&contacthdr,
                                    sizeof(ContactHeader),
                                    timeout);
        
    if (cc == oasys::IOTIMEOUT) {
        log_warn("timeout reading contact header");
        return false;
    }
    
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header (read %d/%u): %d-%s", 
                cc, (u_int)sizeof(ContactHeader), 
                errno, strerror(errno));
        return false;
    }

    /*
     * Check for valid magic number and version.
     */
    if (ntohl(contacthdr.magic) != MAGIC) {
        log_warn("remote sent magic number 0x%.8x, expected 0x%.8x "
                 "-- disconnecting.", contacthdr.magic,
                 MAGIC);
        return false;
    }

    if (contacthdr.version != TCPCL_VERSION) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", contacthdr.version,
                 TCPCL_VERSION);
        return false;
    }

    /*
     * Now do parameter negotiation.
     */
    params_.partial_ack_len_	= MIN(params_.partial_ack_len_,
                                      ntohs(contacthdr.partial_ack_len));
    
    params_.keepalive_interval_ = MIN(params_.keepalive_interval_,
                                      ntohs(contacthdr.keepalive_interval));

    params_.bundle_ack_enabled_ = params_.bundle_ack_enabled_ &&
                                  (contacthdr.flags & BUNDLE_ACK_ENABLED);

    params_.reactive_frag_enabled_ = params_.reactive_frag_enabled_ &&
                                     (contacthdr.flags & REACTIVE_FRAG_ENABLED);

    if (initiate_) {
        // check that the passive side didn't try to set the
        // receiver_connect option
        if (contacthdr.flags & RECEIVER_CONNECT) {
            log_err("receiver_connect option set by passive acceptor");
            return false;
        }

        // direction should already have been set
        ASSERT(direction_ != UNKNOWN);

    } else {
        // for the passive acceptor, set the direction based on the
        // received option for receiver_connect
        ASSERT(direction_ == UNKNOWN);

        if (contacthdr.flags & RECEIVER_CONNECT) {
            params_.receiver_connect_ = true;
            queue_ = new BlockingBundleList(logpath_);
            queue_->notifier()->logpath_appendf("/queue");
            sock_->set_notifier(new oasys::Notifier(logpath_));
            direction_ = SENDER;
        } else {
            direction_ = RECEIVER;
        }
    }
    
    note_data_rcvd();
    return true;
}


/**
 * Helper function to send our local address, used for the receiver
 * connect.
 */
bool
TCPConvergenceLayer::Connection::send_address()
{
    AddressHeader addresshdr;
    addresshdr.addr       = htonl(sock_->local_addr());
    addresshdr.port       = htons(sock_->local_port());
    addresshdr.xx__unused = 0;
        
    log_debug("sending address header %s:%d...",
              intoa(sock_->local_addr()), sock_->local_port());

    int cc = sock_->writeall((char*)&addresshdr, sizeof(AddressHeader));
    if (cc != sizeof(AddressHeader)) {
        log_err("error writing address header (wrote %d/%u): %s",
                cc, (u_int)sizeof(AddressHeader), strerror(errno));
        return false;
    }
    
    return true;
}


/**
 * Helper function to receive the peer's address, used for the
 * receiver connect option to match the connection with a link.
 *
 * Stores the retrieved address and port in the parameters structure.
 */
bool
TCPConvergenceLayer::Connection::recv_address(int timeout)
{
    AddressHeader addresshdr;

    log_debug("retrieving address header...");

    int cc = sock_->timeout_readall((char*)&addresshdr,
                                    sizeof(AddressHeader),
                                    timeout);
            
    if (cc == oasys::IOTIMEOUT) {
        log_warn("timeout reading address header");
        return false;
    }

    if (cc != sizeof(AddressHeader)) {
        log_err("error reading address header (read %d/%u): %s",
                cc, (u_int)sizeof(AddressHeader), strerror(errno));
        return false;
    }

    params_.remote_addr_ = ntohl(addresshdr.addr);
    params_.remote_port_ = ntohs(addresshdr.port);

    return true;
}


/**
 * Active connect side of the initial handshake. First connect to the
 * peer side, then exchange ContactHeaders and negotiate session
 * parameters.
 */
bool
TCPConvergenceLayer::Connection::connect()
{
    // first of all, connect to the receiver side
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    log_debug("connect: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());

    int ret = sock_->timeout_connect(sock_->remote_addr(),
                                     sock_->remote_port(),
                                     params_.rtt_timeout_);

    if (ret == oasys::IOINTR) {
        log_debug("connect thread interrupted");
        return false;
    }
    
    else if (ret == oasys::IOERROR) {
        log_info("connection attempt to %s:%d failed... ",
                 intoa(sock_->remote_addr()), sock_->remote_port());
        return false;
    }
    
    else if (ret == oasys::IOTIMEOUT) {
        log_info("connection attempt to %s:%d timed out...",
                 intoa(sock_->remote_addr()), sock_->remote_port());
        return false;
    }
        
    log_debug("connect: connection established, sending contact header...");
    if (!send_contact_header())
        return false;

    log_debug("connect: waiting for contact header reply...");
    if (!recv_contact_header(params_.rtt_timeout_))
        return false;
    
    if (params_.receiver_connect_) {
        ASSERT(direction_ == RECEIVER);
        log_debug("connect: receiver_connect sending local address");
        if (!send_address()) {
            return false;
        }
    }
    
    return true;
}


bool
TCPConvergenceLayer::Connection::open_opportunistic_link()
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    
    oasys::ScopeLock l(cm->lock(), "TCPCL::open_opportunistic_link");
    LinkSet::const_iterator iter;
    Link* link = NULL;
    
    for (iter = cm->links()->begin(); iter != cm->links()->end(); ++iter) {
        link = *iter;
        
        // ignore non-tcp links and non-opportunistic links
        if (link->clayer() != cl_) {
            continue;
        }

        if (link->type() != Link::OPPORTUNISTIC) {
            continue;
        }
        
        // grab the parameters structure and look for a matching
        // remote_addr and remote_port
        Params* link_params = (Params*)link->cl_info();
        ASSERT(link_params);

        log_debug("open_opportunistic_link: checking link *%p %s:%d", link,
                  intoa(link_params->remote_addr_),
                  link_params->remote_port_);
        
        if (link_params->remote_addr_ == params_.remote_addr_ &&
            link_params->remote_port_ == params_.remote_port_)
        {
            // found it!
            break;
        }

        link = NULL;
    }

    if (link == NULL) {
        log_err("receiver connect can't find matching link for peer %s:%d",
                intoa(params_.remote_addr_), params_.remote_port_);
        return false;
    }
    
    if (link->state() != Link::UNAVAILABLE) {
        log_err("receiver connect link *%p in unexpected state %s", link,
                Link::state_to_str(link->state()));
        return false;
    }

    if (direction_ != SENDER) {
        log_err("receiver_connect connection for link *%p not the sender",
                link);
        return false;
    }
    
    // create a new contact if this is the first time the link is open
    // XXX/demmer this seems kinda bogus...
    if (link->contact() == NULL) {
        contact_ = new Contact(link);
        link->set_contact(contact_);
    } else {
        ASSERT(contact_ == NULL);
        contact_ = link->contact();
    }
    
    // a contact up event is delivered at the start of the send loop,
    // but we need to first put the link into OPENING state
    link->set_state(Link::OPENING);
    
    return true;
}


/**
 * Passive accept side of the initial handshake.
 */
bool
TCPConvergenceLayer::Connection::accept()
{
    ASSERT(contact_ == NULL);

    log_debug("accept: sending contact header...");
    if (!send_contact_header())
        return false;

    log_debug("accept: waiting for peer contact header...");
    if (!recv_contact_header(params_.rtt_timeout_))
        return false;
    
    if (params_.receiver_connect_) {
        ASSERT(direction_ == SENDER);
        
        log_debug("accept: receiver_connect retrieving local address");
        if (!recv_address(params_.rtt_timeout_)) {
            return false;
        }

        if (!open_opportunistic_link()) {
            return false;
        }
    }
    
    return true;
}


/**
 * Send an event to the system indicating that this contact is broken
 * and close the side of the connection.
 * 
 * This results in the Connection thread stopping and the system
 * calling the link->close() call which cleans up the connection.
 *
 * If this is the sender-side, we keep a pointer to the contact and
 * assuming the contact isn't in the process of being closed, we post
 * a request that it be closed.
 */
void
TCPConvergenceLayer::Connection::break_contact(ContactEvent::reason_t reason)
{
    ASSERT(sock_);
    
    // do not log an error when we fail to write, since the
    // SHUTDOWN is basically advisory. Expected errors here
    // include a short write due to socket already closed,
    // or maybe EAGAIN due to socket not ready for write.
    char typecode = SHUTDOWN;
    if (sock_->state() != oasys::IPSocket::CLOSED) {
        sock_->set_nonblocking(true);
        sock_->write(&typecode, 1);
        sock_->close();
    }

    if (contact_) {
        // drain the inflight queue, posting transmitted events for all
        // bundles that haven't yet been fully acked
        while (! inflight_.empty()) {
            InFlightBundle* inflight = &inflight_.front();

            if (inflight->acked_len_ != 0) {
                BundleDaemon::post(
                    new BundleTransmittedEvent(inflight->bundle_.object(),
                                               contact_,
                                               inflight->acked_len_,
                                               true));
            } else {
                BundleDaemon::post(
                    new BundleTransmittedEvent(inflight->bundle_.object(),
                                               contact_,
                                               inflight->bundle_->payload_.length(),
                                               false));
            }

            inflight_.pop_front();
        }

        if (reason != ContactEvent::USER)
        {
            // if the connection isn't being closed by the user, and
            // the link is open, we need to notify the daemon.
            // typically, we then just bounce back to the main run
            // loop to try to re-establish the connection... 
            if (contact_->link()->isopen())
            {
                BundleDaemon::post(new ContactDownEvent(contact_, reason));
            }
        }
                 
        // since we delete ourself using DELETE_ON_EXIT, we need to
        // signal to the main thread that we've quit. we indicate as
        // such by clearing the cl_info slot in the Contact
        if (contact_->cl_info() != NULL) {
            ASSERT(contact_->cl_info() == this);
            contact_->set_cl_info(NULL);
        }
        
    } else {
        // if there's no contact, there shouldn't be any in
        // flight bundles
        ASSERT(inflight_.empty());
    }

    // if we're the passive acceptor, or the link went idle, we want
    // to exit the main loop, so set the should_stop flag as an
    // indicator of this
    if (!initiate_ || (reason == ContactEvent::IDLE)) {
        set_should_stop();
    }
}


/**
 * Handle an incoming message from the receiver side (i.e. an ack,
 * keepalive, or shutdown. We expect that the caller has previously
 * done a poll(), so we expect data to actually be on the socket. As
 * such, we read as much as we can into the receive buffer and process
 * until we run out of complete messages.
 */
bool
TCPConvergenceLayer::Connection::handle_reply()
{
    // Reserve at least one byte of space, which has the side-effect
    // of moving up any needed buffer space if we're at the end.
    //
    // However, the buffer should have been pre-reserved with the
    // configured receive buffer size.
    rcvbuf_.reserve(1);

    int ret = sock_->read(rcvbuf_.end(), rcvbuf_.tailbytes());

    if (ret == oasys::IOINTR) {
        log_info("connection interrupted");
        break_contact(ContactEvent::USER);
        return false;
    }

    if (ret < 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return false;
    }

    rcvbuf_.fill(ret);
    note_data_rcvd();

    do {
        char typecode = *rcvbuf_.start();
        
        if (typecode == BUNDLE_ACK) {
            int ret = handle_ack();

            if (ret == 0) {
                // complete ack, nothing to do                
                
            } else if (ret == ENOMEM) {
                break; // incomplete ack message
            
            } else if (ret == EINVAL) {
                return false; // internal error, break contact was called

            } else {
                PANIC("unexpected return %d from handle_ack", ret);
            }
            
        } else if (typecode == KEEPALIVE) {
            rcvbuf_.consume(1);
            ::gettimeofday(&data_rcvd_, 0);

        } else if (typecode == SHUTDOWN) {
            rcvbuf_.consume(1);
            log_info("got shutdown request from other side");
            break_contact(ContactEvent::SHUTDOWN);
            return false;
        
        } else {
            log_err("got unexpected frame code %d", typecode);
            break_contact(ContactEvent::BROKEN);
            return false;
        }
        
    } while (rcvbuf_.fullbytes() > 0);
    
    return true;
}


/**
 * The sender side main thread loop.
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    // someone should already have established the session
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);

    // store our state in the contact's cl info slot
    ASSERT(contact_);
    ASSERT(contact_->cl_info() == NULL);
    contact_->set_cl_info(this);
    
    // inform the daemon that the contact is available
    BundleDaemon::post(new ContactUpEvent(contact_));

    // reserve space in the buffers
    rcvbuf_.reserve(params_.readbuf_len_);
    sndbuf_.reserve(params_.writebuf_len_);
    
    log_info("connection established -- (keepalive time %d seconds)",
             params_.keepalive_interval_);

    // build up a poll vector since we need to block below on input
    // from the socket and the bundle list
    struct pollfd pollfds[2];

    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd            = queue_->notifier()->read_fd();
    bundle_poll->events        = POLLIN;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd            = sock_->fd();
    sock_poll->events        = POLLIN;

    // flag for whether or not we're in idle state
    bool idle = false;
    
    // main loop
    while (true) {
        // keep track of the time we got data and idle
        struct timeval now;
        struct timeval idle_start;

        BundleRef bundle("TCPCL::send_loop temporary");

        // if we've been interrupted, then the link should close
        if (should_stop()) {
            break_contact(ContactEvent::USER);
            return;
        }

        // pop the bundle (if any) off the queue, which gives us a
        // local reference on it
        bundle = queue_->pop_front();

        if (bundle != NULL) {
            // clear the idle bit
            idle = false;
            
            // if the link is currently busy, notify the router that
            // we're not any more. note that if we're not pipelining,
            // this is done by handle_ack once the bundle has been
            // acknowledged
            if (params_.pipeline_ && (contact_->link()->state() == Link::BUSY))
            {
                BundleDaemon::post(
                    new LinkStateChangeRequest(contact_->link(),
                                               Link::AVAILABLE,
                                               ContactEvent::NO_INFO));
            }
            
            // send the bundle off and remove our local reference
            bool sentok = send_bundle(bundle.object());
            bundle = NULL;
            
            // if the last transmission wasn't completely successful,
            // it's time to break the contact
            if (!sentok) {
                return;
            }

            // otherwise, we loop back to the beginning and check for
            // more bundles on the queue as an optimization to check
            // the list before calling poll
            continue;
        }

        // Check for whether or not we've just become idle, in which
        // case we record the current time
        if (!idle && inflight_.empty()) {
            idle = true;
            ::gettimeofday(&idle_start, 0);
        }
        
        // No bundle, so we'll block for:
        // 1) some activity on the socket, (i.e. keepalive, ack, or shutdown)
        // 2) the bundle list notifier that indicates new bundle to send
        //
        // Note that we pass the negotiated keepalive timer (if set)
        // as the timeout to the poll call so we know when we should
        // send a keepalive
        pollfds[0].revents = 0;
        pollfds[1].revents = 0;

        int timeout = params_.keepalive_interval_ * 1000;
        if (timeout == 0) {
            timeout = -1; // block forever
        }

        log_debug("send_loop: calling poll (timeout %d)", timeout);
        int cc = oasys::IO::poll_multiple(pollfds, 2, timeout,
                                          sock_->get_notifier(), logpath_);
        
        if (cc == oasys::IOINTR) {
            log_info("send_loop: interrupted from poll, breaking connection");
            break_contact(ContactEvent::USER);
            return;
        }

        // check for a message from the other side
        if (sock_poll->revents != 0) {
            if ((sock_poll->revents & POLLHUP) ||
                (sock_poll->revents & POLLERR))
            {
                log_info("send_loop: remote connection error");
                break_contact(ContactEvent::BROKEN);
                return;
            }
            
            if (! (sock_poll->revents & POLLIN)) {
                PANIC("unknown revents value 0x%x", sock_poll->revents);
            }
            
            log_debug("send_loop: data available on the socket");
            if (!handle_reply()) {
                return;
            }
        }

        // check if it's time to send a keepalive
        if (cc == oasys::IOTIMEOUT) {
            log_debug("timeout from poll, sending keepalive");
            ASSERT(params_.keepalive_interval_ != 0);
            send_keepalive();
            continue;
        }

        // check that it hasn't been too long since the other side
        // sent us some data
        ::gettimeofday(&now, 0);
        u_int elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > (2 * params_.keepalive_interval_ * 1000)) {
            log_info("send_loop: no data heard for %d msecs "
                     "(sent %u.%u, rcvd %u.%u, now %u.%u) -- closing contact",
                     elapsed,
                     (u_int)keepalive_sent_.tv_sec,
                     (u_int)keepalive_sent_.tv_usec,
                     (u_int)data_rcvd_.tv_sec, (u_int)data_rcvd_.tv_usec,

                     (u_int)now.tv_sec, (u_int)now.tv_usec);
            
            break_contact(ContactEvent::BROKEN);
            return;
        }

        // check if the connection has been idle for too long
        // (on demand links only)
        if (idle && (contact_->link()->type() == Link::ONDEMAND)) {
            u_int idle_close_time =
                ((OndemandLink*)contact_->link())->idle_close_time_;
            
            elapsed = TIMEVAL_DIFF_MSEC(now, idle_start);
            if (idle_close_time != 0 && (elapsed > idle_close_time * 1000))
            {
                log_info("connection idle for %d msecs, closing.", elapsed);
                set_should_stop();
                break_contact(ContactEvent::IDLE);
                return;
            } else {
                log_debug("connection not idle: %d <= %d",
                          elapsed, idle_close_time * 1000);
            }
        }
    }
}

void
TCPConvergenceLayer::Connection::recv_loop()
{
    int timeout;
    int ret;
    char typecode;

    // reserve space in the buffer
    rcvbuf_.reserve(params_.readbuf_len_);
    
    while (1) {
        // if there's nothing in the buffer,
        // block waiting for the one byte typecode
        if (rcvbuf_.fullbytes() == 0) {
            ASSERT(rcvbuf_.end() == rcvbuf_.data()); // sanity
            
            timeout = 2 * params_.keepalive_interval_ * 1000;
            log_debug("recv_loop: blocking on frame... (timeout %d)", timeout);
            
            ret = sock_->timeout_read(rcvbuf_.end(), rcvbuf_.tailbytes(), timeout);
            
            if (ret == oasys::IOEOF || ret == oasys::IOERROR) {
                log_info("recv_loop: remote connection unexpectedly closed");
 shutdown:
                break_contact(ContactEvent::BROKEN);
                return;
                
            } else if (ret == oasys::IOTIMEOUT) {
                log_info("recv_loop: no message heard for > %d msecs -- "
                         "breaking contact", timeout);
                goto shutdown;
            }

            rcvbuf_.fill(ret);
            note_data_rcvd();
        }

        typecode = *rcvbuf_.start();
        rcvbuf_.consume(1);
        
        log_debug("recv_loop: got frame packet type 0x%x...", typecode);

        if (typecode == SHUTDOWN) {
            break_contact(ContactEvent::SHUTDOWN);
            return;
        }
        
        if (typecode == KEEPALIVE) {
            log_debug("recv_loop: " "got keepalive, sending response");

            if (!send_keepalive()) {
                return;
            }
            continue;
        }
        
        if (typecode != BUNDLE_DATA) {
            log_err("recv_loop: "
                    "unexpected typecode 0x%x waiting for BUNDLE_DATA",
                    typecode);
            goto shutdown;
        }
        
        // process the bundle
        if (! recv_bundle()) {
            goto shutdown;
        }
     }
}

} // namespace dtn
