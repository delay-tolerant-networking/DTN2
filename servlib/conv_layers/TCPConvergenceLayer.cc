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
#include "bundling/ContactManager.h"
#include "bundling/FragmentManager.h"
#include "bundling/InternetAddressFamily.h"
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
    // set defaults here, then let ../cmd/ParamCommand.cc handle changing them
    defaults_.bundle_ack_enabled_ = true;
    defaults_.reactive_frag_enabled_ = true;
    defaults_.receiver_connect_	 = false;
    defaults_.partial_ack_len_ 	 = 1024;
    defaults_.writebuf_len_ 	 = 4096;
    defaults_.readbuf_len_ 	 = 4096;
    defaults_.keepalive_interval_= 2;
    defaults_.idle_close_time_ 	 = 30;
    defaults_.connect_timeout_ 	 = 10000; // msecs
    defaults_.rtt_timeout_ 	 = 5000;  // msecs
    defaults_.test_fragment_size_ = -1;
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
 * Register a new interface.
 */
bool
TCPConvergenceLayer::add_interface(Interface* iface,
                                   int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port;
    
    log_debug("adding interface %s", iface->admin().c_str());
    
    // parse out the address / port from the contact tuple
    if (! InternetAddressFamily::parse(iface->admin(), &addr, &port)) {
        log_err("interface address '%s' not a valid internet url",
                iface->admin().c_str());
        return false;
    }
    
    // make sure the port was specified
    if (port == 0) {
        log_err("port not specified in admin url '%s'",
                iface->admin().c_str());
        return false;
    }

    // parse options
    Params params = TCPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // now, special case the receiver_connect option which means we
    // create a Connection, not a Listener
    if (params.receiver_connect_) {
        log_debug("new receiver_connect interface -- starting connection thread");
        Connection* conn = new Connection(addr, port, false, &params);

        // store the connection object in the cl specific part of the
        // interface
        iface->set_cl_info(conn);

        // XXX/demmer again, there should be some structure to manage
        // the receiver side of connections
        conn->set_flag(oasys::Thread::DELETE_ON_EXIT);
        conn->start();

        return true;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(&params);
    listener->logpathf("%s/iface/%s:%d",
                       logpath_, intoa(addr), port);
    
    int ret = listener->bind(addr, port);

    // XXX/demmer temporary hack
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);

        ret = listener->bind(addr, port);
    }

    if (ret != 0) {
        listener->logf(oasys::LOG_ERR,
                       "error binding to requested socket: %s",
                       strerror(errno));
        return false;
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
 * Remove an interface
 */
bool
TCPConvergenceLayer::del_interface(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* listener = (Listener*)iface->cl_info();
    listener->set_should_stop();
    listener->interrupt();
    
    while (! listener->is_stopped()) {
        oasys::Thread::yield();
    }

    delete listener;
    return true;
}

/**
 * Create any CL-specific components of the Link.
 *
 * This creates a new Connection object and stores it in the LinkInfo
 * slot after parsing the options. We special case OPPORTUNISTIC links
 * which are created as a result of another node coming knocking with
 * the receiver_connect option.
 */
bool
TCPConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // if the link is OPPORTUNISTIC, we don't do anything here since
    // the connection will be established by someone else coming
    // knocking on our door.
    if (link->type() == Link::OPPORTUNISTIC) {
        return true;
    }
    
    // parse out the address / port from the contact tuple
    if (! InternetAddressFamily::parse(link->nexthop(), &addr, &port)) {
        log_err("next hop address '%s' not a valid internet url",
                link->nexthop());
        return false;
    }

    // make sure the port was specified
    if (port == 0) {
        log_err("port not specified in next hop address '%s'",
                link->nexthop());
        return false;
    }

    // Create the parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params();
    *params = defaults_;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }
    
    link->set_cl_info(params);
    return true;
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

    // Parse out the address / port from the contact tuple (again).
    // The next hop should have already been validated in init_link
    if (! InternetAddressFamily::parse(link->nexthop(), &addr, &port)) {
        PANIC("nexthop %s should have already been validated",
              link->nexthop());
    }

    // start the connection that is cached in the link, passing in the
    // parameters
    Params* params = (Params*)link->cl_info();
    ASSERT(params);
    
    Connection* conn = new Connection(addr, port, true, params);
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
            conn->interrupt();
        }
            
        while (!conn->is_stopped()) {
            log_debug("waiting for connection thread to stop...");
            oasys::Thread::yield();
        }
        
        log_debug("connection thread stopped...");

        delete conn;
        contact->set_cl_info(NULL);
    }
    
    return true;
}


