
#include "TclRegistration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"
#include "util/StringBuffer.h"

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
    CommandInterp* cmdinterp = CommandInterp::instance();
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
    CommandInterp* cmdinterp = CommandInterp::instance();
    cmdinterp->set_result(Tcl_GetChannelName(notifier_channel_));
    return TCL_OK;
}


int
TclRegistration::get_bundle_data(Tcl_Interp* interp)
{
    CommandInterp* cmdinterp = CommandInterp::instance();
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
    StringBuffer payload_buf(payload_len);
    u_char* payload_data =
        (u_char*)b->payload_.read_data(0, payload_len, payload_buf.data());
    log_debug("got %d bytes of bundle data", payload_len);
    
    Tcl_Obj* objv[4];
    objv[0] = Tcl_NewStringObj(b->source_.data(), b->source_.length());
    objv[1] = Tcl_NewStringObj(b->dest_.data(), b->dest_.length());
    objv[2] = Tcl_NewByteArrayObj(payload_data, payload_len);
    objv[3] = Tcl_NewIntObj(payload_len);

    cmdinterp->set_objresult(Tcl_NewListObj(4, objv));

    b->del_ref("TclRegistration");
    
    BundleForwarder::post(
        new BundleTransmittedEvent(b, this, payload_len, true));
        
    return TCL_OK;
}
