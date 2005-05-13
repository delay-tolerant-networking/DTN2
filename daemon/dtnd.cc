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
#include <oasys/io/FileUtils.h>
#include <oasys/memory/Memory.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/StringBuffer.h>

#include "version.h"
#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"
#include "storage/StorageConfig.h"

/**
 * Namespace for the dtn daemon source code.
 */
namespace dtn {}

using namespace dtn;

int
main(int argc, char* argv[])
{
    TestCommand        testcmd;
    int                random_seed;
    bool               random_seed_set = false;
    bool	       daemon = false;
    std::string        conf_file;
    bool               conf_file_set = false;
    std::string        logfile("-");
    std::string        loglevelstr;
    oasys::log_level_t loglevel;
    bool	       print_version = false;
    int                console_port = 0;

#ifdef OASYS_DEBUG_MEMORY_ENABLED
    oasys::DbgMemInfo::init();
#endif

    // Register all command line options
    oasys::Getopt::addopt(
        new oasys::BoolOpt('v', "version", &print_version,
                           "print version information and exit"));
    
    oasys::Getopt::addopt(
        new oasys::StringOpt('o', "output", &logfile, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr, "<level>",
                             "default log level [debug|warn|info|crit]"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('c', "conf", &conf_file, "<conf>",
                             "set the configuration file", &conf_file_set));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('d', "daemon", &daemon,
                           "run as a daemon"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('t', "tidy", &StorageConfig::instance()->tidy_,
                           "clear database and initialize tables on startup"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt(0, "init-db", &StorageConfig::instance()->init_,
                           "initialize database on startup"));

    oasys::Getopt::addopt(
        new oasys::IntOpt('s', "seed", &random_seed, "<seed>",
                          "random number generator seed", &random_seed_set));

    oasys::Getopt::addopt(
        new oasys::IntOpt(0, "console-port", &console_port, "<port>",
                          "set the port for a console server (default off)"));
    
    oasys::Getopt::addopt(
        new oasys::IntOpt('i', 0, &testcmd.id_, "<id>",
                          "set the test id"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('f', 0, &testcmd.fork_,
                           "test scripts should fork child daemons"));

    int remainder = oasys::Getopt::getopt(argv[0], argc, argv);
    if (remainder != argc) {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        oasys::Getopt::usage("dtnd");
        exit(1);
    }

    if (print_version) {
        printf("%s\n", dtn_version);
        exit(0);
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

    const char* log = "/dtnd";
    oasys::Log::init(logfile.c_str(), loglevel, "", "~/.dtndebug");
    log_info(log, "Bundle Daemon Initializing...");

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
    log_info(log, "random seed is %u\n", random_seed);
    srand(random_seed);

    // Enable the global thread creation barrier so we make sure all
    // initialization completes before any threads are actually
    // started
    oasys::Thread::activate_start_barrier();
    
    // Set up the command interpreter
    oasys::TclCommandInterp::init(argv[0]);
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    testcmd.bind_vars();
    interp->reg(&testcmd);
    DTNServer::init_commands();
    APIServer::init_commands();

    // Set up components
    oasys::TimerSystem::init();
    oasys::Log::instance()->add_reparse_handler(SIGHUP);
    oasys::Log::instance()->add_rotate_handler(SIGUSR1);
    DTNServer::init_components();

    // Check the supplied config file and/or check for defaults, as
    // long as the user didn't explicitly call with no conf file 
    if (conf_file.length() != 0) {
        if (! oasys::FileUtils::readable(conf_file.c_str(), log)) {
            log_err(log, "configuration file \"%s\" not readable",
                    conf_file.c_str());
            exit(1);
        }

    } else if (!conf_file_set) {
        if (oasys::FileUtils::readable("/etc/dtn.conf", log)) {
            conf_file.assign("/etc/dtn.conf");
            
        } else if (oasys::FileUtils::readable("daemon/dtn.conf", log)) {
            conf_file.assign("daemon/dtn.conf");

        } else {
            log_warn(log, "can't read default config file "
                     "(tried /etc/dtn.conf and daemon/dtn.conf)...");
        }
    }

    // now if one was specified, parse it
    if (conf_file.length() != 0)
    {
        log_info(log, "parsing configuration file %s...", conf_file.c_str());
        
        if (interp->exec_file(conf_file.c_str()) != 0) {
            log_err(log, "error in configuration file, exiting...");
            exit(1);
        }
    }

    // Initialize the database
    DTNServer::init_datastore();

    // If we're running as --init-db, flush the database and we're all done.
    if (StorageConfig::instance()->init_ &&
        ! StorageConfig::instance()->tidy_)
    {
        DTNServer::close_datastore();
        log_info(log, "database initialization complete.");
        exit(0);
    }

    // Otherwise, start up everything else
    DTNServer::start();

    // if the config script wants us to run a test script, do so now
    if (testcmd.initscript_.length() != 0) {
        interp->exec_command(testcmd.initscript_.c_str());
    }

    // boot the application server
    APIServer* apisrv = new APIServer();
    apisrv->bind_listen_start(apisrv->local_addr_, apisrv->local_port_);

    // launch the console server
    if (console_port != 0) {
        log_info(log, "starting console on localhost:%d", console_port);
        interp->command_server("dtn", htonl(INADDR_LOOPBACK), console_port);
    }

    // Now that initialization is complete, disable the global thread
    // creation barrier and create any stalled threads
    oasys::Thread::release_start_barrier();

    // finally, run the main command or event loop (shouldn't return)
    if (daemon) {
        interp->event_loop();
    } else {
        interp->command_loop("dtn");
    }
    
    log_err(log, "command loop exited unexpectedly");
}
