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
/// - test receipt of >1 bundle in one LTP block (code's there now)
/// - sending >1 bundle in one LTP block
/// - add LTP configuration file support with good defaults
/// - figure out if anything leaks between LTPlib and DTN2
/// - maybe try speed up UDP packet sending in LTPlib, probably a bit slow now 


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sys/poll.h>
#include <time.h>

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

//----------------------------------------------------------------------
LTPConvergenceLayer::LTPConvergenceLayer() : IPConvergenceLayer("LTPConvergenceLayer", "ltp")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.mtu_              		= 0;
    defaults_.rg_              			= LTP_ALLRED;
    defaults_.ion_mode_              	= false;
    defaults_.nh_addr_               = INADDR_ANY;
    defaults_.nh_port_               = LTPCL_DEFAULT_PORT;

	ltp_inited=false;

}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
	a->process("mtu",&mtu_);
	a->process("rg",&rg_);
	a->process("ion_mode",&ion_mode_);
    a->process("nh_addr", oasys::InAddrPtr(&nh_addr_));
    a->process("nh_port", &nh_port_);
}

//----------------------------------------------------------------------
CLInfo*
LTPConvergenceLayer::new_link_params()
{
    return new LTPConvergenceLayer::Params(defaults_);
}

//----------------------------------------------------------------------
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
    p.addopt(new oasys::UInt16Opt("mtu", &params->mtu_));
    p.addopt(new oasys::IntOpt("rg", &params->rg_));
    p.addopt(new oasys::BoolOpt("ion_mode", &params->ion_mode_));
    p.addopt(new oasys::InAddrOpt("nh_addr", &params->nh_addr_));
    p.addopt(new oasys::UInt16Opt("nh_port", &params->nh_port_));

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

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
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

	int lmtu=link->params().mtu_;

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
    params->mtu_ = lmtu;

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
    buf->appendf("\tmtu: %d\n", params->mtu_);
    if (params->rg_ == LTP_ALLRED) {
    	buf->appendf("\tRed/Green balance: all red\n");
    } else if (params->rg_ == LTP_ALLGREEN) {
    	buf->appendf("\tRed/Green balance: all green\n");
    } else {
    	buf->appendf("\tRed/Green balance: redlen: %d", params->rg_);
	}
	buf->appendf("\tION mode: %s", params->ion_mode_ ? "true" : "false");
    buf->appendf("\tnh_addr: %s nh_port: %d\n",
                 intoa(params->nh_addr_), params->nh_port_);
}

//----------------------------------------------------------------------
bool
LTPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&defaults_, argc, argv, invalidp);
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
	lmtu = params->mtu_;
	lrg = params->rg_;
	lion_mode = params->ion_mode_;
	// set the forced next hop if need be
	str2ltpaddr((char*)intoa(params->nh_addr_),&lnh);
	lnh.sock.sin_port=params->nh_port_;

    // start our thread
}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::Receiver::set_should_stop() {
	should_stop_ = true;
}

//----------------------------------------------------------------------
bool
LTPConvergenceLayer::Receiver::should_stop() {
	return should_stop_;
}

//----------------------------------------------------------------------
void
LTPConvergenceLayer::Receiver::set_sock(int sockval) {
	s_sock = sockval;
}

