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

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "SimConvergenceLayer.h"
#include "Topology.h"
#include "bundling/BundleList.h"

namespace dtnsim {

class SimCLInfo : public CLInfo {
public:
    SimCLInfo()
    {
        deliver_partial_ = true;
    }

    ~SimCLInfo() {};
    
    /// if contact closes in the middle of a transmission, deliver the
    /// partially received bytes to the router.
    bool deliver_partial_;
    
};

SimConvergenceLayer* SimConvergenceLayer::instance_;

SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("/cl/sim")
{
}

bool
SimConvergenceLayer::add_link(Link* link, int argc, const char* argv[])
{
    oasys::OptParser p;

    SimCLInfo* info = new SimCLInfo();
    
#define DECLARE_OPT(_type, _opt)                            \
    oasys::_type param_##_opt(#_opt, &info->_opt##_);       \
    p.addopt(&param_##_opt);

    DECLARE_OPT(BoolOpt, deliver_partial);

#undef DECLARE_OPT

    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option %s", invalid);
        return false;
    }

    return true;
    
}

void 
SimConvergenceLayer::send_bundles(Contact* contact)
{
    log_debug("send_bundles on contact %s", contact->nexthop());

    
        
//     SimContact* sc = dtnlink2simlink(contact->link()); 
    
//     Bundle* bundle;
//     BundleList* blist = contact->bundle_list();
    
//     Bundle* iter_bundle;
//     BundleList::iterator iter;
//     oasys::ScopeLock lock(blist->lock());
    
//     log_info("N[%d] CL: current bundle list:",sc->src()->id());
        
//     for (iter = blist->begin(); 
//          iter != blist->end(); ++iter) {
//         iter_bundle = *iter;
//         log_info("\tbundle:%d pending:%d",
//                  iter_bundle->bundleid_,iter_bundle->pendingtx());
//     }
//     // check, if the contact is open. If yes, send one msg from the queue
//     if (sc->is_open()) {
//         bundle = blist->pop_front();
//         if(bundle) {
//             log_info("\tsending bundle:%d",bundle->bundleid_);
//             Message* msg = SimConvergenceLayer::bundle2msg(bundle);
//             sc->chew_message(msg);
//         }
//     }
}

} // namespace dtnsim
