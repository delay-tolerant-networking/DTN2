
#include "BundleCommand.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "util/StringBuffer.h"

BundleCommand::BundleCommand() : AutoCommandModule("bundle") {}

const char*
BundleCommand::help_string()
{
    return("bundle inject <source> <dest> <payload> <length?>");
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
            b->payload_.set_data((const char*)payload_data, payload_len);
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
            b->payload_.append_data((const char*)payload_data, payload_len);
        }
        
        log_debug("inject %d byte bundle %s->%s", total,
                  b->source_.c_str(), b->dest_.c_str());

        BundleForwarder::post(new BundleReceivedEvent(b, total));
        return TCL_OK;
    } else if (!strcmp(cmd, "stats")) {
        StringBuffer buf("Bundle Statistics: ");
        BundleForwarder::instance()->get_statistics(&buf);
        set_result(buf.c_str());
        return TCL_OK;
        
    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}

