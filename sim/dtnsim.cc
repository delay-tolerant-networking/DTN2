/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/Random.h>

#include "ConnCommand.h"
#include "Simulator.h"
#include "SimCommand.h"
#include "SimConvergenceLayer.h"
#include "contacts/ContactManager.h"
#include "cmd/ParamCommand.h"
#include "naming/SchemeTable.h"

/**
 * Namespace for the dtn simulator
 */
namespace dtnsim {}

using namespace dtn;
using namespace dtnsim;

int
main(int argc, char** argv)
{
    // command line parameter vars
    int                random_seed;
    bool               random_seed_set = false;
    std::string        conf_file;
    bool               conf_file_set = false;
    std::string        logfile("-");
    std::string        loglevelstr;
    oasys::log_level_t loglevel;

    oasys::Getopt opts;
    opts.addopt(
        new oasys::StringOpt('c', "conf", &conf_file, "<conf>",
                             "set the configuration file", &conf_file_set));
    
    opts.addopt(
        new oasys::IntOpt('s', "seed", &random_seed, "seed",
                          "random number generator seed", &random_seed_set));

    opts.addopt(
        new oasys::StringOpt('o', "output", &logfile, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    opts.addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr, "<level>",
                             "default log level [debug|warn|info|crit]"));

    opts.getopt(argv[0], argc, argv);

    int remainder = opts.getopt(argv[0], argc, argv);

    if (!conf_file_set && remainder != argc) {
        conf_file.assign(argv[remainder]);
        conf_file_set = true;
        remainder++;
    }

    if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        opts.usage("dtnsim");
        exit(1);
    }

    if (!conf_file_set) {
        fprintf(stderr, "must set the simulator conf file\n");
        opts.usage("dtnsim");
        exit(1);
    }

    // Parse the debugging level argument
    if (loglevelstr.length() == 0) {
        loglevel = LOG_DEFAULT_THRESHOLD;
    } else {
        loglevel = oasys::str2level(loglevelstr.c_str());
        if (loglevel == oasys::LOG_INVALID) {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr.c_str());
            exit(1);
        }
    }

    // Initialize the simulator first and foremost since it's needed
    // for LogSim::gettimeofday
    Simulator* s = new Simulator();
    Simulator::init(s);

    // Initialize logging
    oasys::Log::init("-", loglevel, "--");
    log_info("/sim", "dtn simulator initializing...");

    // seed the random number generator
    if (!random_seed_set) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      random_seed = tv.tv_usec;
    }
    log_info("/sim", "random seed is %u\n", random_seed);
    oasys::Random::seed(random_seed);
    
    // Set up the command interpreter
    oasys::TclCommandInterp::init(argv[0]);
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    interp->reg(new ConnCommand());
    interp->reg(new ParamCommand());
    interp->reg(new SimCommand());

    // Set up components
    SchemeTable::create();
    SimConvergenceLayer::init();
    ConvergenceLayer::add_clayer(SimConvergenceLayer::instance());

    if (interp->exec_file(conf_file.c_str()) != 0) {
        log_err("/sim", "error in configuration file, exiting...");
        exit(1);
    }

    // Run the event loop of simulator
    Simulator::instance()->run();
}
