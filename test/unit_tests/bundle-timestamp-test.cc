/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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

#include <oasys/util/UnitTest.h>
#include "bundling/BundleProtocol.h"

using namespace dtn;

DECLARE_TEST(NowTest) {
    struct timeval now, test;
    u_int64_t ts;

    ::gettimeofday(&now, 0);

    BundleProtocol::set_timestamp(&ts, &now);
    BundleProtocol::get_timestamp(&test, &ts);

    CHECK_EQUAL(now.tv_sec,  test.tv_sec);
    CHECK_EQUAL(now.tv_usec, test.tv_usec);
    
    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TEST(AlignmentTest) {
    struct timeval now, test;
    char buf[16];
    u_int64_t* ts;

    ::gettimeofday(&now, 0);
    for (int i = 0; i < 8; ++i) {
        ts = (u_int64_t*)&buf[i];
    
        BundleProtocol::set_timestamp(ts, &now);
        BundleProtocol::get_timestamp(&test, ts);

        CHECK_EQUAL(now.tv_sec,  test.tv_sec);
        CHECK_EQUAL(now.tv_usec, test.tv_usec);
    }
    
    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TESTER(BundleTimestampTester) {
    ADD_TEST(NowTest);
    ADD_TEST(AlignmentTest);
}

DECLARE_TEST_FILE(BundleTimestampTester, "bundle timestamp test");