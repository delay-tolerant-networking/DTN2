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
#  include <config.h>
#endif

#include <math.h>

#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>

#include "TrAgent.h"
#include "Simulator.h"
#include "Node.h"
#include "SimEvent.h"
#include "SimLog.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"

namespace dtnsim {

//----------------------------------------------------------------------
TrAgent::TrAgent(const EndpointID& src, const EndpointID& dst)
    : Logger("TrAgent", "/sim/tragent/%s", Node::active_node()->name()),
      src_(src), dst_(dst),
      size_(0), expiration_(30), reps_(0), batch_(1), interval_(0)
{
}

//----------------------------------------------------------------------
TrAgent*
TrAgent::init(const EndpointID& src, const EndpointID& dst,
              int argc, const char** argv)
{
    TrAgent* a = new TrAgent(src, dst);

    oasys::OptParser p;
    p.addopt(new oasys::UIntOpt("size", &a->size_));
    p.addopt(new oasys::UIntOpt("expiration", &a->expiration_));
    p.addopt(new oasys::UIntOpt("reps", &a->reps_));
    p.addopt(new oasys::UIntOpt("batch", &a->batch_));
    p.addopt(new oasys::DoubleOpt("interval", &a->interval_));

    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        a->logf(oasys::LOG_ERR, "invalid option: %s", invalid);
        return NULL;
    }

    if (a->size_ == 0) {
        a->logf(oasys::LOG_ERR, "size must be set in configuration");
        return NULL;
    }

    if (a->reps_ == 0) {
        a->logf(oasys::LOG_ERR, "reps must be set in configuration");
        return NULL;
    }

    if (a->reps_ != 1 && a->interval_ == 0) {
        a->logf(oasys::LOG_ERR, "interval must be set in configuration");
        return NULL;
    }

    a->schedule_immediate();
    return a;
}

//----------------------------------------------------------------------
void
TrAgent::timeout(const struct timeval& /* now */)
{
    for (u_int i = 0; i < batch_; i++) {
        send_bundle();
    }
        
    if (--reps_ > 0) {
        log_debug("scheduling timer in %u ms", (u_int)(interval_ * 1000));
        schedule_in((int)(interval_ * 1000));
    } else {
        log_debug("all batches finished");
    }
}

//----------------------------------------------------------------------
void
TrAgent::send_bundle()
{
    Bundle* b = new Bundle(BundlePayload::NODATA);
        
    //oasys::StaticStringBuffer<1024> buf;
    //b->format_verbose(&buf);
    //log_multiline(oasys::LOG_DEBUG, buf.c_str());
        
    b->source_.assign(src_);
    b->replyto_.assign(src_);
    b->custodian_.assign(EndpointID::NULL_EID());
    b->dest_.assign(dst_);
    b->payload_.set_length(size_);
        
    b->priority_ = 0;
    b->custody_requested_ = false;
    b->local_custody_ = false;
    b->singleton_dest_ = false;
    b->receive_rcpt_ = false;
    b->custody_rcpt_ = false;
    b->forward_rcpt_ = false;
    b->delivery_rcpt_ = false;
    b->deletion_rcpt_ = false;
    b->app_acked_rcpt_ = false;
    b->creation_ts_.seconds_ = BundleTimestamp::get_current_time();
    b->creation_ts_.seqno_ = b->bundleid_;
    b->expiration_ = expiration_;
    b->is_fragment_	= false;
    b->is_admin_ = false;
    b->do_not_fragment_ = false;
    b->in_datastore_ = false;
    //b->orig_length_   = 0;
    //b->frag_offset_   = 0;    
    
    log_info("N[%s]: GEN id:%d %s -> %s size:%d",
             Node::active_node()->name(), b->bundleid_,
             src_.c_str(), dst_.c_str(), size_);

    SimLog::instance()->log_gen(Node::active_node(), b);
		
    BundleDaemon::post(new BundleReceivedEvent(b, EVENTSRC_APP, size_));
}


} // namespace dtnsim
