#include "DTNServer.h"

#include "bundling/AddressFamily.h"
#include "bundling/BundleForwarder.h"
#include "bundling/InterfaceTable.h"

#include "cmd/APICommand.h"
#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

#include "conv_layers/ConvergenceLayer.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

void
DTNServer::init_commands()
{
    TclCommandInterp* interp = TclCommandInterp::instance();
    
    interp->reg(new APICommand());
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new ParamCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new StorageCommand());

    log_debug("/dtnserver", "registered dtn commands");
}

/**
 * Initialize all components before modifying any configuration.
 */
void
DTNServer::init_components()
{
    AddressFamilyTable::init();
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();
    BundleForwarder::init(new BundleForwarder());
    
    log_debug("/dtnserver", "intialized dtn components");
}

/**
 * Post configuration, start up all components.
 */
void
DTNServer::start()
{
    BundleRouter* router;
    router = BundleRouter::create_router(BundleRouter::type_.c_str());

    BundleForwarder* forwarder = BundleForwarder::instance();
    forwarder->set_active_router(router);
    forwarder->start();

    // XXX/demmer change this to do the storage init
    if (!BundleStore::initialized() ||
        !GlobalStore::initialized() ||
        !RegistrationStore::initialized() )
    {
        logf("/daemon", LOG_ERR,
             "configuration did not initialize storage, exiting...");
        exit(1);
    }

    // create (and auto-register) the default administrative registration
    new AdminRegistration();
    
    // load in the various storage tables
    GlobalStore::instance()->load();
    RegistrationTable::init(RegistrationStore::instance());

    //RegistrationTable::instance()->load();
    //BundleStore::instance()->load();

    log_debug("/dtnserver", "started dtn server");
}
