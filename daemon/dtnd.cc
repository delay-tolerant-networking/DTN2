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

#include <oasys/debug/FatalSignals.h>
#include <oasys/debug/Log.h>
#include <oasys/io/NetUtils.h>
#include <oasys/memory/Memory.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/tclcmd/ConsoleCommand.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Getopt.h>
#include <oasys/util/Random.h>
#include <oasys/util/StringBuffer.h>

#include "applib/APIServer.h"
#include "cmd/TestCommand.h"
#include "servlib/DTNServer.h"

extern const char* dtn_version;

/**
 * Namespace for the dtn daemon source code.
 */
namespace dtn {

/**
 * Thin class that implements the daemon itself.
 */
class DTND {
public:
    DTND();
    int main(int argc, char* argv[]);

protected:
    bool                  daemonize_;
    int                   daemonize_pipe_[2];
    int                   random_seed_;
    bool                  random_seed_set_;
    std::string           conf_file_;
    bool                  conf_file_set_;
    bool                  print_version_;
    std::string           loglevelstr_;
    oasys::log_level_t    loglevel_;
    std::string           logfile_;
    TestCommand*          testcmd_;
    oasys::ConsoleCommand* consolecmd_;
    oasys::StorageConfig  storage_config_;

    void get_options(int argc, char* argv[]);
    void daemonize();
    void notify_parent(char status);
    void notify_and_exit(char status);
    void seed_random();
    void init_log();
    void init_testcmd(int argc, char* argv[]);
    void run_console();
};

//----------------------------------------------------------------------
DTND::DTND()
    : daemonize_(false),
      random_seed_(0),
      random_seed_set_(false),
      conf_file_(""),
      conf_file_set_(false),
      print_version_(false),
      loglevelstr_(""),
      loglevel_(LOG_DEFAULT_THRESHOLD),
      logfile_("-"),
      testcmd_(NULL),
      consolecmd_(NULL),
      storage_config_("storage",	// command name
                      "berkeleydb",	// storage type
                      "DTN",		// DB name
                      "/var/dtn/db")	// DB directory
{
    // override defaults from oasys storage config
    storage_config_.db_max_tx_ = 1000;

    daemonize_pipe_[0] = -1;
    daemonize_pipe_[1] = -1;

    testcmd_    = new TestCommand();
    consolecmd_ = new oasys::ConsoleCommand("dtn% ");
}

//----------------------------------------------------------------------
void
DTND::get_options(int argc, char* argv[])
{
    
    // Register all command line options
    oasys::Getopt opts;
    opts.addopt(
        new oasys::BoolOpt('v', "version", &print_version_,
                           "print version information and exit"));

    opts.addopt(
        new oasys::StringOpt('o', "output", &logfile_, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    opts.addopt(
        new oasys::StringOpt('l', NULL, &loglevelstr_, "<level>",
                             "default log level [debug|warn|info|crit]"));

    opts.addopt(
        new oasys::StringOpt('c', "conf", &conf_file_, "<conf>",
                             "set the configuration file", &conf_file_set_));
    opts.addopt(
        new oasys::BoolOpt('d', "daemonize", &daemonize_,
                           "run as a daemon"));

    opts.addopt(
        new oasys::BoolOpt('t', "tidy", &storage_config_.tidy_,
                           "clear database and initialize tables on startup"));

    opts.addopt(
        new oasys::BoolOpt(0, "init-db", &storage_config_.init_,
                           "initialize database on startup"));

    opts.addopt(
        new oasys::IntOpt('s', "seed", &random_seed_, "<seed>",
                          "random number generator seed", &random_seed_set_));

    opts.addopt(
        new oasys::InAddrOpt(0, "console-addr", &consolecmd_->addr_, "<addr>",
                             "set the console listening addr (default off)"));
    
    opts.addopt(
        new oasys::UInt16Opt(0, "console-port", &consolecmd_->port_, "<port>",
                             "set the console listening port (default off)"));
    
    opts.addopt(
        new oasys::IntOpt('i', 0, &testcmd_->id_, "<id>",
                          "set the test id"));
    
    opts.addopt(
        new oasys::BoolOpt('f', 0, &testcmd_->fork_,
                           "test scripts should fork child daemons"));

    int remainder = opts.getopt(argv[0], argc, argv);
    if (remainder != argc) 
    {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        opts.usage("dtnd");
        exit(1);
    }
}

//----------------------------------------------------------------------
void
DTND::daemonize()
{
    if (!daemonize_) {
        return;
    }
       
    /*
     * If we're running as a daemon, we fork the parent process as
     * soon as possible, and in particular, before TCL has been
     * initialized.
     *
     * Then, the parent waits on a pipe for the child to notify it as
     * to whether or not the initialization succeeded.
     */
    fclose(stdin);
    
    if (pipe(daemonize_pipe_) != 0) {
        fprintf(stderr, "error creating pipe for daemonize process: %s",
                strerror(errno));
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "error forking daemon process: %s",
                strerror(errno));
        exit(1);
    }

    if (pid > 0) {
        // the parent closes the write half of the pipe, then waits
        // for the child to return its status on the read half
        close(daemonize_pipe_[1]);
        
        char status;
        int count = read(daemonize_pipe_[0], &status, 1);
        if (count != 1) {
            fprintf(stderr, "error reading from daemon pipe: %s",
                    strerror(errno));
            exit(1);
        }

        close(daemonize_pipe_[1]);
        exit(status);

    } else {
        // the child continues on in a new session, closing the
        // unneeded read half of the pipe
        close(daemonize_pipe_[0]);
        setsid();
    }
}

//----------------------------------------------------------------------
void
DTND::notify_parent(char status)
{
    if (daemonize_) {
        write(daemonize_pipe_[1], &status, 1);
        close(daemonize_pipe_[1]);
    }
}

//----------------------------------------------------------------------
void
DTND::notify_and_exit(char status)
{
    notify_parent(status);
    exit(status);
}

//----------------------------------------------------------------------
void
DTND::seed_random()
{
    // seed the random number generator
    if (!random_seed_set_) 
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed_ = tv.tv_usec;
    }
    
    log_notice("/dtnd", "random seed is %u\n", random_seed_);
    oasys::Random::seed(random_seed_);
}

//----------------------------------------------------------------------
void
DTND::init_log()
{
    // Parse the debugging level argument
    if (loglevelstr_.length() == 0) 
    {
        loglevel_ = oasys::LOG_NOTICE;
    }
    else 
    {
        loglevel_ = oasys::str2level(loglevelstr_.c_str());
        if (loglevel_ == oasys::LOG_INVALID) 
        {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr_.c_str());
            notify_and_exit(1);
        }
    }
    oasys::Log::init(logfile_.c_str(), loglevel_, "", "~/.dtndebug");
    oasys::Log::instance()->add_reparse_handler(SIGHUP);
    oasys::Log::instance()->add_rotate_handler(SIGUSR1);

