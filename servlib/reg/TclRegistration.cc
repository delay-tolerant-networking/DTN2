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
 * return a list of bundle data to a registered procedure each time a
 * bundle arrives. The returned TCL list is of the following form.
 *
 * If the bundle is an admin bundle a 12-element list is returned.
 * Otherwise there are 5 elements. The first 4 elements are commond to
 * both bundle types:
 * 
 * LIST INDEX: CONTENT
 *
 * 0:  Is it an admin bundle? (boolean)
 * 1:  Source EID
 * 2:  Destination EID
 * 3:  Payload Length
 * 4:  Payload contents [for non-admin, regular bundles]
 * 4:  Admin Type [for admin bundles]
 * 5:  Status Flags
 * 6:  Reason Code
 * 7:  Fragment_Offset or "" if not fragmented
 * 8:  Fragment Length or "" if not fragmented
 * 9:  0 or more comma-separated "TIMESTAMP_TYPE=TIMESTAMP_VALUE"'s
 * 10: Bundle creation timestamp
 * 11: Source EID (as contained in the Status Report payload)
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

    // These are return values common to both regular and admin bundles:
    Tcl_Obj* objv[12];
    objv[0] = Tcl_NewBooleanObj(b->is_admin_);
    objv[1] = Tcl_NewStringObj(b->source_.data(), b->source_.length());
    objv[2] = Tcl_NewStringObj(b->dest_.data(), b->dest_.length());
    objv[3] = Tcl_NewIntObj(payload_len);
    
    if (!b->is_admin_) {
        
        objv[4] = Tcl_NewByteArrayObj((u_char*)payload_data, payload_len);
        cmdinterp->set_objresult(Tcl_NewListObj(5, objv));
        
    } else {
        
        // admin bundle:

        status_report_data_t sr;
        if (!BundleStatusReport::parse_status_report(&sr, payload_data,
                                                     payload_len)) {
            cmdinterp->resultf("Admin Bundle Status Report parsing failed");
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;

        // go through the SR fields and build up the return list

        // Admin Type:
        switch (sr.admin_type_) {
        case BundleProtocol::ADMIN_STATUS_REPORT:
            buf.append("Admin Status Report");
            break;

        case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
            buf.append("Admin Custody Signal");
            break;

        case BundleProtocol::ADMIN_ECHO:
            buf.append("Admin Echo Signal");
            break;

        case BundleProtocol::ADMIN_NULL:
            buf.append("Admin Null Signal");
            break;

        default:
            buf.appendf("Error: Unknown Status Report Type 0x%x", sr.admin_type_);
            break;
        }

        objv[4] = Tcl_NewStringObj(buf.data(), buf.length());
        buf.set_length(0);
        
        if (sr.admin_type_ != BundleProtocol::ADMIN_STATUS_REPORT) {
            // not handled yet
            cmdinterp->set_objresult(Tcl_NewListObj(5, objv));
            BundleDaemon::post(
                new BundleTransmittedEvent(b.object(), this, payload_len, true));
            return TCL_OK;
        }

        // construct the return result of optional TimeStamps and
        // Fragment data while parsing the status flags; everything is a
        // comma-separated lists
        oasys::StringBuffer timebuf;

        // Fragments (will be set to "" if not a fragment ACK:
        if (sr.status_flags_ & BundleProtocol::STATUS_FRAGMENT) {
            buf.append("Fragment,");
            objv[7] = Tcl_NewLongObj(sr.frag_offset_);
            objv[8] = Tcl_NewLongObj(sr.frag_length_);
        } else {
            objv[7] = Tcl_NewStringObj("", 0);
            objv[8] = Tcl_NewStringObj("", 0);
        }            
    
        if (sr.status_flags_ & BundleProtocol::STATUS_RECEIVED) {
            buf.append("Received,");
            timebuf.appendf("ReceiptTime=%ld.%06ld",
                            (long)sr.receipt_tv_.tv_sec,
                             (long)sr.receipt_tv_.tv_usec);
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_CUSTODY_ACCEPTED) {
            buf.append("Custody Accepted,");
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_FORWARDED) {
            buf.append("Forwarded,");
            timebuf.appendf("ForwardingTime=%ld.%06ld",
                            (long)sr.forwarding_tv_.tv_sec,
                             (long)sr.forwarding_tv_.tv_usec);
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_DELIVERED) {
            buf.append("Delivered,");
            timebuf.appendf("DeliveryTime=%ld.%06ld",
                            (long)sr.delivery_tv_.tv_sec,
                             (long)sr.delivery_tv_.tv_usec);
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_DELETED) {
            buf.append("Deleted,");
            timebuf.appendf("DeletionTime=%ld.%06ld",
                            (long)sr.deletion_tv_.tv_sec,
                             (long)sr.deletion_tv_.tv_usec);
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_UNAUTHENTIC) {
            buf.append("Unauthentic,");
        }

        if (sr.status_flags_ & BundleProtocol::STATUS_UNUSED) {
            buf.append("Unused Status Flag,");
        }

        // strip trailing "," if any
        if (buf.length() > 0) {
            buf.trim(1);
        }
        if (timebuf.length() > 0) {
            timebuf.trim(1);
        }

        objv[5] = Tcl_NewStringObj(buf.data(), buf.length());
        objv[9] = Tcl_NewStringObj(timebuf.data(), timebuf.length());
        buf.set_length(0);

        // Reason Code:
        switch (sr.reason_code_) {
        case BundleProtocol::REASON_NO_ADDTL_INFO:
            buf.append("No additional information.");
            break;
            
        case BundleProtocol::REASON_LIFETIME_EXPIRED:
            buf.append("Lifetime expired.");
            break;
            
        case BundleProtocol::REASON_RESERVERED_FUTURE_USE:
            buf.append("Reserved for future use.");
            break;
                        
        case BundleProtocol::REASON_TRANSMISSION_CANCELLED:
            buf.append("Transmission cancelled.");
            break;
            
        case BundleProtocol::REASON_DEPLETED_STORAGE:
            buf.append("Depleted storage.");
            break;
            
        case BundleProtocol::REASON_ENDPOINT_ID_UNINTELLIGIBLE:
            buf.append("Destination endpoint ID unintelligible.");
            break;
            
        case BundleProtocol::REASON_NO_ROUTE_TO_DEST:
            buf.append("No known route to destination from here.");
            break;
            
        case BundleProtocol::REASON_NO_TIMELY_CONTACT:
            buf.append("No timely contact with next node on route.");
            break;
            
        case BundleProtocol::REASON_HEADER_UNINTELLIGIBLE:
            buf.append("Header unintelligible.");
            break;
            
        default:
            buf.appendf("Error: Unknown Status Report Reason Code 0x%x", sr.reason_code_);
            break;
        }

        objv[6] = Tcl_NewStringObj(buf.data(), buf.length());
        buf.set_length(0);
        

        // Bundle creation timestamp
        buf.appendf("%ld.%06ld",
                    (long)sr.creation_tv_.tv_sec,
                     (long)sr.creation_tv_.tv_usec);
        objv[10] = Tcl_NewStringObj(buf.data(), buf.length());
        buf.set_length(0);


        // Source EID:
        objv[11] = Tcl_NewStringObj(sr.EID_.data(), sr.EID_.length());
        cmdinterp->set_objresult(Tcl_NewListObj(12, objv));

    } // end admin bundle
    
    BundleDaemon::post(
        new BundleTransmittedEvent(b.object(), this, payload_len, true));
        
    return TCL_OK;
}

} // namespace dtn
