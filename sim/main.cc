#include <errno.h>
#include <string>
#include <sys/time.h>
#include "debug/Log.h"
#include "cmd/Command.h"
#include "cmd/Options.h"
#include "Simulator.h"
#include "Log_sim.h"

int
main(int argc, char** argv)
{
    
    
    Log_sim::init(LOG_DEBUG);
    // Initialize the simulator
    Simulator* s = new Simulator();
    Simulator::init(s);



    logf("/sim", LOG_DEBUG, "simulator initializing...");


    
    // command line parameter vars
    std::string conffile("sim/top.conf");
    int random_seed;
    bool random_seed_set = false;
    
    
    new StringOpt("c", &conffile, "conf", "config file");
    new IntOpt("s", &random_seed, &random_seed_set, "seed",
               "random number generator seed");

    // Set up the command interpreter, then parse argv
    CommandInterp::init(argv[0]);
    Options::getopt(argv[0], argc, argv);

    // Seed the random number generator
    if (!random_seed_set) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed = tv.tv_usec;
    }
    logf("/sim", LOG_INFO, "random seed is %u\n", random_seed);
    srand(random_seed);


    // Initialize the topology
    //  Topology::init();
    
    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (CommandInterp::instance()->exec_file(conffile.c_str()) != 0) {
            logf("/simulator", LOG_ERR, "error in configuration file, exiting...");
            exit(1);
        }
    }

    Simulator::instance()->run();


}