/******************************************************************************
 *
 * TCPConvergenceLayer::Listener
 *
 *****************************************************************************/
TCPConvergenceLayer::Listener::Listener(Params* params)
    : TCPServerThread("/cl/tcp/listener")
{
    logfd_ = false;
    Thread::flags_ |= INTERRUPTABLE;
    params_ = *params;
}

void
TCPConvergenceLayer::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    log_debug("new connection from %s:%d", intoa(addr), port);
    Connection* conn = new Connection(fd, addr, port, &params_);
    
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
TCPConvergenceLayer::Connection::Connection(in_addr_t remote_addr,
                                            u_int16_t remote_port,
                                            bool is_sender,
                                            Params* params)

    : params_(*params), is_sender_(is_sender), contact_(NULL)
{
    Thread::flags_ |= INTERRUPTABLE;

    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);

    // cache the remote addr and port in the fields in the socket
    // since we want to actually connect to the peer side from the
    // Connection thread, not from the caller thread
    sock_ = new oasys::TCPClient();

    // XXX/demmer the basic socket logging emits errors and the like
    // when connections break. that may not be great since we kinda
    // expect them to happen... so either we should add some flag as
    // to the severity of error messages that can be passed into the
    // IO routines, or just suppress the IO output altogether
    sock_->logpathf("%s/sock", logpath_);
    sock_->set_logfd(false);
    
    sock_->set_remote_addr(remote_addr);
    sock_->set_remote_port(remote_port);
}    

TCPConvergenceLayer::Connection::Connection(int fd,
                                            in_addr_t remote_addr,
                                            u_int16_t remote_port,
                                            Params* params)

    : params_(*params), is_sender_(false), contact_(NULL)
{
    logpathf("/cl/tcp/conn/%s:%d", intoa(remote_addr), remote_port);

    sock_ = new oasys::TCPClient(fd, remote_addr, remote_port, logpath_);
    sock_->set_logfd(false);
}

