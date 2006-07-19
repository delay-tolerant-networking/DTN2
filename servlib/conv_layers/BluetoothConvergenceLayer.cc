#include <config.h>
#ifdef OASYS_BLUETOOTH_ENABLED

#ifndef MIN
# define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif

#include <sys/poll.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <bluetooth/bluetooth.h>
#include <errno.h>
extern int errno;

#include <oasys/thread/Notifier.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/bluez/Bluetooth.h>
#include <oasys/bluez/BluetoothSocket.h>
#include <oasys/bluez/RFCOMMClient.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/Random.h>

#include "BluetoothConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/AnnounceBundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "contacts/OndemandLink.h"
#include "contacts/ContactManager.h"
#include "routing/BundleRouter.h"

namespace dtn {

struct BluetoothConvergenceLayer::Params BluetoothConvergenceLayer::defaults_;
struct BluetoothConvergenceLayer::ConnectionManager BluetoothConvergenceLayer::connections_;


/******************************************************************************
 *
 * BluetoothConvergenceLayer
 *
 *****************************************************************************/

BluetoothConvergenceLayer::BluetoothConvergenceLayer() :
    ConvergenceLayer("BluetoothConvergenceLayer", "bt")
{
    // set defaults here, then let ../cmd/ParamCommand.cc (as well as the link
    // specific options) handle changing them
    bacpy(&defaults_.local_addr_,BDADDR_ANY);
    bacpy(&defaults_.remote_addr_,BDADDR_ANY);
    defaults_.hcidev_             = "hci0";
    defaults_.bundle_ack_enabled_ = true;
    defaults_.partial_ack_len_    = 1024;
    defaults_.writebuf_len_       = 32768;
    defaults_.readbuf_len_        = 32768;
    defaults_.keepalive_interval_ = 30;
    defaults_.retry_interval_     = 5;
    defaults_.min_retry_interval_ = 5;
    defaults_.max_retry_interval_ = 10 * 60;
    defaults_.rtt_timeout_        = 30000;  // msecs
    defaults_.neighbor_poll_interval_ = 0; // no polling
}

/**
 * Parse variable args into a parameter structure
 */
bool
BluetoothConvergenceLayer::parse_params(Params* params, int argc, const char** argv,
                                 const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::BdAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::BdAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::StringOpt("hcidev", &params->hcidev_));
    p.addopt(new oasys::BoolOpt("bundle_ack_enabled",
                                &params->bundle_ack_enabled_));
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
    p.addopt(new oasys::UIntOpt("neighbor_poll_interval",
                                &params->neighbor_poll_interval_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
}

/**
 * Register a new interface.
 */
bool
BluetoothConvergenceLayer::interface_up(Interface* iface,
                                 int argc, const char* argv[])
{
    log_debug("adding interface %s",iface->name().c_str());

    Params *params = new Params(defaults_);
    const char* invalid;
    if (!parse_params(params, argc, argv, &invalid)) {
      log_err("error parsing interface options: invalid option '%s'",
              invalid);
      return false;
    }

    // check for valid Bluetooth address or device name
    if (bacmp(&params->local_addr_,BDADDR_ANY) == 0 ) {
        // try to read bdaddr from HCI device name
        oasys::Bluetooth::hci_get_bdaddr(params->hcidev_.c_str(),
                                         &params->local_addr_);
        if (bacmp(&params->local_addr_,BDADDR_ANY) == 0 ) {
            // cannot proceed without valid local Bluetooth device
            log_err("invalid local address setting of BDADDR_ANY");
            return false;
        }
    }
    
    // create a new server socket for the requested interface using 
    // ConnectionManager's factory method (Bluetooth can only allow one 
    // process at a time to bind to bdaddr_t so we track anything that 
    // wants to bind using ConnectionManager)
    Listener* receiver = connections_.listener(this,params);
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());

    // Scan each of RFCOMM's 30 channels, bind to first available
    if (receiver->rc_bind() != 0)
        return false; // error log already emitted

    if (receiver->listen() != 0)
        return false; // error log already emitted

    receiver->start();

    // scan for neighbors
    if (params->neighbor_poll_interval_ > 0) {
        log_debug("Starting up neighbor polling with interval=%d",
                  params->neighbor_poll_interval_);
        receiver->nd_ = new NeighborDiscovery(this,params);
        receiver->nd_->logpathf("%s/neighbordiscovery",logpath_);
        receiver->nd_->start();
    }

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);

    return true;
}

/**
 * Bring down the interface
 */
bool
BluetoothConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Listener* receiver = (Listener*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt_from_io();

    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }
    receiver->close();

    if (receiver->nd_ != NULL) {
        receiver->nd_->set_should_stop();
        receiver->nd_->interrupt();
        while (! receiver->nd_->is_stopped()) {
            oasys::Thread::yield();
        }
        delete receiver->nd_;
        receiver->nd_ = NULL;
    }

    bool res = connections_.del_listener(receiver);
    delete receiver;
    return res;
}

/**
 * Dump out CL specific interface information.
 */
void
BluetoothConvergenceLayer::dump_interface(Interface* iface,
                                   oasys::StringBuffer* buf)
{
    Params* params = &((Listener*)iface->cl_info())->params_;

    char buff[18];
    buf->appendf("\tlocal_addr: %s device: %s\n",
                 oasys::Bluetooth::batostr(&params->local_addr_,buff),
                 params->hcidev_.c_str());

    if (bacmp(&params->remote_addr_,BDADDR_ANY) != 0)
        buf->appendf("\tremote_addr: %s\n",
                     oasys::Bluetooth::batostr(&params->remote_addr_,buff));

    if (params->neighbor_poll_interval_ > 0) 
        buf->appendf("\tneighbor_poll_interval: %d\n",
                     params->neighbor_poll_interval_);
}

/**
 * Create any CL-specific components of the Link.
 *
 * This parses and validates the parameters and stores them in the
 * CLInfo slot in the Link class.
 */
