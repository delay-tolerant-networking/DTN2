#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <oasys/debug/Log.h>

#include "prophet/Params.h"
#include "prophet/Bundle.h"
#include "prophet/BundleList.h"
#include "prophet/QueuePolicy.h"
#include "prophet/BundleCore.h"
#include "prophet/Repository.h"

using namespace oasys;

class RepCoreImpl : public prophet::Repository::BundleCoreRep
{
public:
    RepCoreImpl(prophet::BundleCoreTestImpl* core) 
        : core_(core) {}
    virtual ~RepCoreImpl() {}
    void print_log(const char* name, int level, const char* fmt, ...) PRINTFLIKE(4,5);
    void drop_bundle(const prophet::Bundle* b) { core_->drop_bundle(b); }
    u_int64_t max_bundle_quota() const { return core_->max_bundle_quota(); }
    prophet::BundleCoreTestImpl* core_;
};

void 
RepCoreImpl::print_log(const char* name, int level, const char* fmt, ...)
{
    printf("[%s][%d]\n",name,level);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n"); 
}

//                           dest_id, cts, seq, ets, size, num_fwd
prophet::BundleImpl a("dtn://test-a",0xfc,   1,  60,    1,      5);
prophet::BundleImpl b("dtn://test-b",0xfd,   2,  60,    2,      7);
prophet::BundleImpl c("dtn://test-c",0xfe,   3,  60,    2,      9);
prophet::BundleImpl d("dtn://test-d",0xff,   4,  60,    1,      8);
prophet::BundleImpl e("dtn://test-e",0x100,  5,  60,    3,      6);

prophet::BundleCoreTestImpl core;
RepCoreImpl repcore(&core);

#define DUMP_LIST(a) do { \
    printf("pos  dest                 cts ets seq NF sz\n"); \
    for (prophet::BundleList::const_iterator i = (a).begin(); \
         i!=(a).end(); i++) \
        printf("%3d: %-20s %3d %3d %3d %2d %2d\n", \
               (int)(i - (a).begin()), \
               (*i)->destination_id().c_str(), \
               (*i)->creation_ts(), \
               (*i)->creation_ts() + (*i)->expiration_ts(), \
               (*i)->sequence_num(), \
               (*i)->num_forward(), \
               (*i)->size()); \
    } while (0) 

#define DUMP_PVALUE_LIST(a,_table) do { \
    printf("pos  dest                 cts seq NF sz PV\n"); \
    for (prophet::BundleList::const_iterator i = (a).begin(); \
         i!=(a).end(); i++) \
        printf("%3d: %-20s %3d %3d %2d %2d %.2f\n", \
               (int)(i - (a).begin()), \
               (*i)->destination_id().c_str(), \
               (*i)->creation_ts(), \
               (*i)->sequence_num(), \
               (*i)->num_forward(), \
               (*i)->size(), \
               _table.p_value((*i))); \
    } while (0) 

#define DUMP_STAT_LIST(a,b) do { \
    printf("pos  dest                 cts ets seq NF sz pmax mopr lmopr\n"); \
    for (prophet::BundleList::const_iterator i = (a).begin(); \
         i!=(a).end(); i++) \
        printf("%3d: %-20s %3d %3d %3d %2d %2d %.02f %.02f  %.02f\n", \
               (int)(i - (a).begin()), \
               (*i)->destination_id().c_str(), \
               (*i)->creation_ts(),  \
               (*i)->creation_ts() + (*i)->expiration_ts(), \
               (*i)->sequence_num(), \
               (*i)->num_forward(),  \
               (*i)->size(),         \
               b.get_p_max((*i)),   \
               b.get_mopr((*i)),    \
               b.get_lmopr((*i)));  \
    } while (0) 

bool is_heap(const prophet::BundleList& list,
             const prophet::QueueComp* comp)
{
    size_t len = list.size();
    size_t parent = 0;
    for (size_t child = 1; child < len; child++)
    {
        if (comp->operator()(list[parent],list[child]))
            return false;
        if ((child & 1) == 0)
            parent++;
    }
    return true;
}

bool find(const prophet::BundleList& list,
          const prophet::Bundle* b)
{
    size_t len = list.size();
    for (size_t pos = 0; pos < len; pos++)
        if (list[pos] == b) return true;
    return false;
}