TCPConvergenceLayer::Connection::~Connection()
{
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
    if (is_sender_ || params_.receiver_connect_)
    {
        if (! connect()) {
            /*
             * If the connection fails, even though the router hasn't
             * gotten a CONTACT_UP event, we still send a CONTACT_DOWN
             * event. XXX/demmer revisit this.
             */
            log_debug("connection failed");
            break_contact();
//             set_should_stop();
//             sock_->close();
            return;
        }
    } else {
        if (! accept()) {
            log_debug("accept failed");
            break_contact();
            return;
        }

        /*
         * Now if the receiver_connect option is set, then we're going
         * to be the sender, so we notify the contact manager and
         * enter the send loop.
         */
        if (params_.receiver_connect_) {
            log_debug("accept negotiated receiver_connect... "
                      "adding new opportunistic contact");
                
            ContactManager* cm = BundleDaemon::instance()->contactmgr();

            ConvergenceLayer* clayer = ConvergenceLayer::find_clayer("tcp");
            ASSERT(clayer);

            // XXX/demmer work this out better -- should it just use
            // the admin or should it use the full tuple?
            contact_ = cm->new_opportunistic_contact(clayer, this,
                                                     nexthop_.admin().c_str());
            
            if (!contact_) {
                log_err("receiver_connect: error finding opportunistic link");
                break_contact();
                return;
            }

            // XXX/demmer fix all this memory management stuff
            Thread::clear_flag(DELETE_ON_EXIT);

            is_sender_ = true;
        }
    }
    
    if (is_sender_) {
        send_loop();
    } else {
        recv_loop();
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
    int iovcnt;
    int header_iovcnt = BundleProtocol::MAX_IOVCNT;
    struct iovec iov[header_iovcnt + 2];
    size_t block_len;
    size_t payload_len = bundle->payload_.length();
    size_t payload_offset = 0;
    const u_char* payload_data;
    oasys::StringBuffer payload_buf(params_.writebuf_len_);
        
    // use iov slot zero for the one byte header type, slot one for
    // the frame header, then N slots for the bundle header, and
    // finally one more for the first chunk of data
    BundleDataHeader datahdr;
    char typecode = BUNDLE_DATA;
    iov[0].iov_base = &typecode;
    iov[0].iov_len  = 1;
    iov[1].iov_base = (char*)&datahdr;
    iov[1].iov_len  = sizeof(BundleDataHeader);
        
    // fill in the bundle header into the iovec
    u_int16_t header_len =
        BundleProtocol::format_headers(bundle, &iov[2], &header_iovcnt);

    log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);

    // fill in the frame header
    datahdr.bundle_id     = bundle->bundleid_;
    datahdr.bundle_length = htonl(header_len + payload_len);
    datahdr.header_length = htons(header_len);

    // all set to go, now write it all out 
    iovcnt = 2 + header_iovcnt;
    size_t total = 1 + sizeof(BundleDataHeader) + header_len;
    
    log_debug("send_bundle: sending %d byte tcpcl hdr, %d byte bundle hdr",
              1 + sizeof(BundleDataHeader), header_len);
    
    int cc = sock_->writev(iov, iovcnt);

    // free up the iovec data used in the header representation
    BundleProtocol::free_header_iovmem(bundle, &iov[2], header_iovcnt);

    if (cc != (int)total) {
        log_crit("XXX/demmer fix this -- for now, closing the connection");
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
            bundle->payload_.read_data(payload_offset, block_len,
                                       (u_char*)payload_buf.data(), true);
        
        log_debug("send_bundle: sending %d byte block %p",
                  block_len, payload_data);
        
        cc = sock_->write((char*)payload_data, block_len);
        
        if (cc == 0) {
            // eof from the other side
            log_warn("send_bundle: remote side closed connection");
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
                         "error writing bundle block (wrote %d/%d): %s",
                         cc, block_len, strerror(errno));
                goto done;
            }
        }
        
        if (cc < (int)block_len) {
            // unexpected short write -- just adjust block_len to
            // reflect the amount actually written and fall through
            log_warn("send_bundle: "
                     "short write sending bundle block (wrote %d/%d): %s",
                     cc, block_len, strerror(errno));

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
            cc = sock_->poll(POLLIN | POLLOUT, &revents, params_.rtt_timeout_);
            if (cc < 0) {
                log_warn("send_bundle: error waiting for ack: %s",
                         strerror(errno));
                goto done;
            }
            
            if (cc == 0) {
                log_warn("send_bundle: timeout waiting for ack or write-ready");
                goto done;
            }

            ASSERT(cc == 1);

            if (revents & POLLIN) {
                cc = handle_ack(bundle, acked_len);
                
                if (cc == oasys::IOERROR) {
                    goto done;
                }
                ASSERT(cc == 1);
                
                log_debug("send_bundle: got ack for %d/%d -- "
                          "looping to send more", *acked_len,
                          bundle->payload_.length());
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
                  *acked_len, payload_len);

        cc = sock_->poll(POLLIN, NULL, params_.rtt_timeout_);
        if (cc < 0) {
            log_warn("send_bundle: error waiting for ack: %s",
                     strerror(errno));
            goto done;
        }
        
        if (cc == 0) {
            log_warn("send_bundle: timeout waiting for ack");
            goto done;
        }

        ASSERT(cc == 1);
        
        cc = handle_ack(bundle, acked_len);
        if (cc == oasys::IOERROR) {
            goto done;
        }
        ASSERT(cc == 1);
        
        log_debug("send_bundle: got ack for %d/%d",
                  *acked_len, payload_len);
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
    Bundle* bundle = NULL;
    BundleDataHeader datahdr;
    u_int16_t header_len;
    size_t bundle_len;
    size_t rcvd_len = 0;
    size_t acked_len = 0;
    size_t payload_len = 0;

    oasys::StringBuffer payload_buf(params_.readbuf_len_);

    bool valid = false;
    bool recvok = false;

    // read in the data header
    log_debug("recv_bundle: reading bundle data header...");
    int cc = sock_->readall((char*)&datahdr, sizeof(BundleDataHeader));
    if (cc != sizeof(BundleDataHeader)) {
        log_err("recv_bundle: "
                "error reading bundle data header (read %d/%d): %s",
                cc, sizeof(BundleDataHeader), strerror(errno));
        return false;
    }
    note_data_rcvd();
 
    // extract the lengths from the data header
    bundle_len = ntohl(datahdr.bundle_length);
    header_len = ntohs(datahdr.header_length);

    log_debug("recv_bundle: got bundle data header -- bundle id %d, "
              "header_length %d bundle_length %d",
              datahdr.bundle_id, header_len, bundle_len);

    // now read in the whole bundle header
    cc = sock_->timeout_readall(payload_buf.data(), header_len,
                                params_.rtt_timeout_);
    note_data_rcvd();

    if (cc != (int)header_len) {
        log_err("recv_bundle: "
                "error reading bundle headers (read %d/%d): %s",
                cc, header_len, strerror(errno));
        goto done;
    }

    // parse the headers into the new bundle.
    //
    // note that this extracts payload_len from the bundle's header
    // and assigns it in the bundle payload
    bundle = new Bundle();
    cc = BundleProtocol::parse_headers(bundle, (u_char*)payload_buf.data(), header_len);
    if (cc != (int)header_len) {
        log_err("recv_bundle: error parsing bundle headers (parsed %d/%d)",
                cc, header_len);
        goto done;
    }

    // all lengths have been parsed, so we can do some length
    // validation checks
    payload_len = bundle->payload_.length();
    if (bundle_len != header_len + payload_len) {
        log_err("recv_bundle: error in bundle lengths: "
                "bundle_length %d, header_length %d, payload_length %d",
                bundle_len, header_len, payload_len);
        goto done;
    }

    // so far so good, now loop until we've read the whole payload
    // done with the rest. note that all reads have a timeout
    while (rcvd_len < payload_len) {

        // test hook for reactive fragmentation.
        if (params_.test_fragment_size_ != -1) {
            if ((int)rcvd_len > params_.test_fragment_size_) {
                log_info("send_bundle: "
                         "XXX forcing fragmentation size %d, rcvd %d",
                         params_.test_fragment_size_, rcvd_len);
                usleep(100000);
                goto done;
            }
        }

        // read a chunk of data
        cc = sock_->timeout_read(payload_buf.data(), params_.readbuf_len_,
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

        log_debug("recv_bundle: got %d byte chunk, rcvd_len %d", cc, rcvd_len);
        
        // append the chunk of data and update the amount received
        bundle->payload_.append_data((u_char*)payload_buf.data(), cc);
        rcvd_len += cc;

        // at this point, we can make at least a valid bundle fragment
        // from what we've gotten thus far (assuming reactive
        // fragmentation is enabled)
        valid = true;

        // check if we've read enough to send an ack
        if (rcvd_len - acked_len > params_.partial_ack_len_) {
            log_debug("recv_bundle: got %d bytes acked %d, sending partial ack",
                      rcvd_len, acked_len);
            
            if (! send_ack(datahdr.bundle_id, rcvd_len)) {
                log_err("recv_bundle: error sending ack");
                goto done;
            }
            acked_len = rcvd_len;
        }
    }

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
              "new bundle id %d arrival, payload length %d (rcvd %d)",
              bundle->bundleid_, payload_len, rcvd_len);
    
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
        log_err("handle_ack: error reading ack header (got %d/%d): %s",
                cc, sizeof(BundleAckHeader), strerror(errno));
        return oasys::IOERROR;
    }
    note_data_rcvd();

    new_acked_len = ntohl(ackhdr.acked_length);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("handle_ack: got ack length %d for bundle id %d length %d",
              new_acked_len, ackhdr.bundle_id, payload_len);

    if (ackhdr.bundle_id != bundle->bundleid_)
    {
        log_err("handle_ack: error: bundle id mismatch %d != %d",
                ackhdr.bundle_id, bundle->bundleid_);
        return oasys::IOERROR;
    }

    if (new_acked_len < *acked_len ||
        new_acked_len > payload_len)
    {
        log_err("handle_ack: invalid acked length %d (acked %d, bundle %d)",
                new_acked_len, *acked_len, payload_len);
        return oasys::IOERROR;
    }

    *acked_len = new_acked_len;
    return 1;
}

