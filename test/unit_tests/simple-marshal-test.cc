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
#include <oasys/debug/Log.h>
#include <oasys/serialize/MarshalSerialize.h>
#include "bundling/Bundle.h"

using namespace dtn;
using namespace oasys;

int
main(int argc, const char** argv)
{
    Log::init();
    
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.assign("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

    MarshalSize s;
    if (s.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshal sizing");
        exit(1);
    }

    size_t sz = s.size();
    logf("/test", LOG_INFO, "marshalled size is %d", (u_int)sz);

    u_char* buf = (u_char*)malloc(sz);

    Marshal m(buf, sz);
    if (m.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshalling");
        exit(1);
    }

    Unmarshal u(buf, sz);
    if (u.action(&b2) != 0) {
        logf("/test", LOG_ERR, "error in unmarshalling");
        exit(1);
    }
    
    if (b.bundleid_ != b2.bundleid_) {
        logf("/test", LOG_ERR, "error: bundle id mismatch");
        exit(1);
    }

    if (!b2.source_.valid()) {
        logf("/test", LOG_ERR, "error: b2 source not valid");
        exit(1);
    }


#define COMPARE(x)                                                      \
    if (b.source_.x().compare(b2.source_.x()) != 0) {                   \
         logf("/test", LOG_ERR, "error: mismatch of %s: %s != %s",      \
              #x, b.source_.x().c_str(), b2.source_.x().c_str());       \
    }

    COMPARE(tuple);
    COMPARE(region);
    COMPARE(admin);

    logf("/test", LOG_INFO, "simple marshal test passed");
}
