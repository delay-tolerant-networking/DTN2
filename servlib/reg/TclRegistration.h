#ifndef _TCL_REGISTRATION_H_
#define _TCL_REGISTRATION_H_

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Thread.h>

#include "Registration.h"
#include "bundling/BundleTuple.h"

/**
 * A simple utility class used mostly for testing registrations.
 *
 * When created, this sets up a new registration within the daemon,
 * and for any bundles that arrive, outputs logs of the bundle header
 * fields as well as the payload data (if ascii). The implementation
 * is structured as a thread that blocks (forever) waiting for bundles
 * to arrive on the registration's bundle list, then tcl the
 * bundles and looping again.
 */
class TclRegistration : public Registration {
public:
    TclRegistration(const BundleTuplePattern& endpoint,
                    Tcl_Interp* interp);
    int exec(int argc, const char** argv, Tcl_Interp* interp);

    /**
     * Return in the tcl result a Tcl_Channel to wrap the BundleList's
     * notifier pipe.
     */
    int get_list_channel(Tcl_Interp* interp);

    /**
     * Assuming the list channel has been notified, pops a bundle off
     * the list and then returns in the tcl result a list of the
     * relevant metadata and the payload data.
     */
    int get_bundle_data(Tcl_Interp* interp);

protected:
    Tcl_Channel notifier_channel_;
};

#endif /* _TCL_REGISTRATION_H_ */