bool
BluetoothConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    bdaddr_t addr;

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // validate the link next hop address
    if (! parse_nexthop(link->nexthop(), &addr)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (bacmp(&addr,BDADDR_ANY) == 0 ) {
        log_err("invalid host in next hop address '%s'", link->nexthop());
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
    if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
        // try to read local adapter's address
        oasys::Bluetooth::hci_get_bdaddr(params->hcidev_.c_str(),
                                         &params->local_addr_);
        if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
            log_err("invalid local address setting of BDADDR_ANY");
            return false;
        }
    }

    // XXX/demmer this seems like a 1/2 done patch
    //char buff[18];
    //link->set_local(oasys::Bluetooth::batostr(&params->local_addr_,buff));

    // Nothing further, if OpLink
    if (link->type() == Link::OPPORTUNISTIC) {
        delete params;
        return true;
    }

    // copy the retry parameters from the link itself
    params->retry_interval_     = link->retry_interval_;
    params->min_retry_interval_ = link->params().min_retry_interval_;
    params->max_retry_interval_ = link->params().max_retry_interval_;

    link->set_cl_info(params);

    return true;
}

/**
 * Dump out CL specific link information
 */
void
BluetoothConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    Params* params = (Params*) link->cl_info();

    char buff[18];
    buf->appendf("\tlocal_addr: %s\n",
                 oasys::Bluetooth::batostr(&params->local_addr_,buff));
    buf->appendf("\tremote_addr: %s\n",
                 oasys::Bluetooth::batostr(&params->remote_addr_,buff));
}

/**
 * Open the connection to the given contact and prepare for
 * bundles to be transmitted.
 */
bool
BluetoothConvergenceLayer::open_contact(const ContactRef& contact)
{
    bdaddr_t addr;

    Link* link = contact->link();
    log_debug("opening contact on link *%p", link);

    // parse out the address / port from the nexthop address. note
    // that these should have been validated in init_link() above, so
    // we ASSERT as such
    bool valid = parse_nexthop(link->nexthop(),&addr);
    ASSERT(valid == true);
    ASSERT(bacmp(&addr,BDADDR_ANY) != 0);

    Params* params = (Params*)link->cl_info();

    // Using ConnectionManager factory method to manage bind() contention;
    // reuse existing passive or create new active
    Connection* sender = connections_.connection(this,addr,params);

    if (sender == NULL) return false;

    // save this contact
    sender->set_contact(contact);
    sender->start();

    return true;
}

/**
 * Close the connection to the contact.
 */