/**
 * Helper function to format and send a contact header and our local
 * identifier.
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
        log_err("error writing contact header (wrote %d/%d): %s",
                cc, sizeof(ContactHeader), strerror(errno));
        return false;
    }

    // if this is the receiver connection option, we immediately
    // follow the contact header with an identity header and the local
    // tuple information
    if (!is_sender_ && params_.receiver_connect_) {
        BundleRouter* router = BundleDaemon::instance()->router();
        const BundleTuple& local_tuple = router->local_tuple();
        size_t region_len = local_tuple.region().length();
        size_t admin_len  = local_tuple.admin().length();
        
        log_debug("sending identity header... (region_len %d admin_len %d)",
                  region_len, admin_len);
        
        size_t len = sizeof(IdentityHeader) + region_len + admin_len;

        // XXX/demmer use scratch buffer instead of malloc
        IdentityHeader* identityhdr = (IdentityHeader*)malloc(len);
        identityhdr->region_len = htons(region_len);
        identityhdr->admin_len  = htons(admin_len);

        char* data = (char*)&identityhdr[1];
        memcpy(data, local_tuple.region().data(), region_len);
        memcpy(data + region_len, local_tuple.admin().data(), admin_len);
        
        cc = sock_->writeall((char*)identityhdr, len);
        if (cc != (int)len) {
            log_err("error writing identity header / tuple (wrote %d/%d): %s",
                    cc, len, strerror(errno));
            return false;
        }

        free(identityhdr);
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
    
    int cc;
    if (timeout != 0) {
        cc = sock_->timeout_readall((char*)&contacthdr, sizeof(ContactHeader),
                                    timeout);
        
        if (cc == oasys::IOTIMEOUT) {
            log_warn("timeout reading contact header");
            return false;
        }
    } else {
        cc = sock_->readall((char*)&contacthdr, sizeof(ContactHeader));
    }
    
    if (cc != sizeof(ContactHeader)) {
        log_err("error reading contact header (read %d/%d): %s", 
                cc, sizeof(ContactHeader), strerror(errno));
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
    
    params_.receiver_connect_   = params_.receiver_connect_ |
                                  contacthdr.flags & RECEIVER_CONNECT;

    // if the other side sent the receiver connect option, then it
    // should send an identity header immediately following the
    // contact header, so read it in and parse the next hop tuple
    // identifier
    if (contacthdr.flags & RECEIVER_CONNECT) {
        log_debug("got receiver_connect option: reading identity header");
        IdentityHeader identityhdr;

        cc = sock_->timeout_readall((char*)&identityhdr,
                                    sizeof(IdentityHeader),
                                    params_.rtt_timeout_);
        if (cc == oasys::IOTIMEOUT) {
            log_warn("timeout reading receiver_connect identity header");
            return false;
        }

        if (cc != sizeof(IdentityHeader)) {
            log_err("error reading receiver_connect identity (read %d/%d): %s",
                cc, sizeof(IdentityHeader), strerror(errno));
            return false;
        }

        size_t region_len = ntohs(identityhdr.region_len);
        size_t admin_len  = ntohs(identityhdr.admin_len);

        oasys::StringBuffer region_buf(region_len);
        oasys::StringBuffer  admin_buf(admin_len);

        log_debug("reading region data (length %d)", region_len);
        cc = sock_->timeout_readall(region_buf.data(), region_len,
                                    params_.rtt_timeout_);
        if (cc != (int)region_len) {
            log_err("error reading region string (read %d/%d): %s",
                cc, region_len, strerror(errno));
            return false;
        }

        log_debug("reading admin data (length %d)", admin_len);
        cc = sock_->timeout_readall(admin_buf.data(), admin_len,
                                    params_.rtt_timeout_);
        if (cc != (int)admin_len) {
            log_err("error reading admin string (read %d/%d): %s",
                cc, admin_len, strerror(errno));
            return false;
        }

        nexthop_.assign(region_buf.data(), region_len,
                        admin_buf.data(),  admin_len);
        
        if (!nexthop_.valid()) {
            log_err("invalid peer tuple received: region='%.*s' admin='%.*s'",
                    (int)region_buf.length(), region_buf.data(),
                    (int)admin_buf.length(),  admin_buf.data());
            return false;
        }
    }

    note_data_rcvd();
    return true;
}


/**
 * Sender side of the initial handshake. First connect to the peer
 * side, then exchange ContactHeaders and negotiate session
 * parameters.
 */
