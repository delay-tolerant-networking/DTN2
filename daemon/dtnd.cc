#include <errno.h>
#include <string>
#include <sys/time.h>
#include "debug/Log.h"
#include "applib/APIServer.h"
#include "bundling/BundleForwarder.h"
#include "bundling/InterfaceTable.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"
#include "cmd/Command.h"
#include "cmd/Options.h"
#include "cmd/TestCommand.h"
#include "conv_layers/ConvergenceLayer.h"
#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"
#include "storage/StorageConfig.h"
#include "thread/Timer.h"

int
main(int argc, char** argv)
{
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
    Log::init(logfd, level);
    logf("/daemon", LOG_DEBUG, "main()");

    // command line parameter vars
    std::string conffile("daemon/bundleNode.conf");
    int random_seed;
    bool random_seed_set = false;
    bool daemon = false;
    std::string ignored; // ignore warnings for -l option
        
    // Create the test command now, so testing related options can be
    // registered
    TestCommand testcmd;
    
    // Set up the command line options
    new StringOpt("c", &conffile, "conf", "config file");
    new BoolOpt("t", &StorageConfig::instance()->tidy_,
                "clear database on startup");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");
    new IntOpt("i", &testcmd.id_, "id", "set the test id");
    new BoolOpt("d", &daemon, "run as a daemon");
    new StringOpt("o", &ignored, "output",
                  "file name for logging output ([-o -] indicates stdout)");
    new StringOpt("l", &ignored, "level",
                  "default log level [debug|warn|info|crit]");
        
    // Set up the command interpreter, then parse argv
    CommandInterp::init(argv[0]);
    Options::getopt(argv[0], argc, argv);

    logf("/daemon", LOG_DEBUG, "Bundle Daemon Initializing...");
    
    // Seed the random number generator
    if (!random_seed_set) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed = tv.tv_usec;
    }
    logf("/daemon", LOG_INFO, "random seed is %u\n", random_seed);
    srandom(random_seed);

    // Set up all components
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();
    TimerSystem::init();

    // Create the forwarder but don't start it running yet. This lets
    // the conf file post events but they won't get dispatched until
    // after the router has been created
    BundleForwarder* forwarder = new BundleForwarder();
    BundleForwarder::init(forwarder);
    
    CommandInterp::instance()->reg(&testcmd);

    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (CommandInterp::instance()->exec_file(conffile.c_str()) != 0) {
            logf("/daemon", LOG_ERR,
                 "error in configuration file, exiting...");
            exit(1);
        }
    }

    // The conf file had a chance to set the types used for routing
    // and storage, so initialize those components now.
    BundleRouter* router;
    router = BundleRouter::create_router(BundleRouter::type_.c_str());
    forwarder->set_active_router(router);
    forwarder->start();

    // Check that the storage system was all initialized properly
    if (!BundleStore::initialized() ||
        !GlobalStore::initialized() ||
        !RegistrationStore::initialized() )
    {
        logf("/daemon", LOG_ERR,
             "configuration did not initialize storage, exiting...");
        exit(1);
    }
    
    // now initialize and load in the various storage tables
    GlobalStore::instance()->load();
    RegistrationTable::init(RegistrationStore::instance());
    RegistrationTable::instance()->load();
    //BundleStore::instance()->load();
    
    // if the config script wants us to run a test script, do so now
    if (testcmd.initscript_.length() != 0) {
        CommandInterp::instance()->exec_command(testcmd.initscript_.c_str());
    }

    // now boot up the application interface
    MasterAPIServer* apiserv = new MasterAPIServer();
    apiserv->start();

    // finally, run the main command or event loop (shouldn't return)
    if (daemon) {
        CommandInterp::instance()->event_loop();
    } else {
        CommandInterp::instance()->command_loop("dtn");
    }
    
    logf("/daemon", LOG_ERR, "command loop exited unexpectedly");
}
