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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include <oasys/io/NetUtils.h>
#include <oasys/io/IO.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/URL.h>
#include <oasys/util/StringBuffer.h>

#include "EthConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/ContactManager.h"
#include "bundling/Link.h"

using namespace oasys;
namespace dtn {

/******************************************************************************
 *
 * EthConvergenceLayer
 *
 *****************************************************************************/

EthConvergenceLayer::EthConvergenceLayer()
    : ConvergenceLayer("/cl/eth")
{
}

/* 
 *   Start listening to, and sending beacons on, the provided interface.
 *
 *   For now, we support interface strings on the form
 *   string://eth0
 *   
 *   this should change further down the line to simply be
 *    eth0
 *  
 */

bool
EthConvergenceLayer::add_interface(Interface* iface,
                                   int argc, const char* argv[])
{
    // grab the interface name out of the string:// mess
    // XXX/jakob - this fugly mess needs to change when we get the config stuff right
    
    const char* if_name=iface->admin().c_str()+strlen("string://");  
    
    log_info("EthConvergenceLayer::add_interface(%s).",if_name);
    
    Receiver* receiver = new Receiver(if_name);
    receiver->logpathf("/cl/eth");
    receiver->start();

    Beacon* beacon = new Beacon(if_name);
    beacon->logpathf("/cl/eth");
    beacon->start();
    
    return true;
}


bool
EthConvergenceLayer::del_interface(Interface* iface)
{
  // not implemented;
  // XXX/jakob - need to keep track of the Beacon and Receiver threads for each 
  //             interface and kill them.
  return false;
}

bool
EthConvergenceLayer::open_contact(Contact* contact)
{
    eth_addr_t addr;
    
    log_debug("opening contact *%p", contact);

    // parse out the address / port from the contact tuple
    if (! EthernetAddressFamily::parse(contact->nexthop(), &addr)) {
        log_err("next hop address '%s' not a valid eth uri",
                contact->nexthop());
        return false;
    }
    
    // create a new connection for the contact
    
    Sender* sender = new Sender(contact);
    sender->logpathf("/cl/eth");
    // sender->set_logfd(false);

    /*    if (sender->connect(addr, port) != 0) {
      log_err("error issuing eth connect %s",
	      strerror(errno));
      delete sender;
      return false;
      }*/

    sender->start();
    
    return true;
}

bool
EthConvergenceLayer::close_contact(Contact* contact)
{
  
    // XXX/jakob close the raw eth socket
/*
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
*/    
    return true;
}

/******************************************************************************
 *
 * EthConvergenceLayer::Receiver
 *
 *****************************************************************************/
EthConvergenceLayer::Receiver::Receiver(const char* if_name)
    : Logger("/cl/eth/receiver")
{
    strcpy(if_name_,if_name);
    Thread::flags_ |= INTERRUPTABLE;
}

void
EthConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    Bundle* bundle = NULL;       
    EthCLHeader ethclhdr;
    size_t header_len, bundle_len;
    struct ether_header* ethhdr=(struct ether_header*)bp;
    
    log_debug("Received DTN packet on interface %s.",if_name_);    

    // copy in the ethcl header.
    if (len < sizeof(EthCLHeader)) {
        log_err("process_data: "
                "incoming packet too small (len = %d)", len);
        return;
    }
    memcpy(&ethclhdr, bp+sizeof(struct ether_header), sizeof(EthCLHeader));

    // check for valid magic number and version
    if (ethclhdr.version != ETHCL_VERSION) {
        log_warn("remote sent version %d, expected version %d "
                 "-- disconnecting.", ethclhdr.version, ETHCL_VERSION);
        return;
    }

