
#include "SimCommands.h"
#include "cmd/LogCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RouteCommand.h"

LogCommand LogCommand::instance_;
ParamCommand ParamCommand::instance_;
RouteCommand RouteCommand::instance_;

void
SimCommands::register_commands()
{
}
