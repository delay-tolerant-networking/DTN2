/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/debug/Log.h>
#include <oasys/io/NetUtils.h>
#include <oasys/tclcmd/ConsoleCommand.h>
#include <oasys/tclcmd/TclCommand.h>
#include <oasys/util/Getopt.h>
#include <dtn_api.h>

typedef std::map<int, dtn_handle_t> HandleMap;

struct State : public oasys::Singleton<State> {
    State() : handle_num_(0) {}
        
    HandleMap handles_;
    int handle_num_;
};

template <> State* oasys::Singleton<State>::instance_ = 0;

//----------------------------------------------------------------------
class DTNOpenCommand : public oasys::TclCommand {
public:
    DTNOpenCommand() : TclCommand("dtn_open") {}
    int exec(int argc, const char **argv,  Tcl_Interp* interp)
    {
        (void)argc;
        (void)argv;
        (void)interp;

        if (argc != 1) {
            wrong_num_args(argc, argv, 1, 1, 1);
            return TCL_ERROR;
        }

        dtn_handle_t h = dtn_open();
        if (h == NULL) {
            resultf("can't connect to dtn daemon");
            return TCL_ERROR;
        }

        int n = State::instance()->handle_num_++;
        State::instance()->handles_[n] = h;

        resultf("%d", n);
        return TCL_OK;
    }
};

//----------------------------------------------------------------------
class DTNCloseCommand : public oasys::TclCommand {
public:
    DTNCloseCommand() : TclCommand("dtn_close") {}
    int exec(int argc, const char **argv,  Tcl_Interp* interp)
    {
        (void)argc;
        (void)argv;
        (void)interp;

        if (argc != 2) {
            wrong_num_args(argc, argv, 1, 2, 2);
            return TCL_ERROR;
        }

        int n = atoi(argv[1]);
        HandleMap::iterator iter = State::instance()->handles_.find(n);
        if (iter == State::instance()->handles_.end()) {
            resultf("invalid dtn handle %d", n);
            return TCL_ERROR;
        }

        dtn_handle_t h = iter->second;
        dtn_close(h);

        return TCL_OK;
    }
};

//----------------------------------------------------------------------
class ShutdownCommand : public oasys::TclCommand {
public:
    ShutdownCommand() : TclCommand("shutdown") {}
    static void call_exit(void* clientData);
    int exec(int argc, const char **argv,  Tcl_Interp* interp);
};

void
ShutdownCommand::call_exit(void* clientData)
{
    (void)clientData;
    exit(0);
}

//----------------------------------------------------------------------
int
ShutdownCommand::exec(int argc, const char **argv, Tcl_Interp* interp)
{
    (void)argc;
    (void)argv;
    (void)interp;
    Tcl_CreateTimerHandler(0, ShutdownCommand::call_exit, 0);
    return TCL_OK;
}

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    oasys::TclCommandInterp* interp;
    oasys::ConsoleCommand console_cmd;
    std::string conf_file;
    bool conf_file_set = false;
    bool daemon = false;

    oasys::Log::init();
    
    oasys::TclCommandInterp::init("dtn-test");
    interp = oasys::TclCommandInterp::instance();
    
    oasys::Getopt::addopt(
        new oasys::StringOpt('c', "conf", &conf_file, "<conf>",
                             "set the configuration file", &conf_file_set));

    oasys::Getopt::addopt(
        new oasys::BoolOpt('d', "daemon", &daemon,
                           "run as a daemon"));
    
    int remainder = oasys::Getopt::getopt(argv[0], argc, argv);
    if (remainder != argc) 
    {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        oasys::Getopt::usage("dtn-test");
        exit(1);
    }
    
    interp->reg(&console_cmd);
    interp->reg(new DTNOpenCommand());
    interp->reg(new DTNCloseCommand());
    interp->reg(new ShutdownCommand());

    if (conf_file_set) {
        interp->exec_file(conf_file.c_str());
    }
    
    log_notice("/dtn-test", "dtn-test starting up...");
    
    if (console_cmd.port_ != 0) {
        log_notice("/dtn-test", "starting console on %s:%d",
                   intoa(console_cmd.addr_), console_cmd.port_);
        interp->command_server("dtn-test", console_cmd.addr_, console_cmd.port_);
    }

    if (daemon || (console_cmd.stdio_ == false)) {
        oasys::TclCommandInterp::instance()->event_loop();
    } else {
        oasys::TclCommandInterp::instance()->command_loop("dtn-test");
    }

    log_notice("/dtn-test", "dtn-test shutting down...");
}