bool
TCPConvergenceLayer::Connection::connect()
{
    // first of all, connect to the receiver side
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);
    log_debug("connect: connecting to %s:%d...",
              intoa(sock_->remote_addr()), sock_->remote_port());

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
                     params_.connect_timeout_ / 1000);
            sleep(params_.connect_timeout_ / 1000);

            if (should_stop()) {
                log_debug("connect thread interrupted in sleep, should_stop set");
                return false;
            }
        }
        else if (ret == oasys::IOTIMEOUT) {
            log_info("connection attempt to %s:%d timed out... retrying",
                     intoa(sock_->remote_addr()), sock_->remote_port());
        }
        
        sock_->close();
    }

    log_debug("connect: connection established, sending contact header...");
    if (!send_contact_header())
        return false;

    log_debug("connect: waiting for contact header reply...");
    if (!recv_contact_header(params_.rtt_timeout_))
        return false;

    return true;
}

/**
 * Receiver side of the initial handshake.
 */
bool
TCPConvergenceLayer::Connection::accept()
{
    ASSERT(contact_ == NULL);

    log_debug("accept: sending contact header...");
    if (!send_contact_header())
        return false;

    log_debug("accept: waiting for peer contact header...");
    if (!recv_contact_header(0))
        return false;
    
    return true;
}

