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

#include "Simulator.h"
#include "LogSim.h"
#include "conv_layers/ConvergenceLayer.h"
#include "cmd/ParamCommand.h"
#include "cmd/RouteCommand.h"
#include "SimConvergenceLayer.h"
#include "bundling/AddressFamily.h"

#include "bundling/ContactManager.h"

using namespace dtnsim;

int
main(int argc, char** argv)
{
    // Initialize logging
    LogSim::init(oasys::LOG_INFO);

    // Initialize the simulator
    Simulator* s = new Simulator();
    Simulator::init(s);
    log_info("/sim", "simulator initializing...");

    // command line parameter vars
    std::string conffile("sim/top.conf");
    int random_seed;
    bool random_seed_set = false;
    
    oasys::Getopt::addopt(
        new oasys::StringOpt('c', "conf", &conffile, "<conf>", "config file"));

    oasys::Getopt::addopt(
        new oasys::IntOpt('s', "seed", &random_seed, "seed",
                          "random number generator seed", &random_seed_set));

    // Set up the command interpreter, then parse argv
    oasys::TclCommandInterp::init(argv[0]);
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    
    interp->reg(new ParamCommand());
    interp->reg(new RouteCommand());

    oasys::Getopt::getopt(argv[0], argc, argv);

    // Seed the random number generator
    if (!random_seed_set) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed = tv.tv_usec;
    }
    log_info("/sim", "random seed is %u\n", random_seed);
    srand(random_seed);

    AddressFamilyTable::init();
    ContactManager::init();
    
    // add simulator convergence layer (identifies by simcl) to cl list
    ConvergenceLayer::init_clayers();
    ConvergenceLayer::add_clayer("simcl", new SimConvergenceLayer());

    // Parse / exec the config file
    if (conffile.length() != 0) {
        if (interp->exec_file(conffile.c_str()) != 0) {
            log_err("/sim", "error in configuration file, exiting...");
            exit(1);
        }
    }

    // Run the event loop of simulator
    Simulator::instance()->run();
}
