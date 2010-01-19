
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sys/poll.h>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "LTPConvergenceLayer.h"

#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"

#ifdef LTP_ENABLED


namespace dtn{

struct LTPConvergenceLayer::Params LTPConvergenceLayer::defaults_;

void
LTPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
}

LTPConvergenceLayer::LTPConvergenceLayer() : IPConvergenceLayer("LTPConvergenceLayer", "ltp")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
}

bool
LTPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
};

bool
LTPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }
    
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params);
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());

	str2ltpaddr((char*)intoa(params.local_addr_),&receiver->listener);
	receiver->listener.sock.sin_port=params.local_port_;

	receiver->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

bool
LTPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    delete receiver;
    return true;
}

void
LTPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Receiver*)iface->cl_info())->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected\n");
    }
}

bool
LTPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    log_info("adding %s link %s", link->type_str(), link->nexthop());

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &addr, &port);

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);
    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }
    
    link->set_cl_info(params);
    log_debug("LTP::LINK_UP, local: %s:%d, remote: %s:%d",
		intoa(params->local_addr_),params->local_port_,
		intoa(params->remote_addr_),params->remote_port_);
    return true;
}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
}

//----------------------------------------------------------------------
bool
LTPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    log_info("LTP::CONTACT_OPEN");
    log_info("LTPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());
    
    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("can't lookup hostname in next hop address '%s'",
                link->nexthop());
        return false;
    }

    // if the port wasn't specified, use the default
    if (port == 0) {
        port = LTPCL_DEFAULT_PORT;
    }

    Params* params = (Params*)link->cl_info();
    
    // create a new sender structure
    Sender* sender = new Sender(link->contact());

    if (!sender->init(params, addr, port)) {
        log_err("error initializing contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        delete sender;
        return false;
    }
        
    contact->set_cl_info(sender);
    BundleDaemon::post(new ContactUpEvent(link->contact()));
    
    // XXX/demmer should this assert that there's nothing on the link
    // queue??
    
    return true;
}

//----------------------------------------------------------------------
bool
LTPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();

    log_info("LTP: close_contact *%p", contact.object());

    if (sender) {
        delete sender;
        contact->set_cl_info(NULL);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
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

    int len = sender->send_bundle(bundle);

    if (len > 0) {
        link->del_from_queue(bundle, len);
        link->add_to_inflight(bundle, len);
        BundleDaemon::post(
            new BundleTransmittedEvent(bundle.object(), contact, link, len, 0));
    }
}

//----------------------------------------------------------------------
LTPConvergenceLayer::Receiver::Receiver(LTPConvergenceLayer::Params* params)
    : Logger("LTPConvergenceLayer::Receiver",
             "/dtn/cl/ltp/receiver/%p", this),
      Thread("LTPConvergenceLayer::Receiver")

{
    logfd_  = false;
    params_ = *params;
    should_stop_ = false;
    s_sock = 0;

    // start our thread
}

//----------------------------------------------------------------------
void LTPConvergenceLayer::Receiver::set_should_stop() {
	should_stop_ = true;
}

bool LTPConvergenceLayer::Receiver::should_stop() {
	return should_stop_;
}

void LTPConvergenceLayer::Receiver::set_sock(int sockval) {
	s_sock = sockval;
}

int LTPConvergenceLayer::Receiver::get_sock() {
	return s_sock;
}

//----------------------------------------------------------------------


//----------------------------------------------------------------------
LTPConvergenceLayer::Sender::Sender(const ContactRef& contact)
    : Logger("LTPConvergenceLayer::Sender",
             "/dtn/cl/ltp/sender/%p", this),
      contact_(contact.object(), "LTPConvergenceLayer::Sender")
{
}

//----------------------------------------------------------------------
bool
LTPConvergenceLayer::Sender::init(Params* params,
                                  in_addr_t addr, u_int16_t port)
    
