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

#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include <oasys/thread/SpinLock.h>

using namespace dtn;
using namespace oasys;

int
payload_test(BundlePayload::location_t initial_location,
             BundlePayload::location_t expected_location)
{
    u_char buf[64];
    const u_char* data;
    BundlePayload p;
    bzero(buf, sizeof(buf));

    log_debug("/test", "checking initialization");
    p.init(new SpinLock(), 1, initial_location);
    CHECK_EQUAL(p.location(), initial_location);
    CHECK(p.length() == 0);
    
    log_debug("/test", "checking set_data");
    p.set_data((const u_char*)"abcdefghijklmnopqrstuvwxyz", 26);
    CHECK_EQUAL(p.location(), expected_location);
    CHECK_EQUAL(p.length(), 26);
    
    log_debug("/test", "checking read_data");
    data = p.read_data(0, 26, buf);
    CHECK_EQUALSTR((char*)data, "abcdefghijklmnopqrstuvwxyz");
    data = p.read_data(10, 5, buf);
    CHECK_EQUALSTRN((char*)data, "klmno", 5);
    
    log_debug("/test", "checking truncate");
    p.truncate(10);
    data = p.read_data(0, 10, buf);
    CHECK_EQUAL(p.length(), 10);
    CHECK_EQUALSTRN((char*)data, "abcdefghij", 10);

    log_debug("/test", "checking append_data");
    p.set_length(18);
    p.reopen_file();
    CHECK(p.is_file_open());
    p.append_data((u_char*)"ABCDE", 5);
    CHECK(p.is_file_open());
    data = p.read_data(0, 15, buf, BundlePayload::KEEP_FILE_OPEN);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDE", 15);
    CHECK(p.is_file_open());

    p.append_data((u_char*)"FGH", 3);
    CHECK(p.is_file_open());
    data = p.read_data(0, 18, buf);
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDEFGH", 18);
    p.close_file();
    CHECK(! p.is_file_open());
    
    log_debug("/test", "checking write_data overwrite");
    p.reopen_file();
    p.write_data((u_char*)"BCD", 1, 3);
    data = p.read_data(0, 18, buf);
    p.close_file();
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "aBCDefghijABCDEFGH", 18);
    CHECK(! p.is_file_open());

    log_debug("/test", "checking write_data with gap");
    p.reopen_file();
    p.set_length(30);
    p.write_data((u_char*)"XXXXX", 25, 5);
    CHECK_EQUAL(p.length(), 30);

    p.reopen_file();
    p.write_data((u_char*)"_______", 18, 7);
    data = p.read_data(0, 30, buf);
    p.close_file();
    CHECK_EQUAL(p.length(), 30);
    CHECK_EQUALSTR((char*)data, "aBCDefghijABCDEFGH_______XXXXX");
    CHECK(! p.is_file_open());

    log_debug("/test", "checking FORCE_COPY");
    p.reopen_file();
    data = p.read_data(0, 30, buf, BundlePayload::FORCE_COPY);
    CHECK(data == buf);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ForcedMemoryPayloadTest) {
    return payload_test(BundlePayload::MEMORY, BundlePayload::MEMORY);
}

DECLARE_TEST(ForcedDiskPayloadTest) {
    return payload_test(BundlePayload::DISK, BundlePayload::DISK);
}

DECLARE_TEST(MemoryPayloadTest) {
    BundlePayload::mem_threshold_ = 100;
    return payload_test(BundlePayload::UNDETERMINED, BundlePayload::MEMORY);
}

DECLARE_TEST(DiskPayloadTest) {
    BundlePayload::mem_threshold_ = 10;
    return payload_test(BundlePayload::UNDETERMINED, BundlePayload::DISK);
}

DECLARE_TESTER(BundlePayloadTester) {
    ADD_TEST(ForcedMemoryPayloadTest);
    ADD_TEST(ForcedDiskPayloadTest);
    ADD_TEST(MemoryPayloadTest);
    ADD_TEST(DiskPayloadTest);
}

int
main(int argc, const char** argv)
{
    system("rm -rf .bundle-payload-test");
    system("mkdir  .bundle-payload-test");
    BundlePayload::payloaddir_.assign(".bundle-payload-test");
    
    BundlePayloadTester t("bundle payload test");
    t.run_tests(argc, argv);

    system("rm -rf .bundle-payload-test");
}