    if(ethclhdr.type == ETHCL_BEACON) {
        ContactManager* cm = BundleDaemon::instance()->contactmgr();

        char next_hop_string[24];  // 23 chars + string terminator: eth://00:00:00:00:00:00       
        EthernetAddressFamily::to_string((struct ether_addr*)ethhdr->ether_shost, next_hop_string);

        // XXX/jakob this is a bit of a hack. 
        // It could be that we have the peer, but no link at the moment...

        if(!cm->find_peer(next_hop_string)) 
        {
            log_info("Discovered next_hop %s on interface %s.", next_hop_string, if_name_);

            // registers a new contact with the routing layer
            Contact* c=cm->new_opportunistic_contact(ConvergenceLayer::find_clayer("eth"),
                                          new EthCLInfo(if_name_,ethhdr->ether_shost),    
                                          next_hop_string);                        

            // prepares the new contact for sending.
            ConvergenceLayer::find_clayer("eth")->open_contact(c);
        }
    }
    else if(ethclhdr.type == ETHCL_BUNDLE) {
        
        
        // infer the bundle length based on the packet length minus the
        // eth cl header
        bundle_len = len - sizeof(EthCLHeader) - sizeof(struct ether_header);
        
        log_debug("process_data: got ethcl header -- bundle id %d, length %d",
                  ntohl(ethclhdr.bundle_id), bundle_len);
        
        // skip past the cl header
        bp  += (sizeof(EthCLHeader) + sizeof(struct ether_header));
        len -= (sizeof(EthCLHeader) + sizeof(struct ether_header));
        
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
        bundle->payload_.append_data(bp, len);
        
        log_debug("process_data: new bundle id %d arrival, payload length %d",
                  bundle->bundleid_, bundle->payload_.length());
        
        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, len));
    }
}

void
EthConvergenceLayer::Receiver::run()
{
    int sock;
    int cc;
    struct sockaddr_ll iface;
    unsigned char buffer[MAX_ETHER_PACKET];

    if((sock = socket(PF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        log_err("EthConvergenceLayer::Receiver::run()" 
                "Couldn't open socket.");	
	exit(1);
    }
   
    // figure out the interface index of the device with name if_name_
    struct ifreq req;
    strcpy(req.ifr_name, if_name_);
    ioctl(sock, SIOCGIFINDEX, &req);


    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    iface.sll_ifindex=req.ifr_ifindex;
   
    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
      perror("bind");
      exit(1);
    }

    log_warn("Reading from socket...");
    while(true) {
      cc=read (sock, buffer, MAX_ETHER_PACKET);
      if(cc<=0) {
	perror("EthConvergenceLayer::Receiver::run()");
	exit(1);
      }
      struct ether_header* hdr=(struct ether_header*)buffer;
  
      if(ntohs(hdr->ether_type)==ETHERTYPE_DTN) {
	process_data(buffer, cc);
      }
      else if(ntohs(hdr->ether_type)!=0x800)
      {
          log_err("Got non-DTN packet in Receiver, type %4X.",ntohs(hdr->ether_type));
//          exit(1);
      }

      if(should_stop())
	break;
    }
}

/******************************************************************************
 *
 * EthConvergenceLayer::Sender
 *
 *****************************************************************************/

/**
 * Constructor for the active connection side of a connection.
 */
EthConvergenceLayer::Sender::Sender(Contact* contact)
    : contact_(contact)
{
    Thread::flags_ |= INTERRUPTABLE;
}
        
/* 
 * Send one bundle.
 */
bool 
EthConvergenceLayer::Sender::send_bundle(Bundle* bundle) 
{
    int cc;
    int iovcnt = BundleProtocol::MAX_IOVCNT; 
    struct iovec iov[iovcnt + 3];
        
    EthCLHeader ethclhdr;
    struct ether_header hdr;
    
    // iovec slot 0 holds the ethernet header

    iov[0].iov_base = (char*)&hdr;
    iov[0].iov_len = sizeof(struct ether_header);

    // write the ethernet header

    memcpy(hdr.ether_dhost,((EthCLInfo*)contact_->cl_info())->hw_addr_,6);
    memcpy(hdr.ether_shost,hw_addr_,6); // Sender::hw_addr
    hdr.ether_type=htons(ETHERTYPE_DTN);
    
    // iovec slot 1 for the eth cl header

    iov[1].iov_base = (char*)&ethclhdr;
    iov[1].iov_len  = sizeof(EthCLHeader);
    
    // write the ethcl header

    ethclhdr.version	= ETHCL_VERSION;
    ethclhdr.type       = ETHCL_BUNDLE;
    ethclhdr.bundle_id	= htonl(bundle->bundleid_);    

    // fill in the rest of the iovec with the bundle header

    u_int16_t header_len = BundleProtocol::format_headers(bundle, &iov[2], &iovcnt);
    size_t payload_len = bundle->payload_.length();
    
    log_info("send_bundle: bundle id %d, header_length %d payload_length %d",
              bundle->bundleid_, header_len, payload_len);
    
    oasys::StringBuffer payload_buf(payload_len);
    const u_char* payload_data =
        bundle->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());
    
    iov[iovcnt + 2].iov_base = (char*)payload_data;
    iov[iovcnt + 2].iov_len = payload_len;
    

    // We're done assembling the packet. Now write the whole thing to the socket!
    log_info("Sending bundle out interface %s",((EthCLInfo*)contact_->cl_info())->if_name_);

    cc=IO::writevall(sock, iov, iovcnt+3);
    if(cc<0) {
        perror("send");
        printf("Send failed!\n");
    }    
    
    // free up the iovec data used in the header representation
    // (bundle header that is)
    BundleProtocol::free_header_iovmem(bundle, &iov[2], iovcnt);
    
    // check that we successfully wrote it all
    int total = sizeof(EthCLHeader) + sizeof(struct ether_header) + header_len + payload_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }
    
    // all set
    return true;
}

