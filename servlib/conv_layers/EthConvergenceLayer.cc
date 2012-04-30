/*
 *    Copyright 2004-2006 Intel Corporation
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

// Only works on Linux (for now)
#ifdef __linux__

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
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "EthConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"

using namespace oasys;
namespace dtn {

/******************************************************************************
 *
 * EthConvergenceLayer::Params
 *
 *****************************************************************************/
struct EthConvergenceLayer::Params EthConvergenceLayer::defaults_;

void
EthConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
	a->process("beacon_interval", &beacon_interval_);
}

/******************************************************************************
 *
 * EthConvergenceLayer
 *
 *****************************************************************************/

EthConvergenceLayer::EthConvergenceLayer()
    : ConvergenceLayer("EthConvergenceLayer", "eth")
{
    defaults_.beacon_interval_          = 1;
}

//----------------------------------------------------------------------
CLInfo*
EthConvergenceLayer::new_link_params()
{
    return new EthConvergenceLayer::Params(EthConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
/**
 * Parse variable args into a parameter structure.
 */
bool
EthConvergenceLayer::parse_params(Params* params,
                                  int argc, const char* argv[],
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::UIntOpt("beacon_interval", &params->beacon_interval_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
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
EthConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    Params params = EthConvergenceLayer::defaults_;
    const char *invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // grab the interface name out of the string:// 

    // XXX/jakob - this fugly mess needs to change when we get the
    // config stuff right
    const char* if_name=iface->name().c_str()+strlen("string://");
    log_info("EthConvergenceLayer::interface_up(%s).", if_name);
    
    Receiver* receiver = new Receiver(if_name, &params);
    receiver->logpathf("/cl/eth");
    receiver->start();
    iface->set_cl_info(receiver);

    // remembers the interface beacon object
    if_beacon_ = new Beacon(if_name, params.beacon_interval_);
    if_beacon_->logpathf("/cl/eth");
    if_beacon_->start();
    
    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::interface_down(Interface* iface)
{
  // XXX/jakob - need to keep track of the Beacon and Receiver threads for each 
  //             interface and kill them.
  // NOTIMPLEMENTED;

    // xxx/shawn needs to find a way to delete beacon;
    if_beacon_->set_should_stop();
    while (! if_beacon_->is_stopped()) {
        oasys::Thread::yield();
    }
    delete if_beacon_;

    Receiver *receiver = (Receiver *)iface->cl_info();
    receiver->set_should_stop();
    // receiver->interrupt_from_io();
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }
    delete receiver;

    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("EthConvergenceLayer::open_contact: "
              "opening contact to link *%p", link.object());

    // parse out the address from the contact nexthop
    eth_addr_t addr;
    if (!EthernetScheme::parse(link->nexthop(), &addr)) {
        log_err("EthConvergenceLayer::open_contact: "
                "next hop address '%s' not a valid eth uri", link->nexthop());
        return false;
    }
    
    // create a new connection for the contact
    Sender* sender = new Sender(((EthCLInfo*)link->cl_info())->if_name_,
                                link->contact());
    contact->set_cl_info(sender);

    sender->logpathf("/cl/eth");

    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::close_contact(const ContactRef& contact)
{  
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {            
        contact->set_cl_info(NULL);
        delete sender;
    }
    
    return true;
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    log_debug("EthConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    if (link->cl_info() != NULL) {
        delete link->cl_info();
        link->set_cl_info(NULL);
    }
}

//----------------------------------------------------------------------
/**
 * Send bundles queued up for the contact.
 */
void
EthConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    const ContactRef& contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    ASSERT(contact == sender->contact_);

    sender->send_bundle(bundle);
}

//----------------------------------------------------------------------
bool
EthConvergenceLayer::is_queued(const LinkRef& contact, Bundle* bundle)
{
    (void)contact;
    (void)bundle;

    /// The Ethernet convergence layer does not maintain an output queue.
    return false;
}

/******************************************************************************
 *
 * EthConvergenceLayer::Receiver
 *
 *****************************************************************************/
EthConvergenceLayer::Receiver::Receiver(const char* if_name,
                                        EthConvergenceLayer::Params* params)
  : Logger("EthConvergenceLayer::Receiver", "/dtn/cl/eth/receiver"),
    Thread("EthConvergenceLayer::Receiver")
{
    memset(if_name_,0, IFNAMSIZ);
    strcpy(if_name_,if_name);
    Thread::flags_ |= INTERRUPTABLE;
    (void)params;
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    Bundle* bundle = NULL;       
    EthCLHeader ethclhdr;
    size_t bundle_len;
    struct ether_header* ethhdr=(struct ether_header*)bp;
    
    log_debug("Received DTN packet on interface %s, %zu.",if_name_, len);    

    // copy in the ethcl header.
    if (len < sizeof(EthCLHeader)) {
        log_err("process_data: "
                "incoming packet too small (len = %zu)", len);
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

        char bundles_string[60];
        memset(bundles_string,0,60);
        EthernetScheme::to_string(&ethhdr->ether_shost[0],
                                  bundles_string);
        char next_hop_string[50], *ptr;
        memset(next_hop_string,0,50);
        ptr = strrchr(bundles_string, '/');
        strcpy(next_hop_string, ptr+1);
        
        ConvergenceLayer* cl = ConvergenceLayer::find_clayer("eth");
        EndpointID remote_eid(bundles_string);

        LinkRef link = cm->find_link_to(cl,
                                        next_hop_string,
                                        remote_eid,
                                        Link::OPPORTUNISTIC);
        
        if(link == NULL) {
            log_info("EthConvergenceLayer::Receiver::process_data: "
                     "Discovered next_hop %s on interface %s.",
                     next_hop_string, if_name_);
            
            // registers a new contact with the routing layer
            link = cm->new_opportunistic_link(cl,
                                              next_hop_string,
                                              EndpointID(bundles_string));

            if (link == NULL) {
                log_debug("EthConvergenceLayer::Receiver::process_data: "
                          "failed to create new opportunistic link");
                return;
            }

            oasys::ScopeLock l(link->lock(), "EthConvergenceLayer::Receiver");
            // It is doubtful that the link would be deleted already,
            // but check just in case.
            if (link->isdeleted()) {
                log_warn("EthConvergenceLayer::Receiver::process_data: "
                         "link %s deleted before fully initialized",
                         link->name());
                return;
            }
            ASSERT(link->cl_info() == NULL);
            link->set_cl_info(new EthCLInfo(if_name_));
            l.unlock();
        }

        ASSERT(link != NULL);
        oasys::ScopeLock l(link->lock(), "EthConvergenceLayer::Receiver");

        if (link->isdeleted()) {
            log_warn("EthConvergenceLayer::Receiver::process_data: "
                     "link %s already deleted", link->name());
            return;
        }

        ASSERT(link->cl_info() != NULL);
        ASSERT(strcmp(((EthCLInfo*)link->cl_info())->if_name_, if_name_) == 0);

        if(!link->isavailable()) {
            log_info("EthConvergenceLayer::Receiver::process_data: "
                     "Got beacon for previously unavailable link %s",
                     link->name());
            
            // XXX/demmer something should be done here to kick the link...
            log_err("XXX/demmer do something about link availability");
        }
        
        /**
         * If there already is a timer for this link, cancel it, which
         * will delete it when it bubbles to the top of the timer
         * queue. Then create a new timer.
         */
        BeaconTimer *timer = ((EthCLInfo*)link->cl_info())->timer;
        if (timer)
            timer->cancel();

        timer = new BeaconTimer(next_hop_string); 
        timer->schedule_in(ETHCL_BEACON_TIMEOUT_INTERVAL);
        
        ((EthCLInfo*)link->cl_info())->timer = timer;

        l.unlock();
    }
    else if(ethclhdr.type == ETHCL_BUNDLE) {
        // infer the bundle length based on the packet length minus the
        // eth cl header
        bundle_len = len - sizeof(EthCLHeader) - sizeof(struct ether_header);
        
        log_debug("process_data: got ethcl header -- bundle id %d, length %zu",
                  ntohl(ethclhdr.bundle_id), bundle_len);
        
        // skip past the cl header
        bp  += (sizeof(EthCLHeader) + sizeof(struct ether_header));
        len -= (sizeof(EthCLHeader) + sizeof(struct ether_header));

        bundle = new Bundle();
        bool complete = false;
        int cc = BundleProtocol::consume(bundle, bp, len, &complete);

        if (cc < 0) {
            log_err("process_data: bundle protocol error");
            delete bundle;
            return;
        }

        if (!complete) {
            log_err("process_data: incomplete bundle");
            delete bundle;
            return;
        }

        log_debug("process_data: new bundle id %d arrival, bundle length %zu",
                  bundle->bundleid(), bundle_len);
        
        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, 
                                    bundle_len, EndpointID::NULL_EID()));
    }
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::Receiver::run()
{
    int sock;
    int cc;
    struct sockaddr_ll iface;
    unsigned char buffer[MAX_ETHER_PACKET];

    if((sock = socket(PF_PACKET,SOCK_RAW, htons(ETHERTYPE_DTN))) < 0) { 
        perror("socket");
        log_err("EthConvergenceLayer::Receiver::run() " 
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
            log_err("Got non-DTN packet in Receiver, type %4X.",
                    ntohs(hdr->ether_type));
            // exit(1);
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
EthConvergenceLayer::Sender::Sender(char* if_name,
                                    const ContactRef& contact)
    : Logger("EthConvergenceLayer::Sender", "/dtn/cl/eth/sender"),
      contact_(contact.object(), "EthConvergenceLayer::Sender")
{
    struct ifreq req;
    struct sockaddr_ll iface;
    LinkRef link = contact->link();

    memset(src_hw_addr_.octet, 0, 6); // determined in Sender::run()
    EthernetScheme::parse(link->nexthop(), &dst_hw_addr_);

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
        
//----------------------------------------------------------------------
/* 
 * Send one bundle.
 */
bool 
EthConvergenceLayer::Sender::send_bundle(const BundleRef& bundle) 
{
    int cc;
    struct iovec iov[3];
        
    EthCLHeader ethclhdr;
    struct ether_header hdr;

    memset(iov,0,sizeof(iov));
    
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

    ethclhdr.version    = ETHCL_VERSION;
    ethclhdr.type       = ETHCL_BUNDLE;
    ethclhdr.bundle_id  = htonl(bundle->bundleid());

    // iovec slot 2 for the bundle
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);

    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, MAX_ETHER_PACKET);
        return -1;
    }

    iov[2].iov_base = (char *)buf_;
    iov[2].iov_len  = total_len;
    
    // We're done assembling the packet. Now write the whole thing to
    // the socket!
    log_info("Sending bundle out interface %s",if_name_);

    cc=IO::writevall(sock_, iov, 3);
    if(cc<0) {
        perror("send");
        log_err("Send failed!\n");
    }    
    log_info("Sent packet, size: %d",cc );

    // move the bundle off the link queue and onto the inflight queue
    contact_->link()->del_from_queue(bundle, total_len);
    contact_->link()->add_to_inflight(bundle, total_len);

    // check that we successfully wrote it all
    bool ok;
    int total = sizeof(EthCLHeader) + sizeof(struct ether_header) + total_len;
    if (cc != total) {
        log_err("send_bundle: error writing bundle (wrote %d/%d): %s",
                cc, total, strerror(errno));
        ok = false;
    } else {
        // cons up a transmission event and pass it to the router
        // since this is an unreliable protocol, acked_len = 0, and
        // ack = false
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle.object(), contact_,contact_->link(),
                                       total_len, false));
        ok = true;
    }

    return ok;
}

//----------------------------------------------------------------------
EthConvergenceLayer::Beacon::Beacon(const char* if_name,
                                    unsigned int beacon_interval)
  : Logger("EthConvergenceLayer::Beacon", "/dtn/cl/eth/beacon"),
    Thread("EthConvergenceLayer::Beacon")
{
    Thread::flags_ |= INTERRUPTABLE;
    memset(if_name_, 0, IFNAMSIZ);
    strcpy(if_name_, if_name);
    beacon_interval_ = beacon_interval;
}

//----------------------------------------------------------------------
void EthConvergenceLayer::Beacon::run()
{
    // ethernet broadcast address
    char bcast_mac_addr[6]={0xff,0xff,0xff,0xff,0xff,0xff};
    
    struct ether_header hdr;
    struct sockaddr_ll iface;
    EthCLHeader ethclhdr;
    
    int sock,cc;
    struct iovec iov[2];
    
    memset(&hdr,0,sizeof(hdr));
    memset(&ethclhdr,0,sizeof(ethclhdr));
    memset(&iface,0,sizeof(iface));

    ethclhdr.version = ETHCL_VERSION;
    ethclhdr.type    = ETHCL_BEACON;
    
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
    
    memcpy(hdr.ether_dhost,bcast_mac_addr,6);
    memcpy(hdr.ether_shost,req.ifr_hwaddr.sa_data,6);
    
    log_info("Interface %s has interface number %d.",if_name_,req.ifr_ifindex);
    
    iface.sll_family=AF_PACKET;
    iface.sll_protocol=htons(ETHERTYPE_DTN);
    
    if (bind(sock, (struct sockaddr *) &iface, sizeof(iface)) == -1) {
        perror("bind");
        exit(1);
    }

    /*
     * Send the beacon on the socket every beacon_interval_ second.
     */
    while(1) {
        sleep(beacon_interval_);

        if (should_stop())
            break;

        log_debug("Sent beacon out interface %s.\n",if_name_ );
        
        cc=IO::writevall(sock, iov, 2);
        if(cc<0) {
            perror("send beacon");
            log_err("Send beacon failed!\n");
        }
    }
}

//----------------------------------------------------------------------
EthConvergenceLayer::BeaconTimer::BeaconTimer(char * next_hop)
    :  Logger("EthConvergenceLayer::BeaconTimer", "/dtn/cl/eth/beacontimer")
{
    next_hop_=(char*)malloc(strlen(next_hop)+1);
    strcpy(next_hop_, next_hop);
}

//----------------------------------------------------------------------
EthConvergenceLayer::BeaconTimer::~BeaconTimer()
{
    free(next_hop_);
}

//----------------------------------------------------------------------
void
EthConvergenceLayer::BeaconTimer::timeout(const struct timeval& now)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    ConvergenceLayer* cl = ConvergenceLayer::find_clayer("eth");
    LinkRef link = cm->find_link_to(cl, next_hop_);

    (void)now;

    log_info("Neighbor %s timer expired.",next_hop_);

    if(link == NULL) {
      log_warn("No link for next_hop %s.",next_hop_);
    }
    else if(link->isopen()) {
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactDownEvent::BROKEN));
    }
    else {
        log_warn("next_hop %s unexpectedly not open",next_hop_);
    }
}

//----------------------------------------------------------------------
Timer *
EthConvergenceLayer::BeaconTimer::copy()
{
    return new BeaconTimer(*this);
}

} // namespace dtn

#endif // __linux
