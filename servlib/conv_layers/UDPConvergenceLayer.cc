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

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/URL.h>
#include <oasys/util/StringBuffer.h>

#include "UDPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/InternetAddressFamily.h"

namespace dtn {

/******************************************************************************
 *
 * UDPConvergenceLayer
 *
 *****************************************************************************/

UDPConvergenceLayer::UDPConvergenceLayer()
    : IPConvergenceLayer("/cl/udp")
{
}

/*
 * Register a new Interface 
 */
bool
UDPConvergenceLayer::add_interface(Interface* iface,
                                   int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port;
    
    log_debug("adding interface %s", iface->admin().c_str());
    
    // parse out the address / port from the contact tuple
    if (! InternetAddressFamily::parse(iface->admin(), &addr, &port)) {
        log_err("interface address '%s'not a valid internet url",
                iface->admin().c_str());
        return false;
    }
    
    // make sure the port was specified
    if (port == 0) {
        log_err("port not specified in admin url '%s'",
                iface->admin().c_str());
        return false;
    }

    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver();
    receiver->logpathf("%s/iface/%s:%d", logpath_, intoa(addr), port);

    if (receiver->bind(addr, port) != 0) {
        receiver->logf(oasys::LOG_ERR,
                       "error binding to requested socket: %s",
                       strerror(errno));
        return false;
    }

    // start the thread which automatically listens for data
    receiver->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}


bool UDPConvergenceLayer::del_interface(Interface* iface) {
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt();
    
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;
    return true;
}

bool
UDPConvergenceLayer::open_contact(Contact* contact)
{
    in_addr_t addr;
    u_int16_t port;
    
    log_debug("opening contact *%p", contact);

    // parse out the address / port from the contact tuple
    if (! InternetAddressFamily::parse(contact->nexthop(), &addr, &port)) {
        log_err("next hop address '%s' not a valid internet url",
                contact->nexthop());
        return false;
    }
    
    // make sure the port was specified
    if (port == 0) {
        log_err("port not specified in next hop address '%s'",
                contact->nexthop());
        return false;
    }
    
    // create a new connection for the contact
    // XXX/demmer: bind?
    Sender* snd = new Sender(contact, addr, port);
    snd->start();

    return true;
}

bool UDPConvergenceLayer::close_contact(Contact* contact) {

    Sender* snd = (Sender*)contact->cl_info();

    log_info("close_contact *%p", contact);

    if (snd) {
        while (!snd->is_stopped()) {
            if (!snd->should_stop()) {
                PANIC("YYY/Nabeel need to stop the connection thread");
                snd->set_should_stop();
                snd->interrupt();
            }
            
            log_warn("waiting for connection thread to stop...");
            oasys::Thread::yield();
        }
        
        delete snd;
        
        contact->set_cl_info(NULL);
    }
    
    return true;
}

/******************************************************************************
 *
 * UDPConvergenceLayer::Receiver
 *
 *****************************************************************************/
UDPConvergenceLayer::Receiver::Receiver()
    : IPSocket("/cl/udp/receiver", SOCK_DGRAM)
{
    logfd_ = false;
    params_.reuseaddr_ = 1;
    Thread::flags_ |= INTERRUPTABLE;
}

void UDPConvergenceLayer::Receiver::process_data(char* payload, size_t payload_ln) {

    // declare dynamic storage elements that may need to be cleaned up
    // on error
    u_char* buf = (u_char*)payload;
    Bundle* bundle = NULL;       
    BundleStartHeader starthdr;
    int cc;
    
    u_int16_t header_len;
    size_t bundle_len;
    size_t block_len;
    
    log_debug("process_data: starting buffer parsing...");

    // Peek to see the packet typecode, verify packet contains data    
    if ( (*payload) != BUNDLE_START ) {
        log_err("process_data: unknown frame type code received\n");
        return;
    }

    /** Read from the start of the BundleStartHeader **/
    memcpy(&starthdr, (buf + sizeof(ContactHeader) + 1),
           sizeof(BundleStartHeader));
    log_debug("process_data: reading start header...");
    
    // extract the lengths from the start header
    header_len = ntohs(starthdr.header_length);
    bundle_len = ntohl(starthdr.bundle_length);
    block_len = ntohl(starthdr.block_length);
    
    log_debug("process_data: got start header -- bundle id %d, "
	      "header_length %d bundle_length %d block_length %d",
	      starthdr.bundle_id, header_len, bundle_len, block_len);
    
    u_char* bundle_pos =
        buf + 1 + sizeof(ContactHeader) + sizeof(BundleStartHeader);
    
    /** Now read the bundle header next..... **/
    
    // parse the headers into a new bundle. this sets the
    // payload_len appropriately
    bundle = new Bundle();
    cc = BundleProtocol::parse_headers(bundle, bundle_pos, header_len);
    if (cc != (int)header_len) {
        log_err("process_data: error parsing bundle headers (parsed %d/%d)",
                cc, header_len);
shutdown:
        if (bundle)
            delete bundle;        
        return;
    }
    
    // do some length validation checks
    size_t payload_len = bundle->payload_.length();
    if (bundle_len != header_len + payload_len) {
        log_err("process_data: error in bundle lengths: "
                "bundle_length %d, header_length %d, payload_length %d",
                bundle_len, header_len, payload_len);
        goto shutdown;
    }

    if (payload_len != block_len) {
        log_err("process_data: error in payload length: "
                "payload_length %d, block_len %d", 
                payload_len, block_len); 
        goto shutdown;
    }
    
    /** Now read and store the payload into a local bundle **/
    
    // so far so good, now tack the block onto the payload
    bundle->payload_.append_data((bundle_pos + header_len), block_len);
    
    // all set, notify the router of the new arrival
    log_debug("process_data: new bundle id %d arrival, payload length %d",
	      bundle->bundleid_, bundle->payload_.length());
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, block_len));
}