bool
BluetoothConvergenceLayer::close_contact(const ContactRef& contact)
{
    Connection* sender = (Connection*)contact->cl_info();
    log_info("close_contact *%p", contact.object());

    if (sender != NULL) {

        if (!sender->is_stopped() && !sender->should_stop()) {
            log_debug("interrupting connection thread");
            sender->set_should_stop();
            sender->interrupt_from_io();
            oasys::Thread::yield();
        }
    
        // the connection thread will delete itself when it terminates
        // so we can't check any state in the thread itself (i.e. the
        // is_stopped flag). however, it first clears the cl_info slot
        // in the Contact class which is our indication that it has
        // exited, allowing us to return

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
 * Send a bundle to the Contact, mark the link as busy, and queue the
 * bundle on the Connection's bundle queue.
 */
void BluetoothConvergenceLayer::send_bundle(const ContactRef& contact,Bundle* bundle) 
{
    log_info("send_bundle *%p to *%p",bundle,contact.object());
    Connection* sender = (Connection*)contact->cl_info();

    ASSERT(sender != NULL);

    // set the busy state to apply bundle backpressure
    if ((sender->queue_->size() + 1) >= 3)
         contact->link()->set_state(Link::BUSY);

    sender->queue_->push_back(bundle);
}

bool
BluetoothConvergenceLayer::parse_nexthop(const char* nexthop, 
                                         bdaddr_t* addr)
{
    std::string tmp;
    bdaddr_t ba;
    const char* p = nexthop;
    int numcolons = 5; // expecting 6 colons total

    while (numcolons > 0) {
        p = strchr(p+1, ':');
        if (p != NULL) {
            numcolons--;
        } else {
            log_warn("bad format for remote Bluetooth address: '%s'",
                     nexthop);
            return false;
        }
    }
    tmp.assign(nexthop, p - nexthop + 3);
    oasys::Bluetooth::strtoba(tmp.c_str(),&ba);

    bacpy(addr,&ba);
    return true;
}


/******************************************************************************
 *
 * BluetoothConvergenceLayer::ConnectionManager
 *
 *****************************************************************************/
BluetoothConvergenceLayer::Listener*
BluetoothConvergenceLayer::ConnectionManager::listener(
                                                BluetoothConvergenceLayer *cl,
                                                Params* params)
{
    ASSERT(params);
    ASSERT(bacmp(&params->local_addr_,BDADDR_ANY) != 0);

    Listener *l = listener(params->local_addr_);
    if (l != NULL) return l;

    // Create a new one, store it, then return it
    l = new Listener(cl,params);
    addListener(l);
    return l;
}

BluetoothConvergenceLayer::Listener*
BluetoothConvergenceLayer::ConnectionManager::listener(bdaddr_t& addr)
{
    if (bacmp(&addr,BDADDR_ANY) == 0) {
        log_info("ConnectionManager::listener called with BDADDR_ANY");
        return NULL;
    }

    char buff[18];

    // Search listeners
    it_ = l_map_.find(addr);
    if( it_ != l_map_.end() ) {
        log_debug("Retrieving Listener(%p) for bdaddr %s",
                  (*it_).second,oasys::Bluetooth::batostr(&addr,buff));
        return (*it_).second;
    }

    // No luck!
    log_debug("Nothing found in ConnectionManager for bdaddr %s",
              oasys::Bluetooth::batostr(&addr,buff));

    return NULL;
}

bool
BluetoothConvergenceLayer::ConnectionManager::delListener(Listener* l)
{
    ASSERT(l != NULL);

    bdaddr_t addr = l->local_addr();
    return (l_map_.erase(addr) > 0);
}

void
BluetoothConvergenceLayer::ConnectionManager::addListener(Listener* l)
{
    ASSERT(l != NULL);

    char buff[18];
    bdaddr_t addr;
    l->local_addr(addr);

    ASSERT(bacmp(&addr,BDADDR_ANY) != 0);

    log_debug("Adding Listener(%p) for bdaddr %s",
              l,oasys::Bluetooth::batostr(&addr,buff));
    l_map_[addr]=l;
}

bool
BluetoothConvergenceLayer::ConnectionManager::del_listener(Listener* l)
{
    ASSERT(l != NULL);

    return delListener(l);
}

BluetoothConvergenceLayer::Connection*
BluetoothConvergenceLayer::ConnectionManager::connection(
                                          BluetoothConvergenceLayer* cl,
                                          bdaddr_t& addr,
                                          Params* params)
{
    ASSERT(bacmp(&addr,BDADDR_ANY) != 0);
    ASSERT(params != NULL);
    ASSERT(bacmp(&params->local_addr_,BDADDR_ANY) != 0);

    // search for passive connections first
    Listener* prev = listener(params->local_addr_);

    //XXX/wilson Need to keep some registry of Connections to
    // keep count and reflect the seven or less simultaneous Bluetooth
    // limitation here at the application layer
    if (prev != NULL ) {

        log_debug("Instantiating new connection");
        Connection *c = new Connection(cl,addr,params);
        c->listener_ = prev;
        return c;

    } else {

        // Failure state ... dump then panic

        char buff[18];
        // Dump listeners
        for(it_ = l_map_.begin(); it_ != l_map_.end(); it_++) {
            Listener *l = (*it_).second;
            bdaddr_t ba = (*it_).first;
            log_debug("Listener\tListener %p bdaddr %s",
                      l,oasys::Bluetooth::batostr(&ba,buff));
        }
        // Complain loudly
        PANIC("ConnectionManager: new connection requested for %s "
              "where no previous listener existed",
              oasys::Bluetooth::batostr(&addr,buff));
    }

    // not reached
    return NULL;
}


/*****************************************************************************
 *
 * BluetoothConvergenceLayer::Listener
 *
 *****************************************************************************/
BluetoothConvergenceLayer::Listener::Listener(BluetoothConvergenceLayer* cl,
                                       BluetoothConvergenceLayer::Params* params)
    : RFCOMMServerThread("/cl/bt/listener",oasys::Thread::INTERRUPTABLE),
      params_(*params),
      cl_(cl)
{
    set_notifier(new oasys::Notifier("/cl/bt/listener"));

    ASSERT(bacmp(&params->local_addr_,BDADDR_ANY) != 0);

    set_local_addr(params->local_addr_);

    // pause every second to check for interrupt
    set_accept_timeout(1000);

    logfd_  = false;
    nd_ = NULL;
}

void
BluetoothConvergenceLayer::Listener::accepted(int fd, bdaddr_t addr, u_int8_t channel)
{
    ASSERT(cl_ != NULL);
    Connection *c = new Connection(cl_,fd,addr,channel,&params_);
    c->listener_ = this;
    c->start();
    // this channel is now taken over by the passive, so close() and rc_bind()
    // all over again
    close();
    ASSERT(rc_bind() == 0);
    ASSERT(listen() == 0);
}


/******************************************************************************
 *
 * BluetoothConvergenceLayer::Connection
 *
 *****************************************************************************/

/**
 * Constructor for the active connection side of a connection.
 */
BluetoothConvergenceLayer::Connection::Connection(
        BluetoothConvergenceLayer* cl,
        bdaddr_t remote_addr,
        BluetoothConvergenceLayer::Params* params)
    : Thread("BluetoothConvergenceLayer::Connection"),
      Logger("BluetoothConvergenceLayer::Connection", "/dtn/cl/bt/conn"),
      params_(*params), listener_(NULL), cl_(cl), initiate_(true),
      contact_("BluetoothConvergenceLayer::Connection"),
      announce_("BluetoothConvergenceLayer::Connection")
{
    char buff[18];
    logpath_appendf("/%s",oasys::Bluetooth::batostr(&remote_addr,buff));

    queue_ = new BlockingBundleList(logpath_);
    queue_->notifier()->logpath_appendf("/queue");
    Thread::set_flag(Thread::DELETE_ON_EXIT);
    sock_ = new oasys::RFCOMMClient();
    sock_->set_local_addr(params->local_addr_);
    sock_->logpathf("%s/sock",logpath_);
    sock_->set_logfd(false);
    sock_->set_remote_addr(remote_addr);
    sock_->set_notifier(new oasys::Notifier("/cl/bt/active-connection"));
    sock_->get_notifier()->logpath_appendf("/sock_notifier");
    event_notifier_ = new oasys::Notifier(logpath_);
    event_notifier_->logpath_appendf("/event_notifier");
}

/**
 * Constructor for the passive accept side of a connection.
 */
BluetoothConvergenceLayer::Connection::Connection(
        BluetoothConvergenceLayer* cl,
        int fd,
        bdaddr_t remote_addr,
        u_int8_t channel,
        Params* params)
    : Thread("BluetoothConvergenceLayer::Connection"),
      Logger("BluetoothConvergenceLayer::Connection", "/dtn/cl/bt/conn"),
      params_(*params), listener_(NULL), cl_(cl), initiate_(false),
      contact_("BluetoothConvergenceLayer::Connection"),
      announce_("BluetoothConvergenceLayer::Connection")
{
    char buff[18];
    logpathf("/dtn/cl/bt/passive-conn/%s:%d", oasys::Bluetooth::batostr(&remote_addr,buff),
             channel);
    queue_ = NULL;
    Thread::set_flag(Thread::DELETE_ON_EXIT);
    sock_ = new oasys::RFCOMMClient(fd,remote_addr,channel,logpath_);
    sock_->set_local_addr(params->local_addr_);
    sock_->logpathf("%s/sock",logpath_);
    sock_->set_logfd(false);
    sock_->set_notifier(new oasys::Notifier("/cl/bt/passive-connection"));
    sock_->get_notifier()->logpath_appendf("/sock_notifier");
    event_notifier_ = NULL;
}

/**
 * Destructor
 */
BluetoothConvergenceLayer::Connection::~Connection()
{
    if (queue_) delete queue_;
    delete sock_;
    if (event_notifier_) delete event_notifier_;
}


/**
 * The main loop for a connection. Based on the initial construction state, it
 * either initiates a connection to the other side, or accepts one, then calls
 * main_loop.
 *
 * The whole thing is wrapped in a big loop so the active side of the
 * connection is restarted if it's supposed to be, e.g. for links that are
 * configured as ALWAYSON but end up breaking the connection.
 */
void
BluetoothConvergenceLayer::Connection::run()
{
    // XXX/demmer much of this could be abstracted into a generic CL
    // ConnectionThread class, assuming we had another
    // connection-oriented CL that we wanted to support (e.g. SCTP)
    
    while (1) {
        log_debug("connection main loop starting up...");
        
        if (initiate_)
        {
            ASSERT(contact_ != NULL);
            Link* link = contact_->link();
            
            if (! connect()) {

                //XXX/wilson or demmer
                // Add params for number of times to retry
                // and for how long to sleep between tries
                int param_num_retries = 3;
                int param_how_long = 5;

                if (link->type() == Link::OPPORTUNISTIC) {
                    do {
                        sleep(param_how_long);
                        if(--param_num_retries == 0) break;
                    } while (!connect());
                }
                log_debug("connection failed");
                break_contact(ContactEvent::BROKEN);
                goto broken;
            }

            // now that we've successfully connected, reset the retry timer
            // to the minimum interval
            params_.retry_interval_ = params_.min_retry_interval_;

            // signal the OPENING state
            if (link->state() != Link::OPENING) {
                link->set_state(Link::OPENING);
            }

            send_loop();

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


            recv_loop();
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
        // if we're interrupted) and try to re-establish the connection
        ASSERT(sock_->get_notifier());
        ASSERT(params_.retry_interval_ != 0);
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
 * Send announcement bundle to bluetooth address
 */
bool
BluetoothConvergenceLayer::Connection::send_announce()
{
    char buff[18]; //used by oasys::batostr below

    bdaddr_t remote;
    sock_->remote_addr(remote);

    ASSERT( bacmp(&remote,BDADDR_ANY) != 0 );

    // create the ANNOUNCE bundle
    if (announce_ == NULL) {
        announce_ = new Bundle();
        // Mark this bundle as Admin,
        // set bundle type to ADMIN_ANNOUNCE, and
        // set source eid to local_eid
        AnnounceBundle::create_announce_bundle(announce_.object(),
            BundleDaemon::instance()->local_eid());
    }

    log_debug("attempting to contact %s with ANNOUNCE",
              oasys::Bluetooth::batostr(&remote,buff));

    oasys::StaticStringBuffer<1024> buf;
    buf.appendf("BTCL AnnounceBundle:\n");
    announce_->format_verbose(&buf);
    log_multiline(oasys::LOG_DEBUG, buf.c_str());

    // first reserve space in the buffers
    rcvbuf_.reserve(params_.readbuf_len_);
    sndbuf_.reserve(params_.writebuf_len_);

    // sock_ will be established if this is the receiving thread
    // sending back a response
    if (sock_->state() != oasys::BluetoothSocket::ESTABLISHED) {
        if (! connect()) {
            log_debug("connect failed");
            return false;
        }
    }

    return send_bundle(announce_.object());
}


/**
 * Receive bundle (expected to be response Bundle Announcement)
 */
void
BluetoothConvergenceLayer::Connection::recv_announce()
{
    int timeout;
    int ret;
    char typecode;
    // if there's nothing in the buffer,
    // block waiting for the one byte typecode
    if (rcvbuf_.fullbytes() == 0) {
        ASSERT(rcvbuf_.end() == rcvbuf_.data()); // sanity

        timeout = 2 * params_.keepalive_interval_ * 1000;
        log_debug("recv_announce: blocking on frame... (timeout %d)", timeout);

        ret = sock_->timeout_read(rcvbuf_.end(),
                                  rcvbuf_.tailbytes(),
                                  timeout);
        if (ret == oasys::IOEOF || ret == oasys::IOERROR) {
            log_info("recv_announce: remote connection unexpectedly closed");
shutdown:
            break_contact(ContactEvent::BROKEN);
            return;

        } else if (ret == oasys::IOTIMEOUT) {
            log_info("recv_announce: no message heard for > %d msecs -- "
                     "breaking contact", timeout);
            goto shutdown;
        }

        rcvbuf_.fill(ret);
        note_data_rcvd();
    }

    typecode = *rcvbuf_.start();
    rcvbuf_.consume(1);

    if (typecode != BUNDLE_DATA)
        goto shutdown;

    if (! recv_bundle() )
        goto shutdown;
}


/**
 * Send one bundle over the wire.
 *
 * Return true if the bundle was sent successfully, false if not.
 */
bool
BluetoothConvergenceLayer::Connection::send_bundle(Bundle* bundle)
{
    int header_len;
    size_t sdnv_len;
    size_t bthdr_len;
    u_char bthdr_buf[32];
    size_t block_len;
    size_t payload_len = bundle->payload_.length();
    size_t payload_offset = 0;
    const u_char* payload_data;

    // first put the bundle on the inflight_ queue
    if (AnnounceBundle::parse_announce_bundle(bundle) == false)
        inflight_.push_back(InFlightBundle(bundle));

    // Each bundle is preceded by a BUNDLE_DATA typecode and then a
    // BundleDataHeader that is variable-length since it includes an
    // SDNV for the total bundle length.
    //
    // So, first calculate the length of the bundle headers while we
    // put it into the send buffer, then fill in the small CL buffer

retry_headers:
    header_len =
        BundleProtocol::format_header_blocks(bundle, sndbuf_.buf(),
                                             sndbuf_.buf_len());
    if (header_len < 0) {
        log_debug("send_bundle: bundle header too big for buffer len %zu -- "
                  "doubling size and retrying", sndbuf_.buf_len());
        goto retry_headers;
    }

    sdnv_len = SDNV::encoding_len(header_len + payload_len);
    bthdr_len = 1 + sizeof(BundleDataHeader) + sdnv_len;

    ASSERT(sizeof(bthdr_buf) >= bthdr_len);

    // Now fill in the type code and the bundle data header (with the
    // sdnv for the total bundle length)
    bthdr_buf[0] = BUNDLE_DATA;
    BundleDataHeader* dh = (BundleDataHeader*)(&bthdr_buf[1]);
    memcpy(&dh->bundle_id, &bundle->bundleid_, sizeof(bundle->bundleid_));
    SDNV::encode(header_len + payload_len, &dh->total_length[0], sdnv_len);

    // Build up a two element iovec
    struct iovec iov[2];
    iov[0].iov_base = (char*)bthdr_buf;
    iov[0].iov_len  = bthdr_len;

    iov[1].iov_base = (char*)sndbuf_.buf();
    iov[1].iov_len  = header_len;

    // send off the preamble and the headers
    log_debug("send_bundle: sending bundle id %d "
              "btcl hdr len: %zu, bundle header len: %zu payload len: %zu",
              bundle->bundleid_, bthdr_len, header_len, payload_len);

    int cc = sock_->writevall(iov, 2);

    if (cc == oasys::IOINTR) {
        log_info("send_bundle: interrupted while sending bundle header");
        break_contact(ContactEvent::USER);
        return false;
    } else if (cc != (int)bthdr_len + header_len) {
        if (cc == 0) {
            log_err("send_bundle: remote side closed connection");
        } else {
            log_err("send_bundle: "
                    "error sending bundle header (wrote %d/%zu): %s",
                    cc, (bthdr_len + header_len), strerror(errno));
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
        payload_data = bundle->payload_.read_data(
                         payload_offset,
                         block_len,
                         sndbuf_.buf(block_len),
                         BundlePayload::KEEP_FILE_OPEN);

        log_debug("send_bundle: sending %zu byte block %p",
                  block_len, payload_data);

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
                        "error sending bundle block (wrote %d/%zu): %s",
                        cc, block_len, strerror(errno));
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
    if (! params_.bundle_ack_enabled_ && 
          (AnnounceBundle::parse_announce_bundle(bundle) == false)) {
        inflight_.pop_front();
        BundleDaemon::post(
                new BundleTransmittedEvent(bundle, contact_, payload_len, 0));
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
BluetoothConvergenceLayer::Connection::recv_bundle()
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
 incomplete_bt_header:
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
        goto incomplete_bt_header;
    }

    // copy out the data header but don't advance the buffer (yet)
    memcpy(&datahdr, rcvbuf_.start(), sizeof(BundleDataHeader));
    
    // parse out the SDNV that encodes the whole bundle length
    sdnv_len = SDNV::decode((u_char*)rcvbuf_.start() + sizeof(BundleDataHeader),
                            rcvbuf_.fullbytes() - sizeof(BundleDataHeader),
                            &total_len);
    if (sdnv_len < 0) {
        log_err("recv_bundle: read too short to encode SDNV");
        goto incomplete_bt_header;
    }

    // got the full bt header, so skip that much in the buffer
    rcvbuf_.consume(sizeof(BundleDataHeader) + sdnv_len);

 incomplete_bundle_header:
    // now try to parse the headers into the new bundle, which may
    // require reading in more data and possibly increasing the size
    // of the stream buffer
    header_len = BundleProtocol::parse_header_blocks(bundle,
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
              "sender bundle id %d, header_length %zu, total_length %llu",
              datahdr.bundle_id, header_len, total_len);
    rcvbuf_.consume(header_len);

    // all lengths have been parsed, so we can do some length
    // validation checks
    payload_len = bundle->payload_.length();
    if (total_len != header_len + payload_len) {
        log_err("recv_bundle: bundle length mismatch -- "
                "total_len %llu, header_len %zu, payload_len %zu",
                total_len, header_len, payload_len);
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
        
        log_debug("recv_bundle: got %zu byte chunk, rcvd_len %zu",
                  rcvbuf_.fullbytes(), rcvd_len);
        
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
        if (rcvd_len - acked_len > params_.partial_ack_len_ &&
               AnnounceBundle::parse_announce_bundle(bundle) == false) {
            log_debug("recv_bundle: "
                      "got %zu bytes acked %zu, sending partial ack",
                      rcvd_len, acked_len);
            
            if (! send_ack(datahdr.bundle_id, rcvd_len)) {
                goto done;
            }
            acked_len = rcvd_len;
        }
        
    } while (rcvd_len < payload_len);

    // if the sender requested a bundle ack and we haven't yet sent
    // one for the whole bundle in the partial ack check above, send
    // one now
    if (params_.bundle_ack_enabled_ && 
        (AnnounceBundle::parse_announce_bundle(bundle) == false) &&
        (payload_len == 0 || (acked_len != rcvd_len)))
    {
        if (! send_ack(datahdr.bundle_id, payload_len)) {
            goto done;
        }
    }
    
    recvok = true;

 done:
    bundle->payload_.close_file();
    
    if ((!valid) || (!recvok)) {
        // the bundle isn't valid or we didn't get the whole thing
        // so just return
        if (bundle)
            delete bundle;
        
        return false;
    }

    log_debug("recv_bundle: "
              "new bundle id %d arrival, payload length %zu (rcvd %zu)",
              bundle->bundleid_, payload_len, rcvd_len);
    
    // inform the daemon that we got a valid bundle, though it may not
    // be complete (as indicated by passing the rcvd_len)
    ASSERT(rcvd_len <= bundle->payload_.length());
    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, rcvd_len));
    
    // snoop on bundles to see if Announce bundle crosses by
    EndpointID eid;
    if (AnnounceBundle::parse_announce_bundle(bundle,&eid)) {

        ASSERT(params_.neighbor_poll_interval_ > 0);

        char buff[18];
        bdaddr_t remote;
        sock_->remote_addr(remote);
        const char *nexthop =
            (const char *) oasys::Bluetooth::batostr(&remote,buff);

        // AnnounceBundle has been received, which either means
        // 1) this is the first news we've heard of remote
        // or
        // 2) remote is sending this AnnounceBundle as ack to our original

        // log the AnnounceBundle, source EID and remote BT addr
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("received BTCL AnnounceBundle from %s at %s\n",
                    eid.c_str(),
                    nexthop);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_INFO, buf.c_str());

        // if this is the first we've heard of it, announce_ will be NULL
        bool receiver = false;
        if (announce_ == NULL) {
            // so respond on the same connection
            send_announce();
            receiver = true;
        }

        // otherwise, we are the initiator, and this is the response.

        // either way, it's time to light up a link and post it up
        // to BundleDaemon
        ContactManager* cm = BundleDaemon::instance()->contactmgr();

        // Finds the link (if it exists) otherwise creates the link and
        // posts LinkCreatedEvent 
        // Either way, it posts LinkAvailableEvent for this link
        (void)cm->new_opportunistic_link(cl_,new Params(params_),nexthop,&eid);

        if (receiver) {
            // our work here is done; time to self delete
            set_should_stop();
            break_contact(ContactEvent::BROKEN);
            return false; // tells recv_loop to back out
        }
    }
    return true;
}

/**
 * Send an ack message.
 */
bool
BluetoothConvergenceLayer::Connection::send_ack(u_int32_t bundle_id,
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
        log_info("send_ack: problem sending ack (wrote %d/%d): %s",
                cc, total, strerror(errno));
        return false;
    }

    return true;
}


/**
 * Send a keepalive byte.
 */
bool
BluetoothConvergenceLayer::Connection::send_keepalive()
{
    char typecode = KEEPALIVE;
    int ret = sock_->write(&typecode, 1);

    if (ret == oasys::IOINTR) { 
        log_info("send_keepalive: connection interrupted");
        break_contact(ContactEvent::USER);
        return false;
    }

    if (ret != 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return false;
    }
    
    return true;
}


/**
 * Note that we observed some data inbound on this connection.
 */
void
BluetoothConvergenceLayer::Connection::note_data_rcvd()
{
    log_debug("noting receipt of data");
    ::gettimeofday(&data_rcvd_, 0);
}


/**
 * Handle an incoming ack from the receive buffer.
 */
int
BluetoothConvergenceLayer::Connection::handle_ack()
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
        log_debug("handle_ack: not enough space in buffer (got %zu, need %zu...)",
                  rcvbuf_.fullbytes(), sizeof(BundleAckHeader));
        return ENOMEM;
    }

    // if we do, copy it out, after skipping the typecode
    BundleAckHeader ackhdr;
    memcpy(&ackhdr, rcvbuf_.start() + 1, sizeof(BundleAckHeader));
    rcvbuf_.consume(1 + sizeof(BundleAckHeader));

    Bundle* bundle = ifbundle->bundle_.object();
    size_t new_acked_len = ntohl(ackhdr.acked_length);
    size_t payload_len = bundle->payload_.length();
    
    log_debug("handle_ack: got ack length %zu for bundle id %d length %zu",
              new_acked_len, ackhdr.bundle_id, payload_len);
    
    if (ackhdr.bundle_id != bundle->bundleid_) {
        log_err("handle_ack: error: bundle id mismatch %d != %d",
                ackhdr.bundle_id, bundle->bundleid_);
        goto protocol_error;
    }

    if (new_acked_len < ifbundle->acked_len_ || new_acked_len > payload_len)
    {
        log_err("handle_ack: invalid acked length %zu (acked %zu, bundle %zu)",
                new_acked_len, ifbundle->acked_len_,
                payload_len);
        goto protocol_error;
    }

    // check if we're done with the bundle and if we need to unblock the link
    if (new_acked_len == payload_len) {
        inflight_.pop_front();
        
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle, contact_, payload_len, new_acked_len));
        
        if (contact_->link()->state() == Link::BUSY) {
            BundleDaemon::post_and_wait(
                new LinkStateChangeRequest(contact_->link(), Link::AVAILABLE,
                                           ContactEvent::NO_INFO),
                event_notifier_);
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
BluetoothConvergenceLayer::Connection::send_contact_header()
{
    BTCLHeader contacthdr;
    contacthdr.magic = htonl(MAGIC);
    contacthdr.version = BTCL_VERSION;

    contacthdr.flags = 0;
    if (params_.bundle_ack_enabled_)
        contacthdr.flags |= BUNDLE_ACK_ENABLED;

    contacthdr.partial_ack_len    = htons(params_.partial_ack_len_);
    contacthdr.keepalive_interval = htons(params_.keepalive_interval_);
    contacthdr.xx__unused         = 0;

    int cc = sock_->writeall((char*)&contacthdr, sizeof(BTCLHeader));
    if (cc != sizeof(BTCLHeader)) {
        log_err("error writing contact header (wrote %d/%zu): %s",
                cc, sizeof(BTCLHeader), strerror(errno));
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
BluetoothConvergenceLayer::Connection::recv_contact_header(int timeout)
{
    BTCLHeader contacthdr;

    ASSERT(timeout != 0);
    int cc = sock_->timeout_readall((char*)&contacthdr,
                                    sizeof(BTCLHeader),
                                    timeout);

    if (cc == oasys::IOTIMEOUT) {
        log_warn("timeout reading contact header");
        return false;
    }

    if (cc != sizeof(BTCLHeader)) {
        log_err("error reading contact header (read %d/%zu): %d-%s",
                cc, sizeof(BTCLHeader),
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

    if (contacthdr.version != BTCL_VERSION) {
         log_warn("remote sent version %d, expected version %d "
                  "-- disconnecting.", contacthdr.version,
                  BTCL_VERSION);
         return false;
    }

    /*
     * Now do parameter negotiation.
     */
    params_.partial_ack_len_    = MIN(params_.partial_ack_len_,
    ntohs(contacthdr.partial_ack_len));
    params_.keepalive_interval_ = MIN(params_.keepalive_interval_,
            ntohs(contacthdr.keepalive_interval));
    params_.bundle_ack_enabled_ = params_.bundle_ack_enabled_ &&
        (contacthdr.flags & BUNDLE_ACK_ENABLED); 

    note_data_rcvd();
    return true;
}

/**
  * Active (MASTER) connect side of the initial handshake. First connect
  * to the peer side, then exchange ContactHeaders and negotiate session
  * parameters.
 */
bool
BluetoothConvergenceLayer::Connection::connect()
{
    ASSERT(sock_->state() != oasys::BluetoothSocket::ESTABLISHED);
    char buff[18];
    bdaddr_t ba;
    sock_->remote_addr(ba);
    ASSERT(bacmp(&ba,BDADDR_ANY) != 0);
    log_debug("connect: connecting to %s ... ",
              oasys::Bluetooth::batostr(&ba,buff));

    // before attempting to scan the remote host for available channels,
    // shut down the local listener to release its channel for bind()
    // NOTE:  This is only the listener.  Child sockets from previous
    // calls to accepted() are not (and should not be) affected
    ASSERT(listener_ != NULL);
    listener_->set_should_stop();
    listener_->interrupt_from_io();
    while(!listener_->is_stopped()) {
        oasys::Thread::yield();
    }
    listener_->close();

    // scan all 30 channels
    int ret = sock_->rc_connect();

    // re-enable local listener
    // Scan each of RFCOMM's 30 channels, bind to first available
    if (listener_->rc_bind() == 0 &&
        listener_->listen() == 0) {
        listener_->start();
    }

    if (ret != 0) return false;
    log_debug("connect: connection established (channel %d), "
              "sending contact header ... ", sock_->channel());
    if (!send_contact_header()) return false;
    log_debug("connect: waiting for contact header reply ... ");
    if (!recv_contact_header(params_.rtt_timeout_)) return false;
    return true;
}


/**
 * Passive (SLAVE) accept side of initial handshake
 */
bool
BluetoothConvergenceLayer::Connection::accept()
{
    // even with passive reuse, this should originally be true
    ASSERT(contact_ == NULL);
    log_debug("accept: sending contact header ...");
    if (!send_contact_header()) return false;
    log_debug("accept: waiting for peer contact header ...");
    if (!recv_contact_header(params_.rtt_timeout_)) return false;
    return true;
}


/**
 * Send an event to the system indicating that this contact is broken and
 * close this side of the connection.
 *
 * This results in the Connection thread stopping and the system calling
 * the link->close() call which cleans up the connection.
 *
 * If this is the sender-side, we keep a pointer to the contact and 
 * assuming the contact isn't in the process of being closed, we post a 
 * request that it be closed
 */
void
BluetoothConvergenceLayer::Connection::break_contact(ContactEvent::reason_t reason)
{
    ASSERT(sock_);
    char typecode = SHUTDOWN;
    if (sock_->state() != oasys::BluetoothSocket::CLOSED) {
        sock_->set_nonblocking(true);
        sock_->write(&typecode,1);
        sock_->close();
    }

    if (contact_ != NULL) {
        while (!inflight_.empty()) {
            InFlightBundle* inflight = &inflight_.front();
            if (inflight->acked_len_ != 0) {
                BundleDaemon::post(
                    new BundleTransmittedEvent(inflight->bundle_.object(),
                                          contact_,
                                          inflight->acked_len_,
                                          inflight->acked_len_));
            } else {
                BundleDaemon::post(
                    new BundleTransmittedEvent(inflight->bundle_.object(),
                                          contact_,
                                          inflight->bundle_->payload_.length(),
                                          0));
            }
            inflight_.pop_front();
        }
        if (reason != ContactEvent::USER) {
            // if the connection isn't being closed by the user, and
            // the link is open, we need to notify the daemon.
            // typically, we then just bounce back to the main run
            // loop to try to re-establish the connection...
            //
            // we block until the daemon has processed the event to
            // make sure that we don't clear the cl_info slot too
            // early (triggering a crash)
            if (contact_->link()->isopen())
            {
                bool ok = BundleDaemon::post_and_wait(
                    new ContactDownEvent(contact_, reason),
                    event_notifier_, 5000);
                // one particularly annoying condition occurs if we
                // attempt to close the link at the same time that the
                // daemon does -- the thread in close_contact is
                // blocked waiting for us to clear the cl_info slot
                // below.
                //
                // XXX/demmer maybe this should be done for all calls
                // to post_and_wait??
                int total = 0;
                while (!ok) {
                    total += 5;
                    if (should_stop()) {
                        log_notice("bundle daemon took > %d seconds to process event: "
                                   "breaking close_contact deadlock", total);
                        break;
                    }
                    if (total >= 60) 
                        PANIC("bundle daemon took > 60 seconds to process event: "
                              "fatal deadlock condition");
                    log_warn("bundle daemon took > %d seconds to process event", total);
                    ok = event_notifier_->wait(0,5000);
                }
            }
        }
        if (queue_->size() > 0 )
            log_warn("%d bundles still in queue", queue_->size());
        // Once the main thread knows the contact is down (by the event above)
        // we need to signal that we've quit, by clearing the cl_info slot
        if (contact_->cl_info() != NULL ) {
            ASSERT(contact_->cl_info() == this);
            contact_->set_cl_info(NULL);
        }
    } else {
        ASSERT(inflight_.empty());
    }
    // for the passive acceptor or idle link, exit the main loop
    if (!initiate_ || (reason == ContactEvent::IDLE)) set_should_stop();

}


/**
 * Handle an incoming message from the receiver side (i.e. an ack, keepalive,
 * or shutdown. We expect that the caller just completed a poll() such that
 * data is waiting on the socket, therefore read as much as is available into
 * the receive buffer and process until the end of complete messages.
 */
bool
BluetoothConvergenceLayer::Connection::handle_reply()
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
        } else if (typecode == BUNDLE_DATA) {
            rcvbuf_.consume(1);
            if(!recv_bundle()) {
                break_contact(ContactEvent::BROKEN);
                return false;
            }
            return true;
        } else if (typecode == KEEPALIVE) {
            rcvbuf_.consume(1);
            if (!initiate_) send_keepalive();
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
 * The sender side main loop.
 */
void
BluetoothConvergenceLayer::Connection::send_loop()
{
    // someone should already have established the session
    ASSERT(sock_->state() == oasys::BluetoothSocket::ESTABLISHED);

    // store our state in the contact's cl info slot
    ASSERT(contact_ != NULL);
    ASSERT(contact_->cl_info() == NULL);
    contact_->set_cl_info(this);

    // inform the daemon that the contact is available
    BundleDaemon::post_and_wait(new ContactUpEvent(contact_), event_notifier_);

    // reserve space in the buffers
    rcvbuf_.reserve(params_.readbuf_len_);
    sndbuf_.reserve(params_.writebuf_len_);

    log_info("connection established -- (keepalive time %d seconds)",
             params_.keepalive_interval_);

    // build up a poll vector since we need to block below on input
    // from the socket and the bundle list
    struct pollfd pollfds[2];

    struct pollfd* bundle_poll = &pollfds[0];
    bundle_poll->fd          = queue_->notifier()->read_fd();
    bundle_poll->events      = POLLIN;

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

        BundleRef bundle("BTCL::send_loop temporary");

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

            if (sock_poll->revents & POLLNVAL)
            {
                PANIC("send_loop: sock_poll->revents returned with "
                      "POLLNVAL (0x%x)", sock_poll->revents);
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
            if (send_keepalive() == false) return;
            continue;
        }

        // check that it hasn't been too long since the other side
        // sent us some data
        ::gettimeofday(&now,0);
        u_int elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > (2 * params_.keepalive_interval_ * 1000)) {
            log_info("send_loop: no data rcvd for %d msecs "
                     "(sent %zu.%zu, rcvd %zu.%zu, now %zu.%zu) -- closing contact",
                     elapsed,
                     (u_int)keepalive_sent_.tv_sec,
                     (u_int)keepalive_sent_.tv_usec,
                     (u_int)data_rcvd_.tv_sec, (u_int)data_rcvd_.tv_usec,
                     (u_int)now.tv_sec, (u_int)now.tv_usec);
            break_contact(ContactEvent::BROKEN);
            return;
        }

        // check if the connection has been idle for too long (ONDEMAND only)
        if (idle && (contact_->link()->type() == Link::ONDEMAND)) {
            u_int idle_close_time =
                ((OndemandLink*)contact_->link())->idle_close_time_;

            elapsed = TIMEVAL_DIFF_MSEC(now, idle_start);
            if (idle_close_time != 0 && (elapsed > idle_close_time * 1000))
            {
                log_info("connection idle for %d msecs, closing.",elapsed);
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
BluetoothConvergenceLayer::Connection::recv_loop()
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

            ret = sock_->timeout_read(rcvbuf_.end(),
                                      rcvbuf_.tailbytes(),
                                      timeout);

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

void
BluetoothConvergenceLayer::NeighborDiscovery::send_announce(bdaddr_t remote)
{
    char buff[18]; // used by oasys::batostr
    const char *nexthop = oasys::Bluetooth::batostr(&remote,buff);

    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    Link* link = cm->find_link_to(nexthop,"bt");
    if (link == NULL) {
        // Using ConnectionManager factory method to manage bind()
        // contention ... returns NULL if error
        Connection *sender = cl_->connections_.connection(
                                      cl_,remote,&params_);
        if (sender != NULL) {
            log_info("sending announce bundle to %s", nexthop);
            if (sender->send_announce()) {
                // new OpportunisticLink gets created within recv_announce,
                // if successful
                sender->recv_announce();
            }
            // thread never started, so cleanup is not automatic
            delete sender;
            sender = NULL;
        }
    }
}

void
BluetoothConvergenceLayer::NeighborDiscovery::run()
{ 
    if (poll_interval_ == 0) {
        return;
    }

    // register DTN service with local SDP daemon
    oasys::BluetoothServiceRegistration sdp_reg(&params_.local_addr_);
    if (sdp_reg.success() == false) {
        log_err("SDP registration failed");
        return;
    }

    // loop forever (until interrupted)
    while (true) {

        // randomize the sleep time:
        // the point is that two nodes with zero prior knowledge of each other
        // need to be able to discover each other in a reasonably short time.
        // if common practice is that all set their poll_interval to 1 or 30 or x
        // then there's the chance of synchronization or syncopation such that 
        // discovery doesn't happen.
        int sleep_time = oasys::Random::rand(poll_interval_);

        log_debug("sleep_time %d",sleep_time);
        sleep(sleep_time);

        // initiate inquiry on local Bluetooth controller
        int nr = inquire(); // blocks until inquiry process completes

        if (should_stop()) break;

        if (nr < 0) {
            log_debug("no Bluetooth devices in range");
            continue;
        }

        // enumerate any remote Bluetooth adapters in range
        oasys::BluetoothInquiryInfo bii;
        while (next(bii) != -1) {

            // query SDP daemon on remote host for DTN's registration
            oasys::BluetoothServiceDiscoveryClient sdpclient;
            sdpclient.set_local_addr(params_.local_addr_);
            if (sdpclient.is_dtn_router(bii.addr_)) {
                send_announce(bii.addr_);
            }
            if (should_stop()) break;
        }
        if (should_stop()) break;

        // flush results of previous inquiry
        reset();
    }

    // interrupted! unregister
    log_info("Bluetooth inquiry interrupted by user");
}

} // namespace dtn

namespace oasys {

BdAddrOpt::BdAddrOpt(const char* opt, bdaddr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

BdAddrOpt::BdAddrOpt(char shortopt, const char* longopt, bdaddr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
BdAddrOpt::set(const char* val, size_t len)
{
    bdaddr_t newval;
    (void)len;

    /* returns NULL on failure */
    if (oasys::Bluetooth::strtoba(val, &newval) == 0) {
        return -1;
    }

    *((bdaddr_t*)valp_) = newval;

    if (setp_)
        *setp_ = true;

    return 0;
}

UInt8Opt::UInt8Opt(const char* opt, u_int8_t* valp,
                   const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

UInt8Opt::UInt8Opt(char shortopt, const char* longopt, u_int8_t* valp,
                   const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
UInt8Opt::set(const char* val, size_t len)
{
    u_int newval;
    char* endptr = 0;

    newval = strtoul(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;

    if (newval > 65535)
        return -1;

    *((u_int8_t*)valp_) = (u_int8_t)newval;

    if (setp_)
        *setp_ = true;

    return 0;
}

} // namespace oasys

#endif  /* OASYS_BLUETOOTH_ENABLED */
