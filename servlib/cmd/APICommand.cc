
#include "APICommand.h"
#include "applib/APIServer.h"

APICommand::APICommand()
    : TclCommand("api")
{
    bind_addr("local_addr",  &APIServer::local_addr_);
    bind_i("handshake_port", &APIServer::handshake_port_);
    bind_i("session_port",   &APIServer::session_port_);
}

const char*
APICommand::help_string()
{
    return("api set <var> <val>");
}