void
EthConvergenceLayer::Sender::run()
{
    Bundle* bundle;
    struct ifreq req;
    struct sockaddr_ll iface;

    // Get and bind a RAW socket for this contact
    // XXX/jakob - seems like it'd be enough with one socket per interface, not one per contact. 
    //             figure this out some time.
    if((sock = socket(AF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        exit(1);
    }

    // get the interface name from the contact info
    strcpy(req.ifr_name, ((EthCLInfo*)contact_->cl_info())->if_name_);

    // ifreq the interface index for binding the socket    
    ioctl(sock, SIOCGIFINDEX, &req);
    
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    iface.sll_ifindex=req.ifr_ifindex;
        
    // store away the ethernet address of the device in question
    if(ioctl(sock, SIOCGIFHWADDR, &req))
    {
        perror("ioctl");
        exit(1);
    } 
    memcpy(hw_addr_,req.ifr_hwaddr.sa_data,6);    

    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }

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
        bundle->del_ref("ethcl");
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
EthConvergenceLayer::Sender::break_contact()
{
//    XXX/jakob - close it!
//    close();
    
    //  Thread::set_flag(STOPPED);
    if (contact_)
        BundleDaemon::post(new ContactDownEvent(contact_));
}

EthConvergenceLayer::Beacon::Beacon(const char* if_name)
{
    Thread::flags_ |= INTERRUPTABLE;
    strcpy(if_name_, if_name);
}

void EthConvergenceLayer::Beacon::run()
{
    // ethernet broadcast address
    char dest[6]={0xff,0xff,0xff,0xff,0xff,0xff};
    
    struct ether_header hdr;
    struct sockaddr_ll iface;
    EthCLHeader ethclhdr;
    
    int sock,cc;
    struct iovec iov[2];
    
    ethclhdr.version = ETHCL_VERSION;
    ethclhdr.type = ETHCL_BEACON;
    
    hdr.ether_type=htons(ETHERTYPE_DTN);
    
    // iovec slot 0 holds the ethernet header
    iov[0].iov_base = (char*)&hdr;
    iov[0].iov_len = sizeof(struct ether_header);
    
    // use iovec slot 1 for the eth cl header
    iov[1].iov_base = (char*)&ethclhdr;
    iov[1].iov_len  = sizeof(EthCLHeader); 
    

    /* 
       Get ourselves a raw socket, and configure it.
    */
    if((sock = socket(AF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        exit(1);
    }

    struct ifreq req;
    strcpy(req.ifr_name, if_name_);
    if(ioctl(sock, SIOCGIFINDEX, &req))
    {
        perror("ioctl");
        exit(1);
    }    
    iface.sll_ifindex=req.ifr_ifindex;

    if(ioctl(sock, SIOCGIFHWADDR, &req))
    {
        perror("ioctl");
        exit(1);
    } 
    
    memcpy(hdr.ether_dhost,dest,6);
    memcpy(hdr.ether_shost,req.ifr_hwaddr.sa_data,6);
    
    log_info("Interface %s has interface number %d.",if_name_,req.ifr_ifindex);
    
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    
    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }

    /*
     * Send the beacon on the socket every second.
     */
    while(1) {
        sleep(1);
        log_debug("Sent beacon out interface %s.\n",if_name_ );
        
        cc=IO::writevall(sock, iov, 2);
        if(cc<0) {
            perror("send beacon");
            printf("Send beacon failed!\n");
        }
    }
}


} // namespace dtn
