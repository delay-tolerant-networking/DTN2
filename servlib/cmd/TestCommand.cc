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

#include "TestCommand.h"

namespace dtn {

TestCommand::TestCommand()
    : TclCommand("test"), id_(0), fork_(false)
{
    add_to_help("segfault", "Generate a segfault.");
    add_to_help("panic", "Trigger a panic.");
    add_to_help("assert", "Trigger a false assert.");
}

void
TestCommand::bind_vars()
{
    bind_i("id", &id_, "The test id. (Defualt is 0.)");
    bind_s("initscript", &initscript_, "The script to start.");
    bind_s("argv", &argv_, "A string to pass as the argument to the script.");
    bind_b("fork", &fork_, "A boolean to control forking. (Default is false.)");
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 2) {
        resultf("need a test subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    if (!strcmp(cmd, "segfault"))
    {
        int* x = NULL;
        (void)*x;
        NOTREACHED;
    }
    else if (!strcmp(cmd, "panic"))
    {
        PANIC("panic");
    }
    else if (!strcmp(cmd, "assert"))
    {
        ASSERT(0);
    }

    return TCL_ERROR;
}

} // namespace dtn
