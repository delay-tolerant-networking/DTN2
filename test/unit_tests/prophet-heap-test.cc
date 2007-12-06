#include <oasys/util/UnitTest.h>
#include <oasys/util/Random.h>
#include <oasys/debug/Log.h>

#include "prophet/Node.h"
#include "prophet/Util.h"

#define TEST_SIZE 10

using namespace oasys;
using namespace prophet;

template<typename UnitType, typename Sequence, typename Comp>
bool is_heap(const Sequence& list, Comp& comp)
{
    typedef typename Sequence::size_type size_type;
    size_type len = list.size();
    size_type parent = 0;
    for (size_type child = 1; child < len; child++)
    {
        if ((comp)(list[parent],list[child]))
        {
            log_notice_p("/test","heap sequence failed, size=%u, parent[%u], "
                                 "child[%u]",list.size(),parent,child);
            return false;
        }
        if ((child & 1) == 0)
            parent++;
    }
    return true;
}

template<typename UnitType, typename Sequence, typename Comp>
bool find(const Sequence& list, UnitType a, Comp& comp)
{
    typedef typename Sequence::size_type size_type;
    size_type len = list.size();
    for (size_type pos = 0; pos < len; pos++) {
        if (!(comp)(a,list[pos]) && !(comp)(list[pos],a))
            return true;
    }
    return false;
}

struct less_node {
    less_node(int type=0) : mytype(type) {}

    bool operator() (Node* a, Node* b) {
        ASSERT(a != NULL && b != NULL);
        switch (mytype) {
        case 1:
            return ((*a).p_value() > (*b).p_value());
        case 0:
        default:
            return ((*a).p_value() < (*b).p_value());
        };
    }
    int mytype;
};

struct greater_node : public less_node {
    greater_node() : less_node(1) {}
};

struct heap_swap {
    inline void operator()(Node* a, size_t pos)
    {
        a->set_heap_pos(pos);
    }
};

DECLARE_TEST(IntHeap) {
    typedef std::vector<int> Seq;
    typedef std::less<int> Comp;
    typedef Heap<int,Seq,Comp> Heap;
    Comp mycomp;
    Heap myheap;

    for (int i = 0; i < TEST_SIZE; i++) {
        //int val = TEST_SIZE - (i + 1);
        int val = oasys::Random::rand(10000);
        myheap.add(val);
        Seq list = myheap.sequence();
        CHECK( (find<int,Seq,Comp>(myheap.sequence(),val,mycomp)) );
        CHECK( (Heap::is_heap(myheap.sequence(),mycomp)) );
    }

    CHECK(myheap.size() == TEST_SIZE);

    while (myheap.size() > 0) {
        log_notice_p("/test","List size = %u",myheap.size());
        int r = myheap.top();
        myheap.remove(0);
        if (!myheap.empty()) {
            log_notice_p("/test","r=%d,top=%d",r,myheap.top());
            CHECK(r >= myheap.top());
            CHECK( Heap::is_heap(myheap.sequence(),mycomp) );
        }
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(NodeHeap) {
    typedef std::vector<Node*> Seq;
    typedef struct less_node Comp;
    typedef Heap<Node*,Seq,Comp,struct heap_swap> Heap;

    struct less_node mycomp;
    Heap myheap;

    std::string id("a");

    for (int i = 0; i < TEST_SIZE; i++) {
        Node* n = new Node(id);
        // vary the compared metric
        for (int j=i; j < TEST_SIZE - 1; j++)
            n->update_pvalue();
        myheap.add(n);
        CHECK( (find<Node*,Seq,Comp>(myheap.sequence(),n,mycomp)) );
        CHECK( (Heap::is_heap(myheap.sequence(),mycomp)) );
        // rough equality test using current compare object
        size_t pos = n->heap_pos();
        CHECK( (!(mycomp)(myheap.sequence()[pos],n)) );
        CHECK( (!(mycomp)(n,myheap.sequence()[pos])) );
        id += ('a' + i + 1);
    }
    CHECK(myheap.size() == TEST_SIZE);
    do { 
        Seq::const_iterator si = myheap.sequence().begin();
        size_t pos = 0;
        while (si != myheap.sequence().end()) {
            CHECK_EQUAL(pos,(*si)->heap_pos());
            si++; pos++;
        }
    } while (0);

    // grab somebody from the middle and up the metric
    Node* n = myheap.sequence()[TEST_SIZE/2];
    for (int z=0;z<TEST_SIZE/2;z++) {
        n->update_pvalue();
    }
    // inform the heap of new metric at this pos
    size_t pos = n->heap_pos();
    myheap.remove(TEST_SIZE/2);
    myheap.add(n);
    log_notice_p("/test","Old pos=%u new pos=%u",TEST_SIZE/2,pos);
    CHECK( (Heap::is_heap(myheap.sequence(),mycomp)) );
    do { 
        Seq::const_iterator si = myheap.sequence().begin();
        size_t pos = 0;
        while (si != myheap.sequence().end()) {
            CHECK_EQUAL(pos,(*si)->heap_pos());
            si++; pos++;
        }
    } while (0);

    // verify that new compare object creates new heap order
    struct greater_node gcomp;
    DO(myheap.set_compare(new struct greater_node()));
    CHECK(myheap.size() == TEST_SIZE);
    CHECK( (Heap::is_heap(myheap.sequence(),gcomp)) );
    do { 
        Seq::const_iterator si = myheap.sequence().begin();
        size_t pos = 0;
        while (si != myheap.sequence().end()) {
            CHECK_EQUAL(pos,(*si)->heap_pos());
            si++; pos++;
        }
    } while (0);

    // go back to original compare object, verify heap order
    DO(myheap.set_compare(new struct less_node()));
    CHECK(myheap.size() == TEST_SIZE);
    CHECK( (Heap::is_heap(myheap.sequence(),mycomp)) );
    do { 
        Seq::const_iterator si = myheap.sequence().begin();
        size_t pos = 0;
        while (si != myheap.sequence().end()) {
            CHECK_EQUAL(pos,(*si)->heap_pos());
            si++; pos++;
        }
    } while (0);

    while (myheap.size() > 0) {
        Node* r = myheap.top();
        myheap.remove(0);
        if (!myheap.empty()) {
            log_notice_p("/test","r=%0.8f,top=%0.8f",
                         r->p_value(),myheap.top()->p_value());
            CHECK((!(mycomp)(r,myheap.top())));
            CHECK( (Heap::is_heap(myheap.sequence(),mycomp)) );
            do {
                Seq::const_iterator si = myheap.sequence().begin();
                size_t pos = 0;
                while (si != myheap.sequence().end()) {
                    CHECK_EQUAL(pos,(*si)->heap_pos());
                    si++; pos++;
                }
            } while (0);
        }
        delete r;
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetHeap) {
    ADD_TEST(IntHeap);
    ADD_TEST(NodeHeap);
}

DECLARE_TEST_FILE(ProphetHeap, "prophet heap test");
