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
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleStatusReport.h"
#include "storage/GlobalStore.h"

namespace dtn {

TclRegistration::TclRegistration(const EndpointIDPattern& endpoint,
                                 Tcl_Interp* interp)
    
    : Registration(GlobalStore::instance()->next_regid(),
                   endpoint, Registration::DEFER, 0) // XXX/demmer expiration??
{
    logpathf("/registration/logging/%d", regid_);
    set_active(true);

    log_info("new tcl registration on endpoint %s", endpoint.c_str());

    bundle_list_ = new BlockingBundleList(logpath_);
    int fd = bundle_list_->notifier()->read_fd();
    notifier_channel_ = Tcl_MakeFileChannel((ClientData)fd, TCL_READABLE);

    log_debug("notifier_channel_ is %p", notifier_channel_);

    Tcl_RegisterChannel(interp, notifier_channel_);
}

void
TclRegistration::consume_bundle(Bundle* bundle)
{
    bundle_list_->push_back(bundle);
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


/**
 * return a Tcl list key-value pairs detailing bundle contents to a
 * registered procedure each time a bundle arrives. The returned TCL
 * list is suitable for assigning to an array, e.g.
 *     array set b $bundle_info
 *
 * Below is a description of the array keys and values in the form
 * "key : description of value"
 *
 * ALWAYS DEFINIED KEY-VALUE PAIRS:
 *
 * isadmin     : Is it an admin bundle? (boolean)
 * source      : Source EID
 * dest        : Destination EID
 * length      : Payload Length
 * payload     : Payload contents
 * creation_ts : Creation timestamp
 *
 *
 * ADMIN-BUNDLE-ONLY KEY-VALUE PAIRS:
 *
 * admin_type       : the Admin Type (the following pairs are only defined
 *                    if the admin_type = "Stauts Report")
 * reason_code      : Reason Code string
 * orig_creation_ts : creation timestamp of original bundle
 * orig_source      : EID of the original bundle's source
 *
 * ADMIN-BUNDLE-ONLY OPTIONAL KEY-VALUE PAIRS:
 *
 * (Note that the presence of these keys implies a corresponding flag
 * has been set true.  For example if forwarded_time is returned the
 * Bundle Forwarded status flag was set; if frag_offset and
 * frag_length are returned the ACK'ed bundle was fragmented.)
 *
 * frag_offset             : Offset of Fragment
 * frag_length             : Length of Fragment
 * received_time           : bundle reception timestamp
 * forwarded_time          : bundle forwarding timestamp
 * delivered_time          : bundle delivery timestamp
 * deletion_time           : bundle deletion timestamp
 * status_unathentic       : flag was set
 * status_unused_flag      : flag was set
 * custody_accepted_status : flag was set
 * 
 */
int
TclRegistration::get_bundle_data(Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    BundleRef b("TclRegistration::get_bundle_data temporary");
    b = bundle_list_->pop_front();
    if (b == NULL) {
        cmdinterp->set_objresult(Tcl_NewListObj(0, 0));
        return TCL_OK; // empty list
    }

    // read in all the payload data (XXX/demmer this will not be nice
    // for big bundles)
    size_t payload_len = b->payload_.length();
    oasys::StringBuffer payload_buf(payload_len);
    const u_char* payload_data =
        b->payload_.read_data(0, payload_len, (u_char*)payload_buf.data());
    log_debug("got %u bytes of bundle data", (u_int)payload_len);

    Tcl_Obj* objv = Tcl_NewListObj(0, NULL);
    char tmp_buf[128];              // used for sprintf strings

#define addElement(e) \
    if (Tcl_ListObjAppendElement(interp, objv, (e)) != TCL_OK) {\
        cmdinterp->resultf("Tcl_ListObjAppendElement failed");\
        return TCL_ERROR;\
    }
    
    // These key-value pairs are common to both regular and admin bundles:

    addElement(Tcl_NewStringObj("isadmin", -1));
    addElement(Tcl_NewBooleanObj(b->is_admin_));
    addElement(Tcl_NewStringObj("source", -1));
    addElement(Tcl_NewStringObj(b->source_.data(), b->source_.length()));
    addElement(Tcl_NewStringObj("dest", -1));
    addElement(Tcl_NewStringObj(b->dest_.data(), b->dest_.length()));
    addElement(Tcl_NewStringObj("length", -1));
    addElement(Tcl_NewIntObj(payload_len));
    addElement(Tcl_NewStringObj("payload", -1));
    addElement(Tcl_NewByteArrayObj((u_char*)payload_data, payload_len));
    addElement(Tcl_NewStringObj("creation_ts", -1));
    sprintf(tmp_buf, "%ld.%06ld",
            (long)b->creation_ts_.tv_sec, (long)b->creation_ts_.tv_usec);
    addElement(Tcl_NewStringObj(tmp_buf, -1));

    // There are (at least for now) no additional key-value pairs with
    // non-admin bundles, so set the result and return
    
    if (!b->is_admin_) {
        cmdinterp->set_objresult(objv);
        BundleDaemon::post(
            new BundleTransmittedEvent(b.object(), this, payload_len, true));
        return TCL_OK;
    }

    // from here on we know we're dealing with an admin bundle

    status_report_data_t sr;
    if (!BundleStatusReport::parse_status_report(&sr, payload_data,
                                                 payload_len)) {
        cmdinterp->resultf("Admin Bundle Status Report parsing failed");
        return TCL_ERROR;
    }

    // go through the SR fields and build up the return list

    // Admin Type:
    addElement(Tcl_NewStringObj("admin_type", -1));
    switch (sr.admin_type_) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
        addElement(Tcl_NewStringObj("Status Report", -1));
        break;

    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
        addElement(Tcl_NewStringObj("Custody Signal", -1));
        break;

    case BundleProtocol::ADMIN_ECHO:
        addElement(Tcl_NewStringObj("Admin Echo Signal", -1));
        break;

    case BundleProtocol::ADMIN_NULL:
        addElement(Tcl_NewStringObj("Admin Null Signal", -1));
        break;

    default:
        sprintf(tmp_buf,
                "Error: Unknown Status Report Type 0x%x", sr.admin_type_);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        break;
    }

    // We don't do anything with admin bundles that aren't status
    // reports:
    if (sr.admin_type_ != BundleProtocol::ADMIN_STATUS_REPORT) {
        cmdinterp->set_objresult(objv);
        BundleDaemon::post(
            new BundleTransmittedEvent(b.object(), this, payload_len, true));
        return TCL_OK;
    }

    // Fragment fields
    if (sr.status_flags_ & BundleProtocol::STATUS_FRAGMENT) {
        addElement(Tcl_NewStringObj("frag_offset", -1));
        addElement(Tcl_NewLongObj(sr.frag_offset_));
        addElement(Tcl_NewStringObj("frag_length", -1));
        addElement(Tcl_NewLongObj(sr.frag_length_));
    }

    // Status fields with timestamps:
    
    if (sr.status_flags_ & BundleProtocol::STATUS_RECEIVED) {
        addElement(Tcl_NewStringObj("received_time", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)sr.receipt_tv_.tv_sec, (long)sr.receipt_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_CUSTODY_ACCEPTED) {
        // XXX the lack of a "Time of Custody Acceptance" field is
        // probably a bug in the spec; if it turns out to be rewrite
        // this to return a "custody_time" (will also need to re-write
        // BundleStatusReport to include it)
        addElement(Tcl_NewStringObj("custody_accepted_status", -1));
        addElement(Tcl_NewBooleanObj(true));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_FORWARDED) {
        addElement(Tcl_NewStringObj("forwarded_time", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)sr.forwarding_tv_.tv_sec, (long)sr.forwarding_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_DELIVERED) {
        addElement(Tcl_NewStringObj("delivered_time", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)sr.delivery_tv_.tv_sec, (long)sr.delivery_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_DELETED) {
        addElement(Tcl_NewStringObj("deleted_time", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)sr.deletion_tv_.tv_sec, (long)sr.deletion_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_UNAUTHENTIC) {
        addElement(Tcl_NewStringObj("status_unathentic", -1));
        addElement(Tcl_NewBooleanObj(true));
    }

    if (sr.status_flags_ & BundleProtocol::STATUS_UNUSED) {
        addElement(Tcl_NewStringObj("status_unused_flag", -1));
        addElement(Tcl_NewBooleanObj(true));
    }

    // Reason Code:
    addElement(Tcl_NewStringObj("reason_code", -1));
    switch (sr.reason_code_) {
    case BundleProtocol::REASON_NO_ADDTL_INFO:
        addElement(Tcl_NewStringObj("No additional information.", -1));
        break;
            
    case BundleProtocol::REASON_LIFETIME_EXPIRED:
        addElement(Tcl_NewStringObj("Lifetime expired.", -1));
        break;
            
    case BundleProtocol::REASON_RESERVERED_FUTURE_USE:
        addElement(Tcl_NewStringObj("Reserved for future use.", -1));
        break;
                        
    case BundleProtocol::REASON_TRANSMISSION_CANCELLED:
        addElement(Tcl_NewStringObj("Transmission cancelled.", -1));
        break;
            
    case BundleProtocol::REASON_DEPLETED_STORAGE:
        addElement(Tcl_NewStringObj("Depleted storage.", -1));
        break;
            
    case BundleProtocol::REASON_ENDPOINT_ID_UNINTELLIGIBLE:
        addElement(
            Tcl_NewStringObj("Destination endpoint ID unintelligible.", -1));
        break;
            
    case BundleProtocol::REASON_NO_ROUTE_TO_DEST:
        addElement(
            Tcl_NewStringObj("No known route to destination from here.", -1));
        break;
            
    case BundleProtocol::REASON_NO_TIMELY_CONTACT:
        addElement(
            Tcl_NewStringObj("No timely contact with next node on route.", -1));
        break;
            
    case BundleProtocol::REASON_HEADER_UNINTELLIGIBLE:
        addElement(
            Tcl_NewStringObj("Header unintelligible.", -1));
        break;
            
    default:
        sprintf(tmp_buf, "Error: Unknown Status Report Reason Code 0x%x",
                sr.reason_code_);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        break;
    }

    // Bundle creation timestamp
    addElement(Tcl_NewStringObj("orig_creation_ts", -1));
    sprintf(tmp_buf, "%ld.%06ld",
            (long)sr.creation_tv_.tv_sec, (long)sr.creation_tv_.tv_usec);
    addElement(Tcl_NewStringObj(tmp_buf, -1));

    // Status Report's Source EID:
    addElement(Tcl_NewStringObj("orig_source", -1));
    addElement(Tcl_NewStringObj(sr.EID_.data(), sr.EID_.length()));


    // Finished with the admin bundle
    
    cmdinterp->set_objresult(objv);

    BundleDaemon::post(
        new BundleTransmittedEvent(b.object(), this, payload_len, true));
    
    return TCL_OK;
}

} // namespace dtn
