/*
 *    Copyright 2005-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#include <oasys/util/UnitTest.h>

#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include <oasys/thread/SpinLock.h>
#include "storage/BundleStore.h"
#include "storage/DTNStorageConfig.h"

using namespace dtn;
using namespace oasys;

int
payload_test(BundlePayload::location_t initial_location,
             BundlePayload::location_t expected_location)
{
    u_char buf[64];
    const u_char* data;
    SpinLock l;
    BundlePayload p(&l);
    bzero(buf, sizeof(buf));

    int errno_; const char* strerror_;

    log_debug("/test", "checking initialization");
    p.init(1, initial_location);
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
    p.append_data((u_char*)"ABCDE", 5);
    data = p.read_data(0, 15, buf);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDE", 15);

    p.append_data((u_char*)"FGH", 3);
    data = p.read_data(0, 18, buf);
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDEFGH", 18);
    
    log_debug("/test", "checking write_data overwrite");
    p.write_data((u_char*)"BCD", 1, 3);
    data = p.read_data(0, 18, buf);
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "aBCDefghijABCDEFGH", 18);

    log_debug("/test", "checking write_data with gap");
    p.set_length(30);
    p.write_data((u_char*)"XXXXX", 25, 5);
    CHECK_EQUAL(p.length(), 30);

    p.write_data((u_char*)"_______", 18, 7);
    data = p.read_data(0, 30, buf);
    CHECK_EQUAL(p.length(), 30);
    CHECK_EQUALSTR((char*)data, "aBCDefghijABCDEFGH_______XXXXX");

    log_debug("/test", "checking FORCE_COPY");
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
    DTNStorageConfig cfg("", "memorydb", "", "");
    cfg.init_ = true;
    cfg.payload_dir_.assign(".bundle-payload-test");
    cfg.leave_clean_file_ = false;
    Log::init();
    oasys::DurableStore ds("/test/ds");
    ds.create_store(cfg);
    BundleStore::init(cfg, &ds);
    
    BundlePayloadTester t("bundle payload test");
    t.run_tests(argc, argv, false);

    system("rm -rf .bundle-payload-test");
}
