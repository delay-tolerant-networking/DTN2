
#include "debug/Log.h"
#include "DTNCommands.h"
#include "cmd/APICommand.h"
#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

void
DTNCommands::init()
{
    TclCommandInterp* interp = TclCommandInterp::instance();
    
    interp->reg(new APICommand());
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new ParamCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new StorageCommand());

    log_debug("/command", "registered commands");
}
