/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products rsulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef LTPUDP_ENABLED

#include <iostream>
#include <sys/timeb.h>
#include <climits>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/HexDumpBuffer.h>
#include <oasys/io/IO.h>
#include <naming/EndpointID.h>
#include <naming/IPNScheme.h>

#include "LTPUDPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/SDNV.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "security/KeyDB.h"

namespace dtn {

struct LTPUDPConvergenceLayer::Params LTPUDPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = (int) bucket_type_;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("inact_intvl", &inactivity_intvl_);
    a->process("retran_intvl", &retran_intvl_);
    a->process("retran_retries", &retran_retries_);
    a->process("engine_id", &engine_id_);
    a->process("wait_till_sent", &wait_till_sent_);
    a->process("hexdump", &hexdump_);
    a->process("comm_aos", &comm_aos_);
    a->process("ion_comp",&ion_comp_);
    a->process("max_sessions", &max_sessions_);
    a->process("seg_size", &seg_size_);
    a->process("agg_size", &agg_size_);
    a->process("agg_time", &agg_time_);
    a->process("sendbuf", &sendbuf_);
    a->process("recvbuf", &recvbuf_);
    // clear_stats_ not needed here
    a->process("icipher_suite", &inbound_cipher_suite_);
    a->process("icipher_keyid", &inbound_cipher_key_id_);
    a->process("icipher_engine",  &inbound_cipher_engine_);
    a->process("ocipher_suite", &outbound_cipher_suite_);
    a->process("ocipher_keyid", &outbound_cipher_key_id_);
    a->process("ocipher_engine",  &outbound_cipher_engine_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPConvergenceLayer()
    : IPConvergenceLayer("LTPUDPConvergenceLayer", "ltpudp"),
      Thread("LTPUDPConvergenceLayer")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPUDPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = LTPUDPCL_DEFAULT_PORT;
    defaults_.bucket_type_              = oasys::RateLimitedSocket::STANDARD;
    defaults_.rate_                     = 0;         // unlimited
    defaults_.bucket_depth_             = 65535 * 8; // default for standard bucket
    defaults_.inactivity_intvl_         = 30;
    defaults_.retran_intvl_             = 3;
    defaults_.retran_retries_           = 3;
    defaults_.engine_id_                = 0;
    defaults_.wait_till_sent_           = true;
    defaults_.hexdump_                  = false;
    defaults_.comm_aos_                 = true;
    defaults_.ion_comp_                 = true;
    defaults_.max_sessions_             = 100;
    defaults_.seg_size_                 = 1400;
    defaults_.agg_size_                 = 100000;
    defaults_.agg_time_                 = 1000;  // milliseconds (1 sec)
    defaults_.recvbuf_                  = 0;
    defaults_.sendbuf_                  = 0;
    defaults_.clear_stats_              = false;
    defaults_.inbound_cipher_suite_     = -1;
    defaults_.inbound_cipher_key_id_    = -1;
    defaults_.inbound_cipher_engine_    = "";
    defaults_.outbound_cipher_suite_    = -1;
    defaults_.outbound_cipher_key_id_   = -1;
    defaults_.outbound_cipher_engine_   = "";

    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;
    sender_                             = NULL;
    receiver_                           = NULL;

    timer_processor_                    = NULL;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::~LTPUDPConvergenceLayer()
{
    if (timer_processor_ != NULL) {
        timer_processor_->set_should_stop();
    }
    if (receiver_ != NULL) {
        receiver_->set_should_stop();
    }
    if (sender_   != NULL) {
        sender_->set_should_stop();
    }
    sleep(1);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::run()
{ 
    aos_counter_ = 0;
    log_debug("CounterThread run invoked");
    while (1) {
        if (should_stop())
            break;
        sleep(1);
        if(session_params_.comm_aos_)aos_counter_++;
    }        
}

//----------------------------------------------------------------------
CLInfo*
LTPUDPConvergenceLayer::new_link_params()
{
    return new LTPUDPConvergenceLayer::Params(LTPUDPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&LTPUDPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    char icipher_from_engine[50];
    char ocipher_from_engine[50];
    oasys::OptParser p;
    int temp = 0;

    string ocipher_engine_str = "";
    string icipher_engine_str = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::IntOpt("bucket_type", &temp));
    p.addopt(new oasys::UIntOpt("rate", &params->rate_));
    p.addopt(new oasys::UIntOpt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::IntOpt("inact_intvl", &params->inactivity_intvl_));
    p.addopt(new oasys::IntOpt("retran_intvl", &params->retran_intvl_));
    p.addopt(new oasys::IntOpt("retran_retries", &params->retran_retries_));
    p.addopt(new oasys::UInt64Opt("engine_id", &params->engine_id_));
    p.addopt(new oasys::BoolOpt("wait_till_sent",&params->wait_till_sent_));
    p.addopt(new oasys::BoolOpt("hexdump",&params->hexdump_));
    p.addopt(new oasys::BoolOpt("comm_aos",&params->comm_aos_));
    p.addopt(new oasys::BoolOpt("ion_comp",&params->ion_comp_));
    p.addopt(new oasys::UIntOpt("max_sessions", &params->max_sessions_));
    p.addopt(new oasys::IntOpt("seg_size", &params->seg_size_));
    p.addopt(new oasys::IntOpt("agg_size", &params->agg_size_));
    p.addopt(new oasys::IntOpt("agg_time", &params->agg_time_));
    p.addopt(new oasys::UIntOpt("recvbuf", &params->recvbuf_));
    p.addopt(new oasys::UIntOpt("sendbuf", &params->sendbuf_));
    p.addopt(new oasys::BoolOpt("clear_stats", &params->clear_stats_));

    p.addopt(new oasys::IntOpt("ocipher_suite", &params->outbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("ocipher_keyid", &params->outbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("ocipher_engine", &ocipher_engine_str));
    p.addopt(new oasys::IntOpt("icipher_suite", &params->inbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("icipher_keyid", &params->inbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("icipher_engine", &icipher_engine_str));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }


    params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;

    if(icipher_engine_str.length() > 0) {
        params->inbound_cipher_engine_.assign(icipher_engine_str);
    } else {
        sprintf(icipher_from_engine, "%"PRIu64, params->engine_id_);
        params->inbound_cipher_engine_ = icipher_from_engine;
    } 
    if(ocipher_engine_str.length() > 0) {
        params->outbound_cipher_engine_.assign(ocipher_engine_str);
    } else {
        sprintf(ocipher_from_engine, "%"PRIu64, params->engine_id_);
        params->outbound_cipher_engine_ = ocipher_from_engine;
    } 

    if(params->outbound_cipher_suite_ == 0 || params->outbound_cipher_suite_ == 1 ) {
         if(params->outbound_cipher_key_id_ == -1)return false;
    } 
    if(params->inbound_cipher_suite_ == 0 || params->inbound_cipher_suite_ == 1 ) {
         if(params->inbound_cipher_key_id_ == -1)return false;
    } 

    log_debug("params ocipher_suite %d", params->outbound_cipher_suite_);
    log_debug("params icipher_suite %d", params->inbound_cipher_suite_);
    return true;
};

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPUDPConvergenceLayer::defaults_;
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

    session_params_ = params;
 
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params,this);
    receiver->logpathf("%s/iface/receiver/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
   
    log_debug(" Starting receiver"); 
    // start the thread which automatically listens for data
    receiver->start();
    
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt_from_io();
    
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;
    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params * params = &((Receiver*)iface->cl_info())->cla_params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("\tinactivity_intvl: %d", params->inactivity_intvl_);
    buf->appendf("\tretran_intvl: %d", params->retran_intvl_);
    buf->appendf("\tretran_retries: %d", params->retran_retries_);

    buf->appendf("Sender Sessions: size() (first 5): ");
    if (NULL != sender_) 
        sender_->Dump_Sessions(buf);
    else
        buf->appendf("\nsender_ not initialized");

    buf->appendf("\nReceiver Sessions: size() (first 5): ");
    if(NULL != receiver_)
        receiver_->Dump_Sessions(buf);
    else
        buf->appendf("\nreceiver not initialized");
    
    buf->appendf("\nStatistics:\n");
    buf->appendf("             Sessions     Sessns   Data Segments    Report Segments  RptAck  Cancel By Sendr  Cancel By Recvr  CanAck  Bundles  Bundles  CanRcvr\n");
    buf->appendf("          Total /  Max    DS R-X   Total  / ReXmit   Total / ReXmit  Total    Total / ReXmit   Total / ReXmit  Total   Success  Failure  But Got\n");
    buf->appendf("         ---------------  ------  ----------------  ---------------  ------  ---------------  ---------------  ------  -------  -------  -------\n");
    if (NULL != sender_) sender_->Dump_Statistics(buf);
    if (NULL != receiver_) receiver_->Dump_Statistics(buf);

    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected");
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

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

    session_params_ = *params;

    this->start();  // start the TimerCounter thread...

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(&session_params_);
    delete params;

    link_ref_ = link;

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPUDPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[]) 
{
    (void) link;

    const char* invalid;
    if (! parse_params(&session_params_, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }

    // inform the sender of the reconfigure
    if (NULL != sender_) {
        sender_->reconfigure();
    }

    if (session_params_.clear_stats_) {
        if (NULL != sender_) sender_->clear_statistics();
        if (NULL != receiver_) receiver_->clear_statistics();
        session_params_.clear_stats_ = false;
    }

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                         AttributeVector& params)
{
    (void) link;

    AttributeVector::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
        if (iter->name().compare("comm_aos") == 0) { 
            session_params_.comm_aos_ = iter->bool_val();
            log_debug("reconfigure_link - new comm_aos = %s", session_params_.comm_aos_ ? "true": "false");
        } else if (iter->name().compare("rate") == 0) { 
            session_params_.rate_ = iter->u_int_val();
            log_debug("reconfigure_link - new rate = %"PRIu32, session_params_.rate_);
        } else if (iter->name().compare("bucket_depth") == 0) { 
            session_params_.bucket_depth_ = iter->u_int_val();
            log_debug("reconfigure_link - new bucket depth = %"PRIu32, session_params_.bucket_depth_);
        } else if (iter->name().compare("bucket_type") == 0) { 
            session_params_.bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) iter->int_val();
            log_debug("reconfigure_link - new bucket type = %d", (int) session_params_.bucket_type_);
        } else if (iter->name().compare("clear_stats") == 0) { 
            session_params_.clear_stats_ = iter->bool_val();
        }
        // any others to change on the fly through the External Router interface?

        else {
            log_crit("reconfigure_link - unknown parameter: %s", iter->name().c_str());
        }
    }    

    // inform the sender of the reconfigure
    if (NULL != sender_) {
        sender_->reconfigure();
    }

    if (session_params_.clear_stats_) {
        if (NULL != sender_) sender_->clear_statistics();
        if (NULL != receiver_) receiver_->clear_statistics();
        session_params_.clear_stats_ = false;
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    ASSERT(params == &session_params_);

    buf->appendf("local_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("remote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);

    if (NULL == sender_ ) {
        buf->appendf("transmit rate: %"PRIu32"\n", params->rate_);
        if (0 == params->bucket_type_) 
            buf->appendf("bucket_type: TokenBucket\n");
        else
            buf->appendf("bucket_type: TokenBucketLeaky\n");
        buf->appendf("bucket_depth: %u\n", params->bucket_depth_);
        buf->appendf("wait_till_sent: %d\n",(int) params->wait_till_sent_);
    } else {
        sender_->dump_link(buf);
    }

    buf->appendf("inactivity_intvl: %d\n", params->inactivity_intvl_);
    buf->appendf("retran_intvl: %d\n", params->retran_intvl_);
    buf->appendf("retran_retries: %d\n", params->retran_retries_);
    buf->appendf("agg_size: %d (bytes)\n",params->agg_size_);
    buf->appendf("agg_time: %d (milliseconds)\n",params->agg_time_);
    buf->appendf("seg_size: %d (bytes)\n",params->seg_size_);
    buf->appendf("Max Sessions: %u\n",params->max_sessions_);
    buf->appendf("Comm_AOS: %d\n",(int) params->comm_aos_);
    buf->appendf("ION_Compatible: %d\n",(int) params->ion_comp_);
    buf->appendf("hexdump: %d\n",(int) params->hexdump_);

    if (params->outbound_cipher_suite_ != -1) {
        buf->appendf("outbound_cipher_suite: %d\n",(int) params->outbound_cipher_suite_);
        if (params->outbound_cipher_suite_ != 255) {
            buf->appendf("outbound_cipher_key_id: %d\n",(int) params->outbound_cipher_key_id_);
            buf->appendf("outbound_cipher_engine_: %s\n", params->outbound_cipher_engine_.c_str());
        }
    }

    if (params->inbound_cipher_suite_ != -1) {
        buf->appendf("inbound_cipher_suite: %d\n",(int) params->inbound_cipher_suite_);
        if (params->inbound_cipher_suite_ != 255) {
            buf->appendf("inbound_cipher_key_id_: %d\n",(int) params->inbound_cipher_key_id_);
            buf->appendf("inbound_cipher_engine_: %s\n", params->inbound_cipher_engine_.c_str());
        }
    }

    buf->appendf("Sender Sessions: size() (first 5): ");
    if (NULL != sender_) 
        sender_->Dump_Sessions(buf);
    else
        buf->appendf("\nsender_ not initialized");

    buf->appendf("\nReceiver Sessions: size() (first 5): ");
    if(NULL != receiver_) {
        receiver_->Dump_Sessions(buf);
        buf->appendf("\nReceiver: 10M Buffers: %d  Raw data bytes queued: %zu  Bundle extraction bytes queued: %zu\n", 
                     receiver_->buffers_allocated(), receiver_->process_data_bytes_queued(), 
                     receiver_->process_bundles_bytes_queued());
    } else {
        buf->appendf("\nreceiver not initialized");
    }

    buf->appendf("\nStatistics:\n");
    buf->appendf("             Sessions     Sessns   Data Segments    Report Segments  RptAck  Cancel By Sendr  Cancel By Recvr  CanAck  Bundles  Bundles  CanRcvr\n");
    buf->appendf("          Total /  Max    DS R-X   Total  / ReXmit   Total / ReXmit  Total    Total / ReXmit   Total / ReXmit  Total   Success  Failure  But Got\n");
    buf->appendf("         ---------------  ------  ----------------  ---------------  ------  ---------------  ---------------  ------  -------  -------  -------\n");
    if (NULL != sender_) sender_->Dump_Statistics(buf);
    if (NULL != receiver_) receiver_->Dump_Statistics(buf);

    buf->appendf("\n");

    if(receiver_ != 0)
        receiver_->Dump_Bundle_Processor(buf);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    log_debug("LTPUDPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());


    // Initialize and start the Sender thread
    
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
        port = LTPUDPCL_DEFAULT_PORT;
    }

    next_hop_addr_  = addr;
    next_hop_port_  = port;
    next_hop_flags_ = MSG_DONTWAIT;

    Params* params = (Params*)link->cl_info();
   
    session_params_ = *params;
 
    // create a new sender structure
    Sender* sender = new Sender(link->contact(),this);
  
    if (!sender->init(&session_params_, addr, port)) {

        log_err("error initializing contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        delete sender;
        return false;
    }
        
    contact->set_cl_info(sender);
    log_debug("LTPUDPCL.. Adding ContactUpEvent ");
    BundleDaemon::post(new ContactUpEvent(link->contact()));

    sender_ = sender;    
    sender->start();


    // Initialize and start the Receive rthread

    // check that the local interface / port are valid
    if (session_params_.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0"); 
        return false;
    }

    if (session_params_.local_port_ == 0) { 
        log_err("invalid local port setting of 0"); 
        return false;
    }

    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&session_params_,this);
    receiver->logpathf("%s/link/receiver/%s", logpath_, link->name());
    
    if (receiver->bind(session_params_.local_addr_, session_params_.local_port_) != 0) { 
        log_debug("error binding to %s:%d: %s",
                 intoa(session_params_.local_addr_), session_params_.local_port_,
                 strerror(errno));
        return false; // error log already emitted 
    } else {
        log_debug("socket bind occured %s:%d",
                 intoa(session_params_.local_addr_), session_params_.local_port_);
    }

    // start the thread which automatically listens for data
    log_debug("Starting receiver thread");

    receiver_ = receiver; 
    receiver->start();


    // Initialize and start the thread that processes timer events
    timer_processor_ = new TimerProcessor();

    return true;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();

    log_info("close_contact *%p", contact.object());

    if (timer_processor_) {
        timer_processor_->set_should_stop();
        timer_processor_ = NULL;
    }

    if (sender) {
        delete sender;
        contact->set_cl_info(NULL);
    }

    if (receiver_) {
        receiver_->set_should_stop();
    }
    

    
    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;
    (void) bundle;
    log_debug("bundle_queued called bundle ID(%"PRIbid")",bundle->bundleid());
    return;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::pop_queued_bundle(const LinkRef& link)
{
    bool result = false;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    if(!session_params_.comm_aos_)return result;
     
    const ContactRef& contact = link->contact();

    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return result;
    }

    BundleRef lbundle = link->queue()->front();
    if (lbundle != NULL) {
        result = true;
        sender->send_bundle(lbundle, next_hop_addr_, next_hop_port_);
    }

    return result;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Process_Sender_Segment(LTPUDPSegment * seg)
{
    sender_->Process_Segment(seg);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Send(LTPUDPSegment * send_segment,int parent_type)
{
    return sender_->Send(send_segment,parent_type);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type)
{
    return sender_->Send_Low_Priority(send_segment, parent_type);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::ProcessSessionEvent(MySendObject * event)
{
    log_debug("ProcessSessionEvent: sess:%s seg;%s seg_typ:%s",
              event->session_.c_str(),
              event->segment_.c_str(),
              LTPUDPSegment::Get_Segment_Type(event->segment_type_));
    if (event->parent_type_     == PARENT_RECEIVER)
        receiver_->ProcessReceiverSessionEvent(event);
    else if (event->parent_type_ == PARENT_SENDER)
        sender_->ProcessSenderSessionEvent(event);
    else
        log_crit("Invalid Parent type from MySendObject");
}

//----------------------------------------------------------------------
u_int32_t 
LTPUDPConvergenceLayer::Inactivity_Interval()
{
     return session_params_.inactivity_intvl_;
}

//----------------------------------------------------------------------
int 
LTPUDPConvergenceLayer::Retran_Interval()
{
     return session_params_.retran_intvl_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_Session_Receiver(string session_key, string segment_key, int segment_type)
{
    oasys::ScopeLock l(receiver_->session_lock(), __FUNCTION__);
    receiver_->cleanup_receiver_session(session_key, segment_key, segment_type);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_RS_Receiver(string session_key, u_int64_t rs_key, int segment_type)
{
    oasys::ScopeLock l(receiver_->session_lock(), __FUNCTION__);
    char temp[24];
    snprintf(temp, sizeof(temp), "%20.20"PRIu64, rs_key);
    string str_key = temp;
    receiver_->cleanup_RS(session_key, str_key, segment_type);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_Session_Sender(string session_key, string segment_key, int segment_type)
{
    oasys::ScopeLock l(sender_->session_lock(), __FUNCTION__);
    sender_->cleanup_sender_session(session_key, segment_key, segment_type);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Post_Timer_To_Process(oasys::Timer* event)
{
    if (timer_processor_) {
        timer_processor_->post(event);
    } else {
        delete event;
    }
}

//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Suite()
{
    return cla_params_.outbound_cipher_suite_;
}
//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Key_Id()
{
    return cla_params_.outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
string LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Engine()
{
    return cla_params_.outbound_cipher_engine_;
}
//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::Receiver(LTPUDPConvergenceLayer::Params* params, LTPUDPCallbackIF * parent)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/ltpudp/receiver")),
      UDPClient("/dtn/cl/ltpudp/receiver"),
      Thread("LTPUDPConvergenceLayer::Receiver", Thread::DELETE_ON_EXIT)
{
    logfd_         = false;
    cla_params_    = *params;
    parent_        = parent;

    memset(&stats_, 0, sizeof(stats_));


    // bump up the receive buffer size
    params_.recv_bufsize_ = cla_params_.recvbuf_;

    bundle_processor_ = new RecvBundleProcessor(this, parent->link_ref());
    bundle_processor_->start();

    data_process_offload_ = new RecvDataProcessOffload(this);
    data_process_offload_->start();

    blank_session_    = new LTPUDPSession(0UL, 0UL, parent_,  PARENT_RECEIVER);
    blank_session_->Set_Outbound_Cipher_Suite(cla_params_.outbound_cipher_suite_);
    blank_session_->Set_Outbound_Cipher_Key_Id(cla_params_.outbound_cipher_key_id_);
    blank_session_->Set_Outbound_Cipher_Engine(cla_params_.outbound_cipher_engine_);

    // create an initial pool of DataProcObejcts that may grow as needed
    buffers_allocated_ = 0; 
    for (int ix=0; ix<100; ++ix) {
        DataProcObject* obj = new DataProcObject();
        dpo_pool_.push_back(obj);
    }
    // force creating first buffer
    get_recv_buffer();

}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::~Receiver()
{
    bundle_processor_->set_should_stop();
    data_process_offload_->set_should_stop();
    sleep(1);

    DataProcObject* obj;
    while (!dpo_pool_.empty()) {
        obj = dpo_pool_.front();
        dpo_pool_.pop_front();
        delete obj;
    }

    BufferObject* bo;
    while (!buffer_pool_.empty()) {
        bo = buffer_pool_.front();
        buffer_pool_.pop_front();
        free(bo->buffer_);
        delete bo;
    }

    while (!used_buffer_pool_.empty()) {
        bo = used_buffer_pool_.front();
        used_buffer_pool_.pop_front();
        free(bo->buffer_);
        delete bo;
    }

    delete blank_session_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Sessions(oasys::StringBuffer* buf)
{
    DS_MAP::iterator segment_iter;
    SESS_MAP::iterator session_iter;
    int count = 0;
    int num_segs = 0;
    int max_segs = 0;
    int last_stop_byte = 0;
    
    buf->appendf("%d\n",(int) ltpudp_rcvr_sessions_.size());

    for(session_iter =  ltpudp_rcvr_sessions_.begin(); 
        session_iter != ltpudp_rcvr_sessions_.end(); 
        session_iter ++)
    {
        LTPUDPSession * session = session_iter->second;

        buf->appendf("    Session: %s   %s\n",session_iter->first.c_str(), session->Get_Session_State());
        if (session->Red_Segments().size() > 0) {
            num_segs = 0;
            last_stop_byte = 0;
            max_segs = session->Red_Segments().size() - 1;
            buf->appendf("        Red Segments: %d  (first 5, gaps & last)\n",(int) session->Red_Segments().size());
            if((int) session->Inbound_Cipher_Suite() != -1)
                buf->appendf("             Inbound_Cipher_Suite: %d\n",session->Inbound_Cipher_Suite());
            for(segment_iter =  session->Red_Segments().begin(); 
                segment_iter != session->Red_Segments().end(); 
                segment_iter ++)
            {
                LTPUDPDataSegment * segment = segment_iter->second;
                if (segment->Start_Byte() != last_stop_byte) {
                    buf->appendf("             gap: %d - %d\n",
                                 last_stop_byte, segment->Start_Byte());
                }
                if (num_segs < 5 || num_segs == max_segs) {
                    if (num_segs > 5) {
                        buf->appendf("             ...\n");
                    }
                    buf->appendf("             key[%d]: %s\n", 
                                 num_segs, segment->Key().c_str()); 
                }
                last_stop_byte = segment->Stop_Byte() + 1;
                ++num_segs;
            }
        } else {
            buf->appendf("        Red Segments: 0\n");
        }
        if (session->Green_Segments().size() > 0) {
            num_segs = 0;
            last_stop_byte = 0;
            max_segs = session->Green_Segments().size() - 1;
            buf->appendf("      Green Segments: %d  (first 5, gaps & last)\n",(int) session->Green_Segments().size());
            if((int) session->Inbound_Cipher_Suite() != -1)
                buf->appendf("             Inbound_Cipher_Suite: %d\n",session->Inbound_Cipher_Suite());
            for(segment_iter =  session->Green_Segments().begin(); 
                segment_iter != session->Green_Segments().end(); 
                segment_iter ++)
            {
                LTPUDPDataSegment * segment = segment_iter->second;
                if (segment->Start_Byte() != last_stop_byte) {
                    buf->appendf("             gap: %d - %d\n",
                                 last_stop_byte, segment->Start_Byte());
                }
                if (num_segs < 5 || num_segs == max_segs) {
                    if (num_segs > 5) {
                        buf->appendf("             ...\n");
                    }
                    buf->appendf("             key[%d]: %s\n", 
                                 num_segs, segment->Key().c_str()); 
                }
                last_stop_byte = segment->Stop_Byte() + 1;
                ++num_segs;
            }
        } else {
            buf->appendf("      Green Segments: 0\n");
           
        }
        count++;
        if(count >= 5)break;
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Statistics(oasys::StringBuffer* buf)
{
//    buf->appendf("\nReceiver stats: DS resends- sessions: %"PRIu64" segments: %"PRIu64
//                 "   Max sessions: %"PRIu64"\n",
//                 stats_.ds_session_resends_, stats_.ds_segment_resends_, stats_.max_sessions_);
//    buf->appendf("                RS resends: %"PRIu64" CS by Sender: %"PRIu64
//                 " CS by Receiver: %"PRIu64" (resends: %"PRIu64")\n",
//                 stats_.rs_session_resends_, stats_.cs_session_cancel_by_sender_, 
//                 stats_.cs_session_cancel_by_receiver_, stats_.cs_session_resends_);
//    buf->appendf("        Totals:  sessions: %"PRIu64" rcvd: DS(chk): %"PRIu64"(%"PRIu64
//                 ") RA: %"PRIu64" CA: %"PRIu64" CSbyS: %"PRIu64"\n",
//                 stats_.total_sessions_, stats_.total_rcv_ds_, stats_.total_rcv_ds_checkpoints_,
//                 stats_.total_rcv_ra_, stats_.total_rcv_ca_, stats_.total_rcv_css_);
//    buf->appendf("                 sent: RS: %"PRIu64" CSbyR: %"PRIu64" CA: %"PRIu64"\n",
//                 stats_.total_snt_rs_, stats_.total_snt_csr_, stats_.total_snt_ca_);


//             Sessions     Sessns   Data Segments    Report Segments  RptAck  Cancel By Sendr  Cancel By Recvr  CanAck  Bundles  Bundles  CanBRcv
//          Total /  Max    DS R-X   Total  / ReXmit   Total / ReXmit  Total    Total / ReXmit   Total / ReXmit  Total   Success  Failure  But Got
//         ---------------  ------  ----------------  ---------------  ------  ---------------  ---------------  ------  -------  -------  --------
//Sender   123456 / 123456  123456  1234567 / 123456  123456 / 123456  123456  123456 / 123456  123456 / 123456  123456  1234567  1234567
//Receiver 123456 / 123456  123456  1234567 / 123456  123456 / 123456  123456  123456 / 123456  123456 / 123456  123456                     1234567

    buf->appendf("Receiver %6"PRIu64" / %6"PRIu64"  %6"PRIu64"  %7"PRIu64" / %6"PRIu64"  %6"PRIu64" / %6"PRIu64
                 "  %6"PRIu64"  %6"PRIu64" / %6"PRIu64"  %6"PRIu64" / %6"PRIu64"  %6"PRIu64"  %7"PRIu64"  %7"PRIu64"  %7"PRIu64"\n",
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_session_resends_, stats_.total_rcv_ds_, stats_.ds_segment_resends_,
                 stats_.total_snt_rs_, stats_.rs_session_resends_, stats_.total_rcv_ra_,
                 stats_.total_rcv_css_, uint64_t(0), stats_.total_snt_csr_, stats_.cs_session_resends_,
                 stats_.total_snt_ca_,
                 stats_.bundles_success_, uint64_t(0), stats_.cs_session_cancel_by_receiver_but_got_it_);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Bundle_Processor(oasys::StringBuffer* buf)
{
    buf->appendf("Bundle Processor Queue Size: %zu\n", bundle_processor_->queue_size());
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::ProcessSegmentEvent(string segment_key,DS_MAP * segments, void* timer)
{
    DS_MAP::iterator seg_iter = segments->find(segment_key);

    if(seg_iter != segments->end()) {

        LTPUDPDataSegment * segment = seg_iter->second;

        if(segment != (LTPUDPDataSegment *) NULL)
        {
            segment->Start_Retransmission_Timer(timer);
        }

    } else {
        log_debug("Lookup failed ProcessSegmentEvent(rcvr).. %s",segment_key.c_str());
    } 
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::ProcessSegmentEvent(string segment_key,RS_MAP * segments, void* timer)
{
    RS_MAP::iterator seg_iter = segments->find(segment_key);

    if(seg_iter != segments->end()) {

        LTPUDPReportSegment * segment = seg_iter->second;

        if(segment != (LTPUDPReportSegment *) NULL)
        {
            segment->Start_Retransmission_Timer(timer);
        }

    } else {
        log_debug("Lookup failed ProcessSegmentEvent(rcvr).. %s",segment_key.c_str());
    } 
}
//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Receiver::build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code)
{
    LTPUDPCancelSegment * cancel_segment = new LTPUDPCancelSegment(session, segment_type, reason_code, parent_);

    log_debug("Receiver:build_CS_segment: Session: %s ReasonCode:%s",
              session->Key().c_str(), cancel_segment->Get_Reason_Code(reason_code));

    session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_CS); 

    cancel_segment->Set_Retrans_Retries(0);
    cancel_segment->Create_Retransmission_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER,
                                                LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

    session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_CS); 
    session->Set_Cancel_Segment(cancel_segment);

    if (session->RedProcessed()) {
      ++stats_.cs_session_cancel_by_receiver_but_got_it_;
    } else {
      ++stats_.cs_session_cancel_by_receiver_;
    }
    ++stats_.total_snt_csr_;

    parent_->Send(cancel_segment,PARENT_RECEIVER);
}

//----------------------------------------------------------------------
RS_MAP::iterator 
LTPUDPConvergenceLayer::Receiver::track_RS(LTPUDPSession * session, LTPUDPReportSegment * this_segment)
{
    log_debug("track_RS: Session: %s Segment:%s",
              session->Key().c_str(), this_segment->Key().c_str());

    RS_MAP::iterator report_segment_iterator = session->Report_Segments().find(this_segment->Key().c_str());
    if(report_segment_iterator == session->Report_Segments().end())
    {
        log_debug("track_RS: adding new Report Segment: %s",this_segment->Key().c_str());
        session->Report_Segments().insert(std::pair<std::string, LTPUDPReportSegment *>(this_segment->Key(), this_segment));
        report_segment_iterator = session->Report_Segments().find(this_segment->Key().c_str());
        
    } else {
        log_debug("track_RS: Found Report Segment key:%s",this_segment->Key().c_str());
    }
    return report_segment_iterator;
}

//----------------------------------------------------------------------
LTPUDPReportSegment * 
LTPUDPConvergenceLayer::Receiver::scan_RS_list(LTPUDPSession * this_session)
{
    LTPUDPReportSegment * report_segment = (LTPUDPReportSegment *) NULL;

    RS_MAP::iterator report_seg_iterator;

    log_debug("scan_RS_list: Session: %s Checkpoint: %"PRIi64,
              this_session->Key().c_str(),
              this_session->Checkpoint_ID());

    for(report_seg_iterator =  this_session->Report_Segments().begin(); 
        report_seg_iterator != this_session->Report_Segments().end(); 
        report_seg_iterator++)
    {
        report_segment  = report_seg_iterator->second;
        if (this_session->Checkpoint_ID() == report_segment->Checkpoint_ID())return report_segment;
    }    
    return (LTPUDPReportSegment *) NULL;
}

//----------------------------------------------------------------------
SESS_MAP::iterator
LTPUDPConvergenceLayer::Receiver::track_receiver_session(u_int64_t engine_id, u_int64_t session_id, bool create_flag)
{
    SESS_MAP::iterator session_iterator; 
    char key[45];
    sprintf(key,"%20.20"PRIu64":%20.20"PRIu64,engine_id,session_id);
    string insert_key = key;

    log_debug("track_receiver_session: %s   session_list size:%zu",
              key, ltpudp_rcvr_sessions_.size());

    session_iterator = ltpudp_rcvr_sessions_.find(insert_key);
    if(session_iterator == ltpudp_rcvr_sessions_.end())
    {
        if (create_flag) {
            log_debug("track_receiver_session: adding new session: %s", key);
            LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_id, parent_,  PARENT_RECEIVER);
            new_session->Set_Inbound_Cipher_Suite(cla_params_.inbound_cipher_suite_);
            new_session->Set_Inbound_Cipher_Key_Id(cla_params_.inbound_cipher_key_id_);
            new_session->Set_Inbound_Cipher_Engine(cla_params_.inbound_cipher_engine_);
            new_session->Set_Outbound_Cipher_Suite(cla_params_.outbound_cipher_suite_);
            new_session->Set_Outbound_Cipher_Key_Id(cla_params_.outbound_cipher_key_id_);
            new_session->Set_Outbound_Cipher_Engine(cla_params_.outbound_cipher_engine_);
            ltpudp_rcvr_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
            session_iterator = ltpudp_rcvr_sessions_.find(insert_key);
            if (ltpudp_rcvr_sessions_.size() > stats_.max_sessions_) {
                stats_.max_sessions_ = ltpudp_rcvr_sessions_.size();
            }
            ++stats_.total_sessions_;
        } else {
            log_debug("track_receiver_session: did not find session: %s",key);
        }
    } else {
        log_debug("track_receiver_session: found session: %s",key);
    }
    return session_iterator;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::cleanup_receiver_session(string session_key, string segment_key, 
                                                           int segment_type)
{ 
    (void) segment_key;

    // lock should've occured before this routine is hit
    ASSERT(session_lock_.is_locked_by_me());

    time_t time_left;

    LTPUDPCancelSegment * cs_segment;

    SESS_MAP::iterator cleanup_iterator = ltpudp_rcvr_sessions_.find(session_key);

    DS_MAP::iterator ds_segment_iterator;
    RS_MAP::iterator rs_segment_iterator;

    if(cleanup_iterator != ltpudp_rcvr_sessions_.end())
    {
        LTPUDPSession * ltpsession = cleanup_iterator->second;
        
        ltpsession->Cancel_Inactivity_Timer();

        switch(segment_type)
        {
            case LTPUDPSegment::LTPUDP_SEGMENT_UNKNOWN: // treat as a DS since the Inactivity timer sends 0
            case LTPUDPSegment::LTPUDP_SEGMENT_DS:

                time_left = ltpsession->Last_Packet_Time() + cla_params_.inactivity_intvl_;
                time_left = time_left - parent_->AOS_Counter();

                if(time_left > 0) /* timeout occured but retries are not exhausted */
                {
                    log_debug("cleanup_receiver_session(DS): %s - Extend Inactivity check %"PRIu32" seconds",
                              session_key.c_str(), (uint32_t)time_left);
                    ltpsession->Start_Inactivity_Timer(time_left); // catch the remaining seconds...
                } else {

                    log_debug("cleanup_receiver_session(DS): %s - Inactivity Timeout - Cancel by Receiver",
                              session_key.c_str());
                    

                    ltpsession->RemoveSegments();   
                    build_CS_segment(ltpsession,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                     LTPUDPCancelSegment::LTPUDP_CS_REASON_RXMTCYCEX);
                }
                break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_TO:

                cs_segment = ltpsession->Cancel_Segment();
                cs_segment->Cancel_Retransmission_Timer();
                cs_segment->Increment_Retries();

                if (cs_segment->Retrans_Retries() <= cla_params_.retran_retries_) {
                    log_debug("cleanup_receiver_session(CS_TO): %s - Retransmit Cancel Segment",
                              session_key.c_str());

                    cs_segment->Create_Retransmission_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                                            LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

                    ++stats_.cs_session_resends_;

                    parent_->Send(cs_segment,PARENT_RECEIVER);
                    return; 
                } else {
                    log_debug("cleanup_receiver_session(CS_TO): %s - Retransmit retries exceeded - delete session",
                              session_key.c_str());

                    delete ltpsession;
                    ltpudp_rcvr_sessions_.erase(cleanup_iterator);
                }

                break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR:
                log_debug("cleanup_receiver_session(CAS_BR): %s - delete session",
                          session_key.c_str());

                delete ltpsession;
                ltpudp_rcvr_sessions_.erase(cleanup_iterator);
                break;

            default:
                log_err("cleanup_receiver_session - unhandled segment type: %d", segment_type);
                break;
        }
    }

    return;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::cleanup_RS(string session_key, string segment_key, 
                                             int segment_type)
{ 
    // lock should've occured before this routine is hit
    ASSERT(session_lock_.is_locked_by_me());

    SESS_MAP::iterator session_iterator = ltpudp_rcvr_sessions_.find(session_key);

    if(session_iterator != ltpudp_rcvr_sessions_.end()) {

        LTPUDPSession * session = session_iterator->second;

        RS_MAP::iterator rs_iterator = session->Report_Segments().find(segment_key);

        if(rs_iterator != session->Report_Segments().end())
        {
            LTPUDPReportSegment * ltpreportsegment = rs_iterator->second;

            ltpreportsegment->Cancel_Retransmission_Timer();

            if(segment_type == (int) LTPUDPSegment::LTPUDP_SEGMENT_RS_TO)
            { 
                LTPUDPCancelSegment * cancel_segment = session->Cancel_Segment();

                if (cancel_segment != NULL) {
                    log_debug("cleanup_RS: Session: %s - Ignoring RS resend while in Cancelling State", 
                              session_key.c_str());
                } else {
                    ltpreportsegment->Increment_Retries();
                    if(ltpreportsegment->Retrans_Retries() <= cla_params_.retran_retries_) {
                        log_debug("cleanup_RS: Session: %s - Resending Report Segment (attempt %d)",
                                  session_key.c_str(), ltpreportsegment->Retrans_Retries());
                        Resend_RS(session, ltpreportsegment);
                    } else {
                        // Exceeded retries - start cancelling
                        build_CS_segment(session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                         LTPUDPCancelSegment::LTPUDP_CS_REASON_RLEXC);
                    }
                } 

                return;
            }

            log_debug("cleanup_RS: Session: %s - Received Report Ack Segment - delete report segment",
                      session_key.c_str());

            ltpreportsegment->Cancel_Retransmission_Timer();
            delete ltpreportsegment;
            session->Report_Segments().erase(rs_iterator);

        } else {
            log_debug("cleanup_RS: Session: %s - Report Segment not found: %s",
                      session_key.c_str(), segment_key.c_str());
        }
    } else {
        log_debug("cleanup_RS: Session: %s - not found", session_key.c_str());
    }
}
                         
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Resend_RS(LTPUDPSession * report_session, LTPUDPReportSegment * report_segment)
{
    (void) report_session;

    report_segment->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                    LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                   report_segment->Report_Serial_Number());

    ++stats_.rs_session_resends_;
    ++stats_.total_snt_rs_;

    parent_->Send(report_segment,PARENT_RECEIVER);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::generate_RS(LTPUDPSession * report_session, int red_green)
{
    LTPUDPReportSegment * report_segment = (LTPUDPReportSegment *) NULL;

    report_segment = scan_RS_list(report_session);

    if(report_segment == (LTPUDPReportSegment *) NULL) {
        report_segment = new LTPUDPReportSegment( report_session, red_green );
        track_RS(report_session, report_segment);
        report_session->inc_reports_sent();
    } 

    report_segment->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                    LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                    report_segment->Report_Serial_Number());

    ++stats_.total_snt_rs_;
    parent_->Send(report_segment,PARENT_RECEIVER);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    int green_bytes_to_process = 0; 
    int red_bytes_to_process   = 0; 
   
    bool create_flag = false;

    LTPUDPSession * ltpudp_session;
    SESS_MAP::iterator session_iterator;

    if (cla_params_.hexdump_) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("process_data: Buffer (%d of %zu):", plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPUDPSegment * seg = LTPUDPSegment::DecodeBuffer(bp, len);

    if(!seg->IsValid()){
        log_warn("process_data: invalid segment.... type:%s length:%d", 
                 LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
        delete seg;
        return;
    }

    log_debug("process_data: SEG(%s)",LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));

#ifdef LTPUDP_AUTH_ENABLED
    if(cla_params_.inbound_cipher_suite_ != -1) {
        if(!seg->IsAuthenticated(cla_params_.inbound_cipher_suite_, 
                                 cla_params_.inbound_cipher_key_id_, 
                                 cla_params_.inbound_cipher_engine_)) {
            log_warn("process_data: invalid segment doesn't authenticate.... type:%s length:%d", 
                     LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
            delete seg;
            return;
        }
    }
#endif

    string temp_string = "";
 
    // after then set the parent....
    seg->Set_Parent(parent_);
 
    //
    // Send over to Sender if it's any of these three SEGMENT TYPES
    //
    if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RS     || 
        seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR  ||
        seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS) { 
        log_debug("process_data: passing %s to Sender to process",
                  LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));

        parent_->Process_Sender_Segment(seg);
        delete seg; // we are done with this segment
        return;
    }

    oasys::ScopeLock my_lock(&session_lock_,"Receiver::process_data");  

    create_flag = (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS);

    // get current session from rcvr_sessions...
    session_iterator = track_receiver_session(seg->Engine_ID(),seg->Session_ID(),create_flag);  

    if(!create_flag && session_iterator == ltpudp_rcvr_sessions_.end())
    {
        if ((int) seg->Segment_Type() != 6) {
            log_debug("process_data: Session: %"PRIu64"-%"PRIu64" - %s received but session not found",
                      seg->Engine_ID(), seg->Session_ID(), LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));
        } else {
            LTPUDPCancelAckSegment * cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) seg, 
                                                                           LTPUDPSEGMENT_CAS_BS_BYTE, blank_session_); 
            ++stats_.total_snt_ca_;
            parent_->Send(cas_seg,PARENT_RECEIVER); 
            delete cas_seg;
        }
        delete seg; // ignore this segment since the session is already deleted.
        return; 
    }
       
    ltpudp_session = session_iterator->second;

    if (ltpudp_session->Session_State() == LTPUDPSession::LTPUDP_SESSION_STATE_CS) {
        if((seg->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR) &&
           (seg->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BS)) {

              log_debug("process_data: Session: %s - Ignoring %s while Session in CS state",
                        ltpudp_session->Key().c_str(), LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));

              delete seg;
              return;
        }
    }

    //
    //   CTRL EXC Flag 1 Flag 0 Code  Nature of segment
    //   ---- --- ------ ------ ----  ---------------------------------------
    //     0   0     0      0     0   Red data, NOT {Checkpoint, EORP or EOB}
    //     0   0     0      1     1   Red data, Checkpoint, NOT {EORP or EOB}
    //     0   0     1      0     2   Red data, Checkpoint, EORP, NOT EOB
    //     0   0     1      1     3   Red data, Checkpoint, EORP, EOB
    //
    //     0   1     0      0     4   Green data, NOT EOB
    //     0   1     0      1     5   Green data, undefined
    //     0   1     1      0     6   Green data, undefined
    //     0   1     1      1     7   Green data, EOB
    //
    
    if(seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS) {  // data segments
        ++stats_.total_rcv_ds_;
        if(seg->Session_ID() > ULONG_MAX)
        {
            build_CS_segment(ltpudp_session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                             LTPUDPCancelSegment::LTPUDP_CS_REASON_SYS_CNCLD);
            delete seg;
            log_debug("process_data: received %s DS with invalid Session ID - exceed ULONG_MAX", 
                      ltpudp_session->Key().c_str());
            return;
        }

        if (seg->Service_ID() != 1 && seg->Service_ID() != 2)
        {
            if(seg->IsRed()) {
                log_warn("process_data: received %s DS with invalid Service ID: %d",
                          ltpudp_session->Key().c_str(), seg->Service_ID());

                // send cancel stuff
                build_CS_segment(ltpudp_session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                 LTPUDPCancelSegment::LTPUDP_CS_REASON_UNREACHABLE);
            } else {
                log_warn("process_data: received %s Green DS with invalid Service ID: %d", 
                         ltpudp_session->Key().c_str(),seg->Service_ID());
                ltpudp_rcvr_sessions_.erase(session_iterator);
                my_lock.unlock();
                delete ltpudp_session;
            }

            delete seg;
            return; 
        }


        ltpudp_session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_DS); 
        if (-1 == ltpudp_session->AddSegment((LTPUDPDataSegment*)seg)) {
            delete seg;
            return;
        }

        if (!ltpudp_session->Has_Inactivity_Timer()) {
            ltpudp_session->Start_Inactivity_Timer(cla_params_.inactivity_intvl_);
        }

        if (ltpudp_session->reports_sent() > 0) {
            // RS has already been sent so this is a resend of the DS
            ++stats_.ds_segment_resends_;
        }

        if(seg->IsEndofblock()) {
            ltpudp_session->Set_EOB((LTPUDPDataSegment*) seg);
        }

        if(seg->IsCheckpoint())
        {
            ++stats_.total_rcv_ds_checkpoints_;
            log_debug("process_data: Session: %s - received checkpoint %"PRIu64" generate RS",
                      ltpudp_session->Key().c_str(),seg->Checkpoint_ID());
 
            ltpudp_session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_RS);
            ltpudp_session->Set_Checkpoint_ID(seg->Checkpoint_ID());
            generate_RS(ltpudp_session, seg->Red_Green());
            seg->Set_Retrans_Retries(0);
        }

        red_bytes_to_process = ltpudp_session->IsRedFull();
        if(red_bytes_to_process > 0) {
            log_debug("process_data: Session: %s - received all red data - post %d to bundle_processor",
                     ltpudp_session->Key().c_str(), ltpudp_session->Expected_Red_Bytes());
 
            bundle_processor_->post(ltpudp_session->GetAllRedData(),(size_t) ltpudp_session->Expected_Red_Bytes(),
                                    seg->Service_ID() == 2 ? true : false);
        } else if (seg->IsCheckpoint()) {
            ++stats_.ds_session_resends_;
        }
 

        green_bytes_to_process = ltpudp_session->IsGreenFull();
        if(green_bytes_to_process > 0) {
            log_debug("process_data: Session: %s - received all green data - post %d to bundle_processor",
                     ltpudp_session->Key().c_str(), ltpudp_session->Expected_Red_Bytes());
 
            bundle_processor_->post(ltpudp_session->GetAllGreenData(),(size_t)ltpudp_session->Expected_Green_Bytes(), 
                                    seg->Service_ID() == 2 ? true : false);

            // CCSDS BP Specification - LTP Sessions must be all red or all green
            if (ltpudp_session->DataProcessed()) {
                ltpudp_rcvr_sessions_.erase(session_iterator);
                my_lock.unlock();
                delete ltpudp_session;
            }
        }

    } else if(seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RAS)  { 
         // Report Ack Segment
         ++stats_.total_rcv_ra_;

        LTPUDPReportAckSegment * look = (LTPUDPReportAckSegment *) seg;
        // cleanup the Report Segment list so we don't keep transmitting.
        char temp[24];
        snprintf(temp, sizeof(temp), "%20.20"PRIu64, look->Report_Serial_Number());
        log_debug("RAS found RSN=%"PRIu64,look->Report_Serial_Number());
        string str_key = temp;
        cleanup_RS(session_iterator->first, str_key, LTPUDPSegment::LTPUDP_SEGMENT_RAS); 
        delete seg;

        if (ltpudp_session->DataProcessed()) {
            log_debug("process_data: Session: %s - Report Ack Segment received - delete session",
                      ltpudp_session->Key().c_str());

            ltpudp_rcvr_sessions_.erase(session_iterator);
            my_lock.unlock();
            delete ltpudp_session;
        } else {
            log_debug("process_data: Session: %s - Report Ack Segment received - wait for more data - restart Inactivity Timer",
                      ltpudp_session->Key().c_str());
            ltpudp_session->Start_Inactivity_Timer(cla_params_.inactivity_intvl_);
        }
    } else if(seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BS) { 
        // Cancel Segment from Block Sender
        ++stats_.total_rcv_css_;

        //dz debug log_debug("process_data: Session: %s - Cancel by Sender Segment received - delete session",
        log_warn("process_data: Session: %s - Cancel by Sender Segment received - delete session",
                  ltpudp_session->Key().c_str());

        ltpudp_session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_CS); 
        ltpudp_session->Set_Cleanup();
        LTPUDPCancelAckSegment * cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) seg, 
                                                                       LTPUDPSEGMENT_CAS_BS_BYTE, blank_session_); 
        ++stats_.total_snt_ca_;
        parent_->Send(cas_seg,PARENT_RECEIVER); 
        delete seg;
        delete cas_seg;

        // clean up the session
        ltpudp_rcvr_sessions_.erase(session_iterator);
        my_lock.unlock();
        delete ltpudp_session;

    } else if(seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR) { 
        // Cancel-acknowledgment Segment from Block Receiver
        ++stats_.total_rcv_ca_;

        log_debug("process_data: Session: %s - Cancel by Receiver ACK Segment received - delete session",
                  ltpudp_session->Key().c_str());

        ltpudp_session->Set_Cleanup();
        ltpudp_rcvr_sessions_.erase(session_iterator); // cleanup session!!!
        my_lock.unlock();
        delete ltpudp_session;
        delete seg;
    } else {
        log_err("process_data - Unhandled segment type received: %d", seg->Segment_Type());
        delete seg;
    }
}


//----------------------------------------------------------------------
char*
LTPUDPConvergenceLayer::Receiver::get_recv_buffer()
{
    BufferObject* bo;
    if (buffer_pool_.empty()) {
        ++buffers_allocated_; 
        bo = new BufferObject();
        bo->buffer_ = (u_char*) malloc(10000000);
        bo->len_ = 10000000;
        bo->used_ = 0;
        bo->last_ptr_ = NULL;
        buffer_pool_.push_back(bo);
    }

    bo = buffer_pool_.front();
    if ((bo->used_ + MAX_UDP_PACKET) > (bo->len_ - bo->used_)) {
        // this buffer is full - move to used pool
        buffer_pool_.pop_front();
        used_buffer_pool_.push_back(bo);

        return get_recv_buffer();
    }

    return (char*) (bo->buffer_ + bo->used_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::DataProcObject* 
LTPUDPConvergenceLayer::Receiver::get_dpo(int len)
{
    DataProcObject* dpo;
    if (dpo_pool_.empty()) {
        dpo = new DataProcObject();
    } else {
        dpo = dpo_pool_.front();
        dpo_pool_.pop_front();
    }

    BufferObject* bo = buffer_pool_.front();    
    dpo->data_ = &bo->buffer_[bo->used_];
    dpo->len_ = len;

    bo->used_ += len;
    ASSERT(bo->used_ <= bo->len_);
    bo->last_ptr_ = dpo;

    return dpo;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Receiver::return_dpo(DataProcObject* obj)
{
    if (!used_buffer_pool_.empty()) {
        BufferObject* bo = used_buffer_pool_.front();
        if (bo->last_ptr_ == obj) {
            // final block of data in the buffer has been processed
            used_buffer_pool_.pop_front();

            // buffer can now be re-used
            bo->used_ = 0;
            buffer_pool_.push_back(bo);
        }
    }

    dpo_pool_.push_back(obj);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::run()
{
    int read_len;
    in_addr_t addr;
    u_int16_t port;

    int bufsize = 0; 
    socklen_t optlen = sizeof(bufsize);
    if (::getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bufsize, &optlen) < 0) {
        log_warn("error getting SO_RCVBUF: %s", strerror(errno));
    } else {
        log_always("LTPUDPConvergenceLayer::Receiver socket recv_buffer size = %d", bufsize);
    }

    log_debug(" run invoked");

    while (1) {
        if (should_stop())
            break;

        read_len = poll_sockfd(POLLIN, NULL, 10);

        if (should_stop())
            break;

        if (1 == read_len) {
            read_len = recvfrom(get_recv_buffer(), MAX_UDP_PACKET, 0, &addr, &port);

            if (read_len <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                log_err("error in recvfrom(): %d %s",
                        errno, strerror(errno));
                close();
                break;
            }
        
            if (should_stop())
                break;

            data_process_offload_->post(get_dpo(read_len));
        }
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::RecvBundleProcessor(Receiver* parent, LinkRef* link)
    : Logger("LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor",
             "/dtn/cl/ltpudp/receiver/bundleproc/%p", this),
      Thread("LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor", Thread::DELETE_ON_EXIT)
{
    parent_ = parent;
    link_ = link->object();

    bytes_queued_ = 0;
    max_bytes_queued_ = 100000000; // 100 MB

    eventq_ = new oasys::MsgQueue< EventObj* >(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::~RecvBundleProcessor()
{
    set_should_stop();
    sleep(1);

    EventObj* event;
    while (eventq_->try_pop(&event)) {
        free(event->data_);
        delete event;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::queue_size()
{
    return eventq_->size();
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::bytes_queued()
{
    return bytes_queued_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::post(u_char* data, size_t len, bool multi_bundle)
{
    while (not should_stop() && ((bytes_queued_ + len) > max_bytes_queued_)) {
        usleep(100);
    }
    if (should_stop()) return;

    bytes_queued_lock_.lock("RecvDataProcessOffload:post");
    bytes_queued_ += len;
    bytes_queued_lock_.unlock();
 
    EventObj* event = new EventObj();
    event->data_ = data;
    event->len_ = len;
    event->multi_bundle_ = multi_bundle;
    eventq_->push_back(event);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::run()
{
    EventObj* event;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (1) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            log_err("RecvBundleProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvBundleProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                bytes_queued_lock_.lock("RecvBundleProcessor:run");
                bytes_queued_ -= event->len_;
                bytes_queued_lock_.unlock();
 
                extract_bundles_from_data(event->data_, event->len_, event->multi_bundle_);

                delete event;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::ProcessReceiverSessionEvent(MySendObject * event)
{
    oasys::ScopeLock my_lock(&session_lock_,"Receiver::ProcessReceiverSessionEvent");  

    SESS_MAP::iterator session_iter = ltpudp_rcvr_sessions_.find(event->session_);

    LTPUDPCancelSegment * cs = (LTPUDPCancelSegment *) NULL;

    if(session_iter != ltpudp_rcvr_sessions_.end()) {

        LTPUDPSession * session = session_iter->second;

        switch(event->segment_type_)
        {
            case LTPUDPSegment::LTPUDP_SEGMENT_DS:

                 if(session->Red_Segments().size() > 0)
                     ProcessSegmentEvent(event->segment_,&session->Red_Segments(), event->timer_);
                 if(session->Green_Segments().size() > 0)
                     ProcessSegmentEvent(event->segment_,&session->Green_Segments(), event->timer_);
                break;
  
            case LTPUDPSegment::LTPUDP_SEGMENT_RS:

                if(session->Report_Segments().size() > 0)
                    ProcessSegmentEvent(event->segment_,&session->Report_Segments(), event->timer_);
                break;
 

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BS:
            case LTPUDPSegment::LTPUDP_SEGMENT_CS_TO:

                cs = session->Cancel_Segment();
                if (NULL != cs) {
                    cs->Start_Retransmission_Timer(event->timer_);
                }
                break;

            default:
                log_err("ProcessReceiverSessionEvent: Session: %s invalid segment type: %s",
                        session->Key().c_str(), LTPUDPSegment::Get_Segment_Type(event->segment_type_));
                break;
        }
    } else {
        // RAS can be received and the session deleted before the RS event gets here
        log_debug("ProcessReceiverSessionEvent: Session not found: %s   (segment type: %s)",
                   event->session_.c_str(), LTPUDPSegment::Get_Segment_Type(event->segment_type_));
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::extract_bundles_from_data(u_char * bp, size_t len, bool multi_bundle)
{
    u_int32_t check_client_service = 0;
    // the payload should contain a full bundle
    int all_data = (int) len;
    u_char * current_bp = bp;
    if(bp == (u_char *) NULL)
    {
        log_err("extract_bundles_from_data: no data to process?");
        return;
    }

    log_debug("extract_bundles_from_data: %d bytes -- LTP SDA? %s", all_data, multi_bundle?"true":"false");

//  if (multi_bundle) {
//      oasys::HexDumpBuffer hex;
//      hex.append(bp, len);
//      log_debug("Extract: (%d):",(int) len);
//      log_multiline(oasys::LOG_DEBUG, hex.hexify().c_str());
//  }

    while(all_data > 0)
    {
        log_debug("Extract:  All_data = %d",all_data);
        Bundle* bundle = new Bundle();
    
        bool complete = false;
        if (multi_bundle) {
            log_debug("extract_bundles_from_data: client_service (%02X)",*current_bp);
            // Multi bundle buffer... must make sure a 1 precedes all packets
            int bytes_used = SDNV::decode(current_bp, all_data, &check_client_service);
            if(check_client_service != 1)
            {
                log_err("extract_bundles_from_data: LTP SDA - invalid Service ID: %d",check_client_service);
                delete bundle;
                break; // so we free the bp
            }
            current_bp += bytes_used;
            all_data   -= bytes_used;
        }
             
        int cc = BundleProtocol::consume(bundle, current_bp, all_data, &complete);
        if (cc < 0)  {
            log_err("extract_bundles_from_data: bundle protocol error");
            delete bundle;
            break; // so we free the bp
        }

        if (!complete) {
            log_err("extract_bundles_from_data: incomplete bundle");
            delete bundle;
            break;  // so we free the bp
        }

        all_data   -= cc;
        current_bp += cc;

        log_debug("extract_bundles_from_data: new Bundle %"PRIbid"  (payload %zu)",
                  bundle->bundleid(), bundle->payload().length());

        parent_->inc_bundles_received();

        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, cc, EndpointID::NULL_EID(), link_.object()));
    }

    if(bp != (u_char *) NULL)free(bp);

}







//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::RecvDataProcessOffload(Receiver* parent)
    : Logger("LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload",
             "/dtn/cl/ltpudp/receiver/dataproc/%p", this),
      Thread("LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload", Thread::DELETE_ON_EXIT)
{
    parent_ = parent;
    bytes_queued_ = 0;
    max_bytes_queued_ = 100000000; // 100 MB

    eventq_ = new oasys::MsgQueue<DataProcObject*>(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::~RecvDataProcessOffload()
{
    set_should_stop();
    sleep(1);

    DataProcObject* obj;
    while (eventq_->try_pop(&obj)) {
        delete obj;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::queue_size()
{
    return eventq_->size();
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::bytes_queued()
{
    return bytes_queued_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::post(DataProcObject* obj)
{
    while (not should_stop() && ((bytes_queued_ + obj->len_) > max_bytes_queued_)) {
        usleep(100);
    }
    if (should_stop()) return;

    bytes_queued_lock_.lock("RecvDataProcessOffload:post");
    bytes_queued_ += obj->len_;
    bytes_queued_lock_.unlock();
 
    eventq_->push_back(obj);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::run()
{
    DataProcObject* obj;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (1) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            log_debug("RecvDataProcessOffload interrupted - continuing");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvDataProcessOffload IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an obj
        if (event_poll->revents & POLLIN) {
            if (eventq_->try_pop(&obj)) {
                ASSERT(obj != NULL)

                bytes_queued_lock_.lock("RecvDataProcessOffload:run");
                bytes_queued_ -= obj->len_;
                bytes_queued_lock_.unlock();
 
                parent_->process_data(obj->data_, obj->len_);

                parent_->return_dpo(obj);
            }
        }
    }
}














//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
string    LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}
//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::Sender(const ContactRef& contact,LTPUDPCallbackIF * parent)
    : 
      Logger("LTPUDPConvergenceLayer::Sender",
             "/dtn/cl/ltpudp/sender/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender"),
      lock_(new oasys::SpinLock()),
      lock2_(new oasys::SpinLock()),
      socket_(logpath_),
      session_ctr_(1),
      rate_socket_(0),
      contact_(contact.object(), "LTPUDPCovergenceLayer::Sender")
{
      char extra_logpath[200];
      strcpy(extra_logpath,logpath_);
      strcat(extra_logpath,"2");
      loading_session_          = (LTPUDPSession *) NULL;
      poller_                   = NULL;
      parent_                   = parent;
      high_eventq_              = new oasys::MsgQueue< MySendObject* >(logpath_, lock_);
      low_eventq_               = new oasys::MsgQueue< MySendObject* >(extra_logpath, lock2_);


      memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::~Sender()
{
    MySendObject *event;

    if (poller_ != NULL) {
        poller_->set_should_stop();
        poller_ = NULL;
    }

    this->set_should_stop();
    sleep(1);

    while (high_eventq_->try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (low_eventq_->try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    delete high_eventq_;
    delete low_eventq_;

    if (rate_socket_) {
        delete rate_socket_;
    }
    delete blank_session_;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::init(Params* params, in_addr_t addr, u_int16_t port)
{
    log_debug("init entered");

    params_ = params;
    
    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);
    socket_.params_.send_bufsize_ = params_->sendbuf_;
    socket_.init_socket();

    int bufsize = 0; 
    socklen_t optlen = sizeof(bufsize);
    if (::getsockopt(socket_.fd(), SOL_SOCKET, SO_SNDBUF, &bufsize, &optlen) < 0) {
        log_warn("error getting SO_SNDBUF: %s", strerror(errno));
    } else {
        log_always("LTPUDPConvergenceLayer::Sender socket send_buffer size = %d", bufsize);
    }

    // do not bind or connect the socket
    if (params->rate_ != 0) {

        rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params->bucket_type_);
 
        rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        rate_socket_->bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params->bucket_depth_);
        }
 
        log_debug("init: Sender rate controller after initialization: rate %"PRIu64" depth %"PRIu64,
                  rate_socket_->bucket()->rate(),
                  rate_socket_->bucket()->depth());
    }

    blank_session_ = new LTPUDPSession(0UL, 0UL, parent_,  PARENT_RECEIVER);
    blank_session_->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
    blank_session_->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
    blank_session_->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::run()
{
    struct timespec sleep_time = { 0, 100000000 }; // 1 tenth of a second

    LTPUDPSession* loaded_session = NULL;
    int ret;

    MySendObject *hevent; 
    MySendObject *levent; 
    int cc = 0;
    // block on input from the socket and
    // on input from the bundle event list

    poller_ = new SendPoller(this, contact_->link());
    poller_->start();


    struct pollfd pollfds[2];

    struct pollfd* high_event_poll = &pollfds[0];

    high_event_poll->fd = high_eventq_->read_fd();
    high_event_poll->events = POLLIN; 
    high_event_poll->revents = 0;

    struct pollfd* low_event_poll = &pollfds[1];

    low_event_poll->fd = low_eventq_->read_fd();
    low_event_poll->events = POLLIN; 
    low_event_poll->revents = 0;

    while (1) {

        if (should_stop()) return; 

        // only need to check loading_session_ if not NULL and the lock is immediately
        // available since send_bundle now checks for agg time expiration also
        if ((NULL != loading_session_) && 
            (0 == loading_session_lock_.try_lock("Sender::run()"))) {

            loaded_session = NULL;

            // loading_session_ may have been cleared out between the two if checks above
            if(NULL != loading_session_ && loading_session_->TimeToSendIt(params_->agg_time_))
            {
                 loaded_session = loading_session_;
                 loading_session_ = NULL;
            }

            // release the lock as quick as possible
            loading_session_lock_.unlock();

            if (NULL != loaded_session) {
                 if(params_->outbound_cipher_suite_ != -1)
                 {
                     log_debug("Timer expired Loading cipher suite into session from params");
                     loaded_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
                     loaded_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
                     loaded_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);
                 }

                 log_debug("run(): LTP Aggregation Time expired - process_bundles...");
                 process_bundles(loaded_session,false);
            }
        }


        ret = oasys::IO::poll_multiple(pollfds, 2, 1, NULL);

        if (ret == oasys::IOINTR) {
            log_err("run(): Sender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("run(): Sender I/O error");
            set_should_stop();
            continue;
        }

        if(!params_->comm_aos_)
        {
            // not currently in contact so sleep and try again later
            nanosleep(&sleep_time,NULL);
            continue; 
        }

        // check for an event
        if (high_event_poll->revents & POLLIN) {
            if (high_eventq_->try_pop(&hevent)) {
                ASSERT(hevent != NULL)
        
                // oasys::HexDumpBuffer hex;
                // hex.append((u_char *) hevent->str_data_->data(),(int) hevent->str_data_->size());
                // log_always("high_event: Buffer (%d):",(int) hevent->str_data_->size());
                // log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
             
                if (params_->rate_ > 0) {
                    cc = rate_socket_->sendto(const_cast< char * >(hevent->str_data_->data()),
                                              hevent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              params_->wait_till_sent_);
                } else {
  
                    cc = socket_.sendto(const_cast< char * > (hevent->str_data_->data()), 
                                        hevent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (hevent->timer_) {
                    Handle_Event(hevent);
                }
               
                if (cc == (int)hevent->str_data_->size()) {
                    log_debug("Send: transmitted high priority %d byte segment to %s:%d",
                              cc, intoa(params_->remote_addr_), params_->remote_port_);
                } else {
                    log_err("Send: error sending high priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, hevent->str_data_->size(), strerror(errno));
                }

                delete hevent->str_data_;
                delete hevent;
            }

            // loop around to see if there is another high priority segment
            // to transmit before checking for low priority segements 
            continue; 
        }


        // check for a low priority event (new Data Segment)
        if (low_event_poll->revents & POLLIN) {
            if (low_eventq_->try_pop(&levent)) {

                ASSERT(levent != NULL)

                // oasys::HexDumpBuffer hex;
                // hex.append((u_char *)levent->str_data_->data(),(int) levent->str_data_->size());
                // log_always("low_event: Buffer (%d):", (int) levent->str_data_->size());
                // log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());

                if (params_->rate_ > 0) {
                    cc = rate_socket_->sendto(const_cast< char * >(levent->str_data_->data()),
                                              levent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              params_->wait_till_sent_);
                } else {
                    cc = socket_.sendto(const_cast< char * > (levent->str_data_->data()), 
                                        levent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (levent->timer_) {
                     Handle_Event(levent);
                }
               
                if (cc == (int)levent->str_data_->size()) {
                    log_debug("Send: transmitted low priority %d byte segment to %s:%d",
                              cc, intoa(params_->remote_addr_), params_->remote_port_);
                } else {
                    log_err("Send: error sending low priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, levent->str_data_->size(), strerror(errno));
                }

                delete levent->str_data_;
                delete levent;
            }
        }
    }
}
 
//---------------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::ProcessSenderSessionEvent(MySendObject * event)
{
    oasys::ScopeLock mylock(&session_lock_,"Sender::ProcessSenderSessionEvent");

    SESS_MAP::iterator session_iter = ltpudp_send_sessions_.find(event->session_);

    LTPUDPCancelSegment * cs = (LTPUDPCancelSegment *) NULL;

    if(session_iter != ltpudp_send_sessions_.end()) {

        LTPUDPSession * session = session_iter->second;

        switch(event->segment_type_)
        {       
            case LTPUDPSegment::LTPUDP_SEGMENT_DS:
                if(session->Red_Segments().size() > 0)
                    ProcessSegmentEvent(event->segment_,&session->Red_Segments(), event->timer_);
                if(session->Green_Segments().size() > 0)
                    ProcessSegmentEvent(event->segment_,&session->Green_Segments(), event->timer_);
                break;
   
            case LTPUDPSegment::LTPUDP_SEGMENT_RS:
                if(session->Report_Segments().size() > 0)
                    ProcessSegmentEvent(event->segment_,&session->Report_Segments(), event->timer_);
                break;
            
            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BS:
                cs = session->Cancel_Segment();
                if (NULL != cs) {
                    cs->Start_Retransmission_Timer(event->timer_);
                }
                break;
            
            default:
                log_err("ProcessSenderSessionEvent: Session: %s - Invalid segment: %s",
                        event->session_.c_str(), LTPUDPSegment::Get_Segment_Type(event->segment_type_));
                break;

        }
    } else {
        log_debug("ProcessSenderSessionEvent: Session %s - not found",
                  event->session_.c_str());
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::ProcessSegmentEvent(string segment_key, DS_MAP * segments, void* timer)
{
    DS_MAP::iterator seg_iter = segments->find(segment_key);

    if(seg_iter != segments->end()) {

        LTPUDPDataSegment * segment = seg_iter->second;

        if(segment != (LTPUDPDataSegment *) NULL)
        {
            segment->Start_Retransmission_Timer(timer);
        }

    } else {
        log_debug("ProcessSegmentEvent: Data Segment not found: %s", segment_key.c_str());
    } 
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::ProcessSegmentEvent(string segment_key, RS_MAP * segments, void* timer)
{
    RS_MAP::iterator seg_iter = segments->find(segment_key);

    if(seg_iter != segments->end()) {

        LTPUDPReportSegment * segment = seg_iter->second;

        if(segment != (LTPUDPReportSegment *) NULL)
        {
            segment->Start_Retransmission_Timer(timer);
        }

    } else {
        log_debug("ProcessSegmentEvent: Report Segment not found: %s", segment_key.c_str());
    } 
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Handle_Event(MySendObject * event)
{
    parent_->ProcessSessionEvent(event);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::PostTransmitProcessing(LTPUDPSession * session, bool success)
{
    LinkRef link = contact_->link();

    BundleRef bundle;

    while ((bundle = session->Bundle_List()->pop_back()) != NULL)
    {
        if (success) {

            ++stats_.bundles_success_;

            if(session->Expected_Red_Bytes() > 0)
            {
                log_debug("PostTransmitProcessing: Session: %s - Bundle ID: %"PRIbid" transmitted size = %d",
                          session->Key().c_str(), bundle->bundleid(), (int) session->Expected_Red_Bytes());

                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle.object(), contact_, 
                                               link, session->Expected_Red_Bytes(), 
                                               session->Expected_Red_Bytes(), success));
            } else if (session->Expected_Green_Bytes() > 0) { 
                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle.object(), contact_, 
                                               link, session->Expected_Green_Bytes(), 
                                               session->Expected_Green_Bytes(), success));
            }
        } else {
            ++stats_.bundles_fail_;

            // signaling failure so External Router can reroute the bundle
            log_debug("PostTransmitProcessing: Session: %s - Bundle ID: %"PRIbid" transmit failure",
                      session->Key().c_str(), bundle->bundleid());

            BundleDaemon::post(
                new BundleTransmittedEvent(bundle.object(), contact_, 
                                           link, 0, 0, success));
        }
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Process_Segment(LTPUDPSegment * segment)
{
    if (segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RS    ||
        segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR ||
        segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS) {

        switch(segment->Segment_Type()) {

              case LTPUDPSegment::LTPUDP_SEGMENT_RS:
                   process_RS((LTPUDPReportSegment *) segment);
                   break;

              case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
                   process_CS((LTPUDPCancelSegment *) segment);
                   break;

              case LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS:
                   process_CAS((LTPUDPCancelAckSegment *) segment);
                   break;

              default:
                   log_err("Unknown Segment Type for Sender::Process_Segment(%s)",
                           segment->Get_Segment_Type()); 
                   break;
        }
    }
    return;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code)
{
    LTPUDPCancelSegment * cancel_segment = new LTPUDPCancelSegment(session, segment_type, reason_code, parent_);

    log_debug("Sender:build_CS_segment: Session: %s ReasonCode:%s",
              session->Key().c_str(), cancel_segment->Get_Reason_Code(reason_code));

    session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_CS); 
    cancel_segment->Set_Retrans_Retries(0);
    cancel_segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

    session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_CS);            
    session->Set_Cancel_Segment(cancel_segment);

    ++stats_.cs_session_cancel_by_sender_;
    ++stats_.total_snt_css_;

    Send(cancel_segment,PARENT_SENDER);
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Dump_Sessions(oasys::StringBuffer* buf)
{
    int count = 0;
    int num_segs = 0;
    int max_segs = 0;
    DS_MAP::iterator segment_iter;
    SESS_MAP::iterator session_iter;

    buf->appendf("%zu\n",ltpudp_send_sessions_.size());

    for(session_iter =  ltpudp_send_sessions_.begin(); 
        session_iter != ltpudp_send_sessions_.end(); 
        session_iter ++)
    {
        LTPUDPSession * session = session_iter->second;

        buf->appendf("    Session: %s   %s\n",session_iter->first.c_str(), session->Get_Session_State());
        num_segs = 0;
        max_segs = session->Red_Segments().size() - 1;
        buf->appendf("        Red Segments: %zu\n",session->Red_Segments().size());
        if ((int) session->Outbound_Cipher_Suite() != -1)
            buf->appendf("             Outbound_Cipher_Suite: %d\n",session->Outbound_Cipher_Suite());
        
        for(segment_iter =  session->Red_Segments().begin(); 
            segment_iter != session->Red_Segments().end(); 
            segment_iter ++)
        {
            LTPUDPDataSegment * segment = segment_iter->second;
            if (num_segs < 5 || num_segs == max_segs) {
                if (num_segs > 5) {
                    buf->appendf("             ...\n");
                }
                buf->appendf("             key[%d]: %s\n", 
                             num_segs, segment->Key().c_str()); 
            }
            ++num_segs;
        }
        num_segs = 0;
        max_segs = session->Green_Segments().size() - 1;
        buf->appendf("      Green Segments: %zu\n",session->Green_Segments().size());
        if ((int) session->Outbound_Cipher_Suite() != -1)
            buf->appendf("             Outbound_Cipher_Suite: %d\n",session->Outbound_Cipher_Suite());
        for(segment_iter =  session->Green_Segments().begin(); 
            segment_iter != session->Green_Segments().end(); 
            segment_iter ++)
        {
            LTPUDPDataSegment * segment = segment_iter->second;
            if (num_segs < 5 || num_segs == max_segs) {
                if (num_segs > 5) {
                    buf->appendf("             ...\n");
                }
                buf->appendf("             key[%d]: %s\n", 
                             num_segs, segment->Key().c_str()); 
            }
            ++num_segs;
        }
        count++;
        if(count >= 5)break;
    }

}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Dump_Statistics(oasys::StringBuffer* buf)
{
//    buf->appendf("\nSender stats: DS resends- sessions: %"PRIu64" checkpoints: %"PRIu64" segments: %"PRIu64
//                 "  Max sessions: %"PRIu64"\n",
//                 stats_.ds_session_resends_, stats_.ds_session_checkpoint_resends_, stats_.ds_segment_resends_,
//                 stats_.max_sessions_);
//    buf->appendf("              CS by Receiver: %"PRIu64" CS by Sender: %"PRIu64" (resends: %"PRIu64")\n",
//                 stats_.cs_session_cancel_by_receiver_, stats_.cs_session_cancel_by_sender_, stats_.cs_session_resends_);
//    buf->appendf("        Totals:  sessions: %"PRIu64" sent: DS(chk): %"PRIu64"(%"PRIu64
//                 ") RA: %"PRIu64" CA: %"PRIu64" CSbyS: %"PRIu64"\n",
//                 stats_.total_sessions_, stats_.total_snt_ds_, stats_.total_snt_ds_checkpoints_,
//                 stats_.total_snt_ra_, stats_.total_snt_ca_, stats_.total_snt_css_);
//    buf->appendf("                 rcvd: RS: %"PRIu64" CSbyR: %"PRIu64" CA: %"PRIu64"\n",
//                 stats_.total_rcv_rs_, stats_.total_rcv_csr_, stats_.total_rcv_ca_);


//             Sessions     Sessns   Data Segments    Report Segments  RptAck  Cancel By Sendr  Cancel By Recvr  CanAck
//          Total /  Max    DS R-X   Total  / ReXmit   Total / ReXmit  Total    Total / ReXmit   Total / ReXmit  Total
//         ---------------  ------  ----------------  ---------------  ------  ---------------  ---------------  ------
//Sender   123456 / 123456  123456  1234567 / 123456  123456 / 123456  123456  123456 / 123456  123456 / 123456  123456
//Receiver 123456 / 123456  123456  1234567 / 123456  123456 / 123456  123456  123456 / 123456  123456 / 123456  123456

    buf->appendf("Sender   %6"PRIu64" / %6"PRIu64"  %6"PRIu64"  %7"PRIu64" / %6"PRIu64"  %6"PRIu64" / %6"PRIu64
                 "  %6"PRIu64"  %6"PRIu64" / %6"PRIu64"  %6"PRIu64" / %6"PRIu64"  %6"PRIu64"  %7"PRIu64"  %7"PRIu64"\n",
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_session_resends_, stats_.total_snt_ds_, stats_.ds_segment_resends_,
                 stats_.total_rcv_rs_, uint64_t(0), stats_.total_snt_ra_,
                 stats_.total_snt_css_, stats_.cs_session_resends_, stats_.total_rcv_csr_, uint64_t(0),
                 stats_.total_rcv_ca_,
                 stats_.bundles_success_, stats_.bundles_fail_);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send(LTPUDPSegment * send_segment,int parent_type)
{
    oasys::HexDumpBuffer hex;

    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_     = new std::string((char *) send_segment->Packet()->buf(), send_segment->Packet_Len());
        obj->session_      = send_segment->Session_Key();
        obj->segment_      = send_segment->Key();
        obj->segment_type_ = send_segment->Segment_Type();
        obj->parent_type_  = parent_type;
        obj->timer_        = send_segment->timer_;

        if (params_->hexdump_) {
            int dump_length = 20;
            if(send_segment->Packet_Len() < 20)dump_length = send_segment->Packet_Len();
            hex.append(send_segment->Packet()->buf(), dump_length);
            log_always("Send(segment): Buffer:%d type:%s", dump_length,LTPUDPSegment::Get_Segment_Type(send_segment->Segment_Type()));
		    log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }

	if (send_segment->IsRed()) {
	    high_eventq_->push_back(obj);
        } else {
	    high_eventq_->push_front(obj);
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type)
{
    if (!should_stop()) {

        MySendObject* obj = new MySendObject();

        obj->str_data_ = new std::string((char *) send_segment->Packet()->buf(), send_segment->Packet_Len());
	obj->session_      = send_segment->Session_Key();
	obj->segment_      = send_segment->Key();
	obj->segment_type_ = send_segment->Segment_Type();
	obj->parent_type_  = parent_type;
	obj->timer_        = send_segment->timer_;

	low_eventq_->push_back(obj);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_CS(LTPUDPCancelSegment * cs_segment)
{
    ++stats_.total_rcv_csr_;
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_CS");  

    SESS_MAP::iterator session_iterator = track_sender_session(cs_segment->Engine_ID(), cs_segment->Session_ID(),false);

    if(session_iterator != ltpudp_send_sessions_.end()) {
	LTPUDPCancelAckSegment * cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) cs_segment, 
									      LTPUDPSEGMENT_CAS_BR_BYTE, blank_session_); 

	log_debug("process_CS: Session: %s Send CAS and clean up", cs_segment->Session_Key().c_str());

	++stats_.total_snt_ca_;

	Send(cas_seg, PARENT_SENDER); 
	delete cas_seg;
	cleanup_sender_session(cs_segment->Session_Key(), cs_segment->Key(), cs_segment->Segment_Type());
    } else {
	log_debug("process_CS: Session: %s  - not found", cs_segment->Session_Key().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_CAS(LTPUDPCancelAckSegment * cas_segment)
{
    ++stats_.total_rcv_ca_;

    // lock session since we potentially alter sender_session 
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_CAS");
    SESS_MAP::iterator session_iterator = track_sender_session(cas_segment->Engine_ID(), cas_segment->Session_ID(),false);

    if(session_iterator != ltpudp_send_sessions_.end()) {
	LTPUDPSession * session = session_iterator->second;

	if (session->Cancel_Segment() != (LTPUDPCancelSegment *) NULL)
	    session->Cancel_Segment()->Cancel_Retransmission_Timer();

	log_debug("process_CAS: Session: %s - ok to delete", cas_segment->Session_Key().c_str());

	delete session;
	ltpudp_send_sessions_.erase(session_iterator);
    } else {
	log_debug("process_CS: Session: %s  - not found", cas_segment->Session_Key().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_RS(LTPUDPReportSegment * report_segment)
{
    ++stats_.total_rcv_rs_;

    // lock sessions for sender since we potentially change sender sessions MAP
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_RS");  

    SESS_MAP::iterator session_iterator = 
		 track_sender_session(report_segment->Engine_ID(), report_segment->Session_ID(),false);

    if(session_iterator == ltpudp_send_sessions_.end()) {
	log_debug("process_RS: Session: %s  - not found", 
		  report_segment->Session_Key().c_str());
	return;
    }

    report_segment->Set_Parent(parent_);

    log_debug("process_RS: Session: %s Send RAS and process RS", report_segment->Session_Key().c_str());

    LTPUDPReportAckSegment * ras = new LTPUDPReportAckSegment(report_segment, blank_session_);

    ++stats_.total_snt_ra_;

    Send(ras, PARENT_SENDER);
    delete ras;

    int checkpoint_removed = 0;

    if(session_iterator != ltpudp_send_sessions_.end()) {
    
	LTPUDPSession * session = session_iterator->second;

	RC_MAP::iterator loop_iterator;

	for (loop_iterator  = report_segment->Claims().begin(); 
	     loop_iterator != report_segment->Claims().end(); 
	     loop_iterator++) 
	{
	    LTPUDPReportClaim * rc = loop_iterator->second;     
	    checkpoint_removed = session->RemoveSegment(&session->Red_Segments(),
							//XXX/dz - ION 3.3.0 - multiple Report Segments if
							//  too many claims for one message (max = 20)
							// - other RS's have non-zero lower bound
							report_segment->LowerBounds()+rc->Offset(),
							rc->Length()); 
	}

	if (checkpoint_removed) {
	    Send_Remaining_Segments(session);
	}
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Remaining_Segments(LTPUDPSession * session)
{
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    LTPUDPDataSegment * segment;
    DS_MAP::iterator loop_iterator;

    if(session->Red_Segments().size() > 0)
    {
	log_debug("Send_Remaining_Segments: Session: %s - resending %zu segments",
		  session->Key().c_str(), session->Red_Segments().size());

	loop_iterator = --session->Red_Segments().end();
	segment       = loop_iterator->second;

	if(!segment->IsCheckpoint())
	{
	    log_debug("We have a new Checkpoint... incrementing Checkpoint");
	    segment->Set_Checkpoint_ID(session->Increment_Checkpoint_ID());
	    segment->SetCheckpoint();
	    segment->Encode_All();
            segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);

            log_debug("Send_Remaining_Segments: Session: %s - created new checkpoint: %"PRIu64,
                      session->Key().c_str(), segment->Checkpoint_ID());
        }

        ++stats_.ds_session_resends_;

        for(loop_iterator = session->Red_Segments().begin(); loop_iterator != session->Red_Segments().end(); loop_iterator++)
        {
            segment = loop_iterator->second;

            ++stats_.ds_segment_resends_;
            ++stats_.total_snt_ds_;
            if (segment->IsCheckpoint()) ++stats_.total_snt_ds_checkpoints_;

            Send(segment,PARENT_SENDER);
        }

    } else {
        log_debug("Send_Remaining_Segments: Session: %s - no segments to resend - clean up",
                  session->Key().c_str());

        cleanup_sender_session(session->Key(), session->Key(), LTPUDPSegment::LTPUDP_SEGMENT_COMPLETED);
    }
    return; 
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::reconfigure()
{
    // NOTE: params_ have already been updated with the new values by the parent

    if (params_->rate_ != 0) {
        if (NULL == rate_socket_) {
            rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params_->bucket_type_);
            rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        }

        // allow updating the rate and the bucket depth on the fly but not the bucket type
        // >> set rate to zero and then reconfigure the bucket type to change it

        rate_socket_->bucket()->set_rate(params_->rate_);

        if (params_->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params_->bucket_depth_);
        }
 
        log_debug("reconfigure: new rate controller values: rate %llu depth %llu",
                  U64FMT(rate_socket_->bucket()->rate()),
                  U64FMT(rate_socket_->bucket()->depth()));
    } else {
        if (NULL != rate_socket_) {
            delete rate_socket_;
            rate_socket_ = NULL;
        }
    }

    return true;
}

//----------------------------------------------------------------------
int
LTPUDPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr, u_int16_t next_hop_port)
{
    (void) next_hop_addr;
    (void) next_hop_port;

    LTPUDPSession* loaded_session = NULL;

    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    int total_len        = BundleProtocol::total_length(blocks);

#ifdef ECOS_ENABLED
    bool green           = bundle->ecos_enabled() && bundle->ecos_streaming();
#else
    bool green           = false;
#endif //ECOS_ENABLED

    if (green) {

        LTPUDPSession * green_session = new LTPUDPSession(params_->engine_id_, session_ctr_, parent_, PARENT_SENDER);
        green_session->SetAggTime();
        green_session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_DS);
        log_debug("Green Session Loading cipher suite into session from params");
        green_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
        green_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
        green_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);

        log_debug("send_bundle: Bundle ID: %"PRIbid" - Transmit immediately as Green Data",
                  bundle->bundleid()); 

        green_session->AddToBundleList(bundle, total_len);
        process_bundles(green_session, green);
        PostTransmitProcessing(green_session);
        delete green_session; // remove it after sent...
        session_ctr_++;  
        if (session_ctr_ > ULONG_MAX) {
            session_ctr_ = LTPUDP_14BIT_RAND;
        }

    } else {  // define scope for the ScopeLock

        // prevent run method from accessing the loading_session_ while working with it
        oasys::ScopeLock loadlock(&loading_session_lock_,"Sender::send_bundle");  

        if(loading_session_ == (LTPUDPSession *) NULL) {
            oasys::ScopeLock mylock(&session_lock_,"Sender::send_bundle");  // dz debug

            SESS_MAP::iterator session_iterator = track_sender_session(params_->engine_id_,session_ctr_,true);
            if(session_iterator == ltpudp_send_sessions_.end()) {
                log_debug("send_bundle: Nothing left to do so return....");
                return -1;
            }
            log_debug("Session created and loading_session is getting set");
            loading_session_ = session_iterator->second;
            session_ctr_++; 
            if (session_ctr_ > ULONG_MAX) {
                    session_ctr_ = LTPUDP_14BIT_RAND;
            }
        }
        // use the loading sesssion and add the bundle to it.
 
        loading_session_->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_DS);


        log_debug("send_bundle: Bundle ID: %"PRIbid" - add to Red Data - Session: %s",
                  bundle->bundleid(), loading_session_->Key().c_str()); 

        loading_session_->AddToBundleList(bundle, total_len);

        // check the aggregation size and time while we have the loading session
        if ((int) loading_session_->Session_Size() >= params_->agg_size_) {
            loaded_session = loading_session_;
            loading_session_ = NULL;
            log_debug("send_bundle: Session: %s - sending after aggregation size was exceeded...",
                      loaded_session->Key().c_str()); 
        } else if (loading_session_->TimeToSendIt(params_->agg_time_)) {
            loaded_session = loading_session_;
            loading_session_ = NULL;
            log_debug("send_bundle: Session: %s - sending after aggregation time was exceeded...",
                      loaded_session->Key().c_str()); 
        }
    }

    // do link housekeeping now that the lock is released
    contact_->link()->del_from_queue(bundle, total_len); 

    if (!green) {
        contact_->link()->add_to_inflight(bundle, total_len);
    }

    if (loaded_session != NULL && !green) {
        log_debug("Red Session Loading cipher suite into session from params");
        loaded_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
        loaded_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
        loaded_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);
        process_bundles(loaded_session, false);
    }

    return total_len;
}
//----------------------------------------------------------------------
int
LTPUDPConvergenceLayer::Sender::process_bundles(LTPUDPSession* loaded_session,bool is_green)
{
    int current_data     = 0;
    int segments_created = 0;
    int data_len         = loaded_session->Session_Size();
    int total_bytes      = loaded_session->Session_Size();
    int max_segment_size = params_->seg_size_;
    int bundle_ctr       = 0;
    int current_bundle   = 0;
    int client_service_id = 1;

    int extra_bytes = ((!params_->ion_comp_ && loaded_session->Bundle_List()->size() > 1) ? loaded_session->Bundle_List()->size() : 0);

    u_char * buffer = (u_char *) malloc(total_bytes+extra_bytes);
   
    Bundle * bundle = (Bundle *) NULL;
    BundleRef br("Sender::process_bundles()");  
    BundleList::iterator bundle_iter; 

    if(!params_->ion_comp_) {
        client_service_id = (extra_bytes > 0 ? 2 : 1);
    }

    oasys::ScopeLock bl(loaded_session->Bundle_List()->lock(),"process_bundles()"); 

    for(bundle_iter =  loaded_session->Bundle_List()->begin(); 
        bundle_iter != loaded_session->Bundle_List()->end(); 
        bundle_iter ++)
    {
        bool complete = false;

        bundle_ctr++;

        bundle = *bundle_iter;
        br     = bundle;

        BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());

        ASSERT(blocks != NULL);
   
        if (extra_bytes > 0) {
           *(buffer+current_bundle) = 1;
           current_bundle ++;
        }
        size_t total_len = BundleProtocol::produce(bundle, blocks,
                                                   buffer+current_bundle, 0, 
                                                   (size_t) total_bytes, &complete);
        if (!complete) {
            size_t formatted_len = BundleProtocol::total_length(blocks);
            log_err("process_bundle: bundle too big (%zu > %u) bundle_ctr:%d",
                    formatted_len, LTPUDPConvergenceLayer::MAX_BUNDLE_LEN,bundle_ctr);
            if(buffer != (u_char *) NULL)free(buffer);

            // signal the BundleDeamon that these bundles failed
            PostTransmitProcessing(loaded_session, false);

            return -1;
        }

        total_bytes    -= total_len;
        current_bundle += total_len;
    }

    data_len   = loaded_session->Session_Size()+extra_bytes;

    while( data_len > 0)
    {
        bool checkpoint = false;

        log_debug("Start of while Data_len = %d ",data_len);

        LTPUDPDataSegment * ds = new LTPUDPDataSegment(loaded_session);

        log_debug("After new DS from session is_secure=%d\n",ds->IsSecurityEnabled());

        segments_created ++;

        ds->Set_Client_Service_ID(client_service_id); 
        log_debug("DS setting client_service_id=%d\n",client_service_id);

        if(is_green)
            ds->Set_RGN(GREEN_SEGMENT);
        else
            ds->Set_RGN(RED_SEGMENT);

        log_debug(" After ds is created %d %d %s",ds->Cipher_Suite(),ds->Cipher_Key_Id(),ds->Cipher_Engine().c_str());

        //
        // always have header and trailer....  
        //
        //if(current_data == 0)
        //{
            ds->Set_Security_Mask(LTPUDP_SECURITY_HEADER | LTPUDP_SECURITY_TRAILER);
        //} else {
        //    ds->Set_Security_Mask(LTPUDP_SECURITY_TRAILER);
        //}
       /* 
        * see if it's a full packet or not
        */ 
        if(data_len  > max_segment_size) {
           ds->Encode_Buffer(&buffer[current_data], max_segment_size, current_data, checkpoint);
           data_len         -= max_segment_size;
           current_data     += max_segment_size; 
        } else {
           log_debug("building a segment with %d bytes",data_len);
           ds->Encode_Buffer(&buffer[current_data], data_len, current_data, true);
           data_len         -= data_len;
           current_data     += data_len; 
        }

        if(ds->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS) {

            // generating the new segments so no possible overlap
            loaded_session->AddSegment((LTPUDPDataSegment*) ds, false);  //false = no check for overlap
                
            if(!is_green) {
                if (ds->IsCheckpoint()) {
                    log_debug("process_bundles: Session %s - sending checkpoint DS: %s",
                              loaded_session->Key().c_str(), ds->Key().c_str());

                    ds->Set_Retrans_Retries(0);
                    ds->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                    LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);

                    ++stats_.total_snt_ds_checkpoints_;
                }
            }

            ++stats_.total_snt_ds_;
 
            log_debug("ds %s Start=%d Stop=%d",(is_green ? "GREEN" : "RED"), ds->Start_Byte(), ds->Stop_Byte());
            Send_Low_Priority(ds,PARENT_SENDER); 

        } else {
            log_crit("Not a DS... send_bundle");
        }
    }

    log_debug("process_bundles: Session %s - sent %d Data Segments - total bytes: %d",
              loaded_session->Key().c_str(), segments_created, current_bundle);

    if(buffer != (u_char *) NULL)free(buffer);
    return total_bytes; 
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Remaining_Segments(string session_key)
{ 
    SESS_MAP::iterator remaining_iterator = ltpudp_send_sessions_.find(session_key);

    if(remaining_iterator != ltpudp_send_sessions_.end())
    {
        LTPUDPSession * ltpsession = remaining_iterator->second;
        Send_Remaining_Segments(ltpsession); 
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::cleanup_sender_session(string session_key, string segment, 
                                                       int segment_type)
{ 
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    ASSERT(session_lock_.is_locked_by_me());

    LTPUDPDataSegment   * ds         = (LTPUDPDataSegment   *) NULL; 
    LTPUDPCancelSegment * cs         = (LTPUDPCancelSegment *) NULL; 
    LTPUDPSession       * ltpsession = (LTPUDPSession       *) NULL;

    DS_MAP::iterator   cleanup_segment_iterator;

    SESS_MAP::iterator cleanup_session_iterator = ltpudp_send_sessions_.find(session_key);

    if(cleanup_session_iterator != ltpudp_send_sessions_.end())
    {
        log_debug("Cleanup Session:Session Found (%s)",session_key.c_str());

        ltpsession = cleanup_session_iterator->second;

        ltpsession->Cancel_Inactivity_Timer();

        // lookup the segment and resubmit if it was a timeout
        switch(segment_type)
        { 
            case LTPUDPSegment::LTPUDP_SEGMENT_DS_TO:
            case LTPUDPSegment::LTPUDP_SEGMENT_RS_TO:
                 cleanup_segment_iterator = ltpsession->Red_Segments().find(segment);
                 if(cleanup_segment_iterator != ltpsession->Red_Segments().end())
                 {
                     ds = cleanup_segment_iterator->second;
                     ds->Cancel_Retransmission_Timer();

                     ds->Increment_Retries();
                     if (ds->Retrans_Retries() <= params_->retran_retries_) {
                         log_debug("cleanup_sender_session(DS_TO): Session: %s - Retransmit Data Segment (Checkpoint)",
                                  session_key.c_str());

                         ++stats_.ds_session_checkpoint_resends_;
                         ltpsession->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_DS);
                         Resend_Checkpoint(ltpsession);
                     } else {
                         log_debug("cleanup_sender_session(DS_TO): Session: %s - Retransmit retries exceeded - Cancel by Sender",
                                  session_key.c_str());

                         build_CS_segment(ltpsession,LTPUDPSegment::LTPUDP_SEGMENT_CS_BS, 
                                          LTPUDPCancelSegment::LTPUDP_CS_REASON_RXMTCYCEX);

                         //signal DTN that transmit failed for the bundle(s)
                         PostTransmitProcessing(ltpsession, false);
                     }
                 } else {
                     log_debug("cleanup_sender_session(DS_TO): Session: %s - Segment not found: %s",
                              session_key.c_str(), segment.c_str());

                 }
                 return;
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_TO:
                 cs = ltpsession->Cancel_Segment();
                 cs->Cancel_Retransmission_Timer();
                 cs->Increment_Retries();

                 if (cs->Retrans_Retries() <= params_->retran_retries_) {
                     log_debug("cleanup_sender_session(CS_TO): Session: %s - Retransmit Cancel Segment by Sender",
                               session_key.c_str());

                     cs->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                     LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

                     ++stats_.cs_session_resends_;

                     Send(cs,PARENT_SENDER);
                     return; 
                 } else {
                     log_debug("cleanup_sender_session(CS_TO): Session: %s - Retransmit retries exceeded - delete session",
                               session_key.c_str());
                 }
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
                 ++stats_.cs_session_cancel_by_receiver_;
                 log_debug("cleanup_sender_session(CS_BR): Session: %s - Cancel by Receiver received - delete session",
                           session_key.c_str());

                 // signal DTN that transmit failed for the bundle(s)
                 PostTransmitProcessing(ltpsession, false);
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_COMPLETED: 
                 log_debug("cleanup_sender_session(COMPLETED): Session: %s - delete session",
                           session_key.c_str());
                 PostTransmitProcessing(ltpsession);
                 break;

            default: // all other cases don't lookup a segment...
                 
                 log_debug("default Switch Sender %d",segment_type);
                 return;
                 break;
        }

        delete ltpsession;
        ltpudp_send_sessions_.erase(cleanup_session_iterator);

    } else {
        log_debug("cleanup_sender_session(CS_BR): Session: %s - not found",
                  session_key.c_str());
    }

    return;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Resend_Checkpoint(string session_key)
{
    SESS_MAP::iterator session_iterator = ltpudp_send_sessions_.find(session_key);
    if(session_iterator != ltpudp_send_sessions_.end())
    {
        LTPUDPSession * session = session_iterator->second;
        Resend_Checkpoint(session);
    } else {
        log_debug("Resend_Checkpoint: Session: %s - not found", session_key.c_str());
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Resend_Checkpoint(LTPUDPSession * session)
{
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    DS_MAP::iterator ltpudp_segment_iterator = --session->Red_Segments().end(); // get the last one!

    if(ltpudp_segment_iterator != session->Red_Segments().end()) {
        LTPUDPDataSegment * segment = ltpudp_segment_iterator->second; 
        if(!segment->IsCheckpoint())
        {
            segment->Set_Report_Serial_Number(session->Increment_Report_Serial_Number());
            if(session->Report_Serial_Number() > ULONG_MAX)
            {
                build_CS_segment(session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BS, 
                                 LTPUDPCancelSegment::LTPUDP_CS_REASON_SYS_CNCLD);
                session->Set_Report_Serial_Number(LTPUDP_14BIT_RAND); // restart the random ctr
                return;
            }
            segment->Set_Checkpoint_ID(session->Increment_Checkpoint_ID());
            segment->SetCheckpoint(); // turn on the checkpoint bits in the control byte
            segment->Encode_All();
            segment->Set_Retrans_Retries(0);
            log_debug("Resend_Checkpoint: Session: %s - create and send new checkpoint",
                      session->Key().c_str());
        } else {
            log_debug("Resend_Checkpoint: Session: %s - resending existing checkpoint",
                      session->Key().c_str());
        }

        segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                             LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);

        ++stats_.total_snt_ds_;
        ++stats_.total_snt_ds_checkpoints_;

        Send(segment,PARENT_SENDER);
    } else {
        log_debug("Resend_Checkpoint: Session: %s - no Red Data Segments",
                  session->Key().c_str());
    }
}

//----------------------------------------------------------------------
SESS_MAP::iterator 
LTPUDPConvergenceLayer::Sender::track_sender_session(u_int64_t engine_id,u_int64_t session_ctr,bool create_flag)
{
    SESS_MAP::iterator session_iterator;
    char key[45];
    sprintf(key,"%20.20"PRIu64":%20.20"PRIu64,engine_id,session_ctr);
    string insert_key = key;

    session_iterator = ltpudp_send_sessions_.find(insert_key);

    if(session_iterator == ltpudp_send_sessions_.end())
    {
        if (create_flag) {
            LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_ctr, parent_, PARENT_SENDER);

            new_session->SetAggTime();
            new_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
            new_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
            new_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);

            ltpudp_send_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
            session_iterator = ltpudp_send_sessions_.find(insert_key);

            if (ltpudp_send_sessions_.size() > stats_.max_sessions_) {
                stats_.max_sessions_ = ltpudp_send_sessions_.size();
            }
            ++stats_.total_sessions_;
        } else {
            log_debug("track_sender_session: Session: %s - not found", key);
        }
    } else {
       log_debug("track_sender_session: Session: %s - found", key);
    }
    
    return session_iterator;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::dump_link(oasys::StringBuffer* buf)
{
    if (NULL == rate_socket_) {
        buf->appendf("transmit rate: unlimited\n");
    } else {
        buf->appendf("transmit rate: %"PRIu64"\n", rate_socket_->bucket()->rate());
        if (0 == params_->bucket_type_) 
            buf->appendf("bucket_type: Standard\n");
        else
            buf->appendf("bucket_type: Leaky\n");
        buf->appendf("bucket_depth: %"PRIu64"\n", rate_socket_->bucket()->depth());
        buf->appendf("wait_till_sent: %d\n",(int) params_->wait_till_sent_);
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::check_ready_for_bundle()
{
    bool result = false;
    if(loading_session_ != NULL || ltpudp_send_sessions_.size() < params_->max_sessions_) {
        result = parent_->pop_queued_bundle(contact_->link());
    }
    return result;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::SendPoller(Sender* parent, const LinkRef& link)
    : Logger("LTPUDPConvergenceLayer::Sender::SendPoller",
             "/dtn/cl/ltpudp/sender/poller/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender::SendPoller", Thread::DELETE_ON_EXIT)
{
    parent_ = parent;
    link_ = link;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::~SendPoller()
{
    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SendPoller::run()
{
    while (!should_stop()) {
        if (!parent_->check_ready_for_bundle()) {
            log_debug("Sender::SendPoller::run() sleeping....");
            usleep(1000);
        }
    }        
}

LTPUDPConvergenceLayer::TimerProcessor::TimerProcessor()
    : Thread("LTPUDPConvergenceLayer::TimerProcessor"),
      Logger("LTPUDPConvergenceLayer::TimerProcessor",
             "/dtn/cl/ltpudp/timerproc/%p", this)
{
    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< oasys::Timer* >(logpath_);
    start();
}

LTPUDPConvergenceLayer::TimerProcessor::~TimerProcessor()
{
    // free all pending events
    oasys::Timer* event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}

/// Post a Timer to trigger
void
LTPUDPConvergenceLayer::TimerProcessor::post(oasys::Timer* event)
{
    eventq_->push_back(event);
}

/// TimerProcessor main loop
void
LTPUDPConvergenceLayer::TimerProcessor::run() 
{
    struct timeval dummy_time;
    ::gettimeofday(&dummy_time, 0);

    // block on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (!should_stop()) {
        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

        if (ret == oasys::IOINTR) {
            log_err("timer processor interrupted");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            oasys::Timer* event;
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                event->timeout(dummy_time);
                // NOTE: these timers delete themselves
            }
        }
    }
}

} // namespace dtn

#endif // LTPUDP_ENABLED