void
UDPConvergenceLayer::Receiver::run()
{
    int fd = 0;
    in_addr_t addr;
    u_int16_t port;
    char* pt_payload = (char*)malloc(MAX_UDP_PACKET);
    int ret;

    while (1) {
        if (should_stop())
            break;
        ret = recvfrom(pt_payload, MAX_UDP_PACKET, 0, &addr, &port);
	if (ret <= 0 ) {	  
            if (errno == EINTR) {
                continue;
            }
            log_err("error in recvfrom(): %d %s",
                    errno, strerror(errno));
            close();
            break;
        }
        
	log_debug("Received data on fd %d from %s:%d",
                  fd, intoa(addr), port);	         
	process_data(pt_payload, ret);
    }
}

/******************************************************************************
 *
 * UDPConvergenceLayer::Sender
 *
 *****************************************************************************/

/**
 * Constructor for the active connection side of a connection.
 */
UDPConvergenceLayer::Sender::Sender(Contact* contact, 
                                    in_addr_t remote_addr,
                                    u_int16_t remote_port) 
    : contact_(contact)
{
    Thread::flags_ |= INTERRUPTABLE;

    logpathf("/cl/udp/conn/%s:%d", intoa(remote_addr), remote_port);
    sock_ = new oasys::UDPClient(logpath_);
    sock_->set_logfd(false);
    
    sock_->set_remote_addr(remote_addr);
    sock_->set_remote_port(remote_port);
}
        
UDPConvergenceLayer::Sender::~Sender()
{
    delete sock_;
}
	
/**
 * The main loop for a connection. Based on the initial construction
 * state, it calls one of send_loop or recv_loop.
 *
 * XXX/demmer for now, this is keyed on whether or not there is a
 * contact_ cached from the constructor -- figure something better to
 * support sending bundles from the passive acceptor
 */
void
UDPConvergenceLayer::Sender::run()
{
    if (contact_) {
        send_loop();
    } else {
        //We only process data from our receiver thread
        log_debug("error: UDP sender trying to send on unknown contact\n"); 
    }
}				   

/* 
 * Protected Helper function implementations for the UDP Convergence Layer
 */

bool 
UDPConvergenceLayer::Sender::send_bundle(Bundle* bundle) {

    int iovcnt = BundleProtocol::MAX_IOVCNT; 
    struct iovec iov[iovcnt + 4];
    /** Our respective headers **/
    BundleStartHeader starthdr;
    ContactHeader contacthdr;

    /** Create the contact header first..... **/
    contacthdr.version = CURRENT_VERSION;
    contacthdr.flags =  0; //No bundle/block acks or keepalives
    
    /** Tack on the contact header now..... **/
    
    // use iov slot zero for the one byte frame header type, and
    // slot one for the frame header itself
    // This is irrelvant for UDP (for semantic purposes)
    char typecode = BUNDLE_START; 
    iov[0].iov_base = &typecode;
    iov[0].iov_len  = 1;
    iov[1].iov_base = (char*)&contacthdr;
    iov[1].iov_len  = sizeof(ContactHeader);
    
    /** Tack on the BundleStartHeader now **/    
    
    // char typecode = BUNDLE_START;
    iov[2].iov_base = (char*)&starthdr;
    iov[2].iov_len  = sizeof(BundleStartHeader);
    
    /** Tack on the Bundle Header now **/
        
    // fill in the rest of the iovec with the bundle header
    u_int16_t header_len =
        BundleProtocol::format_headers(bundle, &iov[3], &iovcnt);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);

    // now fill in the frame header
    starthdr.bundle_id = bundle->bundleid_;
    starthdr.bundle_length = htonl(header_len + payload_len);
    starthdr.header_length = htons(header_len);

    // We will squash all our payload into a single block....
    starthdr.block_length = htonl(payload_len);
    starthdr.partial = 0; // we have the complete payload

    /** Now add *all* the payload data **/
    
    log_debug("send_bundle: sending %d byte frame hdr, %d byte bundle hdr, "
              "and %d bytes of payload data",
              1 + sizeof(ContactHeader) + sizeof(BundleStartHeader),
              header_len, payload_len);

    oasys::StringBuffer payload_buf(payload_len);
    const u_char* payload_data =
        bundle->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());
    iov[iovcnt + 3].iov_base = (char*)payload_data;
    iov[iovcnt + 3].iov_len = payload_len;

    /** Send the header and the payload together...... **/

    int cc = sock_->writev(iov, iovcnt + 4);
    int total = 1 + sizeof(ContactHeader) + sizeof(BundleStartHeader) +
                header_len + payload_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (complete w/headers) "
                "(wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    // free up the iovec data used in the header representation
    // (bundle header that is) this is a compatible call for
    // format_headers()
    BundleProtocol::free_header_iovmem(bundle, &iov[3], iovcnt);
        
    /** we are done! **/
    return true;
}