//----------------------------------------------------------------------
int
LTPConvergenceLayer::Receiver::get_sock() {
	return s_sock;
}

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
	// set the forced next hop if need be
	str2ltpaddr((char*)intoa(params->nh_addr_),&lnh);
	lnh.sock.sin_port=params->nh_port_;

	lmtu=params->mtu_;
	lrg=params->rg_;
	lion_mode = params->ion_mode_;
	/// set the forced next hop if needed
	str2ltpaddr((char*)intoa(params->nh_addr_),&lnh);
	lnh.sock.sin_port=params->nh_port_;
	

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
    size_t total_len = BundleProtocol::total_length(blocks);
	
	u_char *inbuf=(u_char*)calloc (sizeof(char),total_len+1);
	if ( !inbuf) return(-1);
	
	total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               inbuf, 0, total_len,
                                               &complete);

	log_debug("LTP send_bundle, sending %d bytes to %s",
			(int) total_len,ltpaddr2str(&dest));
	
	///code below is a simple test to check ltplib api calls

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
		free(inbuf);
		return(-1);
	}
	// if the params mtu is set to other than zero then pass it on
	if (lmtu > 0 ) {
		log_debug("LTP Tx: setting LTP mtu to %d",lmtu);
		rv=ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_L2MTU,&lmtu,sizeof(lmtu));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_L2MTU failed.\n");
			free(inbuf);
			return(-1);
		}
	} else {
		log_debug("LTP Tx: not setting LTP mtu 'cause its %d",lmtu);
	}
	// setup red/green
	if (lrg!=LTP_ALLRED) {
		rv=ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_RED,&lrg,sizeof(lrg));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_RED failed (%d).\n",lrg);
			free(inbuf);
			return(-1);
		}
		log_info("LTP Tx: set SO_RED to %d (-1=allred,0=allgreen,other=number of red bytes\n",lrg);
	} else {
		log_debug("LTP Tx: not setting SO_RED 'cause its all red (%d)",lrg);
	}

	// setup ion mode if needed
	if (lion_mode) {
		bool tmp=true;
		rv=ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_ION,&tmp,sizeof(tmp));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_ION failed");
			free(inbuf);
			return(-1);
		}
		log_debug("LTP Tx: setting SO_ION 'cause it is ION_MODE");
	} else {
		log_debug("LTP Tx: not setting SO_ION 'cause its not ION_MODE");
	}

	// force a next hop if needed
    in_addr_t tnh_addr_               = INADDR_ANY;
    u_int16_t tnh_port_               = LTPCL_DEFAULT_PORT;
	ltpaddr taddr;
	str2ltpaddr((char*)intoa(tnh_addr_),&taddr);
	taddr.sock.sin_port=tnh_port_;
	if (ltpaddr_cmp(&lnh,&taddr,sizeof(taddr))) {
		// do stuff
		rv=ltp_setsockopt(sock,SOL_SOCKET,LTP_SO_FORCE,&lnh,sizeof(lnh));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_FORCE failed");
			free(inbuf);
			return(-1);
		}
		log_debug("LTP Tx: setting SO_FORCE 'cause I was asked");
	} else {
		log_debug("LTP Tx: not setting SO_FORCE 'cause I wasn't asked");
	}

	///bind
	rv = ltp_bind(sock,(ltpaddr*)&source,sizeof(source));
	if (rv) { 
		log_err("LTP ltp_bind failed.\n");
		free(inbuf);
		return(-1);
	}
	// set local idea of who I am
	rv=ltp_set_whoiam(&source);
	if (rv) { 
		log_err("LTP ltp_set_whoiam failed.\n");
		free(inbuf);
		return(-1);
	}
	rv = ltp_sendto(sock,inbuf,total_len,flags,(ltpaddr*)&dest,sizeof(dest));
	if (rv!=total_len) {
		log_err("LTP ltp_sendto failed: %d\n",(int)rv);
		free(inbuf);
		return(-1);
	}
	ltp_close(sock);
	free(inbuf);
	log_debug("LTP sent bundle apparently ok");
	return(total_len);
}


//----------------------------------------------------------------------
void
LTPConvergenceLayer::Receiver::run()
{

    int ret;
	int rv;
	// set local idea of who I am
	rv=ltp_set_whoiam(&listener);
	if (rv) { 
		log_err("LTP ltp_set_whoiam failed.\n");
		return;
	}

    int s_sock=ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
    if (!s_sock) {
    	return;
	}
	// if the params mtu is set to other than zero then pass it on
	if (lmtu > 0 ) {
		log_debug("LTP Rx: setting LTP mtu to %d",lmtu);
		rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_L2MTU,&lmtu,sizeof(lmtu));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_L2MTU failed.\n");
			return;
		}
	} else {
		log_debug("LTP Rx: not setting LTP mtu 'cause its %d",lmtu);
	}

	// setup ion mode if needed
	if (lion_mode) {
		rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_ION,&lion_mode,sizeof(lion_mode));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_ION failed");
			return;
		}
		log_debug("LTP Tx: setting SO_ION 'cause it is ION_MODE");
	} else {
		log_debug("LTP Tx: not setting SO_ION 'cause its not ION_MODE");
	}

	// force a next hop if needed
    in_addr_t tnh_addr_               = INADDR_ANY;
    u_int16_t tnh_port_               = LTPCL_DEFAULT_PORT;
	ltpaddr taddr;
	str2ltpaddr((char*)intoa(tnh_addr_),&taddr);
	taddr.sock.sin_port=tnh_port_;
	if (ltpaddr_cmp(&lnh,&taddr,sizeof(taddr))) {
		// do stuff
		rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_FORCE,&lnh,sizeof(lnh));
		if (rv) {
			log_err("LTP ltp_setsockopt for SO_FORCE failed");
			return;
		}
		log_debug("LTP Rx: setting SO_FORCE 'cause I was asked");
	} else {
		log_debug("LTP Tx: not setting SO_FORCE 'cause I wasn't asked");
		log_debug("LTP nh: %s",ltpaddr2str(&lnh));
		log_debug("LTP any: %s",ltpaddr2str(&taddr));
	}

	rv=ltp_bind(s_sock,&listener,sizeof(ltpaddr));
	if (rv) { 
		ltp_close(s_sock); 
    	return;
	} 


