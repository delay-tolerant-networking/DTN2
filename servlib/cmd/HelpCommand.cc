
#include "HelpCommand.h"

HelpCommand::HelpCommand() : AutoCommandModule("help") {}

int
HelpCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    const CommandList *theList = NULL;
    CommandList::const_iterator iter;
    
    theList = CommandInterp::instance()->modules();

    for ( iter = theList->begin(); iter!=theList->end(); iter++ ) {
        printf("%s:\n%s\n", (*iter)->name(), (*iter)->help_string());
    }


    return TCL_OK;
}

const char*
HelpCommand::help_string()
{
    return("print this message.\n");
}