DECLARE_TEST(FIFO) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    // use default FIFO comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(prophet::QueuePolicy::FIFO);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);
    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_LIST(rep.get_bundles());

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    CHECK(rep.get_current() <= 8);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( !find(rep.get_bundles(), &a) );
    CHECK(  find(rep.get_bundles(), &b) );
    CHECK(  find(rep.get_bundles(), &c) );
    CHECK(  find(rep.get_bundles(), &d) );
    CHECK(  find(rep.get_bundles(), &e) );

    // remember for later, to reset to original
    u_int32_t cts = e.creation_ts();
    u_int32_t seq = e.sequence_num();

    e.set_creation_ts(0x10);
    e.set_sequence_num(0);
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_LIST(rep.get_bundles());
    // show comparisons
    qc->verbose_ = true;
    DO( rep.change_priority(&e) );
    qc->verbose_ = false;
    DUMP_LIST(rep.get_bundles());
    CHECK(  is_heap(rep.get_bundles(), rep.get_comparator()) );

    // reset back to original
    e.set_creation_ts(cts);
    e.set_sequence_num(seq);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MOFO) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    // use MOFO comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(prophet::QueuePolicy::MOFO);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);
    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_LIST(rep.get_bundles());

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    CHECK(rep.get_current() <= 8);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK(!find(rep.get_bundles(), &c) );
    CHECK( find(rep.get_bundles(), &d) );
    CHECK( find(rep.get_bundles(), &e) );

    // remember for later, to reset to original
    u_int32_t nf = d.num_forward();

    d.set_num_forward(3);
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_LIST(rep.get_bundles());
    DO( rep.change_priority(&d) );
    DUMP_LIST(rep.get_bundles());
    CHECK(  is_heap(rep.get_bundles(), rep.get_comparator()) );

    // reset back to original
    d.set_num_forward(nf);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MOPR) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    prophet::Stats stats;
    stats.update_stats(&a,0.1);
    stats.update_stats(&b,0.3);
    stats.update_stats(&c,0.4);
    stats.update_stats(&d,0.5);
    stats.update_stats(&e,0.2);

    // use MOPR comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(prophet::QueuePolicy::MOPR,&stats);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);

    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_STAT_LIST(rep.get_bundles(),stats);

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_STAT_LIST(rep.get_bundles(),stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    CHECK(rep.get_current() <= 8);
    DUMP_STAT_LIST(rep.get_bundles(),stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK( find(rep.get_bundles(), &c) );
    CHECK(!find(rep.get_bundles(), &d) );
    CHECK( find(rep.get_bundles(), &e) );

    stats.update_stats(&a,0.8);
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_STAT_LIST(rep.get_bundles(), stats);
    DO( rep.change_priority(&a) );
    DUMP_STAT_LIST(rep.get_bundles(), stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(LMOPR) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    prophet::Stats stats;
    stats.update_stats(&a,0.1);
    stats.update_stats(&b,0.3);
    stats.update_stats(&c,0.4);
    stats.update_stats(&d,0.5);
    stats.update_stats(&e,0.2);

    // use LINEAR_MOPR comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(
                prophet::QueuePolicy::LINEAR_MOPR,&stats);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);

    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_STAT_LIST(rep.get_bundles(),stats);

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_STAT_LIST(rep.get_bundles(),stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    CHECK(rep.get_current() <= 8);
    DUMP_STAT_LIST(rep.get_bundles(),stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK( find(rep.get_bundles(), &c) );
    CHECK(!find(rep.get_bundles(), &d) );
    CHECK( find(rep.get_bundles(), &e) );

    stats.update_stats(&a,0.8);
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_STAT_LIST(rep.get_bundles(), stats);
    DO( rep.change_priority(&a) );
    DUMP_STAT_LIST(rep.get_bundles(), stats);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SHLI) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    // use SHLI comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(prophet::QueuePolicy::SHLI);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);

    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_LIST(rep.get_bundles());

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    CHECK(rep.get_current() <= 8);
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK(!find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK( find(rep.get_bundles(), &c) );
    CHECK( find(rep.get_bundles(), &d) );
    CHECK( find(rep.get_bundles(), &e) );

    e.set_expiration_ts(10);
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_LIST(rep.get_bundles());
    DO( rep.change_priority(&e) );
    DUMP_LIST(rep.get_bundles());
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(LEPR) {
    prophet::BundleList list;
    list.push_back(&d);
    list.push_back(&e);

    prophet::Table nodes(&core,"local");
    nodes.update_route(a.destination_id()); // route goes to 0.75
    nodes.update_route(b.destination_id()); // route goes to 0.75
    nodes.update_route(c.destination_id()); // route goes to 0.75
    nodes.update_route(c.destination_id());
    nodes.update_transitive(d.destination_id(),c.destination_id(),0.80);
    // e.destination_id() has route of 0

    // use LEPR comparator
    prophet::QueueComp* qc =
        prophet::QueuePolicy::policy(
                prophet::QueuePolicy::LEPR,NULL,&nodes,7);
    core.set_max(9);
    prophet::Repository rep(&repcore,qc,&list);

    CHECK_EQUAL(rep.get_max(), 9);
    CHECK_EQUAL(rep.get_current(), 4);
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);

    rep.add(&c);
    CHECK_EQUAL(rep.get_current(), 6);
    rep.add(&b);
    CHECK_EQUAL(rep.get_current(), 8);
    rep.add(&a);
    CHECK_EQUAL(rep.get_current(), 9);
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    core.set_max(8);
    rep.handle_change_max();
    // although e has the lowest route, it does not get evicted due
    // to the constraint that min_NF > 7
    CHECK(rep.get_current() <= 8);
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK( find(rep.get_bundles(), &c) );
    CHECK(!find(rep.get_bundles(), &d) );
    CHECK( find(rep.get_bundles(), &e) );

    // increase NF on e
    e.set_num_forward(8);
    core.set_max(7);
    rep.handle_change_max();
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);
    CHECK(rep.get_current() == 5);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );
    CHECK( find(rep.get_bundles(), &a) );
    CHECK( find(rep.get_bundles(), &b) );
    CHECK( find(rep.get_bundles(), &c) );
    CHECK(!find(rep.get_bundles(), &d) );
    CHECK(!find(rep.get_bundles(), &e) );

    core.set_max(9);
    rep.handle_change_max();
    rep.add(&d);
    rep.add(&e);

    nodes.update_route(e.destination_id());
    nodes.update_route(e.destination_id());
    nodes.update_route(e.destination_id());
    CHECK( !is_heap(rep.get_bundles(), rep.get_comparator()) );
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);
    DO( rep.change_priority(&e) );
    DUMP_PVALUE_LIST(rep.get_bundles(),nodes);
    CHECK( is_heap(rep.get_bundles(), rep.get_comparator()) );

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Repository) {
    ADD_TEST(FIFO);
    ADD_TEST(MOFO);
    ADD_TEST(MOPR);
    ADD_TEST(LMOPR);
    ADD_TEST(SHLI);
    ADD_TEST(LEPR);
}

DECLARE_TEST_FILE(Repository, "prophet bundle repository test");
