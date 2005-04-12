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
 *   For now, we only support interface strings on the form
 *   string://<int> where the int is the internal interface number. 
 * 
 *   XXX/jakob - how do we map /dev/eth0-style interface names to these internal numbers?
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
    
    log_debug("process_data: reading eth cl header...");    

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
        char buf[23];
        
        ContactManager* cm = BundleDaemon::instance()->contactmgr();
        EthernetAddressFamily::to_string((struct ether_addr*)ethhdr->ether_shost,buf);

        // XXX/jakob this is a bit of a hack. 
        if(!cm->find_peer(buf)) 
        {
            // XXX/jakob no CLInfo for Eth links so far, using "this" as a placeholder for now
            cm->new_opportunistic_contact(ConvergenceLayer::find_clayer("eth"),
                                          this,                                  
                                          buf);
        }
    }
    else if(ethclhdr.type == ETHCL_BUNDLE) {
        
        
        // infer the bundle length based on the packet length minus the
        // eth cl header
        bundle_len = len - sizeof(EthCLHeader);
        
        log_debug("process_data: got ethcl header -- bundle id %d, length %d",
                  ethclhdr.bundle_id, bundle_len);
        
        // skip past the cl header
        bp  += sizeof(EthCLHeader);
        len -= sizeof(EthCLHeader);
        
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
  // XXX/jakob - these are all bogus
  char src[6]={0x00,0x07,0xe9,0x01,0x00,0xf3};
  char dest[6]={0x00,0x0d,0x93,0xb4,0xf4,0xee};

  int sock,cc;
  int iovcnt = BundleProtocol::MAX_IOVCNT; 
  struct iovec iov[iovcnt + 3];

  EthCLHeader ethclhdr;
  struct ether_header hdr;
  
  struct sockaddr_ll iface;
  
  ethclhdr.version	= ETHCL_VERSION;
  ethclhdr.type         = ETHCL_BUNDLE;
  ethclhdr.bundle_id	= ntohl(bundle->bundleid_);
  
  // iovec slot 0 holds the ethernet header
  iov[0].iov_base = (char*)&hdr;
  iov[0].iov_len = sizeof(struct ether_header);

  // use iovec slot 1 for the eth cl header
  iov[1].iov_base = (char*)&ethclhdr;
  iov[1].iov_len  = sizeof(EthCLHeader);
  
  // fill in the rest of the iovec with the bundle header
  u_int16_t header_len = BundleProtocol::format_headers(bundle, &iov[2], &iovcnt);
  size_t payload_len = bundle->payload_.length();
  
  log_debug("send_bundle: bundle id %d, header_length %d payload_length %d",
	    bundle->bundleid_, header_len, payload_len);
  
  oasys::StringBuffer payload_buf(payload_len);
  const u_char* payload_data =
    bundle->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());

  iov[iovcnt + 2].iov_base = (char*)payload_data;
  iov[iovcnt + 2].iov_len = payload_len;
  
  if((sock = socket(AF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
    perror("socket");
    exit(1);
  }

  // XXX/jakob - how does the sender know which interface?
  //             probably this layer needs to remember who's where

  iface.sll_family=AF_PACKET;
  iface.sll_protocol=htons(ETH_P_ALL);
  iface.sll_ifindex=2;  // XXX/jakob - this is totally arbitrary! Will not work!

  
  if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
    perror("bind");
    exit(1);
  }
  
  memcpy(hdr.ether_dhost,dest,6);
  memcpy(hdr.ether_shost,src,6);
  
  hdr.ether_type=ETHERTYPE_DTN;
  
  cc=IO::writevall(sock, iov, iovcnt+3);
  if(cc<0) {
    perror("send");
    printf("Send failed!\n");
  }
  
  // free up the iovec data used in the header representation
  // (bundle header that is)
  BundleProtocol::free_header_iovmem(bundle, &iov[1], iovcnt);
  
  // check that we successfully wrote it all
  int total = sizeof(EthCLHeader) + header_len + payload_len;
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
       XXX/jakob - should start only looking at DTN packets soon
    */
    if((sock = socket(AF_PACKET,SOCK_RAW, htons(ETH_P_ALL))) < 0) { 
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
    iface.sll_protocol=htons(ETH_P_ALL);

    
    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }

    /*
     * Send the beacon on the socket every second.
     */
    while(1) {
        sleep(1);
        log_info("Sent beacon out interface %s.\n",if_name_ );
        
        cc=IO::writevall(sock, iov, 2);
        if(cc<0) {
            perror("send beacon");
            printf("Send beacon failed!\n");
        }
    }
}


} // namespace dtn
