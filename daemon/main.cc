#include <errno.h>
#include <string>
#include <sys/time.h>
#include "debug/Log.h"
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
        
    // Create the test command now, so testing related options can be
    // registered
    TestCommand testcmd;
    
    // Set up the command line options
    new StringOpt("c", &conffile, "conf", "config file");
    new BoolOpt("t", &StorageConfig::instance()->tidy_,
                "clear database on startup");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");
    new IntOpt("l", &testcmd.loopback_, "loopback",
               "test option for loopback");
    
    // Set up the command interpreter, then parse argv
    CommandInterp::init(argv[0]);
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
    ConvergenceLayer::init_clayers();
    BundleForwarder::init();
    InterfaceTable::init();
    BundleRouter* router = new BundleRouter();
    BundleRouter::register_router(router);
    router->start();

    CommandInterp::instance()->reg(&testcmd);

    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (CommandInterp::instance()->exec_file(conffile.c_str()) != 0) {
            logf("/daemon", LOG_ERR, "error in configuration file, exiting...");
            exit(1);
        }
    }

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

    // finally, run the main command loop (shouldn't return)
    CommandInterp::instance()->loop("dtn");
    
    logf("/daemon", LOG_ERR, "command loop exited unexpectedly");
}
