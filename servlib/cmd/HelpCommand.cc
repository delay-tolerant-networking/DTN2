
#include "HelpCommand.h"

HelpCommand HelpCommand::instance_;

HelpCommand::HelpCommand() : AutoCommandModule("help") {}

int
HelpCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    const CommandList *theList = NULL;
    std::list<CommandModule*>::const_iterator j;

    theList = CommandInterp::instance()->modules();

    for ( j = theList->begin(); j!=theList->end(); j++ ) {
        printf("\t%s - %s\n", (*j)->name(), (*j)->helpString());
    }


    return TCL_OK;
}

char *
HelpCommand::helpString()
{
    return("print this message.\n");
}

