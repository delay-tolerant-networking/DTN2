
#include <errno.h>
#include "debug/Log.h"
#include "cmd/Command.h"

int
main(int argc, const char** argv)
{
    // Initialize logging and the command interpreter
    Log::init();
    CommandInterp::init();
    logf("/daemon", LOG_INFO, "bundle daemon initializing...");

    // Start the main console input loop
    while (1) {
        char buf[256];
        fprintf(stdout, "> ");
        fflush(stdout);
        
        char* line = fgets(buf, sizeof(buf), stdin);
        if (!line) {
            logf("/daemon", LOG_INFO, "got eof on stdin, exiting: %s", strerror(errno));
            continue;
        }
        
        line[strlen(line) - 1] = '\0';
        logf("/daemon", LOG_DEBUG, "got line '%s'", line);
        
        if (CommandInterp::instance()->exec_command(buf) != TCL_OK) {
            fprintf(stdout, "error: ");
        }
        
        const char* res = CommandInterp::instance()->get_result();
        if (res && res[0] != '\0') {
            fprintf(stdout, "%s\r\n", res);
            fflush(stdout);
        }
    }
}