/// TODO: make this a parameter
#define MAXLTPLISTENERS 32

	ltpaddr listeners[MAXLTPLISTENERS];
	int nlisteners;
	int nlastlisteners=-1;
	ltpaddr lastlisteners[MAXLTPLISTENERS];

#define START_INPUTBUNDLE 0x10000
    size_t rxbufsize = START_INPUTBUNDLE;
	bool buf2free=true;
    u_char *buf;
	buf=(u_char*) calloc(sizeof(u_char),START_INPUTBUNDLE);
	if (!buf) {
		log_err("LTP Receiver::calloc failed\n");
		ltp_close(s_sock);
		return;
	}

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
		bool difflisteners=false;
		if (nlastlisteners!=nlisteners) {
			difflisteners=true;
		} else {
			// check in case the set has changed (order independent)
			// inefficient search, but numbers will be small so ok
			for (int j=0;!difflisteners && j!=nlisteners;j++) {
				bool looking=true;
				for (int k=0;looking && k!=nlisteners;k++) {
					if (!ltpaddr_cmp(&lastlisteners[j],&listeners[k],sizeof(listeners[k]))) {
						looking=false;
					}
				}
				if (looking) difflisteners=true;
			}
		}
		if (difflisteners) {
			log_info("LTP who's listening now says %d listeners (was %d)\n",nlisteners,nlastlisteners);
			for (int j=0;j!=nlisteners;j++) {
				log_debug("LTP \tListener %d %s\n",j,ltpaddr2str(&listeners[j]));
			}
			memcpy(lastlisteners,listeners,sizeof(listeners)); 
			nlastlisteners=nlisteners;
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
							i != links->end(); ++i) {

			// other states (e.g. OPENING) exist that we ignore
			bool linkopen=((*i)->state()==Link::OPEN);
			bool linkclosed=(
				(*i)->state()==Link::UNAVAILABLE || 
				(*i)->state()==Link::AVAILABLE );
			ltpaddr linkpeer;
			// might want to use (*i)->nexthop() instead params
			str2ltpaddr((char*)(*i)->nexthop(),&linkpeer);
			if (difflisteners) {
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
		// now check if something's arrived for me
		int flags=0;
		ltpaddr from;
		ltpaddr_len fromlen;
		ret=ltp_recvfrom(s_sock,buf,rxbufsize,flags,(ltpaddr*)&from,(ltpaddr_len*)&fromlen);
		if (ret==0) {
			struct timespec	ts,ts1;
			memset(&ts,0,sizeof(ts));
			memset(&ts1,0,sizeof(ts));
			ts.tv_nsec=1000*1000*20;  // 20ms
			nanosleep(&ts,&ts1);
		} else if (ret < 0) {
            if (errno == EINTR) {
				struct timespec	ts,ts1;
				memset(&ts,0,sizeof(ts));
				memset(&ts1,0,sizeof(ts));
				ts.tv_nsec=1000*1000*20;  // 20ms
				nanosleep(&ts,&ts1);
                continue;
          	}
			if (ret == -1 ) { // special case - close the socket and get another
				struct timespec	ts,ts1;
				memset(&ts,0,sizeof(ts));
				memset(&ts1,0,sizeof(ts));
				ts.tv_nsec=1000*1000*20;  // 20ms
				nanosleep(&ts,&ts1);
				log_info("LTP Rx: closing/opening socket - returned from ltp_recvfrom()");
				ltp_close(s_sock);
				s_sock=ltp_socket(AF_LTP,SOCK_LTP_SESSION,0);
				if (!s_sock) {
					return;
				}

				// if the params mtu is set to other than zero then pass it on
				if (lmtu > 0 ) {
					log_debug("LTP Rx: setting LTP mtu to %d",lmtu);
					rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_L2MTU,&lmtu,sizeof(lmtu));
					if (rv) {
						log_err("LTP ltp_setsockopt for SO_L2MTU failed.\n");
						return;
					}
				} else {
					log_debug("LTP Rx: not setting LTP mtu 'cause its %d",lmtu);
				}
				// setup ion mode if needed
				if (lion_mode) {
					rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_ION,&lion_mode,sizeof(lion_mode));
					if (rv) {
						log_err("LTP ltp_setsockopt for SO_ION failed");
						return;
					}
					log_debug("LTP Tx: setting SO_ION 'cause it is ION_MODE");
				} else {
					log_debug("LTP Tx: not setting SO_ION 'cause its not ION_MODE");
				}

				rv=ltp_bind(s_sock,&listener,sizeof(ltpaddr));
				if (rv) { 
					ltp_close(s_sock); 
					return;
				} 
				continue;
			}
			size_t nbsz=(-1*ret);
			if (ret < -1 &&  nbsz > rxbufsize) {
				// try allocate more and go again
				buf2free=false;
				free(buf);
				buf=(u_char*) calloc(sizeof(u_char),nbsz+100);
				if (!buf) {
					log_err("LTP Receiver::calloc failed when biggering\n");
					break;
				}
				buf2free=true;
				rxbufsize=nbsz+100;
				continue;
			} else {
				break;  // dunno how we'd get here! should't happen
			}
            break;
        } else if (ret>0) {
			log_info("LTP ltp_recvfrom returned %d byte block\n",ret);
    		// allow >1 bundle on receipt
			// get it off the stack - gotta hope the Bundle code
			// properly manages the memory - TODO - check that out
			// I might need to free it
    		// the payload should contain a full bundle
			// I can loop around the consume calls until all bytes are used up (I hope;-)
			size_t remaining=ret;
			unsigned char *bufp=buf;
			bool problem=false;
			while (!problem && remaining > 0 ) {
    			Bundle* bundle = new Bundle();
    			bool complete = false;
    			int tmp = BundleProtocol::consume(bundle, bufp, remaining, &complete);
    			if (tmp < 0 || !complete) {
        			delete bundle;
					problem=true;
    			} else {
					log_debug("LTP sending a bundle up %d bytes remaining, %d consumed block\n",
							(int)(remaining-tmp),(int)tmp);
					BundleDaemon::post(new BundleReceivedEvent(bundle, EVENTSRC_PEER, ret, EndpointID::NULL_EID()));
					bufp+=tmp;
					remaining=(remaining-tmp);
				}
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

			// if the params mtu is set to other than zero then pass it on
			if (lmtu > 0 ) {
				log_debug("LTP Rx: setting LTP mtu to %d",lmtu);
				rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_L2MTU,&lmtu,sizeof(lmtu));
				if (rv) {
					log_err("LTP ltp_setsockopt for SO_L2MTU failed.\n");
					return;
				}
			} else {
				log_debug("LTP Rx: not setting LTP mtu 'cause its %d",lmtu);
			}
			// setup ion mode if needed
			if (lion_mode) {
				rv=ltp_setsockopt(s_sock,SOL_SOCKET,LTP_SO_ION,&lion_mode,sizeof(lion_mode));
				if (rv) {
					log_err("LTP ltp_setsockopt for SO_ION failed");
					return;
				}
				log_debug("LTP Tx: setting SO_ION 'cause it is ION_MODE");
			} else {
				log_debug("LTP Tx: not setting SO_ION 'cause its not ION_MODE");
			}

			rv=ltp_bind(s_sock,&listener,sizeof(ltpaddr));
			if (rv) { 
				ltp_close(s_sock); 
				return;
			} 
		}
    }
    ltp_close(s_sock);
	if (buf2free) free(buf);
    return;
}


}//namespace dtn


#endif  // LTP_ENABLED

