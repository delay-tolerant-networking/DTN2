
#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Options.h>

#include "Simulator.h"
#include "LogSim.h"
#include "conv_layers/ConvergenceLayer.h"
#include "cmd/ParamCommand.h"
#include "cmd/RouteCommand.h"
#include "SimConvergenceLayer.h"
#include "bundling/AddressFamily.h"

#include "bundling/ContactManager.h"


int
main(int argc, char** argv)
{
    
    // Initialize logging
    LogSim::init(LOG_INFO);

    // Initialize the simulator
    Simulator* s = new Simulator();
    Simulator::init(s);
    logf("/sim", LOG_INFO, "simulator initializing...");

    // command line parameter vars
    std::string conffile("sim/top.conf");
    int random_seed;
    bool random_seed_set = false;
    
    new StringOpt("c", &conffile, "conf", "config file");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");

    // Set up the command interpreter, then parse argv
    TclCommandInterp::init(argv[0]);
    TclCommandInterp* interp = TclCommandInterp::instance();
    
    interp->reg(new ParamCommand());
    interp->reg(new RouteCommand());

    Options::getopt(argv[0], argc, argv);

    // Seed the random number generator
    if (!random_seed_set) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed = tv.tv_usec;
    }
    logf("/sim", LOG_INFO, "random seed is %u\n", random_seed);
    srand(random_seed);

    AddressFamilyTable::init();
    ContactManager::init();
    
    // add simulator convergence layer (identifies by simcl) to cl list
    ConvergenceLayer::init_clayers();
    ConvergenceLayer::add_clayer("simcl", new SimConvergenceLayer());

    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (interp->exec_file(conffile.c_str()) != 0) {
            logf("/sim", LOG_ERR, "error in configuration file, exiting...");
            exit(1);
        }
    }

    // Run the event loop of simulator
    Simulator::instance()->run();

}
