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
    int         random_seed;
    bool        random_seed_set = false;
    bool	daemon = false;
    std::string conf_file("daemon/bundleNode.conf");
    std::string logfile("-");
    std::string loglevelstr;
    log_level_t loglevel;

    // Register all command line options
    new StringOpt('o', "output",
                  &logfile, "<output>",
                  "file name for logging output ([-o -] indicates stdout)");

    new StringOpt('l', NULL,
                  &loglevelstr, "<level>",
                  "default log level [debug|warn|info|crit]");

    new StringOpt('c', "conf",
                  &conf_file, "<conf>",
                  "config file");
    
    new BoolOpt('d', "daemon",
                &daemon, "run as a daemon");
    
    new BoolOpt('t', "tidy",
                &StorageConfig::instance()->tidy_,
                "clear database and initialize tables on startup");
    
    new BoolOpt(0, "create-db",
                &StorageConfig::instance()->init_,
                "initialize database on startup");

    new IntOpt('s', "seed",
               &random_seed, &random_seed_set, "<seed>",
               "random number generator seed");

    new IntOpt('i', 0, &testcmd.id_, "<id>", "set the test id");
    new BoolOpt('f', 0, &testcmd.fork_, "test scripts fork child daemons");

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
    Log::instance()->add_rotate_handler(SIGUSR1);
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
