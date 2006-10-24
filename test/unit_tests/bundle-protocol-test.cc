/*
 *    Copyright 2004-2006 Intel Corporation
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
#include "bundling/BundleProtocol.h"

using namespace oasys;
using namespace dtn;

bool
protocol_test(Bundle* b1)
{
    u_char buf[32768];
    int encode_len, decode_len;
    int errno_; const char* strerror_;

    encode_len = BundleProtocol::format_header_blocks(b1, buf, sizeof(buf));
    CHECK(encode_len != -1);
    
    Bundle* b2 = new Bundle(oasys::Builder::builder());
    b2->bundleid_ = 0;
    b2->payload_.init(0, BundlePayload::NODATA);
    
    decode_len = BundleProtocol::parse_header_blocks(b2, buf, encode_len);
    CHECK_EQUAL(decode_len, encode_len);

    CHECK_EQUALSTR(b1->source_.c_str(),    b2->source_.c_str());
    CHECK_EQUALSTR(b1->dest_.c_str(),      b2->dest_.c_str());
    CHECK_EQUALSTR(b1->custodian_.c_str(), b2->custodian_.c_str());
    CHECK_EQUALSTR(b1->replyto_.c_str(),   b2->replyto_.c_str());
    CHECK_EQUAL(b1->is_fragment_,          b2->is_fragment_);
    CHECK_EQUAL(b1->is_admin_,             b2->is_admin_);
    CHECK_EQUAL(b1->do_not_fragment_,      b2->do_not_fragment_);
    CHECK_EQUAL(b1->custody_requested_,    b2->custody_requested_);
    CHECK_EQUAL(b1->priority_,             b2->priority_);
    CHECK_EQUAL(b1->receive_rcpt_,         b2->receive_rcpt_);
    CHECK_EQUAL(b1->custody_rcpt_,         b2->custody_rcpt_);
    CHECK_EQUAL(b1->forward_rcpt_,         b2->forward_rcpt_);
    CHECK_EQUAL(b1->delivery_rcpt_,        b2->delivery_rcpt_);
    CHECK_EQUAL(b1->deletion_rcpt_,        b2->deletion_rcpt_);
    CHECK_EQUAL(b1->creation_ts_.seconds_, b2->creation_ts_.seconds_);
    CHECK_EQUAL(b1->creation_ts_.seqno_,   b2->creation_ts_.seqno_);
    CHECK_EQUAL(b1->expiration_,           b2->expiration_);
    CHECK_EQUAL(b1->frag_offset_,          b2->frag_offset_);
    CHECK_EQUAL(b1->orig_length_,          b2->orig_length_);
    CHECK_EQUAL(b1->payload_.length(),     b2->payload_.length());

    return true;
}

DECLARE_TEST(Basic)
{
    Bundle* bundle = new Bundle(oasys::Builder::builder());
    bundle->bundleid_ = 10;
    bundle->payload_.init(10, BundlePayload::NODATA);
    bundle->source_.assign("dtn://source.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");
    
    bundle->expiration_ = 1000;
    bundle->creation_ts_.seconds_ = 10101010;
    bundle->creation_ts_.seqno_ = 44556677;
    bundle->payload_.set_length(1024);

    CHECK(protocol_test(bundle));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Fragment)
{
    Bundle* bundle = new Bundle(oasys::Builder::builder());
    bundle->bundleid_ = 10;
    bundle->payload_.init(10, BundlePayload::NODATA);
    bundle->source_.assign("dtn://frag.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");
    
    bundle->expiration_ = 30;
    bundle->is_fragment_ = 1;
    bundle->frag_offset_ = 123456789;
    bundle->orig_length_ = 1234567890;
    
    CHECK(protocol_test(bundle));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AllFlags)
{
    Bundle* bundle = new Bundle(oasys::Builder::builder());
    bundle->bundleid_ = 10;
    bundle->payload_.init(10, BundlePayload::NODATA);
    bundle->source_.assign("dtn://source.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");

    bundle->is_admin_ = true;
    bundle->do_not_fragment_ = true;
    bundle->custody_requested_ = true;
    bundle->priority_ = 3;
    bundle->receive_rcpt_ = true;
    bundle->custody_rcpt_ = true;
    bundle->forward_rcpt_ = true;
    bundle->delivery_rcpt_ = true;
    bundle->deletion_rcpt_ = true;
    
    bundle->expiration_ = 1000;
    bundle->creation_ts_.seconds_  = 10101010;
    bundle->creation_ts_.seqno_ = 44556677;

    CHECK(protocol_test(bundle));

    return UNIT_TEST_PASSED;

    // XXX/demmer add tests for malformed / mangled headers, too long
    // sdnv's, etc
}

DECLARE_TESTER(BundleProtocolTest) {
    ADD_TEST(Basic);
    ADD_TEST(Fragment);
    ADD_TEST(AllFlags);
}

DECLARE_TEST_FILE(BundleProtocolTest, "bundle protocol test");
