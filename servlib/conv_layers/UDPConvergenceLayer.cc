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


bool
UDPConvergenceLayer::del_interface(Interface* iface)
{
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
    // XXX/demmer bind?
    Sender* sender = new Sender(contact);
    sender->logpathf("/cl/udp/conn/%s:%d", intoa(addr), port);
    sender->set_logfd(false);

    if (sender->connect(addr, port) != 0) {
        log_err("error issuing udp connect to %s:%d: %s",
                intoa(addr), port, strerror(errno));
        delete sender;
        return false;
    }

    sender->start();

    return true;
}

bool
UDPConvergenceLayer::close_contact(Contact* contact)
{
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact);

    if (sender) {
        while (!sender->is_stopped()) {
            if (!sender->should_stop()) {
                sender->set_should_stop();
                sender->interrupt();
            }
            
            log_warn("waiting for connection thread to stop...");
            oasys::Thread::yield();
        }
        
        delete sender;
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
    : UDPClient("/cl/udp/receiver")
{
    logfd_ = false;
    params_.reuseaddr_ = 1;
    Thread::flags_ |= INTERRUPTABLE;
}

void
UDPConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    Bundle* bundle = NULL;       
    UDPCLHeader udpclhdr;
    size_t header_len, bundle_len;
    
    log_debug("process_data: reading udp cl header...");

    // copy in the udpcl header.
    if (len < sizeof(UDPCLHeader)) {
        log_err("process_data: "
                "incoming packet too small (len = %d)", len);
        return;
    }
    memcpy(&udpclhdr, bp, sizeof(UDPCLHeader));

    // check for valid magic number and version
    if (ntohl(udpclhdr.magic) != MAGIC) {
        log_warn("remote sent magic number 0x%.8x, expected 0x%.8x "
                 "-- disconnecting.", udpclhdr.magic, MAGIC);
        return;
    }

    if (udpclhdr.version != UDPCL_VERSION) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", udpclhdr.version, UDPCL_VERSION);
        return;
    }
    
    // infer the bundle length based on the packet length minus the
    // udp cl header
    bundle_len = len - sizeof(UDPCLHeader);

    log_debug("process_data: got udpcl header -- bundle id %d, length %d",
	      udpclhdr.bundle_id, bundle_len);

    // skip past the cl header
    bp  += sizeof(UDPCLHeader);
    len -= sizeof(UDPCLHeader);
    
    // parse the headers into a new bundle. this sets the payload_len
    // appropriately in the new bundle and returns the number of bytes
    // consumed for the bundle headers
    bundle = new Bundle();
    header_len = BundleProtocol::parse_headers(bundle, (u_char*)bp, len);
    
    size_t payload_len = bundle->payload_.length();
    if (bundle_len != header_len + payload_len) {
        log_err("process_data: error in bundle lengths: "
                "bundle_length %d, header_length %d, payload_length %d",
                bundle_len, header_len, payload_len);
        delete bundle;
        return;
    }

    // store the payload and notify the daemon
    bp  += header_len;
    len -= header_len;
    bundle->payload_.set_data(bp, len);
    
    log_debug("process_data: new bundle id %d arrival, payload length %d",
	      bundle->bundleid_, bundle->payload_.length());
    
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len));
}

void
UDPConvergenceLayer::Receiver::run()
{
    int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char buf[MAX_UDP_PACKET];

    while (1) {
        if (should_stop())
            break;
        
        ret = recvfrom((char*)buf, MAX_UDP_PACKET, 0, &addr, &port);
	if (ret <= 0) {	  
            if (errno == EINTR) {
                continue;
            }
            log_err("error in recvfrom(): %d %s",
                    errno, strerror(errno));
            close();
            break;
        }
        
	log_debug("got %d byte packet from %s:%d",
                  ret, intoa(addr), port);	         
	process_data(buf, ret);
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
UDPConvergenceLayer::Sender::Sender(Contact* contact)
    : contact_(contact)
{
    Thread::flags_ |= INTERRUPTABLE;
}
        
/* 
 * Send one bundle.
 */
bool 
UDPConvergenceLayer::Sender::send_bundle(Bundle* bundle) {

    int iovcnt = BundleProtocol::MAX_IOVCNT; 
    struct iovec iov[iovcnt + 1];
    UDPCLHeader udpclhdr;

    udpclhdr.magic	= ntohl(MAGIC);
    udpclhdr.version	= UDPCL_VERSION;
    udpclhdr.flags	= BUNDLE_DATA;
    udpclhdr.source_id	= ntohs(local_port_);
    udpclhdr.bundle_id	= ntohl(bundle->bundleid_);

    // use iovec slot 0 for the udp cl header
    iov[0].iov_base = (char*)&udpclhdr;
    iov[0].iov_len  = sizeof(UDPCLHeader);
    
    // fill in the rest of the iovec with the bundle header
    u_int16_t header_len =
        BundleProtocol::format_headers(bundle, &iov[1], &iovcnt);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);

    oasys::StringBuffer payload_buf(payload_len);
    const u_char* payload_data =
        bundle->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());
    iov[iovcnt + 1].iov_base = (char*)payload_data;
    iov[iovcnt + 1].iov_len = payload_len;

    int cc = writevall(iov, iovcnt + 2);

    // free up the iovec data used in the header representation
    // (bundle header that is)
    BundleProtocol::free_header_iovmem(bundle, &iov[3], iovcnt);

    // check that we successfully wrote it all
    int total = sizeof(UDPCLHeader) + header_len + payload_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    // all set
    return true;
}

void
UDPConvergenceLayer::Sender::run()
{
    Bundle* bundle;

    while (1) {

        // block waiting for a bundle to be placed on the contact
        bundle = contact_->bundle_list()->pop_blocking();

        // we got a legit bundle, send it off
        if (! send_bundle(bundle)) {
            goto shutdown;
        }	
        
        // cons up a transmission event and pass it to the router
        // since this is an unreliable protocol, acked_len = 0, and
        // ack = false
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_,
                                       bundle->payload_.length(), false));
        
        // finally, remove our local reference on the bundle, which
        // may delete it
        bundle->del_ref("udpcl");
        bundle = NULL;	
    }
    
shutdown:
    /// XXX Sushant added to clean address contact broken events
    break_contact();

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
    close();
    
    //  Thread::set_flag(STOPPED);
    if (contact_)
        BundleDaemon::post(new ContactDownEvent(contact_));
}

} // namespace dtn
