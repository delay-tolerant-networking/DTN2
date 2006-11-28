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


#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>

#include "TrAgent.h"
#include "Simulator.h"
#include "Node.h"
#include "SimEvent.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"

namespace dtnsim {

TrAgent::TrAgent(Node* node, const EndpointID& src, const EndpointID& dst)
    : Logger("TrAgent", "/sim/tragent/%s", node->name()),
      node_(node), src_(src), dst_(dst),
      size_(0), reps_(0), batch_(1), interval_(0)
{
}

TrAgent*
TrAgent::init(Node* node, double start_time, 
              const EndpointID& src, const EndpointID& dst,
              int argc, const char** argv)
{
    TrAgent* a = new TrAgent(node, src, dst);

    oasys::OptParser p;
    p.addopt(new oasys::IntOpt("size", &a->size_));
    p.addopt(new oasys::IntOpt("reps", &a->reps_));
    p.addopt(new oasys::IntOpt("batch", &a->batch_));
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

    if (a->interval_ == 0) {
        a->logf(oasys::LOG_ERR, "interval must be set in configuration");
        return NULL;
    }

    Simulator::post(new SimEvent(SIM_NEXT_SENDTIME, start_time, a));
    return a;
}

void
TrAgent::process(SimEvent* e)
{
    if (e->type() == SIM_NEXT_SENDTIME) {
        for (int i = 0; i < batch_; i++) {
            send_bundle();
        }
        
        if (--reps_ > 0) {
            double sendtime = Simulator::time() + interval_;
            Simulator::post(new SimEvent(SIM_NEXT_SENDTIME, sendtime, this));
        } else {
            log_debug("all batches finished");
        }

    } else {
        PANIC("unhandlable event %s", e->type_str());
    }
}

void
TrAgent::send_bundle()
{
    Bundle* b = new Bundle(oasys::Builder::builder());
	
    //oasys::StaticStringBuffer<1024> buf;
    //b->format_verbose(&buf);
    //log_multiline(oasys::LOG_DEBUG, buf.c_str());
	
    b->bundleid_ = node_->next_bundleid();
    b->payload_.init(b->bundleid_, BundlePayload::NODATA);
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
    b->expiration_ = 30;	
    b->is_fragment_	= false;
    b->is_admin_ = false;
    b->do_not_fragment_	= false;
    b->in_datastore_ = false;
    //b->orig_length_	= 0;
    //b->frag_offset_	= 0;
  	
	
    log_info("N[%s]: GEN id:%d %s -> %s size:%d",
             node_->name(), b->bundleid_, src_.c_str(), dst_.c_str(), size_);	
		
    log_debug("Posting(new SimRouterEvent(%f,%s,BundleReceivedEvent)",
              Simulator::time(), node_->name());	
    
    BundleReceivedEvent* e = new BundleReceivedEvent(b, EVENTSRC_APP, size_);
    Simulator::post(new SimRouterEvent(Simulator::time(), node_, e));
}


} // namespace dtnsim