{

	log_info("initialising sender");

	log_info("LTP Sender init: Addr %s",intoa(addr));
	log_info("LTP Sender init: Port %d",port);
    
	params_ = params;
    
    	int rv;
			
	sock = ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
	log_info("LTPCL: Socket: %d",sock);
	
	/*
	rv = ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_CLIENTONLY,&foo,sizeof(foo));

	if (rv) {
		log_info("LTPCL: setsockopt() failed for LTP_SO_CLIENTONLY: rv=%d\n", rv);
	}
	
	if (!sock) {
		log_info("LTPCL: setsockopt() got a zero socket LTP_SO_CLIENTONLY: rv=%d\n", rv);
	}
	*/
	
	/// set the source
	str2ltpaddr((char*)intoa(params->local_addr_),&source);
	source.sock.sin_port=params->local_port_;
	
	///bind
	rv = ltp_bind(sock,(ltpaddr*)&source,sizeof(source));
	if (rv) { 
		log_err("ltp_bind failed.\n");
		return(false);
	}
	
	// set local idea of who I am
	rv=ltp_cue_set_whoiam(&source);
	if (rv) { 
		log_err("ltp_cue_set_whoiam failed.\n");
		return(false);
	}

	///set the destination using only the address for now
	str2ltpaddr((char*)intoa(addr),&dest);
	dest.sock.sin_port=port;

	char *sstr=strdup(ltpaddr2str(&source));
	char *dstr=strdup(ltpaddr2str(&dest));

	log_debug("LTP Sender src: %s, dest: %s\n",sstr,dstr);

	free(sstr);free(dstr);
   
    return true;
}
    
//----------------------------------------------------------------------
int
LTPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle)
{
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);
    bool complete = false;
	//this is creating the bundle and returning the length
	
	size_t total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);

	log_debug("LTP::Begin sendto(), sending %d bytes to %s",
			total_len,ltpaddr2str(&dest));
	
	///code below is a simple test to check ltplib api calls

	char *inbuf = (char*)buf_;
	size_t rv;
	
	/// unused value in the sendto function?
	static int flags = 0;
	
	rv = ltp_sendto(sock,inbuf,total_len,flags,(ltpaddr*)&dest,sizeof(dest));
	if (rv!=total_len) {
		log_err("ltp_sendto failed: %d\n",rv);
		log_debug("LTP::End sendto()");
		return(-1);
	}
	
	log_debug("LTP::End sendto()");

	return(total_len);
}


void LTPConvergenceLayer::Receiver::run() {
    int ret;
    int s_sock=ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
    if (!s_sock) {
    	return;
	}
	int rv=ltp_bind(s_sock,&listener,sizeof(ltpaddr));
	if (rv) { 
		ltp_close(s_sock); 
    	return;
	} 
    int rxbufsize = 500000;
    u_char buf[rxbufsize];
    while (1) {
        if (should_stop())
            break;
		int flags;
		ltpaddr from;
		ltpaddr_len fromlen;
		ret=ltp_recvfrom(s_sock,buf,rxbufsize,flags,(ltpaddr*)&from,(ltpaddr_len*)&fromlen);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
          	}
            break;
        } else if (ret>0) {
			log_info("LTP ltp_recvfrom returned %d byte block\n",ret);
    		// TODO: allow >1 bundle on receipt
			// get it off the stack - gotta hope the Bundle code
			// properly manages the memory - TODO - check that out
			// I might need to free it
    		// the payload should contain a full bundle
    		Bundle* bundle = new Bundle();
    		bool complete = false;
    		int cc = BundleProtocol::consume(bundle, buf, ret, &complete);
    		if (cc < 0 || !complete) {
        		delete bundle;
    		} else {
				BundleDaemon::post(new BundleReceivedEvent(bundle, EVENTSRC_PEER, ret, EndpointID::NULL_EID()));
			}
		}
    }
    ltp_close(s_sock);
    return;
}


}//namespace


#endif