void UDPConvergenceLayer::Sender::send_loop() {

    // first of all, connect to the receiver side
    ASSERT(sock_->state() != oasys::IPSocket::ESTABLISHED);

    log_debug("send_loop: connecting to %s:%d...", 
	      intoa(sock_->remote_addr()), sock_->remote_port());
    while (sock_->connect(sock_->remote_addr(), sock_->remote_port()) != 0)
    {
        log_info("connection attempt to %s:%d failed... "
                 "will retry in 10 seconds",
                 intoa(sock_->remote_addr()), sock_->remote_port());
        sleep(10);
    }

    log_info("connection established -- (no keepalives exchanged)");
    
    /** No negotiation is done, just send the bundle on the link **/
    
    // build up a poll vector so we can block waiting for a) bundle
    // input from the main daemon thread, b) a message from the other
    // side
    struct pollfd pollfds[2];
    struct pollfd* bundle_poll 	= &pollfds[0];
    struct pollfd* sock_poll 	= &pollfds[1];
    
    bundle_poll->fd = contact_->bundle_list()->read_fd();
    bundle_poll->events = POLLIN | POLLPRI;
    sock_poll->fd = sock_->fd();
    sock_poll->events = POLLIN | POLLPRI;

    // main loop
    while (1) {
        Bundle* bundle;

        // first check the contact's bundle list to see if there's a
        // bundle already there... if not, we'll block waiting for
        // input either from the remote side or the bundle list notifier
        //
        // note that pop_front does _not_ decrement the reference
        // count, so we now have a local reference to the bundle

        bundle = contact_->bundle_list()->pop_front();

        if (!bundle) {
            pollfds[0].revents = 0;
            pollfds[1].revents = 0;
        
	    int nready = poll(pollfds, 2, -1);
	    if (nready < 0) {
                if (errno == EINTR) continue;
                log_err("error return %d from poll: %s", nready, strerror(errno));
                goto shutdown;
	    }
        
            log_debug("send_loop: poll returned %d", nready);

            // check for a message (or shutdown) from the other side
            if (sock_poll->revents != 0) {
                if ((sock_poll->revents & POLLHUP) ||
                    (sock_poll->revents & POLLERR))
                {
                    log_warn("send_loop: "
                             "remote connection unexpectedly closed");
                    goto shutdown;
                }
                else if ((sock_poll->revents & POLLIN) ||
                         (sock_poll->revents & POLLPRI))
                {
                    char typecode;
                    int ret = sock_->read(&typecode, 1);
                    if (ret == -1 || ret == 0)
                    {
                        log_warn("send_loop: "
                                 "remote connection unexpectedly closed");
                        goto shutdown;
                    }
                    else {
                        PANIC("unknown traffic arrived at sender-side");
                    }

		}
                else
                {
                    PANIC("unknown revents value 0x%x", sock_poll->revents);
                }
            }

	    /** Now check to see if we have any local bundles to send **/
	    
            // if the bundle list wasn't triggered, loop again waiting
            // for input
            if (bundle_poll->revents == 0) {
                continue;
            }

            // otherwise, we've been notified that a bundle is there,
            // so grab it off the list
            ASSERT(bundle_poll->revents == POLLIN);
            bundle = contact_->bundle_list()->pop_front();
            ASSERT(bundle);
            contact_->bundle_list()->drain_pipe();
        }

        // we got a legit bundle, send it off
        if (! send_bundle(bundle)) {
            goto shutdown;
        }	

        // cons up a transmission event and pass it to the router
        // No acks received, so acked_len = 0, and ack = false 
        // YYY/Nabeel: Tell BundleDaemon we have artificially received acks
        // for all the payload length 
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_, bundle->payload_.length(), false));
        
        // finally, remove our local reference on the bundle, which
        // may delete it
        bundle->del_ref("udpcl");
        bundle = NULL;	
    }

shutdown:
    /// XXX Sushant added to clean address contact broken events
    break_contact();
//    contact_->close();
    return;               

}


/**
 * Send an event to the system indicating that this contact is broken
 * and close the side of the connection.
 *
 * This results in the Connection thread stopping and the system
 * calling the contact->close() call which cleans up the connection.
 */
void
UDPConvergenceLayer::Sender::break_contact()
{
    ASSERT(sock_);
    sock_->close();
    //  Thread::set_flag(STOPPED);
    if (contact_)
        BundleDaemon::post(new ContactDownEvent(contact_));
}

} // namespace dtn
