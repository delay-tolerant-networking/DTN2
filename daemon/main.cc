#include <errno.h>
#include <string>
#include <sys/time.h>
#include "debug/Log.h"
#include "bundling/BundleForwarding.h"
#include "routing/RouteTable.h"
#include "cmd/Command.h"
#include "cmd/Options.h"
#include "cmd/StorageCommand.h"
#include "cmd/TestCommand.h"
#include "conv_layers/ConvergenceLayer.h"

int
main(int argc, char** argv)
{
    // Initialize logging before anything else
    Log::init();
    logf("/daemon", LOG_INFO, "bundle daemon initializing...");

    // command line parameter vars
    std::string conffile("daemon/bundleNode.conf");
    int random_seed;
    bool random_seed_set = false;
        
    // test command
    TestCommand testcmd;
    
    // Set up the command line options
    new StringOpt("c", &conffile, "conf", "config file");
    new BoolOpt("t", &StorageCommand::instance()->tidy_,
                "clear database on startup");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");
    new IntOpt("l", &testcmd.loopback_, "loopback",
               "test option for loopback");
    
    // Parse argv
    Options::getopt(argv[0], argc, argv);

    // Seed the random number generator
    if (!random_seed_set) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed = tv.tv_usec;
    }
    logf("/daemon", LOG_INFO, "random seed is %u\n", random_seed);
    srand(random_seed);

    // Set up all components
    CommandInterp::init();
    ConvergenceLayer::init_clayers();
    BundleForwarding::init();
    RouteTable::init();

    CommandInterp::instance()->reg(&testcmd);

    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (CommandInterp::instance()->exec_file(conffile.c_str()) != 0) {
            logf("/daemon", LOG_ERR, "error in configuration file, exiting...");
            exit(1);
        }
    }
    
    // Start the main console input loop
    while (1) {
        char buf[256];
        fprintf(stdout, "> ");
        fflush(stdout);
        
        char* line = fgets(buf, sizeof(buf), stdin);
        if (!line) {
            logf("/daemon", LOG_INFO, "got eof on stdin, exiting: %s",
                 strerror(errno));
            continue;
        }

        size_t len = strlen(line);
        if (len == 0) {
            continue; // short circuit blank lines
        }

        // trim trailing newline
        line[len - 1] = '\0';
        
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
