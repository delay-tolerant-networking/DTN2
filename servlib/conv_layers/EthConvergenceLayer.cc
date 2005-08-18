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

// Only works on Linux (for now)
#ifdef __linux

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
    // grab the interface name out of the string:// 
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
  // XXX/jakob - need to keep track of the Beacon and Receiver threads for each 
  //             interface and kill them.
  NOTIMPLEMENTED;
}

bool
EthConvergenceLayer::open_contact(Contact* contact)
{
    eth_addr_t addr;
    
    log_debug("opening contact *%p", contact);

    // parse out the address from the contact nexthop
    BundleTuple bt(contact->nexthop());
    if (! EthernetAddressFamily::parse(bt.admin().c_str(), &addr)) {
        log_err("next hop address '%s' not a valid eth uri",
                contact->nexthop());
        return false;
    }
    
    // create a new connection for the contact
    
    Sender* sender = new Sender(((EthCLInfo*)contact->link()->cl_info())->if_name_, // interface name
				contact);
    contact->set_cl_info(sender);

    sender->logpathf("/cl/eth");

    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

bool
EthConvergenceLayer::close_contact(Contact* contact)
{  
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact);

    if (sender) {            
        contact->set_cl_info(NULL);
        delete sender;
    }
    
    return true;
}

/**
 * Send bundles queued up for the contact.
 */
void
EthConvergenceLayer::send_bundle(Contact* contact, Bundle* bundle)
{
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact);
        return;
    }
    ASSERT(contact == sender->contact_);
    
    sender->send_bundle(bundle); // consumes bundle reference
    bundle = NULL;
}


/******************************************************************************
 *
 * EthConvergenceLayer::Receiver
 *
 *****************************************************************************/
EthConvergenceLayer::Receiver::Receiver(const char* if_name)
  : Logger("/cl/eth/receiver")
{
    memset(if_name_,0, IFNAMSIZ);
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
    
    log_debug("Received DTN packet on interface %s, %d.",if_name_, len);    

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

        char next_hop_string[50];  
	memset(next_hop_string,0,50);
        EthernetAddressFamily::to_string((struct ether_addr*)ethhdr->ether_shost, next_hop_string);
        
        char bundles_string[60];
        memset(bundles_string,0,60);
        sprintf(bundles_string,"bundles://soontobegone/%s",next_hop_string);

        Link* link=cm->find_link_to(bundles_string);	
        if(!link || !link->isopen()) 
        {
            log_info("Discovered next_hop %s on interface %s.", bundles_string, if_name_);

            // registers a new contact with the routing layer
            link=cm->new_opportunistic_link(
                ConvergenceLayer::find_clayer("eth"),
                new EthCLInfo(if_name_),  // saved in Link::cl_info. 
                bundles_string); 
        }

        /**
	 * If there already is a timer for this link, cancel it, which
	 * will delete it when it bubbles to the top of the timer
	 * queue. Then create a new timer.
         */
        BeaconTimer *timer = ((EthCLInfo*)link->cl_info())->timer;
        if (timer)
            timer->cancel();

        timer = new BeaconTimer(bundles_string); 
        timer->schedule_in(ETHCL_BEACON_TIMEOUT_INTERVAL);
	
	((EthCLInfo*)link->cl_info())->timer=timer;

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


    memset(&iface, 0, sizeof(iface));
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
EthConvergenceLayer::Sender::Sender(char* if_name, Contact* contact)
    : contact_(contact)
{
    struct ifreq req;
    struct sockaddr_ll iface;
    
    memset(src_hw_addr_.octet, 0, 6); // determined in Sender::run()
    EthernetAddressFamily::parse(contact->nexthop(), &dst_hw_addr_);

    strcpy(if_name_, if_name);
    sock_ = 0;

    memset(&req, 0, sizeof(req));
    memset(&iface, 0, sizeof(iface));

    // Get and bind a RAW socket for this contact. XXX/jakob - seems
    // like it'd be enough with one socket per interface, not one per
    // contact. figure this out some time.
    if((sock_ = socket(AF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        exit(1);
    }

    // get the interface name from the contact info
    strcpy(req.ifr_name, if_name_);

    // ifreq the interface index for binding the socket    
    ioctl(sock_, SIOCGIFINDEX, &req);
    
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    iface.sll_ifindex=req.ifr_ifindex;
        
    // store away the ethernet address of the device in question
    if(ioctl(sock_, SIOCGIFHWADDR, &req))
    {
        perror("ioctl");
        exit(1);
    } 
    memcpy(src_hw_addr_.octet,req.ifr_hwaddr.sa_data,6);    

    if (bind(sock_, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }
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

    memset(iov,0,(iovcnt+3)*sizeof(struct iovec));
    
    // iovec slot 0 holds the ethernet header

    iov[0].iov_base = (char*)&hdr;
    iov[0].iov_len = sizeof(struct ether_header);

    // write the ethernet header

    memcpy(hdr.ether_dhost,dst_hw_addr_.octet,6);
    memcpy(hdr.ether_shost,src_hw_addr_.octet,6); // Sender::hw_addr
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
    log_info("Sending bundle out interface %s",if_name_);

    cc=IO::writevall(sock_, iov, iovcnt+3);
    if(cc<0) {
        perror("send");
        log_err("Send failed!\n");
    }    
    log_info("Sent packet, size: %d",cc );
    
    // free up the iovec data used in the header representation
    // (bundle header that is)
    BundleProtocol::free_header_iovmem(bundle, &iov[2], iovcnt);
    
    // check that we successfully wrote it all
    bool ok;
    
    int total = sizeof(EthCLHeader) + sizeof(struct ether_header) + header_len + payload_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (wrote %d/%d): %s",
                cc, total, strerror(errno));
        ok = false;
    } else {
        // cons up a transmission event and pass it to the router
        // since this is an unreliable protocol, acked_len = 0, and
        // ack = false
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_->link(),
                                       bundle->payload_.length(), false));
        ok = true;
    }

    return ok;
}

EthConvergenceLayer::Beacon::Beacon(const char* if_name)
{
    Thread::flags_ |= INTERRUPTABLE;
    memset(if_name_, 0, IFNAMSIZ);
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
    
    memset(&hdr,0,sizeof(hdr));
    memset(&ethclhdr,0,sizeof(ethclhdr));
    memset(&iface,0,sizeof(iface));

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
            log_err("Send beacon failed!\n");
        }
    }
}

EthConvergenceLayer::BeaconTimer::BeaconTimer(char * next_hop)
    : Logger("/cl/eth/beacontimer")
{
    next_hop_=(char*)malloc(strlen(next_hop)+1);
    strcpy(next_hop_, next_hop);
}

EthConvergenceLayer::BeaconTimer::~BeaconTimer()
{
//    free(next_hop_);
}

void
EthConvergenceLayer::BeaconTimer::timeout(struct timeval* now)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    Link * l = cm->find_link_to(next_hop_);

    log_info("Neighbor %s timer expired.",next_hop_);

    if(l == 0) {
      log_warn("No link for next_hop %s.",next_hop_);
    }
    else if(l->isopen()) {
	BundleDaemon::post(
            new LinkStateChangeRequest(l, Link::CLOSING, ContactDownEvent::BROKEN));
    }
    else {
	log_warn("next_hop %s unexpectedly not open",next_hop_);
    }
}

Timer *
EthConvergenceLayer::BeaconTimer::copy()
{
    return new BeaconTimer(*this);
}

} // namespace dtn

#endif // __linux
