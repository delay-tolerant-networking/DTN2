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
#include <oasys/util/StreamBuffer.h>
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
    defaults_.bundle_ack_enabled_ 	= true;
    defaults_.reactive_frag_enabled_ 	= true;
    defaults_.receiver_connect_		= false;
    defaults_.partial_ack_len_		= 1024;
    defaults_.writebuf_len_ 		= 32768;
    defaults_.readbuf_len_ 		= 32768;
    defaults_.keepalive_interval_	= 2;
    defaults_.idle_close_time_ 	 	= 30;
    defaults_.connect_retry_		= 10000; // msecs
    defaults_.connect_timeout_		= 30000; // msecs
    defaults_.rtt_timeout_		= 5000;  // msecs
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
    p.addopt(new oasys::UIntOpt("idle_close_time", &params->idle_close_time_));
    p.addopt(new oasys::UIntOpt("connect_retry", &params->connect_retry_));
    p.addopt(new oasys::UIntOpt("connect_timeout", &params->connect_timeout_));
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

        // XXX/demmer again, there should be some structure to manage
        // the receiver side of connections
        conn->set_flag(oasys::Thread::DELETE_ON_EXIT);
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
    
    link->set_cl_info(params);

    if (params->receiver_connect_ && (link->type() != Link::OPPORTUNISTIC)) {
        log_err("receiver_connect option can only be used with "
                "opportunistic links");
        return false;
    }

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
    contact->set_cl_info(conn);
    
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
        }
            
        while (!conn->is_stopped()) {
            log_debug("waiting for connection thread to stop...");
            usleep(100000);
            conn->interrupt_from_io();
            oasys::Thread::yield();
        }
        
        log_debug("connection thread stopped...");

        delete conn;
        contact->set_cl_info(NULL);
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
    contact->link()->set_state(Link::BUSY);

    Connection* conn = (Connection*)contact->cl_info();
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
    : IOHandlerBase(new oasys::Notifier()), 
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
    
    // XXX/demmer this should really make a Contact equivalent or
    // something like that and then not DELETE_ON_EXIT
    conn->set_flag(Thread::DELETE_ON_EXIT);
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
    sock_->set_notifier(new oasys::Notifier());

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
 * state, it may initiate a connection to the other side, then calls
 * one of send_loop or recv_loop.
 */
