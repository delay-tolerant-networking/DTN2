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

#include <oasys/util/ScratchBuffer.h>

#include "TclRegistration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleStatusReport.h"
#include "bundling/CustodySignal.h"
#include "storage/GlobalStore.h"

namespace dtn {

TclRegistration::TclRegistration(const EndpointIDPattern& endpoint,
                                 Tcl_Interp* interp)
    
    : Registration(GlobalStore::instance()->next_regid(),
                   endpoint, Registration::DEFER, 0) // XXX/demmer expiration??
{
    logpathf("/dtn/registration/tcl/%d", regid_);
    set_active(true);

    log_info("new tcl registration on endpoint %s", endpoint.c_str());

    bundle_list_ = new BlockingBundleList(logpath_);
    int fd = bundle_list_->notifier()->read_fd();
    notifier_channel_ = Tcl_MakeFileChannel((ClientData)fd, TCL_READABLE);

    if (notifier_channel_ == NULL) {
        log_err("can't create tcl file channel: %s",
                strerror(Tcl_GetErrno()));
    } else {
        log_debug("notifier_channel_ is %p", notifier_channel_);
        Tcl_RegisterChannel(interp, notifier_channel_);
    }
}

void
TclRegistration::deliver_bundle(Bundle* bundle)
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
 * ALWAYS DEFINED KEY-VALUE PAIRS:
 *
 * isadmin     : Is it an admin bundle? (boolean)
 * source      : Source EID
 * dest        : Destination EID
 * length      : Payload Length
 * payload     : Payload contents
 * creation_ts : Creation timestamp
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
 * orig_frag_offset : Offset of fragment
 * orig_frag_length : Length of original bundle
 *
 * STATUS-REPORT-ONLY OPTIONAL KEY-VALUE PAIRS
 *
 * (Note that the presence of timestamp keys implies a corresponding
 * flag has been set true. For example if forwarded_time is returned
 * the Bundle Forwarded status flag was set; if frag_offset and
 * frag_length are returned the ACK'ed bundle was fragmented.)
 *
 * sr_reason                  : status report reason code
 * sr_received_time           : bundle reception timestamp
 * sr_custody_time            : bundle custody transfer timestamp
 * sr_forwarded_time          : bundle forwarding timestamp
 * sr_delivered_time          : bundle delivery timestamp
 * sr_deletion_time           : bundle deletion timestamp
 *
 * CUSTODY-SIGNAL-ONLY OPTIONAL KEY-VALUE PAIRS
 *
 * custody_succeeded          : boolean if custody transfer succeeded
 * custody_reason             : reason information
 * custody_signal_time        : custody transfer time
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
    oasys::ScratchBuffer<u_char*> payload_buf;
    const u_char* payload_data = (const u_char*)"";
    if (payload_len != 0) {
        payload_data = b->payload_.read_data(0, payload_len, 
                                             payload_buf.buf(payload_len));
    }
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

    // XXX/TODO add is_fragment byte and fragment offset/length fields
    // from original bundle

    // There are (at least for now) no additional key-value pairs with
    // non-admin bundles, so set the result and return
    
    if (!b->is_admin_) {
        goto done;
    }

    // Admin Type:
    addElement(Tcl_NewStringObj("admin_type", -1));
    BundleProtocol::admin_record_type_t admin_type;
    if (!BundleProtocol::get_admin_type(b.object(), &admin_type)) {
        goto done;
    }

    // Now for each type of admin bundle, first append the string to
    // define that type, then all the relevant fields of the type
    switch (admin_type) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
    {
        addElement(Tcl_NewStringObj("Status Report", -1));

        BundleStatusReport::data_t sr;
        if (!BundleStatusReport::parse_status_report(&sr, payload_data,
                                                     payload_len)) {
            cmdinterp->resultf("Admin Bundle Status Report parsing failed");
            return TCL_ERROR;
        }

        // Fragment fields
        if (sr.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT) {
            addElement(Tcl_NewStringObj("orig_frag_offset", -1));
            addElement(Tcl_NewLongObj(sr.orig_frag_offset_));
            addElement(Tcl_NewStringObj("orig_frag_length", -1));
            addElement(Tcl_NewLongObj(sr.orig_frag_length_));
        }

        // Status fields with timestamps:
#define APPEND_TIMESTAMP(_flag, _what, _field)                          \
        if (sr.status_flags_ & BundleProtocol::_flag) {                 \
            addElement(Tcl_NewStringObj(_what, -1));                    \
            sprintf(tmp_buf, "%ld.%06ld",                               \
                    (long)sr._field.tv_sec, (long)sr._field.tv_usec);   \
            addElement(Tcl_NewStringObj(tmp_buf, -1));                  \
        }

        APPEND_TIMESTAMP(STATUS_RECEIVED,
                         "sr_received_time", receipt_tv_);
        APPEND_TIMESTAMP(STATUS_CUSTODY_ACCEPTED,
                         "sr_custody_time",      custody_tv_);
        APPEND_TIMESTAMP(STATUS_FORWARDED,
                         "sr_forwarded_time",    forwarding_tv_);
        APPEND_TIMESTAMP(STATUS_DELIVERED,
                         "sr_delivered_time",    delivery_tv_);
        APPEND_TIMESTAMP(STATUS_DELETED,
                         "sr_deleted_time",      deletion_tv_);
        APPEND_TIMESTAMP(STATUS_ACKED_BY_APP,
                         "sr_acked_by_app_time", acknowledgement_tv_);
#undef APPEND_TIMESTAMP
        
        // Reason Code:
        addElement(Tcl_NewStringObj("sr_reason", -1));
        switch (sr.reason_code_) {
        case BundleProtocol::REASON_NO_ADDTL_INFO:
            addElement(Tcl_NewStringObj("No additional information.", -1));
            break;
            
        case BundleProtocol::REASON_LIFETIME_EXPIRED:
            addElement(Tcl_NewStringObj("Lifetime expired.", -1));
            break;
            
        case BundleProtocol::REASON_FORWARDED_UNIDIR_LINK:
            addElement(Tcl_NewStringObj("Forwarded Over Unidirectional Link.", -1));
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
                (long)sr.orig_creation_tv_.tv_sec,
                (long)sr.orig_creation_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));

        // Status Report's Source EID:
        addElement(Tcl_NewStringObj("orig_source", -1));
        addElement(Tcl_NewStringObj(sr.orig_source_eid_.data(),
                                    sr.orig_source_eid_.length()));
        break;
    }

