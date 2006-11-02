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

Bundle*
new_bundle()
{
    static int next_bundleid = 10;
    
    Bundle* b = new Bundle(oasys::Builder::builder());
    b->bundleid_		= next_bundleid++;
    b->is_fragment_		= false;
    b->is_admin_		= false;
    b->do_not_fragment_		= false;
    b->in_datastore_       	= false;
    b->custody_requested_	= false;
    b->local_custody_      	= false;
    b->singleton_dest_     	= true;
    b->priority_		= 0;
    b->receive_rcpt_		= false;
    b->custody_rcpt_		= false;
    b->forward_rcpt_		= false;
    b->delivery_rcpt_		= false;
    b->deletion_rcpt_		= false;
    b->app_acked_rcpt_		= false;
    b->orig_length_		= 0;
    b->frag_offset_		= 0;
    b->expiration_		= 0;
    b->owner_              	= "";
    b->creation_ts_.seconds_ 	= 0;
    b->creation_ts_.seqno_   	= 0;
    b->payload_.init(b->bundleid_);
    return b;
}

#define NO_CHUNKS      1
#define ONEBYTE_CHUNKS 2
#define RANDOM_CHUNKS  3 

int
protocol_test(Bundle* b1, int chunks)
{
#define AVG_CHUNK 16
    
    u_char payload1[32768];
    u_char payload2[32768];
    u_char buf[32768];
    int encode_len, decode_len;
    int errno_; const char* strerror_;

    Bundle* b2;
    
    encode_len = BundleProtocol::format_bundle(b1, buf, sizeof(buf));
    CHECK(encode_len > 0);

    // extract the payload before we swing the payload directory
    b1->payload_.read_data(0, b1->payload_.length(),
                           payload1, BundlePayload::FORCE_COPY);
        
    b2 = new_bundle();

    if (chunks == NO_CHUNKS) {
        decode_len = BundleProtocol::parse_bundle(b2, buf, encode_len);
    } else {
        bool complete = false;
        decode_len = 0;
        do {
            size_t chunk_size =
                (chunks == ONEBYTE_CHUNKS) ? 1 : oasys::Random::rand(AVG_CHUNK);
            
            int cc = BundleProtocol::consume(b2,
                                             &buf[decode_len],
                                             chunk_size,
                                             &complete);

            ASSERT((cc == (int)chunk_size) || complete);
            decode_len += cc;
            ASSERT(decode_len <= encode_len);
        } while (!complete);
    }

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

    b2->payload_.read_data(0, b2->payload_.length(),
                           payload2, BundlePayload::FORCE_COPY);

    bool payload_ok = true;
    for (u_int i = 0; i < b2->payload_.length(); ++i) {
        if (payload1[i] != payload2[i]) {
            log_err("/test", "payload mismatch at byte %d: 0x%x != 0x%x",
                    i, payload1[i], payload2[i]);
            payload_ok = false;
        }
    }
    CHECK(payload_ok);
    
    delete b1;
    delete b2;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Init) {
    BundleProtocol::init_default_processors();
    return UNIT_TEST_PASSED;
}

#define DECLARE_BP_TESTS(_what)                                 \
DECLARE_TEST(_what ## NoChunks) {                               \
    return protocol_test(init_ ## _what (), NO_CHUNKS);         \
}                                                               \
                                                                \
DECLARE_TEST(_what ## OneByteChunks) {                          \
    return protocol_test(init_ ## _what (), ONEBYTE_CHUNKS);    \
}                                                               \
                                                                \
DECLARE_TEST(_what ## RandomChunks) {                           \
    return protocol_test(init_ ## _what (), RANDOM_CHUNKS);     \
}                                                               \

#define ADD_BP_TESTS(_what)                     \
ADD_TEST(_what ## NoChunks);                    \
ADD_TEST(_what ## OneByteChunks);               \
ADD_TEST(_what ## RandomChunks);

Bundle* init_Basic()
{
    Bundle* bundle = new_bundle();
    bundle->payload_.set_data("test payload");
    
    bundle->source_.assign("dtn://source.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");
    bundle->expiration_ = 1000;
    bundle->creation_ts_.seconds_ = 10101010;
    bundle->creation_ts_.seqno_ = 44556677;

    return bundle;
}

DECLARE_BP_TESTS(Basic);

Bundle* init_Fragment()
{
    Bundle* bundle = new_bundle();
    bundle->payload_.set_data("test payload");
    
    bundle->source_.assign("dtn://frag.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");
    
    bundle->expiration_ = 30;
    bundle->is_fragment_ = 1;
    bundle->frag_offset_ = 123456789;
    bundle->orig_length_ = 1234567890;
    
    return bundle;
}

DECLARE_BP_TESTS(Fragment);

Bundle* init_AllFlags()
{
    Bundle* bundle = new_bundle();
    bundle->payload_.set_data("test payload");
    
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

    return bundle;
}

DECLARE_BP_TESTS(AllFlags);

Bundle* init_RandomPayload()
{
    Bundle* bundle = new_bundle();

    u_char payload[4096];
    for (u_int i = 0; i < sizeof(payload); ++i) {
        payload[i] = oasys::Random::rand(26) + 'a';
    }
    bundle->payload_.set_data(payload, sizeof(payload));
    
    bundle->source_.assign("dtn://source.dtn/test");
    bundle->dest_.assign("dtn://dest.dtn/test");
    bundle->custodian_.assign("dtn:none");
    bundle->replyto_.assign("dtn:none");

    return bundle;
}

DECLARE_BP_TESTS(RandomPayload);

DECLARE_TESTER(BundleProtocolTest) {
    ADD_TEST(Init);
    ADD_BP_TESTS(Basic);
    ADD_BP_TESTS(Fragment);
    ADD_BP_TESTS(AllFlags);
    ADD_BP_TESTS(RandomPayload);

    // XXX/demmer add tests for malformed / mangled headers, too long
    // sdnv's, etc
}

int
main(int argc, const char** argv)
{
    system("rm -rf .bundle-protocol-test");
    system("mkdir  .bundle-protocol-test");
    BundlePayload::payloaddir_.assign(".bundle-protocol-test");
    
    BundleProtocolTest t("bundle protocol test");
    t.run_tests(argc, argv, true);

    system("rm -rf .bundle-protocol-test");
}
