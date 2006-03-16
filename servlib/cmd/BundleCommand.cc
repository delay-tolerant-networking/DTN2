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

#include <oasys/util/HexDumpBuffer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/OptParser.h>

#include "BundleCommand.h"
#include "CompletionNotifier.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

BundleCommand::BundleCommand()
    : TclCommand("bundle") {}

BundleCommand::InjectOpts::InjectOpts()
    : custody_xfer_(false),
      receive_rcpt_(false), 
      custody_rcpt_(false), 
      forward_rcpt_(false), 
      delivery_rcpt_(false), 
      deletion_rcpt_(false), 
      expiration_(60),  // bundle TTL
      length_(0),  // bundle payload length
      replyto_("")
{}
    
bool
BundleCommand::parse_inject_options(InjectOpts* options,
                                    int objc, Tcl_Obj** objv,
                                    const char** invalidp)
{
    // no options specified:
    if (objc < 6) {
        return true;
    }
    
    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("custody_xfer",  &options->custody_xfer_));
    p.addopt(new oasys::BoolOpt("receive_rcpt",  &options->receive_rcpt_));
    p.addopt(new oasys::BoolOpt("custody_rcpt",  &options->custody_rcpt_));
    p.addopt(new oasys::BoolOpt("forward_rcpt",  &options->forward_rcpt_));
    p.addopt(new oasys::BoolOpt("delivery_rcpt", &options->delivery_rcpt_));
    p.addopt(new oasys::BoolOpt("deletion_rcpt", &options->deletion_rcpt_));
    p.addopt(new oasys::UIntOpt("expiration",    &options->expiration_));
    p.addopt(new oasys::UIntOpt("length",        &options->length_));
    p.addopt(new oasys::StringOpt("replyto",     &options->replyto_));

    for (int i=5; i<objc; i++) {
        int len;
        const char* option_name = Tcl_GetStringFromObj(objv[i], &len);
        if (! p.parse_opt(option_name, len)) {
            *invalidp = option_name;
            return false;
        }
    }
    return true;
}

