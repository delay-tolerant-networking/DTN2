/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <string>
#include <sys/time.h>

#include <oasys/debug/Log.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Getopt.h>

#include "LogSim.h"
#include "Simulator.h"
#include "SimCommand.h"
#include "SimConvergenceLayer.h"
#include "bundling/AddressFamily.h"
#include "bundling/ContactManager.h"
#include "cmd/ParamCommand.h"

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
    
    oasys::Getopt::addopt(
        new oasys::StringOpt('c', "conf", &conf_file, "<conf>",
                             "set the configuration file", &conf_file_set));
    
    oasys::Getopt::addopt(
        new oasys::IntOpt('s', "seed", &random_seed, "seed",
                          "random number generator seed", &random_seed_set));

    oasys::Getopt::addopt(
        new oasys::StringOpt('o', "output", &logfile, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr, "<level>",
                             "default log level [debug|warn|info|crit]"));

    oasys::Getopt::getopt(argv[0], argc, argv);

    int remainder = oasys::Getopt::getopt(argv[0], argc, argv);

    if (!conf_file_set && remainder != argc) {
        conf_file.assign(argv[remainder]);
        conf_file_set = true;
        remainder++;
    }

    if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        oasys::Getopt::usage("dtnsim");
        exit(1);
    }

    if (!conf_file_set) {
        fprintf(stderr, "must set the simulator conf file\n");
        oasys::Getopt::usage("dtnsim");
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
    LogSim::init(loglevel);
    log_info("/sim", "dtn simulator initializing...");

    // seed the random number generator
    if (!random_seed_set) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      random_seed = tv.tv_usec;
    }
    log_info("/sim", "random seed is %u\n", random_seed);
    srand(random_seed);

    // Set up the command interpreter
    oasys::TclCommandInterp::init(argv[0]);
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    interp->reg(new ParamCommand());
    interp->reg(new SimCommand());

    // Set up components
    AddressFamilyTable::init();
    AddressFamilyTable::instance()->add_string_family();
    SimConvergenceLayer::init();
    ConvergenceLayer::add_clayer("sim", SimConvergenceLayer::instance());
    
    if (interp->exec_file(conf_file.c_str()) != 0) {
        log_err("/sim", "error in configuration file, exiting...");
        exit(1);
    }

    // Run the event loop of simulator
    Simulator::instance()->run();
}
