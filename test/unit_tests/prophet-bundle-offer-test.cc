#include <dtn-config.h>
#include <oasys/util/UnitTest.h>

#include "prophet/BundleImpl.h"
#include "prophet/BundleList.h"
#include "prophet/Stats.h"
#include "prophet/Link.h"
#include "prophet/Table.h"
#include "prophet/FwdStrategy.h"
#include "prophet/Decider.h"
#include "prophet/Dictionary.h"
#include "prophet/BundleOffer.h"
#include "prophet/BundleCore.h"
#include "prophet/BundleTLVEntryList.h"

using namespace oasys;

//                       dest_id, cts, seq, ets, size, num_fwd
prophet::BundleImpl a("dtn://test-a",0xfc,   1,  60,    1,      5);
prophet::BundleImpl b("dtn://test-b",0xfd,   2,  60,    2,      7);
prophet::BundleImpl c("dtn://test-c",0xfe,   3,  60,    2,      9);
prophet::BundleImpl d("dtn://test-d",0xff,   4,  60,    1,      8);
prophet::BundleImpl e("dtn://test-e",0x100,  5,  60,    3,      6);

prophet::Dictionary ribd;
prophet::BundleCoreTestImpl core;
prophet::BundleList allbundles;
prophet::AckList acks;
prophet::LinkImpl nexthop("dtn://somehost");

const prophet::Bundle*
fetch(u_int16_t sid,const prophet::Dictionary& ribd)
{
    std::string eid = ribd.find(sid);
    if (eid == prophet::Dictionary::NULL_STR)
        return NULL;
    prophet::BundleList::const_iterator i = allbundles.begin();
    while (i != allbundles.end())
        if ((*i)->destination_id() == eid)
            return *i;
        else
            i++;
    return NULL;
}

#define DUMP_PVALUE_LIST(_a,_ribd,_local,_remote,_stats) do { \
    printf("pos  dest                 " \
           "cts seq NF sz PV   rPV  pmax mopr lmopr\n"); \
    const prophet::Bundle* _b = NULL; \
    for (prophet::BundleOfferList::const_iterator i = (_a).begin(); \
         i!=(_a).end(); i++) \
        if ( (_b = fetch((*i)->sid(),_ribd)) != NULL) \
        printf("%3d: %-20s %3d %3d %2d %2d %.2f %.2f %.2f %.2f %.2f\n", \
               (int)(i - (_a).begin()), \
               _b->destination_id().c_str(), \
               (*i)->creation_ts(), \
               (*i)->seqno(), \
               _b->num_forward(), \
               _b->size(), \
               _local.p_value(_b->destination_id()), \
               _remote.p_value(_b->destination_id()), \
               _stats.get_p_max(_b),  \
               _stats.get_mopr(_b),   \
               _stats.get_lmopr(_b)); \
    } while (0) 

DECLARE_TEST(Init) {
    CHECK(ribd.insert(a.destination_id()) != prophet::Dictionary::INVALID_SID);
    CHECK(ribd.insert(b.destination_id()) != prophet::Dictionary::INVALID_SID);
    CHECK(ribd.insert(c.destination_id()) != prophet::Dictionary::INVALID_SID);
    CHECK(ribd.insert(d.destination_id()) != prophet::Dictionary::INVALID_SID);
    CHECK(ribd.insert(e.destination_id()) != prophet::Dictionary::INVALID_SID);
    allbundles.push_back(&a);
    allbundles.push_back(&b);
    allbundles.push_back(&c);
    allbundles.push_back(&d);
    allbundles.push_back(&e);
    return UNIT_TEST_PASSED; 
}

