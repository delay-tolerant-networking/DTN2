#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/debug/Log.h>

#include "prophet/Params.h"
#include "prophet/Stats.h"
#include "prophet/Table.h"
#include "prophet/Repository.h"
#include "prophet/AckList.h"
#include "prophet/BundleCore.h"
#include "prophet/Alarm.h"
#include "prophet/Oracle.h"
#include "prophet/Encounter.h"

#include "prophet/BaseTLV.h"
#include "prophet/HelloTLV.h"
#include "prophet/RIBDTLV.h"
#include "prophet/RIBTLV.h"
#include "prophet/OfferTLV.h"
#include "prophet/ResponseTLV.h"
#include "prophet/ProphetTLV.h"

#define ASSIGN_BUFF(_core) do { \
        DO( b = _core.written_.front().first ); \
        DO( buf = _core.written_.front().second ); \
        DO( _core.sent_.pop_front() ); \
        DO( _core.written_.pop_front() ); \
        DO( pt = prophet::ProphetTLV::deserialize(b->source_id(), \
                                             b->destination_id(), \
                                       (u_char*)buf.data(), buf.size()) ); \
    }  while (0)

class TestOracleImpl : public prophet::Oracle
{
public:
    TestOracleImpl(
            const prophet::ProphetParams* params,
            prophet::Stats* stats,
            prophet::Table* nodes,
            prophet::Repository* bundles,
            prophet::AckList* acks,
            prophet::BundleCore* core)
        : params_(params),
          stats_(stats),
          nodes_(nodes),
          bundles_(bundles),
          acks_(acks),
          core_(core),
          pid_(core->local_eid() + "/prophet") {}

    virtual ~TestOracleImpl() {}

    const prophet::ProphetParams* params() const { return params_; }
    prophet::Stats* stats() { return stats_; }
    prophet::Table* nodes() { return nodes_; }
    prophet::Repository* bundles() { return bundles_; }
    prophet::AckList* acks() { return acks_; }
    prophet::BundleCore* core() { return core_; }
    const std::string& prophet_id() const { return pid_; }

protected:
    const prophet::ProphetParams* params_;
    prophet::Stats* stats_;
    prophet::Table* nodes_;
    prophet::Repository* bundles_;
    prophet::AckList* acks_;
    prophet::BundleCore* core_;
    std::string pid_;
}; // TestOracleImpl

using namespace oasys;

// Local Encounter
prophet::ProphetParams local_params;
prophet::Stats local_stats;
prophet::AckList local_acks;
prophet::BundleCoreTestImpl local_core("dtn://local");
prophet::Table local_nodes(&local_core,"local");
prophet::Repository local_bundles((prophet::Repository::BundleCoreRep*)&local_core);
TestOracleImpl local_oracle(&local_params,&local_stats,&local_nodes,
                            &local_bundles,&local_acks,&local_core);
prophet::LinkImpl local_nexthop("dtn://remote");

// Remote Encounter

prophet::ProphetParams remote_params;
prophet::Stats remote_stats;
prophet::AckList remote_acks;
prophet::BundleCoreTestImpl remote_core("dtn://remote");
prophet::Table remote_nodes(&remote_core,"remote");
prophet::Repository remote_bundles((prophet::Repository::BundleCoreRep*)&remote_core);
TestOracleImpl remote_oracle(&remote_params,&remote_stats,&remote_nodes,
                            &remote_bundles,&remote_acks,&remote_core);
prophet::LinkImpl remote_nexthop("dtn://local");

prophet::ProphetTLV* pt;
prophet::BundleCoreTestImpl::BundleBuffer buf;
const prophet::Bundle* b;

DECLARE_TEST(Encounter) {
    CHECK( local_core.sent_.empty() );
    CHECK( remote_core.sent_.empty() );

    // tiebreaker for who sends first is whose eid is lexigraphically less than
    prophet::Encounter local(&local_nexthop,&local_oracle,1);
    prophet::Encounter remote(&remote_nexthop,&remote_oracle,2);
    // local wins, sends out SYN to remote
    CHECK_EQUALSTR( local.state_str(), "SYNSENT" );

    // local should have sent something, now caught in local_core's buffer
    CHECK( !local_core.sent_.empty() );
    DO( b = local_core.sent_.front() );
    CHECK( b->destination_id() == local_core.prophet_id(&local_nexthop) );
    CHECK( b->source_id() == local_core.prophet_id() );
    CHECK( !local_core.written_.empty() );
    ASSIGN_BUFF(local_core);
    CHECK( b->destination_id() == local_core.prophet_id(&local_nexthop) );
    CHECK( b->source_id() == local_core.prophet_id() );

    CHECK_EQUALSTR( remote.state_str(), "WAIT_NB" );

    // now remote receives HelloTLV::SYN
    DO( remote.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "SYNRCVD" );

    // ... and sends SYNACK
    ASSIGN_BUFF(remote_core);

    // just to make sure nothing's changed ...
    CHECK_EQUALSTR( local.state_str(), "SYNSENT" );

    // now local receives SYNACK, then emits ACK followed by RIBD/RIB
    DO( local.receive_tlv(pt) );
    CHECK_EQUALSTR( local.state_str(), "SEND_DR" );

    // peel off the ACK sent by local and deliver to remote
    ASSIGN_BUFF(local_core);
    DO( remote.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "WAIT_DICT" );

    // peel off RIB/RIBD sent by local and deliver to remote 
    ASSIGN_BUFF(local_core);
    DO( remote.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "OFFER" );

    // peel off OFFER sent by remote and deliver to local
    ASSIGN_BUFF(remote_core);
    DO( local.receive_tlv(pt) );
    CHECK( local_bundles.empty() );

    // because no bundles were offered by remote, local sends empty response
    // and switches to WAIT_DICT
    CHECK_EQUALSTR( local.state_str(), "WAIT_DICT" );

    // peel off empty response from local and deliver to remote
    ASSIGN_BUFF(local_core);

    // remote receives empty response, which signals the end of OFFER phase ...
    // switch to CREATE_DR and continue InfoExch, phase 2
    DO( remote.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "SEND_DR" );

    // deliver RIBD/RIB from remote to local
    ASSIGN_BUFF(remote_core);
    DO( local.receive_tlv(pt) );
    CHECK_EQUALSTR( local.state_str(), "OFFER");

    // deliver OFFER from local to remote ... because no bundles were offered,
    // send empty REQUEST and switch to WAIT_INFO ... and idle 
    ASSIGN_BUFF(local_core);
    DO( remote.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "WAIT_INFO");

    // read empty REQUEST and switch to WAIT_INFO
    ASSIGN_BUFF(remote_core);
    DO( local.receive_tlv(pt) );
    CHECK_EQUALSTR( remote.state_str(), "WAIT_INFO");
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Encounter) {
    ADD_TEST(Encounter);
}

DECLARE_TEST_FILE(Encounter, "prophet encounter test");
