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

#include <oasys/util/StringBuffer.h>

#include "TclRegistration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"

namespace dtn {

TclRegistration::TclRegistration(const BundleTuplePattern& endpoint,
                                 Tcl_Interp* interp)
    
    : Registration(endpoint, Registration::ABORT)
{
    logpathf("/registration/logging/%d", regid_);
    set_active(true);

    log_info("new tcl registration on endpoint %s", endpoint.c_str());
    notifier_channel_ = Tcl_MakeFileChannel((void*)bundle_list_->read_fd(),
                                            TCL_READABLE);
    Tcl_RegisterChannel(interp, notifier_channel_);
}

int
TclRegistration::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    if (argc < 1) {
        cmdinterp->wrong_num_args(argc, argv, 0, 1, INT_MAX);
        return TCL_ERROR;
    }
    const char* op = argv[0];

    if (strcmp(op, "get_list_channel") == 0)
    {
        return get_list_channel(interp);
    }
    else if (strcmp(op, "get_bundle_data") == 0)
    {
        return get_bundle_data(interp);
    }
    else
    {
        cmdinterp->resultf("invalid operation '%s'", op);
        return TCL_ERROR;
    }
}

int
TclRegistration::get_list_channel(Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    cmdinterp->set_result(Tcl_GetChannelName(notifier_channel_));
    return TCL_OK;
}


int
TclRegistration::get_bundle_data(Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    Bundle* b = bundle_list_->pop_front();
    if (!b) {
        cmdinterp->set_objresult(Tcl_NewListObj(0, 0));
        return TCL_OK; // empty list
    }

    // always drain the notification pipe
    bundle_list_->drain_pipe();

    // read in all the payload data (XXX/demmer this will not be nice
    // for big bundles)
    size_t payload_len = b->payload_.length();
    oasys::StringBuffer payload_buf(payload_len);
    const u_char* payload_data =
        b->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());
    log_debug("got %d bytes of bundle data", payload_len);
    
    Tcl_Obj* objv[4];
    objv[0] = Tcl_NewStringObj(b->source_.data(), b->source_.length());
    objv[1] = Tcl_NewStringObj(b->dest_.data(), b->dest_.length());
    objv[2] = Tcl_NewByteArrayObj((u_char*)payload_data, payload_len);
    objv[3] = Tcl_NewIntObj(payload_len);

    cmdinterp->set_objresult(Tcl_NewListObj(4, objv));

    b->del_ref("TclRegistration");
    
    BundleForwarder::post(
        new BundleTransmittedEvent(b, this, payload_len, true));
        
    return TCL_OK;
}

} // namespace dtn
