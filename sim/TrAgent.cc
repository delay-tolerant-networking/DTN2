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

#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>

#include "TrAgent.h"
#include "Simulator.h"
#include "Node.h"
#include "SimEvent.h"
#include "bundling/Bundle.h"

namespace dtnsim {

TrAgent::TrAgent(Node* node, const BundleTuple& src, const BundleTuple& dst)
    : node_(node), src_(src), dst_(dst),
      size_(0), reps_(0), batch_(1), interval_(0)
{
    logpathf("/tragent/%s", node->name());
}

TrAgent*
TrAgent::init(Node* node, int start_time, 
              const BundleTuple& src, const BundleTuple& dst,
              int argc, const char** argv)
{
    TrAgent* a = new TrAgent(node, src, dst);

    oasys::OptParser p;
    p.addopt(new oasys::IntOpt("size", &a->size_));
    p.addopt(new oasys::IntOpt("reps", &a->reps_));
    p.addopt(new oasys::IntOpt("batch", &a->batch_));
    p.addopt(new oasys::IntOpt("interval", &a->interval_));

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
	    int sendtime = Simulator::time() + interval_;
	    Simulator::post(new SimEvent(SIM_NEXT_SENDTIME, sendtime, this));
        }
	else {
	    log_info("All batches finished");
	}

    } else {
        PANIC("unhandlable event %s", e->type_str());
    }
}

void
TrAgent::send_bundle()
{
    Bundle* b = new Bundle(node_->next_bundleid(), BundlePayload::NODATA);

    b->source_.assign(src_);
    b->replyto_.assign(src_);
    b->custodian_.assign(src_);
    b->dest_.assign(dst_);
    b->payload_.set_length(size_);

    log_info("N[%s]: GEN id:%d %s -> %s size:%d",
             node_->name(), b->bundleid_, src_.c_str(), dst_.c_str(), size_);

    BundleReceivedEvent* e = new BundleReceivedEvent(b, EVENTSRC_APP, size_);
    Simulator::post(new SimRouterEvent(Simulator::time(), node_, e));
}


} // namespace dtnsim
