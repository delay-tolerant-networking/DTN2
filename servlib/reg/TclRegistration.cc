
#include "TclRegistration.h"
#include "RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"

TclRegistration::TclRegistration(u_int32_t regid,
                                 const BundleTuplePattern& endpoint,
                                 Tcl_Interp* interp)
    
    : Registration(regid, endpoint, Registration::ABORT)
{
    logpathf("/registration/logging/%d", regid);
    set_active(true);

    if (! RegistrationTable::instance()->add(this)) {
        log_err("unexpected error adding registration to table");
    }

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
    u_char* payload_data = (u_char*)b->payload_.read_data(0, b->payload_.length());
    log_debug("got %d bytes of bundle data", b->payload_.length());
    
    Tcl_Obj* objv[4];
    objv[0] = Tcl_NewStringObj(b->source_.data(), b->source_.length());
    objv[1] = Tcl_NewStringObj(b->dest_.data(), b->dest_.length());
    objv[2] = Tcl_NewByteArrayObj(payload_data, b->payload_.length());
    objv[3] = Tcl_NewIntObj(b->payload_.length());

    cmdinterp->set_objresult(Tcl_NewListObj(4, objv));

    b->del_ref();
    
    return TCL_OK;
}