/**
 * Send an event to the system indicating that this contact is broken
 * and close the side of the connection.
 * 
 * This results in the Connection thread stopping and the system
 * calling the link->close() call which cleans up the connection.
 */
void
TCPConvergenceLayer::Connection::break_contact()
{
    char typecode = SHUTDOWN;
    ASSERT(sock_);

#if 0
    // debugging code to confirm the state of the socket
    if (sock_->get_nonblocking(&is_nonblocking)) {
        log_warn("failure getting non-blocking status of sock");
    } else {
        log_debug("sock %s non-blocking", (is_nonblocking) ? "is" : "is not");
    }
#endif

    /// XXX/jra: debug code above confirms that receiver sockets
    /// not currently in nonblocking mode. demmer thinks they should
    /// be, but until we think that change through, we set nonblocking
    /// here before we write to the socket, to be safe.

    if (sock_->set_nonblocking(true) == 0) {
        (void) sock_->write(&typecode, 1);
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

    if (contact_ && !contact_->link()->isclosing())
        BundleDaemon::post(new ContactDownEvent(contact_));
}

/**
 * The sender side main thread loop.
 */
void
TCPConvergenceLayer::Connection::send_loop()
{
    Bundle* bundle;
    char typecode;
    int ret;

    // someone should already have either connected or accepted the
    // connection
    ASSERT(sock_->state() == oasys::IPSocket::ESTABLISHED);
    
    // inform the router that the contact is available
    ASSERT(contact_);
    BundleDaemon::post(new ContactUpEvent(contact_));
    
    log_info("connection established -- (keepalive time %d seconds)",
             params_.keepalive_interval_);

    // from now on, all our operations will use non-blocking semantics
    sock_->set_nonblocking(true);
    
    // build up a poll vector since we need to block below on input
    // from both the socket and the bundle list notifier
    struct pollfd pollfds[2];
    
    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd = contact_->bundle_list()->read_fd();
    bundle_poll->events = POLLIN;
    
    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = sock_->fd();
    sock_poll->events = POLLIN;

    // keep track of the time we got data and keepalives
    struct timeval now, keepalive_rcvd, keepalive_sent;

    // let's give the remote end credit for a keepalive, even though all they
    // have done so far is open the connection.
    ::gettimeofday(&keepalive_rcvd, 0);

    // main loop
    while (1) {
        // first see if someone wants us to stop
        // XXX/demmer debug this and make it a clean close_contact
        if (should_stop()) {
            break_contact();
            return;
        }
        
        // first check the contact's bundle list to see if there's a
        // bundle already there, but don't pop it off the list yet
        bundle = contact_->bundle_list()->front();

        if (bundle) {
            size_t acked_len = 0;
            
            // we got a bundle, so send it off. 
            bool sentok = send_bundle(bundle, &acked_len);

            // if any data was acked, pop the bundle off the contact
            // list (making sure it's still the right bundle) and
            // inform the router. note that pop_front does _not_
            // decrement the reference count, so we now have a local
            // reference to the bundle
            if (acked_len != 0) {
                // give them credit for sending data
                ::gettimeofday(&keepalive_rcvd, 0);

                Bundle* bundle2 = contact_->bundle_list()->pop_front();
                ASSERT(bundle == bundle2);

                // XXX/demmer this won't be true wrt expiration...
                // maybe mark the bundle as in-progress or something
                // like that?
                
                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle, contact_, acked_len, true));
                
                bundle->del_ref("tcpcl");
                bundle = NULL;
            }

            // if the last transmission wasn't completely successful,
            // it's time to break the contact
            if (!sentok) {
                goto shutdown;
            }

            continue;
        }

        // no bundle, so we'll block waiting for a) some activity on
        // the socket, i.e. a keepalive or shutdown, or b) the bundle
        // list notifier indicating there's a new bundle for us to
        // send
        //
        // note that we pass the negotiated keepalive as the timeout
        // to the poll call to make sure the other side sends its
        // keepalive in time
        pollfds[0].revents = 0;
        pollfds[1].revents = 0;
        
        log_debug("send_loop: calling poll (timeout %d)",
                  params_.keepalive_interval_ * 1000);
        int nready = poll(pollfds, 2, params_.keepalive_interval_ * 1000);
        if (nready < 0) {
            if (errno == EINTR) continue;
            log_err("error return %d from poll: %s", nready, strerror(errno));
            goto shutdown;
        }

        // check for a message (or shutdown) from the other side
        if (sock_poll->revents != 0) {
            if ((sock_poll->revents & POLLHUP) ||
                (sock_poll->revents & POLLERR))
            {
                log_warn("send_loop: remote connection error");
                goto shutdown;
            }

            if (! (sock_poll->revents & POLLIN)) {
                PANIC("unknown revents value 0x%x", sock_poll->revents);
            }

            log_debug("send_loop: data available on the socket");
            ret = sock_->read(&typecode, 1);
            if (ret != 1) {
                log_warn("send_loop: "
                         "remote connection unexpectedly closed");
                goto shutdown;
            }

            // do not note_data_rcvd() after this read, because it
            // is the one where we expect KEEPALIVEs to arrive,
            // and they don't count to keep the connection from
            // being declared idle.

            if (typecode == KEEPALIVE) {
                // mark that we got a keepalive as expected
                ::gettimeofday(&keepalive_rcvd, 0);
            } else if (typecode == SHUTDOWN) {
                goto shutdown;
            } else {
                log_warn("send_loop: "
                         "got unexpected frame code %d", typecode);
                goto shutdown;
            }

        }
        
        // if the bundle list was triggered, we check the list at the
        // beginning of the loop, but make sure to drain the pipe here
        if (bundle_poll->revents != 0) {
            ASSERT(bundle_poll->revents == POLLIN);
            ASSERT(contact_->bundle_list()->front() != NULL);
            contact_->bundle_list()->drain_pipe();
        }

        // if nready is zero then the command timed out, implying that
        // it's time to send a keepalive.
        if (nready == 0) {
            log_debug("send_loop: timeout fired, sending keepalive");
            typecode = KEEPALIVE;
            ret = sock_->write(&typecode, 1);
            if (ret != 1) {
                log_warn("send_loop: "
                         "remote connection unexpectedly closed");
                goto shutdown;
            }

            ::gettimeofday(&keepalive_sent, 0);
        }

        // check that it hasn't been too long since the other side
        // sent us a keepalive
        ::gettimeofday(&now, 0);
        u_int elapsed = TIMEVAL_DIFF_MSEC(now, keepalive_rcvd);
        if (elapsed > (2 * params_.keepalive_interval_ * 1000)) {
            log_warn("send_loop: no keepalive heard for %d msecs "
                     "(sent %u.%u, rcvd %u.%u, now %u.%u) -- closing contact",
                     elapsed,
                     (u_int)keepalive_sent.tv_sec,
                     (u_int)keepalive_sent.tv_usec,
                     (u_int)keepalive_rcvd.tv_sec,
                     (u_int)keepalive_rcvd.tv_usec,
                     (u_int)now.tv_sec, (u_int)now.tv_usec);
            goto shutdown;
        }
        // see if it is time to close the connection due to it going idle
        elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > (params_.idle_close_time_ * 1000)) {
            log_info("connection idle for %d msecs, closing.", elapsed);
            goto shutdown;
        } else {
            log_debug("connection not idle: %d <= %d",
                      elapsed, params_.idle_close_time_ * 1000);
        }
    }
        
 shutdown:
    break_contact();
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
            break_contact();
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
            log_warn("recv_loop: remote connection unexpectedly closed");
 shutdown:
            break_contact();
            return;
            
        } else if (ret == oasys::IOTIMEOUT) {
            log_warn("recv_loop: no message heard for > %d msecs -- "
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
                log_warn("recv_loop: remote connection unexpectedly closed");
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
