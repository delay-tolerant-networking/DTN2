
#include "debug/Log.h"
#include "DTNCommands.h"
#include "cmd/APICommand.h"
#include "cmd/BundleCommand.h"
#include "cmd/HelpCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/LogCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/QuitCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

APICommand APICommand::instance_;
BundleCommand BundleCommand::instance_;
HelpCommand HelpCommand::instance_;
InterfaceCommand InterfaceCommand::instance_;
LogCommand LogCommand::instance_;
ParamCommand ParamCommand::instance_;
QuitCommand QuitCommand::instance_;
RegistrationCommand RegistrationCommand::instance_;
RouteCommand RouteCommand::instance_;
StorageCommand StorageCommand::instance_;

void
DTNCommands::register_commands()
{
    log_debug("/command", "registered commands");
}
