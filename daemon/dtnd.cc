
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Options.h>

#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"
#include "storage/StorageConfig.h"

int
main(int argc, char* argv[])
{
    int         random_seed;
    bool        random_seed_set = false;
    bool	daemon = false;
    std::string conf_file("daemon/bundleNode.conf");
    std::string ignored; // ignore warnings for -l option

    // First and foremost, scan argv to look for -o <file> and/or -l
    // <level> so we can initialize logging.
    const char* logfile = "-";
    const char* levelstr = 0;
    
    for (int i = 0, j = 1; j < argc ; i++, j++) {
        if (!strcmp(argv[i], "-o")) {
            logfile = argv[j];
        }

        if (!strcmp(argv[i], "-l")) {
            levelstr = argv[j];
        }
    }

    int logfd;
    if (!strcmp(logfile, "-")) {
        logfd = 1; // stdout
    } else {
        logfd = open(logfile, O_CREAT | O_WRONLY | O_APPEND,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (logfd < 0) {
            fprintf(stderr, "fatal error opening log file '%s': %s\n",
                    logfile, strerror(errno));
            exit(1);
        }
    }

    log_level_t level = LOG_DEFAULT_THRESHOLD;

    if (levelstr != 0) {
        level = str2level(levelstr);
        if (level == LOG_INVALID) {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    levelstr);
            exit(1);
        }
    }
    
    // Initialize logging before anything else
    Log::init(logfd, level, "~/.dtndebug");
    logf("/daemon", LOG_DEBUG, "Bundle Daemon Initializing...");

    // Create the test command now, so testing related options can be
    // registered
    TestCommand testcmd;
    
    // Set up the command line options
    new StringOpt("o", &ignored, "output",
                  "file name for logging output ([-o -] indicates stdout)");
    new StringOpt("l", &ignored, "level",
                  "default log level [debug|warn|info|crit]");
    new StringOpt("c", &conf_file, "conf", "config file");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");
    new BoolOpt("d", &daemon, "run as a daemon");
    new BoolOpt("t", &StorageConfig::instance()->tidy_,
                "clear database on startup");
    new IntOpt("i", &testcmd.id_, "id", "set the test id");
        
    // parse command line options
    Options::getopt(argv[0], argc, argv);

    // seed the random number generator
    if (!random_seed_set) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      random_seed = tv.tv_usec;
    }
    logf("/tierd", LOG_INFO, "random seed is %u\n", random_seed);
    srand(random_seed);
    
    // Set up the command interpreter
    TclCommandInterp::init(argv[0]);
    TclCommandInterp* interp = TclCommandInterp::instance();
    interp->reg(&testcmd);
    DTNServer::init_commands();
    APIServer::init_commands();

    // Set up components
    TimerSystem::init();
    DTNServer::init_components();

    // Parse / exec the config file
    if (conf_file.length() != 0) {
        if (interp->exec_file(conf_file.c_str()) != 0) {
            logf("/daemon", LOG_ERR,
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
    
    logf("/daemon", LOG_ERR, "command loop exited unexpectedly");
}
