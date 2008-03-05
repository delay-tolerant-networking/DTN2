#include <dtn-config.h>
#include <oasys/util/UnitTest.h>

using namespace oasys;

#include "prophet/Link.h"
#include "prophet/Params.h"
#include "prophet/BundleImpl.h"
#include "prophet/BundleCore.h"
#include "prophet/Controller.h"

#define TRANSFER_BUNDLES(_from,_to) while (! _from.written_.empty()) { \
        prophet::BundleImpl* _b = (prophet::BundleImpl*) \
                                         const_cast<prophet::Bundle*>( \
            _from.written_.front().first); \
        _b->set_size(_from.written_.front().second.size()); \
        _to.rcvd_.push_back(std::make_pair(_b,_from.written_.front().second)); \
        _from.written_.pop_front(); \
    }

#define GET_SESSION(_ctrl) *((_ctrl).begin())

prophet::BundleCoreTestImpl local_core("dtn://local");
prophet::BundleCoreTestImpl remote_core("dtn://remote");
prophet::LinkImpl local_link("dtn://remote");
prophet::LinkImpl remote_link("dtn://local");
prophet::Repository
    local_bundles((prophet::Repository::BundleCoreRep*)&local_core);
prophet::Repository
    remote_bundles((prophet::Repository::BundleCoreRep*)&remote_core);
prophet::ProphetParams params;

DECLARE_TEST(Controller) {

    prophet::Controller local(&local_core,&local_bundles,&params);
    prophet::Controller remote(&remote_core,&remote_bundles,&params);

    // two controllers with nothing in them
    CHECK( local.empty() );
    CHECK( remote.empty() );

    DO( local.new_neighbor(&local_link) );
    DO( remote.new_neighbor(&remote_link) );

    // should create one encounter per controller
    CHECK( !local.empty() );
    CHECK( !remote.empty() );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "SYNSENT" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "WAIT_NB" );

    // local sends SYN to remote
    CHECK( !local_core.written_.empty() );
    CHECK( remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(local_core,remote_core);

    // alert remote controller to waiting bundle
    DO( remote.handle_bundle_received(remote_core.rcvd_.front().first,
                                      &remote_link) );
    CHECK( remote_core.rcvd_.empty() );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "SYNSENT" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "SYNRCVD" );

    // remote sends SYNACK to local
    CHECK( local_core.written_.empty() );
    CHECK( !remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(remote_core,local_core);

    // alert local controller to waiting bundle
    DO( local.handle_bundle_received(local_core.rcvd_.front().first,
                                     &local_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "SEND_DR" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "SYNRCVD" );

    // local sends ACK and RIBD/RIB to remote
    CHECK( !local_core.written_.empty() );
    CHECK( remote_core.written_.empty() );
    // pop them off and deliver
    TRANSFER_BUNDLES(local_core,remote_core);

    // alert remote controller to waiting bundle (ACK)
    DO( remote.handle_bundle_received(remote_core.rcvd_.front().first,
                                      &remote_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "SEND_DR" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "WAIT_DICT" );

    // alert remote controller to the next waiting bundle (RIBD/RIB)
    DO( remote.handle_bundle_received(remote_core.rcvd_.front().first,
                                      &remote_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "SEND_DR" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "OFFER" );

    // remote sends OFFER to local
    CHECK( local_core.written_.empty() );
    CHECK( !remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(remote_core,local_core);

    // alert the controller to the waiting bundle (OFFER)
    DO( local.handle_bundle_received(local_core.rcvd_.front().first,
                                     &local_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "WAIT_DICT" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "OFFER" );

    // local sends empty REQUEST to remote
    CHECK( !local_core.written_.empty() );
    CHECK( remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(local_core,remote_core);

    // alert remote controller to waiting bundle (REQUEST)
    DO( remote.handle_bundle_received(remote_core.rcvd_.front().first,
                                      &remote_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "WAIT_DICT" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "SEND_DR" );

    // remote sends RIBD/RIB to local
    CHECK( local_core.written_.empty() );
    CHECK( !remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(remote_core,local_core);

    // alert local controller to the waiting bundle (RIBD/RIB)
    DO( local.handle_bundle_received(local_core.rcvd_.front().first,
                                     &local_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "OFFER" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "SEND_DR" );

    // local sends OFFER to remote
    CHECK( !local_core.written_.empty() );
    CHECK( remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(local_core,remote_core);

    // alert remote controller to waiting bundle (OFFER)
    DO( remote.handle_bundle_received(remote_core.rcvd_.front().first,
                                      &remote_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "OFFER" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "WAIT_INFO" );

    // remote sends empty REQUEST to local, completing this InfoExch
    CHECK( local_core.written_.empty() );
    CHECK( !remote_core.written_.empty() );
    // pop it off and deliver
    TRANSFER_BUNDLES(remote_core,local_core);

    // alert local controller to the waiting bundle (REQUEST)
    DO( local.handle_bundle_received(local_core.rcvd_.front().first,
                                     &local_link) );
    CHECK_EQUALSTR( (GET_SESSION(local))->state_str(), "WAIT_INFO" );
    CHECK_EQUALSTR( (GET_SESSION(remote))->state_str(), "WAIT_INFO" );

    return oasys::UNIT_TEST_PASSED;
}

DECLARE_TESTER(Controller) {
    ADD_TEST(Controller);
}

DECLARE_TEST_FILE(Controller, "prophet controller test");
