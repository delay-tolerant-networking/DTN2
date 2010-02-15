/*
 *    Copyright 2010 Trinity College Dublin
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


/// TODO:
/// - receipt of >1 bundle in one LTP block
/// - make MAXINPUTBUNDLE a parameter or something
/// - figure out if anything leaks between LTPlib and DTN2
/// - tests: with schedules, stress, at each point in a 3-hop path
/// - maybe try speed up UDP packet sending in LTPlib, probably a bit slow now 
/// - add LTP configuration file support with good defaults



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

#include "contacts/ContactManager.h"

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

	ltp_inited=false;

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

	// initialise LTPlib
	if (!ltp_inited) {
		int rv=ltp_init();
		if (rv) {
			log_err("LTP initialisation error: %d\n",rv);
		} else {
			log_debug("LTP initialised.\n");
			ltp_inited=true;
		}
	}

    return true;
};

bool
LTPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("LTP adding interface %s", iface->name().c_str());
	iface_  = iface;

	// initialise LTPlib
	if (!ltp_inited) {
		int rv=ltp_init();
		if (rv) {
			log_err("LTP initialisation error: %d\n",rv);
		} else {
			log_debug("LTP initialised.\n");
			ltp_inited=true;
		}
	}
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("LTP error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("LTP invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("LTP invalid local port setting of 0");
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
    log_info("LTP adding %s link %s", link->type_str(), link->nexthop());

	// initialise LTPlib
	if (!ltp_inited) {
		int rv=ltp_init();
		if (rv) {
			log_err("LTP initialisation error: %d\n",rv);
		} else {
			log_debug("LTP initialised.\n");
			ltp_inited=true;
		}
	}

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
        log_err("LTP error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }
    
    link->set_cl_info(params);
    log_debug("LTP Link init'd, local: %s:%d, remote: %s:%d",
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

    log_debug("LTP LTPConvergenceLayer::delete_link: "
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
    log_info("LTP opening contact for link *%p", link.object());
    
    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("LTP invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("LTP can't lookup hostname in next hop address '%s'",
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
        log_err("LTP error initializing contact");
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
        log_crit("LTP send_bundles called on contact *%p with no Sender!!",
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
LTPConvergenceLayer::Receiver::Receiver(LTPConvergenceLayer::Params *params)
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
	params_ = params;

	/// set the source
	str2ltpaddr((char*)intoa(params->local_addr_),&source);
	source.sock.sin_port=params->local_port_;
	// set the destination 
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

	log_debug("LTP send_bundle, sending %d bytes to %s",
			total_len,ltpaddr2str(&dest));
	
	///code below is a simple test to check ltplib api calls

	char *inbuf = (char*)buf_;
	size_t rv;
	
	/// unused value in the sendto function?
	static int flags = 0;

	sock = ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
	log_debug("LTP Socket: %d",sock);
	// need to set the LTP_SO_LINGER sockopt, (its default is false)
	// we know we can tx the data segments (since the LTPCL link is 
	// only up when that's true), but we don't know if reports can 
	// be done in time and we don't want the ltp_close to result 
	// in sending cancel segments
	int foo=1; // sockopt parameter
	rv=ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_LINGER,&foo,sizeof(foo));
	if (rv) { 
		log_err("LTP ltp_setsockopt for SO_LINGER failed.\n");
		return(-1);
	}
	///bind
	rv = ltp_bind(sock,(ltpaddr*)&source,sizeof(source));
	if (rv) { 
		log_err("LTP ltp_bind failed.\n");
		return(-1);
	}
	// set local idea of who I am
	rv=ltp_set_whoiam(&source);
	if (rv) { 
		log_err("LTP ltp_set_whoiam failed.\n");
		return(-1);
	}
	rv = ltp_sendto(sock,inbuf,total_len,flags,(ltpaddr*)&dest,sizeof(dest));
	if (rv!=total_len) {
		log_err("LTP ltp_sendto failed: %d\n",rv);
		return(-1);
	}
	ltp_close(sock);
	
	log_debug("LTP sent bundle apparently ok");
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

/// TODO: make this a parameter
#define MAXINPUTBUNDLE 10000000
    int rxbufsize = MAXINPUTBUNDLE;
    u_char buf[rxbufsize];

/// TODO: make this a parameter
#define MAXLTPLISTENERS 32

	ltpaddr listeners[MAXLTPLISTENERS];
	int nlisteners;
	int lastlisteners=-1;
    while (1) {
        if (should_stop()) {
			log_info("LTP Receiver::run done\n");
            break;
		}
		// who's listening now?
		nlisteners=MAXLTPLISTENERS;
		rv=ltp_whos_listening_now(&nlisteners,listeners);
		if (rv) { 
			log_err("LTP ltp_whos_listening_now error: %d\n",rv);
			break;
		}
		// don't want crazy logging so just when there's a change
		if (lastlisteners!=nlisteners) {
			log_info("LTP who's listening now says %d listeners (was %d)\n",nlisteners,lastlisteners);
			for (int j=0;j!=nlisteners;j++) {
				log_debug("LTP \tListener %d %s\n",j,ltpaddr2str(&listeners[j]));
			}
		}
		// if we're in "opportunistic mode"
		// check if I should change link state, depends on who's
		// listening and linkpeer;
		// note that whos_listening can return wildcard type 
		// ltpaddr's (privately formatted) to handle cases where
		// LTP has no config. ltpaddr_cmp knows how to handle 
		// that and can do wildcard matches as needed
		ContactManager *cm = BundleDaemon::instance()->contactmgr();
		oasys::ScopeLock cmlock(cm->lock(), "LTPCL::whoslistening");
		const LinkSet* links=cm->links();
		for (LinkSet::const_iterator i=links->begin();
							i != links->end(); i++) {

			// other states (e.g. OPENING) exist that we ignore
			bool linkopen=(*i)->state()==Link::OPEN;
			bool linkclosed=(
				(*i)->state()==Link::UNAVAILABLE || 
				(*i)->state()==Link::AVAILABLE );
			ltpaddr linkpeer;
			// might want to use (*i)->nexthop() instead params
			str2ltpaddr((char*)(*i)->nexthop(),&linkpeer);
			if (lastlisteners!=nlisteners) {
				log_debug("LTP linkpeer: %s\n",ltpaddr2str(&linkpeer));
				log_debug("LTP link state: %s, link cl name: %s\n",
					Link::state_to_str((*i)->state()),
					(*i)->clayer()->name());
			}
			if ( ( (*i)->clayer()->name() == (char*) "ltp" ) &&
				(*i)->type()==Link::OPPORTUNISTIC) {

				if (linkclosed) {
					// if the linkpeer is a listener then open it
					bool ispresent=false;
					for (int j=0;j!=nlisteners && !ispresent;j++) {
						if (!ltpaddr_cmp(&linkpeer,&listeners[j],sizeof(linkpeer))) {
							// mark link open!!!
        					BundleDaemon::post(new LinkStateChangeRequest((*i), Link::OPEN, ContactEvent::NO_INFO));
							ispresent=true;
							log_debug("LTP changing link %s to OPEN\n",(*i)->name());
						}
					}
				} else if (linkopen) {
					// if the linkpeer is not a listener then close it
					bool ispresent=false;
					int listenermatch=-1;
					for (int j=0;j!=nlisteners && !ispresent;j++) {
						if (!ltpaddr_cmp(&linkpeer,&listeners[j],sizeof(linkpeer))) {
							ispresent=true;
							listenermatch=j;
						}
					}
					if (!ispresent) {
						// close that link
        				BundleDaemon::post(new LinkStateChangeRequest((*i), Link::CLOSED, ContactEvent::NO_INFO));
						log_debug("LTP changing link %s to CLOSED\n",(*i)->name());
					}
				} // do nothing for other states for now

			}
		}
		cmlock.unlock();
		// don't log stuff next time 'round
		lastlisteners=nlisteners;
		// now check if something's arrived for me
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
			// need to close that socket since its now bound to that
			// sender within LTPlib (its no longer an "emptylistener")
			// TODO: have two sockets (at least) so I don't miss out on
			// something when I'm in the middle of doing this close()/open()
			// sequence
    		ltp_close(s_sock);
			s_sock=ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
			if (!s_sock) {
				return;
			}
			rv=ltp_bind(s_sock,&listener,sizeof(ltpaddr));
			if (rv) { 
				ltp_close(s_sock); 
				return;
			} 
		}
    }
    ltp_close(s_sock);
    return;
}


}//namespace


#endif