const char*
BundleCommand::help_string()
{
    // XXX/matt ugly way to represent the optional "option" argument
    // that requires a Tcl List if present, also need to enumerate
    // what options you can provide
    return "bundle inject <source> <dest> <payload> <opt1<=val1?>...optN<=valN?>?> \n"
        "    valid options:\n"
        "        custody_xfer\n"
        "        receive_rcpt\n"
        "        custody_rcpt\n"
        "        forward_rcpt\n"
        "        delivery_rcpt\n"
        "        deletion_rcpt\n"
        "        expiration=integer\n"
        "        length=integer\n"
        "bundle stats \n"
        "bundle daemon_stats \n"
        "bundle daemon_status \n"
        "bundle reset_stats \n"
        "bundle list \n"
        "bundle info <id>\n"
        "bundle dump <id>\n"
        "bundle dump_ascii <id>"
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
        // bundle inject <source> <dest> <payload> <param1<=value1?>?> ... <paramN<=valueN?>?>
        if (objc < 5) {
            wrong_num_args(objc, objv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }
        
        Bundle* b = new Bundle();
        b->source_.assign(Tcl_GetStringFromObj(objv[2], 0));
        b->replyto_.assign(Tcl_GetStringFromObj(objv[2], 0));
        b->custodian_.assign(EndpointID::NULL_EID());
        b->dest_.assign(Tcl_GetStringFromObj(objv[3], 0));
        
        int payload_len;
        u_char* payload_data = Tcl_GetByteArrayFromObj(objv[4], &payload_len);
        int total = payload_len;

        // now process any optional parameters:
        InjectOpts options;
        const char* invalid;
        if (!parse_inject_options(&options, objc, objv, &invalid)) {
            resultf("error parsing bundle inject options: invalid option '%s'",
                    invalid);
            return TCL_ERROR;
        }

        b->custody_requested_ = options.custody_xfer_;
        b->receive_rcpt_      = options.receive_rcpt_;
        b->custody_rcpt_      = options.custody_rcpt_;
        b->forward_rcpt_      = options.forward_rcpt_;
        b->delivery_rcpt_     = options.delivery_rcpt_;
        b->deletion_rcpt_     = options.deletion_rcpt_;
        b->expiration_        = options.expiration_;

        if (options.length_ != 0) {
            // explicit length but some of the data may just be left
            // as garbage
            b->payload_.set_length(options.length_);
            b->payload_.append_data(payload_data, payload_len);
            b->payload_.close_file();
        } else {
            // use the object length
            b->payload_.set_data(payload_data, payload_len);
        }

        if (options.replyto_ != "") {
            b->replyto_.assign(options.replyto_.c_str());
        }

        oasys::StringBuffer error;
        if (!b->validate(&error)) {
            resultf("bundle validation failed: %s", error.data());
            return TCL_ERROR;
        }
        
        log_debug("inject %d byte bundle %s->%s", total,
                  b->source_.c_str(), b->dest_.c_str());

        BundleDaemon::post(new BundleReceivedEvent(b, EVENTSRC_APP, total));

        // return the creation timestamp (can use with source EID to
        // create a globally unique bundle identifier
        resultf("%ld.%06ld",
                (long)b->creation_ts_.tv_sec, (long)b->creation_ts_.tv_usec);
        return TCL_OK;
        
    } else if (!strcmp(cmd, "stats")) {
        oasys::StringBuffer buf("Bundle Statistics: ");
        BundleDaemon::instance()->get_bundle_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;

    } else if (!strcmp(cmd, "daemon_stats")) {
        oasys::StringBuffer buf("Bundle Daemon Statistics: ");
        BundleDaemon::instance()->get_daemon_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    } else if (!strcmp(cmd, "daemon_status")) {
        BundleDaemon::post_and_wait(new StatusRequest(),
                                    CompletionNotifier::notifier());
        set_result("DTN daemon ok");
        return TCL_OK;
    } else if (!strcmp(cmd, "reset_stats")) {
        BundleDaemon::instance()->reset_stats();
        return TCL_OK;
        
    } else if (!strcmp(cmd, "list")) {
        Bundle* b;
        BundleList::const_iterator iter;
        oasys::StringBuffer buf;
        BundleList* pending =
            BundleDaemon::instance()->pending_bundles();
        
        oasys::ScopeLock l(pending->lock(), "BundleCommand::exec");
        buf.appendf("Currently Pending Bundles (%u): \n", (u_int)pending->size());
    
        for (iter = pending->begin(); iter != pending->end(); ++iter) {
            b = *iter;
            buf.appendf("\t%-3d: %s -> %s length %u\n",
                        b->bundleid_,
                        b->source_.c_str(),
                        b->dest_.c_str(),
                        (u_int)b->payload_.length());
        }
        
        set_result(buf.c_str());
        
        return TCL_OK;
        
    } else if (!strcmp(cmd, "info") ||
               !strcmp(cmd, "dump") ||
               !strcmp(cmd, "dump_ascii"))
    {
        // bundle [info|dump|dump_ascii] <id>
        if (objc != 3) {
            wrong_num_args(objc, objv, 2, 3, 3);
            return TCL_ERROR;
        }

        int bundleid;
        if (Tcl_GetIntFromObj(interp, objv[2], &bundleid) != TCL_OK) {
            resultf("invalid bundle id %s",
                    Tcl_GetStringFromObj(objv[2], 0));
            return TCL_ERROR;
        }

        BundleList* pending =
            BundleDaemon::instance()->pending_bundles();
        
        BundleRef bundle = pending->find(bundleid);

        if (bundle == NULL) {
            resultf("no bundle with id %d", bundleid);
            return TCL_ERROR;
        }

        if (strcmp(cmd, "info") == 0) {
            oasys::StringBuffer buf;
            bundle->format_verbose(&buf);
            buf.append("\n");
            bundle->fwdlog_.dump(&buf);
            set_result(buf.c_str());
            
        } else {
            size_t len = bundle->payload_.length();
            oasys::HexDumpBuffer buf(len);
            const u_char* bp =
                bundle->payload_.read_data(0, len, (u_char*)buf.data());
            
            // XXX/demmer inefficient
            buf.append((const char*)bp, len);
            if (!strcmp(cmd, "dump")) {
                buf.hexify();
            }
            set_result(buf.c_str());
        }
        
        return TCL_OK;

    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}


} // namespace dtn