void
TCPConvergenceLayer::Connection::run()
{
    if (initiate_)
    {
        if (! connect()) {
            /*
             * If the connection fails, assuming the link is not
             * already in the process of being broken we tell the
             * daemon to close the link with a BROKEN reason code. If
             * it succeeds, then the first thing that send_loop() does
             * is post the ContactUpEvent.
             */
            log_debug("connection failed");

            if (contact_ != NULL && !contact_->link()->isclosing()) {
                BundleDaemon::post(
                    new LinkStateChangeRequest(contact_->link(),
                                               Link::CLOSING,
                                               ContactDownEvent::BROKEN));
            }
            
            return; // trigger a deletion
        }
    } else {
        /*
         * If accepting a connection failed, we just return which
         * triggers the thread to be deleted and therefore to clean up
         * our state.
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

    log_debug("connection thread main loop complete -- thread exiting");
}


/**
 * Send one bundle over the wire.
 *
 * Return true if the bundle was sent successfully, false if not.
 */
bool
TCPConvergenceLayer::Connection::send_bundle(Bundle* bundle, size_t* acked_len)
{
    int header_len;
    size_t block_len;
    size_t payload_len = bundle->payload_.length();
    size_t payload_offset = 0;
    const u_char* payload_data;
    oasys::StreamBuffer buf(params_.writebuf_len_);

    // Each bundle is preceded by a BUNDLE_DATA typecode and then a
    // BundleDataHeader that is variable-length since it includes an
    // SDNV for the total bundle length. So, leave some space at the
    // beginning of the buffer that can be filled in with the
    // appropriate preamble once we know the actual bundle header
    // length.
    size_t space = 32;

 retry_headers:
    header_len = BundleProtocol::format_headers(bundle,
                                                (u_char*)buf.data() + space,
                                                buf.size() - space);
    if (header_len < 0) {
        log_debug("send_bundle: bundle header too big for buffer len %d -- "
                  "doubling size and retrying",
                  params_.writebuf_len_);
        buf.reserve(buf.size() * 2);
        goto retry_headers;
    }
    
    size_t sdnv_len = SDNV::encoding_len(header_len + payload_len);
    size_t tcphdr_len = 1 + sizeof(BundleDataHeader) + sdnv_len;
    
    if (space < tcphdr_len) {
        // this is unexpected, but we can handle it
        log_err("send_bundle: bundle frame header too big for space of %u",
                (u_int)space);
        space = space * 2;
        goto retry_headers;
    }
    
    // Now fill in the type code and the bundle data header (with the
    // sdnv for the total bundle length)
    char* bp = buf.data() + space - tcphdr_len;
    *bp++ = BUNDLE_DATA;
    
    BundleDataHeader* dh = (BundleDataHeader*)bp;
    memcpy(&dh->bundle_id, &bundle->bundleid_, sizeof(bundle->bundleid_));
    bp += sizeof(BundleDataHeader);
    
    int len = SDNV::encode(header_len + payload_len, (u_char*)bp, sdnv_len);
    bp += len;
    ASSERT(bp == buf.data() + space);
    bp -= tcphdr_len; // reset to the beginning of the encoding (not
                      // necessarily the start of the buffer)

    // send off the preamble and the headers
    log_debug("send_bundle: sending bundle id %d "
              "tcpcl hdr len: %u, bundle header len: %u payload len: %u",
              bundle->bundleid_, (u_int)tcphdr_len, (u_int)header_len,
              (u_int)payload_len);

    int cc = sock_->writeall(bp, tcphdr_len + header_len);
    if (cc != (int)tcphdr_len + header_len) {
        log_err("send_bundle: error sending bundle header (wrote %d/%u): %s",
                cc, (u_int)(tcphdr_len + header_len), strerror(errno));
        bundle->payload_.close_file();
        return false;
    }
    
    // now loop through the the payload, sending blocks and checking
    // for acks as they come in.
    bool sentok = false;
    bool writeblocked = false;
    
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
                                       (u_char*)buf.data(),
                                       BundlePayload::KEEP_FILE_OPEN);
        
        log_debug("send_bundle: sending %u byte block %p",
                  (u_int)block_len, payload_data);
        
        cc = sock_->write((char*)payload_data, block_len);
        
        if (cc == 0) {
            // eof from the other side
            log_info("send_bundle: remote side closed connection");
            goto done;
        }
        
        else if (cc < 0) {
            if (errno == EWOULDBLOCK) {
                // socket buffer full. we'll loop down below waiting
                // for it to drain
                log_debug("send_bundle: write returned EWOULDBLOCK");
                writeblocked = true;
                goto blocked;
                
            } else {
                // some other error, bail
                log_warn("send_bundle: "
                         "error writing bundle block (wrote %d/%u): %s",
                         cc, (u_int)block_len, strerror(errno));
                goto done;
            }
        }
        
        if (cc < (int)block_len) {
            // unexpected short write -- just adjust block_len to
            // reflect the amount actually written and fall through
            log_warn("send_bundle: "
                     "short write sending bundle block (wrote %d/%u): %s",
                     cc, (u_int)block_len, strerror(errno));

            block_len = cc;
        }
        
        // update payload_offset and payload_len
        payload_offset += block_len;
        payload_len    -= block_len;

        // call poll() to check for any pending ack replies on the
        // socket, and if we're write blocked, to check for an
        // indication that the socket buffer has drained. note that if
        // we're not actually write blocked, the call should return
        // immediately, but will still give us the indication of
        // pending data to read.
 blocked:
        int revents;
        do {
            cc = sock_->poll_sockfd(POLLIN | POLLOUT, &revents, 
                                    params_.rtt_timeout_);
            if (cc == oasys::IOTIMEOUT) {
                log_info("send_bundle: "
                         "timeout waiting for ack or write-ready");
                goto done;
            } else if (cc == oasys::IOINTR) {
                PANIC("Handle intr?");
            } else if (cc < 0) {
                log_warn("send_bundle: error waiting for ack: %s",
                         strerror(errno));
                goto done;
            }

            ASSERT(cc == 1);
            if (revents & POLLIN) {
                cc = handle_ack(bundle, acked_len);
                
                if (cc == oasys::IOERROR) {
                    goto done;
                }
                ASSERT(cc == 1);
                
                log_debug("send_bundle: got ack for %d/%u -- "
                          "looping to send more", (u_int)*acked_len,
                          (u_int)bundle->payload_.length());
            }
            
            if (!(revents & POLLOUT)) {
                log_debug("poll returned write not ready");
                writeblocked = true;
            }

            else if (writeblocked && (revents & POLLOUT)) {
                log_debug("poll returned write ready");
                writeblocked = false;
            }
            
        } while (writeblocked);
    }

    // now we've written the whole payload, so loop and block until we
    // get all the acks, but cap the wait based on rtt_timeout
    payload_len = bundle->payload_.length();

    while (*acked_len < payload_len) {
        log_debug("send_bundle: acked %d/%d, waiting for more",
                  (u_int)*acked_len, (u_int)payload_len);

        cc = sock_->poll_sockfd(POLLIN, NULL, params_.rtt_timeout_);
        if (cc < 0) {
            log_warn("send_bundle: error waiting for ack: %s",
                     strerror(errno));
            goto done;
        }
        
        if (cc == 0) {
            log_info("send_bundle: timeout waiting for ack");
            goto done;
        }

        ASSERT(cc == 1);
        
        cc = handle_ack(bundle, acked_len);
        if (cc == oasys::IOERROR) {
            goto done;
        }
        ASSERT(cc == 1);
        
        log_debug("send_bundle: got ack for %d/%d",
                  (u_int)*acked_len, (u_int)payload_len);
    }
    
    sentok = true;

 done:
    bundle->payload_.close_file();
        
    if (*acked_len > 0) {
        ASSERT(!sentok || (*acked_len == payload_len));
    }
    
    // all done!
    return sentok;
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
    u_int64_t bundle_len;
    size_t rcvd_len = 0;
    size_t acked_len = 0;
    size_t payload_len = 0;

    oasys::StreamBuffer buf(params_.readbuf_len_);

    bool valid = false;
    bool recvok = false;

    log_debug("recv_bundle: reading bundle headers...");

 incomplete_tcp_header:
    // read in whatever has shown up on the wire, up to the maximum
    // length of the stream buffer
    cc = sock_->timeout_read(buf.end(), buf.tailbytes(), params_.rtt_timeout_);
    if (cc < 0) {
        log_err("recv_bundle: error reading bundle headers: %s",
                strerror(errno));
        delete bundle;
        return false;
    }
    buf.fill(cc);
    note_data_rcvd();
    
    // parse out the BundleDataHeader
    if (buf.fullbytes() < sizeof(BundleDataHeader)) {
        log_err("recv_bundle: read too short to encode data header");
        goto incomplete_tcp_header;
    }
        
    memcpy(&datahdr, buf.start(), sizeof(BundleDataHeader));

    // parse out the SDNV that encodes the whole bundle length
    int sdnv_len = SDNV::decode((u_char*)buf.start() + sizeof(BundleDataHeader),
                                buf.fullbytes() - sizeof(BundleDataHeader),
                                &bundle_len);
    if (sdnv_len < 0) {
        log_err("recv_bundle: read too short to encode SDNV");
        goto incomplete_tcp_header;
    }
    buf.consume(sizeof(BundleDataHeader) + sdnv_len);

 incomplete_bundle_header:
    // now try to parse the headers into the new bundle, which may
    // require reading in more data and possibly increasing the size
    // of the stream buffer
    header_len = BundleProtocol::parse_headers(bundle,
                                               (u_char*)buf.start(),
                                               buf.fullbytes());
    
    if (header_len < 0) {
        if (buf.tailbytes() == 0) {
            buf.reserve(buf.size() * 2);
        }
        cc = sock_->timeout_read(buf.end(), buf.tailbytes(),
                                 params_.rtt_timeout_);
        if (cc < 0) {
            log_err("recv_bundle: error reading bundle headers: %s",
                    strerror(errno));
            delete bundle;
            return false;
        }
        buf.fill(cc);
        note_data_rcvd();
        goto incomplete_bundle_header;
    }

    log_debug("recv_bundle: got valid bundle header -- "
              "sender bundle id %d, header_length %u, bundle_length %llu",
              datahdr.bundle_id, (u_int)header_len, bundle_len);
    buf.consume(header_len);

    // all lengths have been parsed, so we can do some length
    // validation checks
    payload_len = bundle->payload_.length();
    if (bundle_len != header_len + payload_len) {
        log_err("recv_bundle: bundle length mismatch -- "
                "bundle_len %llu, header_len %u, payload_len %u",
                bundle_len, (u_int)header_len, (u_int)payload_len);
        delete bundle;
        return false;
    }

    // so far so good, now loop until we've read the whole payload
    // done with the rest. note that all reads have a timeout. note
    // also that we may have some payload data in the buffer
    // initially, so we check for that before trying to read more
    do {
        if (buf.fullbytes() == 0) {
            // read a chunk of data
            cc = sock_->timeout_read(buf.data(), params_.readbuf_len_,
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
            buf.fill(cc);
        }
        
        log_debug("recv_bundle: got %u byte chunk, rcvd_len %u",
                  (u_int)buf.fullbytes(), (u_int)rcvd_len);
        
        // append the chunk of data and update the amount received
        bundle->payload_.append_data((u_char*)buf.start(), buf.fullbytes());
        rcvd_len += buf.fullbytes();
        buf.clear();
        
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
    if (params_.bundle_ack_enabled_ && (acked_len != rcvd_len)) {
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
 * Note that we observed some data inbound on this connection.
 */
void
TCPConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting receipt of data");
    ::gettimeofday(&data_rcvd_, 0);
}


/**
 * Handle an incoming ack, updating the acked length count. Timeout
 * specifies the amount of time the sender is willing to wait for the
 * ack.
 */
int
TCPConvergenceLayer::Connection::handle_ack(Bundle* bundle, size_t* acked_len)
{
    char typecode;
    size_t new_acked_len;

    // first read in the typecode 
    int cc = sock_->read(&typecode, 1);
    if (cc != 1) {
        log_err("handle_ack: error reading typecode: %s",
                strerror(errno));
        return oasys::IOERROR;
    }
    note_data_rcvd();

    // ignore KEEPALIVE messages, we are looking for acks
    // ignore SHUTDOWN, because the caller will figure it out anyway
    if (typecode == KEEPALIVE || typecode == SHUTDOWN) {
        return 1;
    }

    // other than that, all we should get are acks
    if (typecode != BUNDLE_ACK) {
        log_err("handle_ack: unexpected frame header type code 0x%x",
                typecode);
        return oasys::IOERROR;
    }

    // now just read in the ack header and validate the acked length
    // and the bundle id
    BundleAckHeader ackhdr;
    cc = sock_->read((char*)&ackhdr, sizeof(BundleAckHeader));
    if (cc != sizeof(BundleAckHeader))
    {
        log_err("handle_ack: error reading ack header (got %d/%u): %s",
                cc, (u_int)sizeof(BundleAckHeader), strerror(errno));
        return oasys::IOERROR;
    }
    note_data_rcvd();

    new_acked_len = ntohl(ackhdr.acked_length);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("handle_ack: got ack length %d for bundle id %d length %d",
              (u_int)new_acked_len, ackhdr.bundle_id, (u_int)payload_len);

    if (ackhdr.bundle_id != bundle->bundleid_)
    {
        log_err("handle_ack: error: bundle id mismatch %d != %d",
                ackhdr.bundle_id, bundle->bundleid_);
        return oasys::IOERROR;
    }

    if (new_acked_len < *acked_len ||
        new_acked_len > payload_len)
    {
        log_err("handle_ack: invalid acked length %u (acked %u, bundle %u)",
                (u_int)new_acked_len, (u_int)*acked_len, (u_int)payload_len);
        return oasys::IOERROR;
    }
    
    *acked_len = new_acked_len;
    return 1;
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
    contacthdr.idle_close_time    = htons(params_.idle_close_time_);

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
    
    params_.idle_close_time_    = MIN(params_.idle_close_time_,
                                      ntohs(contacthdr.idle_close_time));

    params_.bundle_ack_enabled_ = params_.bundle_ack_enabled_ &
                                  contacthdr.flags & BUNDLE_ACK_ENABLED;

    params_.reactive_frag_enabled_ = params_.reactive_frag_enabled_ &
                                     contacthdr.flags & REACTIVE_FRAG_ENABLED;

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
            sock_->set_notifier(new oasys::Notifier());
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
    addresshdr.addr = htonl(sock_->local_addr());
    addresshdr.port = htons(sock_->local_port());
    
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

    struct timeval start, now;
    ::gettimeofday(&start, 0);
    
    while (1) {
        int ret = sock_->timeout_connect(sock_->remote_addr(),
                                         sock_->remote_port(),
                                         params_.connect_timeout_);

        if (should_stop()) {
            log_debug("connect thread interrupted, should_stop set");
            return false;
        }

        if (ret == 0) { 
            break; // success
        }
        else if (ret == oasys::IOERROR) {
            log_info("connection attempt to %s:%d failed... "
                     "waiting %d seconds and retrying connect",
                     intoa(sock_->remote_addr()), sock_->remote_port(),
                     params_.connect_retry_ / 1000);
            sleep(params_.connect_retry_ / 1000);
            
            if (should_stop()) {
                log_debug("connect thread interrupted in sleep, "
                          "should_stop set");
                return false;
            }
        }
        else if (ret == oasys::IOTIMEOUT) {
            log_info("connection attempt to %s:%d timed out... retrying",
                     intoa(sock_->remote_addr()), sock_->remote_port());
        }
        
        sock_->close();

        ::gettimeofday(&now, 0);
        if (TIMEVAL_DIFF_MSEC(start, now) > params_.connect_timeout_) {
            log_info("connection attempt to %s:%d failed: "
                     "timeout exceeded, link unavailable",
                     intoa(sock_->remote_addr()), sock_->remote_port());
            
            return false;
        }
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
    
    // create a new contact
    contact_ = new Contact(link);
    link->set_contact(contact_);
    contact_->set_cl_info(this);
    
    // handle memory management
    Thread::clear_flag(DELETE_ON_EXIT);

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
TCPConvergenceLayer::Connection::break_contact(ContactDownEvent::reason_t reason)
{
    char typecode = SHUTDOWN;
    ASSERT(sock_);

    if (sock_->set_nonblocking(true) == 0) {
        sock_->write(&typecode, 1);
        // do not log an error when we fail to write, since the
        // SHUTDOWN is basically advisory. Expected errors here
        // include a short write due to socket already closed,
        // or maybe EAGAIN due to socket not ready for write.
    }
    sock_->close();

    // In all cases where break_contact is called, the Connection
    // thread is about to terminate. However, once the
    // ContactDownEvent is posted, TCPCL::close_contact() will try
    // to stop the Connection thread. So we preemptively indicate in
    // the thread flags that we're gonna stop shortly.
    set_should_stop();
    
    if (contact_ && !contact_->link()->isclosing()) {
        BundleDaemon::post(
            new LinkStateChangeRequest(contact_->link(), Link::CLOSING, reason));
    }
}

/**
 * The sender side main thread loop.
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    char typecode;
    int ret;

    // someone should already have established the session
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);
    
    // inform the daemon that the contact is available
    ASSERT(contact_);
    BundleDaemon::post(new ContactUpEvent(contact_));
    
    log_info("connection established -- (keepalive time %d seconds)",
             params_.keepalive_interval_);

    // from now on, all our operations will use non-blocking semantics
    sock_->set_nonblocking(true);

    // build up a poll vector since we need to block below on input
    // from both the socket and the bundle list notifier
    struct pollfd pollfds[3];

    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd            = queue_->notifier()->read_fd();
    bundle_poll->events        = POLLIN;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd            = sock_->fd();
    sock_poll->events        = POLLIN;

    struct pollfd* notifier_poll = &pollfds[2];
    notifier_poll->fd            = sock_->get_notifier()->read_fd();
    notifier_poll->events        = POLLIN;

    // keep track of the time we got data and keepalives
    struct timeval now;
    struct timeval keepalive_rcvd;
    struct timeval keepalive_sent;

    // let's give the remote end credit for a keepalive, even though all they
    // have done so far is open the connection.
    ::gettimeofday(&keepalive_rcvd, 0);

    // main loop
    while (true) {
        BundleRef bundle("TCPCL::send_loop temporary");

        // XXX/demmer debug this and make it a clean close_contact
        if (should_stop()) {
            return;
        }
        
        // pop the bundle (if any) off the queue, which gives us a
        // local reference on it
        bundle = queue_->pop_front();

        if (bundle != NULL) {
            size_t acked_len = 0;
            
            // we got a bundle, so send it off. 
            bool sentok = send_bundle(bundle.object(), &acked_len);

            // reset the keepalive timer
            ::gettimeofday(&keepalive_rcvd, 0);

            // if we sent some part of the bundle successfully, mark
            // the link as not busy any more and notify the daemon
            // that the transmission succeeded.
            if (sentok || (acked_len > 0)) {
                contact_->link()->set_state(Link::OPEN);
                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle.object(),
                                               contact_->link(),
                                               acked_len, true));
            }

            // remove the local reference
            bundle = NULL;

            // if the last transmission wasn't completely successful,
            // it's time to break the contact
            if (!sentok) {
                goto broken;
            }

            // otherwise, we loop back to the beginning and check for
            // more bundles on the queue
            continue;
        }

        // No bundle, so we'll block for:
        // 1) some activity on the socket, i.e. a keepalive or shutdown
        // 2) the bundle list notifier indicates new bundle to send
        // 3) thread is interrupted XXX/bowei
        // note that we pass the negotiated keepalive as the timeout
        // to the poll call to make sure the other side sends its
        // keepalive in time
        pollfds[0].revents = 0;
        pollfds[1].revents = 0;
        pollfds[2].revents = 0;        

        int timeout = params_.keepalive_interval_ * 1000;
        log_debug("send_loop: calling poll (timeout %d)", timeout);
                  
        // XXX/bowei - this use of poll probably needs cleanup
        int nready = poll(pollfds, 2, timeout);
        if (nready < 0) {
            if (errno == EINTR)
                continue;
            
            log_err("error return %d from poll: %s", nready, strerror(errno));
            goto broken;
        }

        // check for a message (or shutdown) from the other side
        if (sock_poll->revents != 0) {
            if ((sock_poll->revents & POLLHUP) ||
                (sock_poll->revents & POLLERR))
            {
                log_info("send_loop: remote connection error");
                goto broken;
            }

            if (! (sock_poll->revents & POLLIN)) {
                PANIC("unknown revents value 0x%x", sock_poll->revents);
            }

            log_debug("send_loop: data available on the socket");
            ret = sock_->read(&typecode, 1);
            if (ret != 1) {
                log_info("send_loop: "
                         "remote connection unexpectedly closed");
                goto broken;
            }

            // do not note_data_rcvd() after this read, because it
            // is the one where we expect KEEPALIVEs to arrive,
            // and they don't count to keep the connection from
            // being declared idle.

            if (typecode == KEEPALIVE) {
                // mark that we got a keepalive as expected
                ::gettimeofday(&keepalive_rcvd, 0);
            } else if (typecode == SHUTDOWN) {
                goto idle;
            } else {
                log_warn("send_loop: "
                         "got unexpected frame code %d", typecode);
                goto broken;
            }

        }

        // if nready is zero then the command timed out, implying that
        // it's time to send a keepalive.
        if (nready == 0) {
            log_debug("send_loop: timeout fired, sending keepalive");
            typecode = KEEPALIVE;
            ret = sock_->write(&typecode, 1);
            if (ret != 1) {
                log_info("send_loop: "
                         "remote connection unexpectedly closed");
                goto broken;
            }

            ::gettimeofday(&keepalive_sent, 0);
        }

        // check that it hasn't been too long since the other side
        // sent us a keepalive
        ::gettimeofday(&now, 0);
        u_int elapsed = TIMEVAL_DIFF_MSEC(now, keepalive_rcvd);
        if (elapsed > (2 * params_.keepalive_interval_ * 1000)) {
            log_info("send_loop: no keepalive heard for %d msecs "
                     "(sent %u.%u, rcvd %u.%u, now %u.%u) -- closing contact",
                     elapsed,
                     (u_int)keepalive_sent.tv_sec,
                     (u_int)keepalive_sent.tv_usec,
                     (u_int)keepalive_rcvd.tv_sec,
                     (u_int)keepalive_rcvd.tv_usec,
                     (u_int)now.tv_sec, (u_int)now.tv_usec);
            goto broken;
        }
        // see if it is time to close the connection due to it going idle
        elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > (params_.idle_close_time_ * 1000)) {
            log_info("connection idle for %d msecs, closing.", elapsed);
            goto idle;
        } else {
            log_debug("connection not idle: %d <= %d",
                      elapsed, params_.idle_close_time_ * 1000);
        }
    }
        
 broken:
    break_contact(ContactDownEvent::BROKEN);
    return;

 idle:
    break_contact(ContactDownEvent::IDLE);
    return;
}

void
TCPConvergenceLayer::Connection::recv_loop()
{
    char typecode;
    struct timeval now;
    u_int elapsed;

    while (1) {
        // see if it is time to close the connection due to it being idle
        ::gettimeofday(&now, 0);
        elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > params_.idle_close_time_ * 1000) {
            log_info("connection idle for %d milliseconds, closing.", elapsed);
            break_contact(ContactDownEvent::IDLE);
            return;
        } else {
            log_debug("connection not idle: %d <= %d",
                      elapsed, params_.idle_close_time_ * 1000);
        }

        // block on the one byte typecode
        int timeout = 2 * params_.keepalive_interval_ * 1000;
        log_debug("recv_loop: blocking on frame typecode... (timeout %d)",
                  timeout);
        int ret = sock_->timeout_read(&typecode, 1, timeout);
        if (ret == oasys::IOEOF || ret == oasys::IOERROR) {
            log_info("recv_loop: remote connection unexpectedly closed");
 shutdown:
            break_contact(ContactDownEvent::BROKEN);
            return;
            
        } else if (ret == oasys::IOTIMEOUT) {
            log_info("recv_loop: no message heard for > %d msecs -- "
                     "breaking contact", timeout);
            goto shutdown;
        }
        
        ASSERT(ret == 1);

        log_debug("recv_loop: got frame packet type 0x%x...", typecode);

        if (typecode == SHUTDOWN) {
            goto shutdown;
        }

        if (typecode == KEEPALIVE) {
            log_debug("recv_loop: "
                      "got keepalive, sending response");
            ret = sock_->write(&typecode, 1);
            if (ret != 1) {
                log_info("recv_loop: remote connection unexpectedly closed");
                goto shutdown;
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
