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

#include "BundleCommand.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"

BundleCommand::BundleCommand()
    : TclCommand("bundle") {}

const char*
BundleCommand::help_string()
{
    return "bundle inject <source> <dest> <payload> <length?> \n"
        "bundle stats \n"
        "bundle list \n"
        ;
}

int
BundleCommand::exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp)
{
    // need a subcommand
    if (objc < 2) {
        wrong_num_args(objc, objv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = Tcl_GetStringFromObj(objv[1], 0);

    if (strcmp(cmd, "inject") == 0) {
        // bundle inject <source> <dest> <payload> <length?>
        if (objc < 5 || objc > 6) {
            wrong_num_args(objc, objv, 2, 5, 6);
            return TCL_ERROR;
        }
        
        Bundle* b = new Bundle();
        b->source_.assign(Tcl_GetStringFromObj(objv[2], 0));
        b->replyto_.assign(Tcl_GetStringFromObj(objv[2], 0));
        b->custodian_.assign(Tcl_GetStringFromObj(objv[2], 0));
        b->dest_.assign(Tcl_GetStringFromObj(objv[3], 0));

        int payload_len;
        u_char* payload_data = Tcl_GetByteArrayFromObj(objv[4], &payload_len);
        int total = payload_len;

        if (objc == 5) {
            // no explicit length, use the object length
            b->payload_.set_data(payload_data, payload_len);
        } else {
            int ok = Tcl_GetIntFromObj(interp, objv[5], &total);
                            
            if (ok != TCL_OK) {
                resultf("invalid length parameter %s", Tcl_GetStringFromObj(objv[5], 0));
                return TCL_ERROR;
            }
            
            if (total == 0) {
                resultf("invalid zero length parameter");
                return TCL_ERROR;
            }
            
            b->payload_.set_length(total);
            b->payload_.append_data(payload_data, payload_len);
        }
        
        log_debug("inject %d byte bundle %s->%s", total,
                  b->source_.c_str(), b->dest_.c_str());

        BundleForwarder::post(new BundleReceivedEvent(b, EVENTSRC_APP, total));
        return TCL_OK;
    } else if (!strcmp(cmd, "stats")) {
        StringBuffer buf("Bundle Statistics: ");
        BundleForwarder::instance()->get_statistics(&buf);
        set_result(buf.c_str());
        return TCL_OK;
        
    } else if (!strcmp(cmd, "list")) {
        StringBuffer buf;
        BundleForwarder::instance()->get_pending(&buf);
        set_result(buf.c_str());
        return TCL_OK;
        
    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}