    if (daemonize_) {
        if (logfile_ == "-") {
            fprintf(stderr, "daemon mode requires setting of -o <logfile>\n");
            notify_and_exit(1);
        }
        
        oasys::Log::instance()->redirect_stdio();
    }
}

//----------------------------------------------------------------------
void
DTND::init_testcmd(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i) {
        testcmd_->argv_.append(argv[i]);
        testcmd_->argv_.append(" ");
    }

    testcmd_->bind_vars();
    oasys::TclCommandInterp::instance()->reg(testcmd_);
}

//----------------------------------------------------------------------
void
DTND::run_console()
{
    // launch the console server
    if (consolecmd_->port_ != 0) {
        log_info("/dtnd", "starting console on %s:%d",
                 intoa(consolecmd_->addr_), consolecmd_->port_);
        
        oasys::TclCommandInterp::instance()->
            command_server(consolecmd_->prompt_.c_str(),
                           consolecmd_->addr_, consolecmd_->port_);
    }
    
    if (daemonize_ || (consolecmd_->stdio_ == false)) {
        oasys::TclCommandInterp::instance()->event_loop();
    } else {
        oasys::TclCommandInterp::instance()->
            command_loop(consolecmd_->prompt_.c_str());
    }
}

//----------------------------------------------------------------------
int
DTND::main(int argc, char* argv[])
{
#ifdef OASYS_DEBUG_MEMORY_ENABLED
    oasys::DbgMemInfo::init();
#endif

    get_options(argc, argv);

    if (print_version_) 
    {
        printf("%s\n", dtn_version);
        exit(0);
    }

    daemonize();

    init_log();
    
    log_notice("/dtnd", "DTN daemon starting up... (pid %d)", getpid());
    oasys::FatalSignals::init("dtnd");

    if (oasys::TclCommandInterp::init(argv[0]) != 0)
    {
        log_crit("/dtnd", "Can't init TCL");
        notify_and_exit(1);
    }

    seed_random();

    // stop thread creation b/c of initialization dependencies
    oasys::Thread::activate_start_barrier();

    DTNServer* dtnserver = new DTNServer("/dtnd", &storage_config_);
    APIServer* apiserver = new APIServer();

    dtnserver->init();

    oasys::TclCommandInterp::instance()->reg(consolecmd_);
    init_testcmd(argc, argv);

    if (! dtnserver->parse_conf_file(conf_file_, conf_file_set_)) {
        log_err("/dtnd", "error in configuration file, exiting...");
        notify_and_exit(1);
    }

    if (storage_config_.init_)
    {
        log_notice("/dtnd", "initializing persistent data store");
    }

    if (! dtnserver->init_datastore()) {
        log_err("/dtnd", "error initializing data store, exiting...");
        notify_and_exit(1);
    }
    
    // If we're running as --init-db, make an empty database and exit
    if (storage_config_.init_ && !storage_config_.tidy_)
    {
        dtnserver->close_datastore();
        log_info("/dtnd", "database initialization complete.");
        notify_and_exit(0);
    }

    // if we've daemonized, now is the time to notify our parent
    // process that we've successfully initialized
    notify_parent(0);
    
    dtnserver->start();
    apiserver->bind_listen_start(apiserver->local_addr(), 
                                 apiserver->local_port());
    oasys::Thread::release_start_barrier(); // run blocked threads

    // if the test script specified something to run for the test,
    // then execute it now
    if (testcmd_->initscript_.length() != 0) {
        oasys::TclCommandInterp::instance()->
            exec_command(testcmd_->initscript_.c_str());
    }

    run_console();

    log_notice("/dtnd", "command loop exited... shutting down daemon");
    oasys::TclCommandInterp::shutdown();
    dtnserver->shutdown();

    // doesn't return
    NOTREACHED;

    return 0;
}

} // namespace dtn

int
main(int argc, char* argv[])
{
    dtn::DTND dtnd;
    dtnd.main(argc, argv);
}