DECLARE_TEST(GRTR) { 

    prophet::BundleList list;

    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");

    local.update_route(a.destination_id());
    local.update_route(b.destination_id());
    local.update_route(b.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());

    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(e.destination_id());
    remote.update_route(e.destination_id());

    // local.p_value(a) < remote.p_value(a)
    // local.p_value(b) > remote.p_value(b)
    // local.p_value(c) < remote.p_value(c)
    // local.p_value(d) > remote.p_value(d)
    // local.p_value(e) < remote.p_value(e)

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR,
                                  &nexthop, &core,
                                  &local, &remote);
    CHECK( decider != NULL);
    // takes ownership for comp & decider

    list.push_back(&a);
    list.push_back(&b);
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( !bo.empty() );
    // should not forward b
    CHECK_EQUAL( bo.size(), 1 );
    bo.add_bundle(&c);
    CHECK_EQUAL( bo.size(), 2 );
    // should not forward d
    bo.add_bundle(&d);
    CHECK_EQUAL( bo.size(), 2 );
    bo.add_bundle(&e);
    CHECK_EQUAL( bo.size(), 3 );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    prophet::Stats s;
    DUMP_PVALUE_LIST(offers,ribd,local,remote,s);
    CHECK( offers.size() == 3 );

    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(a.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(c.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(e.destination_id()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX) {
    prophet::BundleList list;
    list.push_back(&a);
    list.push_back(&b);

    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");

    local.update_route(a.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(e.destination_id());

    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());

    // local.p_value(a) < remote.p_value(a), NF = 5
    // local.p_value(b) < remote.p_value(b), NF = 7
    // local.p_value(c) < remote.p_value(c), NF = 9
    // local.p_value(d) > remote.p_value(d), NF = 8
    // local.p_value(e) > remote.p_value(e), NF = 6

    // only a and e have NF < MaxFWD

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GTMX);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GTMX,
                                  &nexthop, &core,
                                  // set max fwd to 8
                                  &local, &remote, NULL, 8);
    CHECK( decider != NULL);
    // takes ownership for comp & decider
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( !bo.empty() );
    // decider says yes to a and b (GRTR and NF < Max)
    CHECK_EQUAL( bo.size(), 2 );
    // decider says no to c (GRTR && NF > Max) and d (NF > Max) and e (GRTR)
    bo.add_bundle(&c);
    bo.add_bundle(&d);
    bo.add_bundle(&e);
    CHECK_EQUAL( bo.size(), 2 );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    prophet::Stats s;
    DUMP_PVALUE_LIST(offers,ribd,local,remote,s);
    CHECK( offers.size() == 2 );

    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(a.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(b.destination_id()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_PLUS) {
    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");
    prophet::Stats stats;
    prophet::BundleList list;

    local.update_route(a.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(e.destination_id());

    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());

    stats.update_stats(&a,0.99);
    stats.update_stats(&b,0.5);
    stats.update_stats(&c,0.5);
    stats.update_stats(&d,0.5);
    stats.update_stats(&e,0.5);

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_PLUS);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_PLUS,
                                  &nexthop, &core,
                                  // set max fwd to 8
                                  &local, &remote, &stats, 8);
    CHECK( decider != NULL );
    // bo takes ownership for comp and decider
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( bo.empty() );
    // decider rejects a because pmax(a) > remote.pvalue(a)
    DO( bo.add_bundle(&a) );
    CHECK( bo.empty() );
    // decider accepts b because remote.pvalue(b) > pmax(b)
    DO( bo.add_bundle(&b) );
    CHECK( bo.size() == 1 );
    // decider accepts c because remote.pvalue(c) > pmax(c)
    DO( bo.add_bundle(&c) );
    CHECK( bo.size() == 2 );
    // decider rejects d because local.pvalue(d) > remote.pvalue(d)
    DO( bo.add_bundle(&d) );
    CHECK( bo.size() == 2 );
    // decider rejects e because local.pvalue(e) > remote.pvalue(e)
    DO( bo.add_bundle(&e) );
    CHECK( bo.size() == 2 );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    DUMP_PVALUE_LIST(offers,ribd,local,remote,stats);

    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(b.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(c.destination_id()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX_PLUS) {
    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");
    prophet::Stats stats;
    prophet::BundleList list;

    local.update_route(a.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(e.destination_id());

    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());

    stats.update_stats(&a,0.99);
    stats.update_stats(&b,0.5);
    stats.update_stats(&c,0.5);
    stats.update_stats(&d,0.5);
    stats.update_stats(&e,0.5);

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GTMX_PLUS);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GTMX_PLUS,
                                  &nexthop, &core,
                                  // set max fwd to 8
                                  &local, &remote, &stats, 8);
    CHECK( decider != NULL );

    // bo takes ownership for comp and decider
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( bo.empty() );
    // decider rejects a:
    //   remote(a) < pmax(a)
    DO( bo.add_bundle(&a) );
    CHECK( bo.empty() );
    // decider accepts b:
    //   local(b) < remote(b) && pmax(b) < remote(b) && nf < maxNF
    DO( bo.add_bundle(&b) );
    CHECK( bo.size() == 1 );
    // decider rejects c: 
    //   nf > maxNF
    DO( bo.add_bundle(&c) );
    CHECK( bo.size() == 1 );
    // decider rejects d
    //   local(d) > remote(d)
    DO( bo.add_bundle(&d) );
    CHECK( bo.size() == 1 );
    // decider rejects e
    //   local(e) > remote(e)
    DO( bo.add_bundle(&e) );
    CHECK( bo.size() == 1 );
    CHECK( list.empty() );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    DUMP_PVALUE_LIST(offers,ribd,local,remote,stats);
    CHECK( offers.size() == 1 );

    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(b.destination_id()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_SORT) {
    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");
    prophet::BundleList list;

    local.update_route(a.destination_id());
    local.update_route(b.destination_id());
    local.update_route(b.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(d.destination_id());
    local.update_route(e.destination_id());

    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_SORT,
                                       &local, &remote);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_SORT,
                                  &nexthop, &core,
                                  // set max fwd to 8
                                  &local, &remote);
    CHECK( decider != NULL );
    CHECK( list.empty() );

    // bo takes ownership for comp and decider
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( bo.empty() );
    // bo accepts a:
    //   remote(a) > local(a)
    DO( bo.add_bundle(&a) );
    CHECK( !bo.empty() );
    // bo accepts b
    //   remote(b) > local(b)
    DO( bo.add_bundle(&b) );
    CHECK( bo.size() == 2 );
    // bo accepts c
    //   remote(c) > local(c)
    DO( bo.add_bundle(&c) );
    CHECK( bo.size() == 3 );
    // bo rejects d
    //   remote(d) < local(d)
    DO( bo.add_bundle(&d) );
    CHECK( bo.size() == 3 );
    // bo rejects e
    //   remote(e) < local(e)
    DO( bo.add_bundle(&e) );
    CHECK( bo.size() == 3 );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    prophet::Stats s;
    DUMP_PVALUE_LIST(offers,ribd,local,remote,s);

    // sorted in descending order of largest difference between
    // remote_p and local_p
    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(c.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(a.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(b.destination_id()) );
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_MAX) {
    prophet::Table local(&core,"local");
    prophet::Table remote(&core,"remote");
    prophet::BundleList list;

    remote.update_route(a.destination_id());
    remote.update_route(a.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(b.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());
    remote.update_route(c.destination_id());

    prophet::FwdStrategyComp* comp =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_SORT,
                                       &local, &remote);
    CHECK( comp != NULL);
    prophet::Decider* decider =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_SORT,
                                  &nexthop, &core,
                                  // set max fwd to 8
                                  &local, &remote);
    CHECK( decider != NULL );
    CHECK( list.empty() );

    // bo takes ownership for comp and decider
    prophet::BundleOffer bo(&core, list, comp, decider);

    CHECK( bo.empty() );

    // bo accepts a:
    //   remote(a) > local(a)
    DO( bo.add_bundle(&a) );
    CHECK( !bo.empty() );
    // bo accepts b
    //   remote(b) > local(b)
    DO( bo.add_bundle(&b) );
    CHECK( bo.size() == 2 );
    // bo accepts c
    //   remote(c) > local(c)
    DO( bo.add_bundle(&c) );
    CHECK( bo.size() == 3 );
    // bo rejects d
    //   remote(d) < local(d)
    DO( bo.add_bundle(&d) );
    CHECK( bo.size() == 3 );
    // bo rejects e
    //   remote(e) < local(e)
    DO( bo.add_bundle(&e) );
    CHECK( bo.size() == 3 );

    prophet::BundleOfferList offers = bo.get_bundle_offer(ribd,&acks);
    prophet::Stats s;
    DUMP_PVALUE_LIST(offers,ribd,local,remote,s);

    // sorted in descending order of largest difference between
    // remote_p and local_p
    prophet::BundleOfferList::iterator i = offers.begin();
    CHECK( (*i++)->sid() == ribd.find(b.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(c.destination_id()) );
    CHECK( (*i++)->sid() == ribd.find(a.destination_id()) );
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(BundleOffer) {
    ADD_TEST(Init);
    ADD_TEST(GRTR);
    ADD_TEST(GTMX);
    ADD_TEST(GRTR_PLUS);
    ADD_TEST(GTMX_PLUS);
    ADD_TEST(GRTR_SORT);
    ADD_TEST(GRTR_MAX);
}

DECLARE_TEST_FILE(BundleOffer, "prophet bundle offer test");
