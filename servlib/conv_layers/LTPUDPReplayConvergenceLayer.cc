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


#include <time.h>
#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <naming/EndpointID.h>
#include <naming/IPNScheme.h>
#include <oasys/util/HexDumpBuffer.h>

#include "LTPUDPReplayConvergenceLayer.h"
#include "LTPUDPCommon.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/SDNV.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

struct LTPUDPReplayConvergenceLayer::Params LTPUDPReplayConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = 0;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("ttl", &ttl_);
    a->process("dest", &dest_pattern_);
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("inact_intvl", &inactivity_intvl_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("wait_till_sent", &wait_till_sent_);
    a->process("icipher_suite", &inbound_cipher_suite_);
    a->process("icipher_keyid", &inbound_cipher_key_id_);
    a->process("icipher_engine",  &inbound_cipher_engine_);

}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::LTPUDPReplayConvergenceLayer()
    : IPConvergenceLayer("LTPUDPReplayConvergenceLayer", "ltpudpreplay")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPUDPREPLAYCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.bucket_type_              = (oasys::RateLimitedSocket::BUCKET_TYPE) 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.bucket_depth_             = 0; // default
    defaults_.wait_till_sent_           = false;
    defaults_.ttl_                      = 259200;
    defaults_.inactivity_intvl_         = 3600;
    defaults_.dest_pattern_             = EndpointID();
    defaults_.no_filter_                = true;
    defaults_.retran_intvl_             = 60;
    defaults_.retran_retries_           = 0;
    defaults_.inbound_cipher_suite_     = -1;
    defaults_.inbound_cipher_key_id_    = -1;
    defaults_.inbound_cipher_engine_    = "";

    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;

    timer_processor_                    = new TimerProcessor();
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::~LTPUDPReplayConvergenceLayer()
{
    if (timer_processor_ != NULL) {
        timer_processor_->set_should_stop();
    }
}
//----------------------------------------------------------------------
CLInfo*
LTPUDPReplayConvergenceLayer::new_link_params()
{
    return new LTPUDPReplayConvergenceLayer::Params(LTPUDPReplayConvergenceLayer::defaults_);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&LTPUDPReplayConvergenceLayer::defaults_, argc, argv, invalidp);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;
    int temp                     = 0;
    string      icipher_eng_str  = "";
    std::string dest_filter_str  = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::IntOpt("bucket_type", &temp));
    p.addopt(new oasys::IntOpt("retran_retries", &params->retran_retries_));
    p.addopt(new oasys::IntOpt("retran_intvl", &params->retran_intvl_));
    p.addopt(new oasys::UInt64Opt("ttl", &params->ttl_));
    p.addopt(new oasys::UIntOpt("rate", &params->rate_));
    p.addopt(new oasys::UIntOpt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::BoolOpt("wait_till_sent",&params->wait_till_sent_));
    p.addopt(new oasys::UIntOpt("inact_intvl", &params->inactivity_intvl_));
    p.addopt(new oasys::StringOpt("dest", &dest_filter_str));
    p.addopt(new oasys::IntOpt("icipher_suite", &params->inbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("icipher_keyid", &params->inbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("icipher_engine", &icipher_eng_str));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    if(icipher_eng_str.length() > 0) {
        params->inbound_cipher_engine_ = icipher_eng_str;
    }

    if(dest_filter_str.length() > 0)
    {
        params->no_filter_ = false;
        params->dest_pattern_.assign(dest_filter_str);
    }

    params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;

    return true;
};
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPUDPReplayConvergenceLayer::defaults_;
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
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
   
    receiver_ = receiver;
 
    // start the thread which automatically listens for data
    receiver->start();
    
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::interface_down(Interface* iface)
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
LTPUDPReplayConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Receiver*)iface->cl_info())->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("\tttl: %ld\n", params->ttl_);
    buf->appendf("\tinactivity_intvl: %d\n", params->inactivity_intvl_);
    buf->appendf("\tdest: %s\n", params->dest_pattern_.c_str());
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected\n");
    }
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::init_link(const LinkRef& link,
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

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }
    session_params_ = *params;

    link->set_cl_info(params);

    link_ref_ = link;

    return true;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPUDPReplayConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_),  params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
    buf->appendf("inact_intvl: %u\n",         params->inactivity_intvl_);
    buf->appendf("rate: %u\n",                params->rate_);
    buf->appendf("bucket_depth: %u\n",        params->bucket_depth_);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::open_contact(const ContactRef& contact)
{
//    in_addr_t addr;
//    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    log_debug("LTPUDPReplayConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());

//------------------------------------- Replay doesn't need this stuff
//    
//    // parse out the address / port from the nexthop address
//    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
//        log_err("invalid next hop address '%s'", link->nexthop());
//        return false;
//    }
//
//    // make sure it's really a valid address
//    if (addr == INADDR_ANY || addr == INADDR_NONE) {
//        log_err("can't lookup hostname in next hop address '%s'",
//                link->nexthop());
//        return false;
//    }
//
//    // if the port wasn't specified, use the default
//    if (port == 0) {
//        port = LTPUDPREPLAYCL_DEFAULT_PORT;
//    }
//
//    next_hop_addr_  = addr;
//    next_hop_port_  = port;
//    next_hop_flags_ = MSG_DONTWAIT;
//
//-----------------------------------------------------------------

      Params* params = (Params*)link->cl_info();

//-----------------------------------------------------------------
//    
//    // create a new sender structure
//    Sender* sender = new Sender(link->contact());
//
//    if (!sender->init(params, addr, port)) {
//        log_err("error initializing contact");
//        BundleDaemon::post(
//            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
//                                       ContactEvent::NO_INFO));
//        delete sender;
//        return false;
//    }
//        
//    contact->set_cl_info(sender);
//    log_crit("LTPUDPReplayCL.. Adding ContactUpEvent ");
//    BundleDaemon::post(new ContactUpEvent(link->contact()));
//
//------------------------------------- Replay doesn't need this stuff

    // XXX/demmer should this assert that there's nothing on the link
    // queue??

    log_debug("adding link %s", link->name());
    
    // check that the local interface / port are valid
    if (params->local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params->local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }
    
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(params,this);
    receiver->logpathf("%s/link/%s", logpath_, link->name());
    
    if (receiver->bind(params->local_addr_, params->local_port_) != 0) {
        return false; // error log already emitted
    }
//    
// check if the user specified a remote addr/port to connect to
//
//    if (params->remote_addr_ != INADDR_NONE) {
//        if (receiver->connect(params->remote_addr_, params->remote_port_) != 0) {
//            return false; // error log already emitted
//        }
//    }
//   
    receiver_ = receiver;

    // start the thread which automatically listens for data
    receiver->start();



    
    return true;
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();
    
    log_info("close_contact *%p", contact.object());

    if (sender) {
        delete sender;
        contact->set_cl_info(NULL);
    }
    
    return true;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    log_debug("bundle_queued called");
    log_crit(" Bundle ref %s\n",bundle->dest().c_str());   
     
    const ContactRef& contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    BundleRef lbundle = link->queue()->front();
    ASSERT(contact == sender->contact_);

    int len = sender->send_bundle(lbundle, next_hop_addr_, next_hop_port_);

    if (len > 0) {
        link->del_from_queue(lbundle, len);
        link->add_to_inflight(lbundle, len);
        BundleDaemon::post(
            new BundleTransmittedEvent(lbundle.object(), contact, link, len, 0));
    }
}
//----------------------------------------------------------------------
u_int32_t
LTPUDPReplayConvergenceLayer::Inactivity_Interval()
{
     return session_params_.inactivity_intvl_;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Cleanup_Session_Receiver(string session_key,int force)
{
    receiver_->Cleanup_Session(session_key,force);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Post_Timer_To_Process(oasys::Timer* event)
{
    timer_processor_->post(event);
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::Receiver::Receiver(LTPUDPReplayConvergenceLayer::Params* params, LTPUDPCallbackIF * parent)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/udp/receiver")),
      UDPClient("/dtn/cl/ltpudpreplay/receiver"),
      Thread("LTPUDPReplayConvergenceLayer::Receiver"),
      rcvr_lock_()
{
    logfd_     = false;
    params_    = *params;
    parent_    = parent;
}
//----------------------------------------------------------------------
void 
LTPUDPReplayConvergenceLayer::Receiver::track_session(u_int64_t engine_id, u_int64_t session_id)
{
    char key[45];
    sprintf(key,"%20.20ld:%20.20ld",engine_id,session_id);
    string insert_key = key;
    ltpudpreplay_session_iterator_ = ltpudpreplay_sessions_.find(insert_key);
    if(ltpudpreplay_session_iterator_ == ltpudpreplay_sessions_.end())
    {
        LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_id, parent_, PARENT_RECEIVER);
        ltpudpreplay_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
        ltpudpreplay_session_iterator_ = ltpudpreplay_sessions_.find(insert_key);
    }
    return;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::cleanup_session(string session_key, int force)
{ // lock should've occured before this routine is hit

    SESS_MAP::iterator cleanup_iterator = ltpudpreplay_sessions_.find(session_key);

    if(cleanup_iterator != ltpudpreplay_sessions_.end())
    {
        if(!force)
        {
            LTPUDPSession * ltpsession = cleanup_iterator->second;
            time_t time_left = ltpsession->Last_Packet_Time() + (params_.inactivity_intvl_);
            time_left = time_left - time(NULL);
            if(time_left > 0)
            {
                log_debug("Trying to restart the timer to %d\n",(int) time_left);
                ltpsession->Start_Inactivity_Timer(time_left);
                return;
            }
        } 

        LTPUDPSession * ltpsession = cleanup_iterator->second;
        ltpsession->Cancel_Inactivity_Timer();
        delete ltpsession;   // calls the session destructor that calls the segments destructor...
        ltpudpreplay_sessions_.erase(cleanup_iterator); // cleanup session!!!
    }
    return;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    int bytes_to_process = 0; 

    log_debug("process_data: about to decode data... (%d)",(int) len);

    LTPUDPSession * ltpudpreplay_session;

    LTPUDPSegment * new_segment = LTPUDPSegment::DecodeBuffer(bp,len);
  
    if(new_segment == NULL)return; // didn't find a valid segment

    if (new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_DS     && 
        new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BS  &&   
        new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BR) { // clean up session if a cancel flag is seen...
       log_debug("process_data: not a DS or CS_BS or CS_BR... ");
       delete new_segment;
       return;
    }

#ifdef LTPUDP_AUTH_ENABLED
    if(new_segment->IsSecurityEnabled()) {
        if(!new_segment->IsAuthenticated(params_.inbound_cipher_suite_, 
                                         params_.inbound_cipher_key_id_,
                                         params_.inbound_cipher_engine_)) { 
            log_warn("process_data: invalid segment doesn't authenticate.... type:%s length:%d",
                 LTPUDPSegment::Get_Segment_Type(new_segment->Segment_Type()),(int) len);
            delete new_segment;
            return;
        }
    }
#endif
    log_debug("process_data: Good segment DS or CS_BS or CS_BR... ");
    {
        oasys::ScopeLock l(&rcvr_lock_,"RCVRprocess_data");  // lock here since we reuse something set in track session!!!

        track_session(new_segment->Engine_ID(),new_segment->Session_ID());  // this guy sets the ltpudpreplay_session_iterator...

        if (new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BS || 
            new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR)  // clean up session if a cancel flag is seen...
        {
            delete new_segment;
            ltpudpreplay_session->Set_Cleanup();
            cleanup_session(ltpudpreplay_session_iterator_->first, 1);
            return;
        } 

        // create a map entry unless one exists and add the segment to either red or green segments
        // we know it's a data segment....
        // create key for segment  (Range of numbers)????  will be done base off new_segment
    
        if(new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS) { // only handle data segments right now!!!
   
            if(new_segment->Service_ID() != 1 &&
               new_segment->Service_ID() != 2)
            {
                 log_debug("Service ID != 1 | 2 (%d)",new_segment->Service_ID());
                 delete new_segment;
                 return;
            }
 
            ltpudpreplay_session = ltpudpreplay_session_iterator_->second;
    
            ltpudpreplay_session->AddSegment((LTPUDPDataSegment*)new_segment);
    
            if(new_segment->IsCheckpoint()) {
                log_debug("Checkpoint found ... ");
                ltpudpreplay_session->Set_EOB((LTPUDPDataSegment*)new_segment);
            }
    
            bytes_to_process = ltpudpreplay_session->IsRedFull();
            if(bytes_to_process > 0) {
                log_debug("process_data: red full and about to process ");
                process_all_data(ltpudpreplay_session->GetAllRedData(),
                                 (size_t) ltpudpreplay_session->Expected_Red_Bytes(), 
                                 new_segment->Service_ID() == 2 ? true : false);
            } else {
                log_debug("process_data: red NOT full and about to process ");
            }
    
            bytes_to_process = ltpudpreplay_session->IsGreenFull();
            if(bytes_to_process > 0) {
                log_debug("process_data: green full and about to process ");
                process_all_data(ltpudpreplay_session->GetAllGreenData(),
                                 (size_t)ltpudpreplay_session->Expected_Green_Bytes(), 
                                 new_segment->Service_ID() == 2 ? true : false);
            } else {
                log_debug("process_data: green NOT full and about to process ");
            }
    
            if(ltpudpreplay_session->DataProcessed())
            {
                log_debug("process_data:  Cleaning up session ..");
                ltpudpreplay_session->Set_Cleanup();
                cleanup_session(ltpudpreplay_session_iterator_->first,1);
            }
        }
    }
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::process_all_data(u_char * bp, size_t len, bool multi_bundle)
{
    u_int32_t check_client_service = 0;
    // the payload should contain a full bundle
    int all_data = (int) len;
    u_char * current_bp = bp;

    if(bp == (u_char *) NULL)
    {
        log_err("process_all_data: no LTPUDPReplay bundle protocol error");
    }

    log_debug("process all data %d bytes\n",all_data);

    oasys::HexDumpBuffer hex;
    hex.append(bp, len);
    log_always("process_data: Buffer (%d):",(int) len);
    log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());

    while(all_data > 0)
    {
        Bundle* bundle = new Bundle();
   

        if (multi_bundle) {
            log_debug("extract_bundles_from_data: client_service (%02X)",*current_bp);
            // Multi bundle buffer... must make sure a 1 precedes all packets
            int bytes_used = SDNV::decode(current_bp, all_data, &check_client_service);
            if(check_client_service != 1)
            {
                log_err("process_all_data: LTPReplay SDA - invalid Service ID: %d",check_client_service);
                delete bundle;
                break; // so we free the bp
            }
            current_bp += bytes_used;
            all_data   -= bytes_used;
        }
 
        bool complete = false;
        int cc = BundleProtocol::consume(bundle, current_bp, len, &complete);

        if (cc < 0) {
            log_err("process_all_data: bundle protocol error");
            delete bundle;
            break;
        }

        if (!complete) {
            log_err("process_all_data: incomplete bundle");
            delete bundle;
            break;
        }

        // add filter for destination and adjust lifetime data based on params
        all_data   -= cc;
        current_bp += cc;

        log_debug("Checking %s matching %s\n",params_.dest_pattern_.c_str(), bundle->dest().c_str());

        if(params_.no_filter_ || params_.dest_pattern_.match(bundle->dest())) {

            int now =  BundleTimestamp::get_current_time();
            bundle->set_expiration((now-bundle->creation_ts().seconds_)+params_.ttl_); // update expiration time

            log_debug("process_all_data: new bundle id %"PRIbid" arrival, length %zu (payload %zu)",
                      bundle->bundleid(), len, bundle->payload().length());
    
            BundleDaemon::post(
                new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, EndpointID::NULL_EID()));
        }
    }
    if(bp != (u_char *) NULL)free(bp);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::run()
{
    int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char buf[MAX_UDP_PACKET];

    while (1) {

        if (should_stop())break;
        
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

//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::Sender::Sender(const ContactRef& contact)
    : Logger("LTPUDPReplayConvergenceLayer::Sender",
             "/dtn/cl/udp/sender/%p", this),
      socket_(logpath_),
      rate_socket_(0),
      contact_(contact.object(), "LTPUDPReplayCovergenceLayer::Sender")
{
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::Sender::~Sender()
{
    if (rate_socket_) {
        delete rate_socket_;
    }
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::Sender::init(Params* params,
                                  in_addr_t addr, u_int16_t port)
{
    log_debug("initializing sender");

    params_ = params;
    
    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);

    if (params->local_addr_ != INADDR_NONE || params->local_port_ != 0)
    {
        if (socket_.bind(params->local_addr_, params->local_port_) != 0) {
            log_err("error binding to %s:%d: %s",
                    intoa(params->local_addr_), params->local_port_,
                    strerror(errno));
            return false;
        }
    }
   
    socket_.init_socket();
    
//    if (socket_.connect(addr, port) != 0) 
//    {
//        log_err("error issuing udp init_socket to %s:%d: %s",
//                intoa(addr), port, strerror(errno));
//        return false;
//    }

    if (params->rate_ != 0) {

        rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params->bucket_type_);
        rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        rate_socket_->bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params->bucket_depth_);
        }
 
        log_debug("initialized rate controller: rate %llu depth %llu",
                  U64FMT(rate_socket_->bucket()->rate()),
                  U64FMT(rate_socket_->bucket()->depth()));
    }

    return true;
}
//----------------------------------------------------------------------
int
LTPUDPReplayConvergenceLayer::Sender::send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr_, u_int16_t next_hop_port_)
{
    int cc = 0;
    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);

    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle.object(), blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, LTPUDPReplayConvergenceLayer::MAX_BUNDLE_LEN);
        return -1;
    }
        
    // write it out the socket and make sure we wrote it all
    if(params_->rate_ > 0) {
        log_crit("about to call rate_socket_.sendto");
        cc = rate_socket_->sendto((char*)buf_, total_len, 0, next_hop_addr_, next_hop_port_, params_->wait_till_sent_);
    } else {
        cc = socket_.sendto((char*)buf_, total_len, 0, next_hop_addr_, next_hop_port_);
    }

    //int cc = socket_.write((char*)buf_, total_len);
    if (cc == (int)total_len) {
        log_info("send_bundle: successfully sent bundle (id:%"PRIbid") length %d", 
                 bundle.object()->bundleid(), cc);
        return total_len;
    } else {
        log_err("send_bundle: error sending bundle (id:%"PRIbid") (wrote %d/%zu): %s",
                bundle.object()->bundleid(), 
                cc, total_len, strerror(errno));
        return -1;
    }
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::TimerProcessor::TimerProcessor()
    : Thread("LTPUDPReplayConvergenceLayer::TimerProcessor"),
      Logger("LTPUDPReplayConvergenceLayer::TimerProcessor",
             "/dtn/cl/ltpudp/timerproc/%p", this)
{
    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< oasys::Timer* >(logpath_);
    start();
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::TimerProcessor::~TimerProcessor()
{
    // free all pending events
    oasys::Timer* event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}
/// Post a Timer to trigger
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::TimerProcessor::post(oasys::Timer* event)
{
    eventq_->push_back(event);
}
/// TimerProcessor main loop
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::TimerProcessor::run() 
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
            log_debug("timer processor interrupted");
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
