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

#include <stdio.h>
#include <errno.h>
#include "cmd/Command.h"

using namespace std;

class TestModule : public CommandModule {
public:
    TestModule() : CommandModule("test") {
        bind_b("bool", &bool_);
        bind_addr("addr", &addr_);
        bind_i("int", &int_);
        bind_s("str", &str_);
    }

    virtual int exec(int argc, const char** argv, Tcl_Interp* interp) {
        if (argc >= 2 && !strcmp(argv[1], "error")) {
            resultf("error in test::exec: %d", 100);
            return TCL_ERROR;
        }
            
        set_result("test::exec ");
        for (int i = 0; i < argc; ++i) {
            append_result(argv[i]);
            append_result(" ");
        }
        return TCL_OK;
    }

    bool bool_;
    in_addr_t addr_;
    int  int_;
    std::string str_;
};

int
main(int argc, char* argv[])
{
    Log::init(LOG_INFO);
    
    CommandInterp::init("test");
    
    CommandModule* mod = new CommandModule("mod1");
    TestModule*   test = new TestModule();

    CommandInterp::instance()->reg(mod);
    CommandInterp::instance()->reg(test);

    CommandInterp::instance()->loop("test");
}

