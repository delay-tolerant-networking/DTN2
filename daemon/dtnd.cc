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
#include <oasys/memory/Memory.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/StringBuffer.h>

#include "version.h"
#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"

/**
 * Namespace for the dtn daemon source code.
 */
namespace dtn {}

using namespace dtn;

// Configuration variables
int         g_random_seed      = 0;
bool        g_random_seed_set  = false;
bool	    g_daemon           = false;
bool        g_conf_file_set    = false;
std::string g_conf_file        = "";
bool	    g_print_version    = false;
int         g_console_port     = 0;

std::string        loglevelstr;
oasys::log_level_t loglevel;
std::string        logfile     = "-";

TestCommand g_testcmd;

oasys::StorageConfig g_storage_config("storage",
                                      "berkeleydb",
                                      false,
                                      false,
                                      3,
                                      "DTN",
                                      "/var/dtn/db",
                                      "dberror.log",
                                      0);

void
get_options(int argc, char* argv[])
{
    // Register all command line options
    oasys::Getopt::addopt(
        new oasys::BoolOpt('v', "version", &g_print_version,
                           "print version information and exit"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('o', "output", &logfile, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr, "<level>",
                             "default log level [debug|warn|info|crit]"));

    oasys::Getopt::addopt(
        new oasys::StringOpt('c', "conf", &g_conf_file, "<conf>",
                             "set the configuration file", &g_conf_file_set));
    oasys::Getopt::addopt(

        new oasys::BoolOpt('d', "daemon", &g_daemon,
                           "run as a daemon"));

    oasys::Getopt::addopt(
        new oasys::BoolOpt('t', "tidy", &g_storage_config.tidy_,
                           "clear database and initialize tables on startup"));

    oasys::Getopt::addopt(
        new oasys::BoolOpt(0, "init-db", &g_storage_config.init_,
                           "initialize database on startup"));

    oasys::Getopt::addopt(
        new oasys::IntOpt('s', "seed", &g_random_seed, "<seed>",
                          "random number generator seed", &g_random_seed_set));

    oasys::Getopt::addopt(
        new oasys::IntOpt(0, "console-port", &g_console_port, "<port>",
                          "set the port for a console server (default off)"));
    
    oasys::Getopt::addopt(
        new oasys::IntOpt('i', 0, &g_testcmd.id_, "<id>",
                          "set the test id"));
    
    oasys::Getopt::addopt(
        new oasys::BoolOpt('f', 0, &g_testcmd.fork_,
                           "test scripts should fork child daemons"));

    int remainder = oasys::Getopt::getopt(argv[0], argc, argv);
    if (remainder != argc) 
    {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        oasys::Getopt::usage("dtnd");
        exit(1);
    }
}

void
seed_random(bool seed_set, int seed)
{
    // seed the random number generator
    if (!seed_set) 
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = tv.tv_usec;
    }
    
    log_info("/dtnd", "random seed is %u\n", seed);
    srand(seed);
}

void
init_log(const std::string& loglevelstr)
{
    // Parse the debugging level argument
    if (loglevelstr.length() == 0) 
    {
        loglevel = LOG_DEFAULT_THRESHOLD;
    }
    else 
    {
        loglevel = oasys::str2level(loglevelstr.c_str());
        if (loglevel == oasys::LOG_INVALID) 
        {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr.c_str());
            exit(1);
        }
    }
    oasys::Log::init(logfile.c_str(), loglevel, "", "~/.dtndebug");
    oasys::Log::instance()->add_reparse_handler(SIGHUP);
    oasys::Log::instance()->add_rotate_handler(SIGUSR1);
}

void
init_testcmd(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i) {
        g_testcmd.argv_.append(argv[i]);
        g_testcmd.argv_.append(" ");
    }

    g_testcmd.bind_vars();
    oasys::TclCommandInterp::instance()->reg(&g_testcmd);
}

int
main(int argc, char* argv[])
{
#ifdef OASYS_DEBUG_MEMORY_ENABLED
    oasys::DbgMemInfo::init();
#endif

    get_options(argc, argv);

    if (g_print_version) 
    {
        printf("%s\n", dtn_version);
        exit(0);
    }

    init_log(loglevelstr);
    if (oasys::TclCommandInterp::init(argv[0]) != 0)
    {
        log_crit("/dtnd", "Can't init TCL");
        exit(1);
    }

    seed_random(g_random_seed_set, g_random_seed);

    // stop thread creation b/c of initialization dependencies
    oasys::Thread::activate_start_barrier();
    oasys::TimerSystem::instance()->start();
    
    DTNServer* dtnserver = new DTNServer(&g_storage_config);
    APIServer* apiserver = new APIServer();

    dtnserver->init();
    apiserver->init();
    init_testcmd(argc, argv);

    dtnserver->parse_conf_file(g_conf_file, g_conf_file_set);

    // If we're running as --init-db, make an empty database and exit
    if (g_storage_config.init_ && !g_storage_config.tidy_)
    {
        dtnserver->start_datastore();
        delete_z(dtnserver);

        log_info("/dtnd", "database initialization complete.");
        exit(0);
    }
    
    dtnserver->start();
    apiserver->bind_listen_start(APIServer::local_addr_, 
                                 APIServer::local_port_);
    oasys::Thread::release_start_barrier(); // run blocked threads

    // if the test script specified something to run for the test,
    // then execute it now
    if (g_testcmd.initscript_.length() != 0) {
        oasys::TclCommandInterp::instance()->
            exec_command(g_testcmd.initscript_.c_str());
    }

    // launch the console server
    if (g_console_port != 0) {
        log_info("/dtnd", "starting console on localhost:%d", g_console_port);
        oasys::TclCommandInterp::instance()->
            command_server("dtn", htonl(INADDR_LOOPBACK), g_console_port);
    }
    
    if (g_daemon) {
        oasys::TclCommandInterp::instance()->event_loop();
    } else {
        oasys::TclCommandInterp::instance()->command_loop("dtn");
    }
    
    log_err("/dtnd", "command loop exited unexpectedly");

    delete_z(dtnserver);
}
