
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Options.h>
#include <oasys/util/StringBuffer.h>

#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"
#include "storage/StorageConfig.h"

int
main(int argc, char* argv[])
{
    TestCommand testcmd;
    bool	testid_set = false;
    int         random_seed;
    bool        random_seed_set = false;
    bool	daemon = false;
    std::string conf_file("daemon/bundleNode.conf");
    std::string logfile("-");
    std::string loglevelstr;
    log_level_t loglevel;

    // Register all command line options
    new StringOpt("o", &logfile, "output",
                  "file name for logging output ([-o -] indicates stdout)");
    new StringOpt("l", &loglevelstr, "level",
                  "default log level [debug|warn|info|crit]");
    new StringOpt("c", &conf_file, "conf", "config file");
    new BoolOpt("d", &daemon, "run as a daemon");
    new BoolOpt("t", &StorageConfig::instance()->tidy_,
                "clear database and initialize tables on startup");
    new BoolOpt("C", &StorageConfig::instance()->init_,
                "initialize database on startup");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");

    new IntOpt("i", &testcmd.id_, &testid_set,
               "id", "set the test id");

    Options::getopt(argv[0], argc, argv);

    // Parse the debugging level argument
    if (loglevelstr.length() == 0) {
        loglevel = LOG_DEFAULT_THRESHOLD;
    } else {
        loglevel = str2level(loglevelstr.c_str());
        if (loglevel == LOG_INVALID) {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr.c_str());
            exit(1);
        }
    }
    
    Log::init(logfile.c_str(), loglevel, "", "~/.dtndebug");
    logf("/dtnd", LOG_INFO, "Bundle Daemon Initializing...");

    // bind a copy of argv to be accessible to test scripts
    for (int i = 0; i < argc; ++i) {
        testcmd.argv_.append(argv[i]);
        testcmd.argv_.append(" ");
    }
    
    // seed the random number generator
    if (!random_seed_set) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      random_seed = tv.tv_usec;
    }
    logf("/dtnd", LOG_INFO, "random seed is %u\n", random_seed);
    srand(random_seed);
    
    // Set up the command interpreter
    TclCommandInterp::init(argv[0]);
    TclCommandInterp* interp = TclCommandInterp::instance();
    testcmd.bind_vars();
    interp->reg(&testcmd);
    DTNServer::init_commands();
    APIServer::init_commands();

    // Set up components
    TimerSystem::init();
    DTNServer::init_components();

    // Parse / exec the config file
    if (conf_file.length() != 0) {
        if (interp->exec_file(conf_file.c_str()) != 0) {
            logf("/dtnd", LOG_ERR,
                 "error in configuration file, exiting...");
            exit(1);
        }
    }

    // Start everything up
    DTNServer::start();

    // if the config script wants us to run a test script, do so now
    if (testcmd.initscript_.length() != 0) {
        interp->exec_command(testcmd.initscript_.c_str());
    }

    // boot the application server
    APIServer::start_master();

    // finally, run the main command or event loop (shouldn't return)
    if (daemon) {
        interp->event_loop();
    } else {
        interp->command_loop("dtn");
    }
    
    logf("/dtnd", LOG_ERR, "command loop exited unexpectedly");
}