    //-------------------------------------------

    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
    {
        addElement(Tcl_NewStringObj("Custody Signal", -1));

        CustodySignal::data_t cs;
        if (!CustodySignal::parse_custody_signal(&cs, payload_data,
                                                 payload_len))
        {
            cmdinterp->resultf("Admin Custody Signal parsing failed");
            return TCL_ERROR;
        }

        // Fragment fields
        if (cs.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT) {
            addElement(Tcl_NewStringObj("orig_frag_offset", -1));
            addElement(Tcl_NewLongObj(cs.orig_frag_offset_));
            addElement(Tcl_NewStringObj("orig_frag_length", -1));
            addElement(Tcl_NewLongObj(cs.orig_frag_length_));
        }
        
        addElement(Tcl_NewStringObj("custody_succeeded", -1));
        addElement(Tcl_NewBooleanObj(cs.succeeded_));

        addElement(Tcl_NewStringObj("custody_reason", -1));
        switch(cs.reason_) {
        case BundleProtocol::CUSTODY_NO_ADDTL_INFO:
            addElement(Tcl_NewStringObj("No additional information.", -1));
            break;
            
        case BundleProtocol::CUSTODY_REDUNDANT_RECEPTION:
            addElement(Tcl_NewStringObj("Redundant bundle reception.", -1));
            break;
            
        case BundleProtocol::CUSTODY_DEPLETED_STORAGE:
            addElement(Tcl_NewStringObj("Depleted Storage.", -1));
            break;
            
        case BundleProtocol::CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE:
            addElement(Tcl_NewStringObj("Destination endpoint ID unintelligible.", -1));
            break;
            
        case BundleProtocol::CUSTODY_NO_ROUTE_TO_DEST:
            addElement(Tcl_NewStringObj("No known route to destination from here", -1));
            break;
            
        case BundleProtocol::CUSTODY_NO_TIMELY_CONTACT:
            addElement(Tcl_NewStringObj("No timely contact with next node en route.", -1));
            break;
            
        case BundleProtocol::CUSTODY_HEADER_UNINTELLIGIBLE:
            addElement(Tcl_NewStringObj("Header unintelligible.", -1));
            break;
            
        default:
            sprintf(tmp_buf, "Error: Unknown Custody Signal Reason Code 0x%x",
                    cs.reason_);
            addElement(Tcl_NewStringObj(tmp_buf, -1));
            break;
        }

        // Custody signal timestamp
        addElement(Tcl_NewStringObj("custody_signal_time", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)cs.custody_signal_tv_.tv_sec,
                (long)cs.custody_signal_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        
        // Bundle creation timestamp
        addElement(Tcl_NewStringObj("orig_creation_ts", -1));
        sprintf(tmp_buf, "%ld.%06ld",
                (long)cs.orig_creation_tv_.tv_sec,
                (long)cs.orig_creation_tv_.tv_usec);
        addElement(Tcl_NewStringObj(tmp_buf, -1));

        // Original source eid
        addElement(Tcl_NewStringObj("orig_source", -1));
        addElement(Tcl_NewStringObj(cs.orig_source_eid_.data(),
                                    cs.orig_source_eid_.length()));
        break;
    }

    //-------------------------------------------

    case BundleProtocol::ADMIN_ECHO:
    {
        addElement(Tcl_NewStringObj("Admin Echo Signal", -1));
        break;
    }
    
    //-------------------------------------------

    case BundleProtocol::ADMIN_NULL:
    {
        addElement(Tcl_NewStringObj("Admin Null Signal", -1));
        break;
    }
    
    default:
        sprintf(tmp_buf,
                "Error: Unknown Status Report Type 0x%x", admin_type);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        break;
    }

    // all done
 done:
    cmdinterp->set_objresult(objv);
    BundleDaemon::post(new BundleDeliveredEvent(b.object(), this));
    
    return TCL_OK;
}

} // namespace dtn
