
#include <oasys/util/StringBuffer.h>

#include "SimConvergenceLayer.h"
#include "Topology.h"
#include "bundling/BundleList.h"

/******************************************************************************
 *
 * SimConvergenceLayer
 *
 *****************************************************************************/
SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("/cl/sim")
{
}

void
SimConvergenceLayer::init()
{
}

void
SimConvergenceLayer::fini()
{
}

bool
SimConvergenceLayer::validate(const std::string& admin)
{
    NOTIMPLEMENTED;
}

bool
SimConvergenceLayer::match(const std::string& demux, const std::string& admin)
{
    NOTIMPLEMENTED;
}


void 
SimConvergenceLayer::send_bundles(Contact* contact)
{
 
    SimContact* sc = dtnlink2simlink(contact->link()); 
    
    Bundle* bundle;
    BundleList* blist = contact->bundle_list();
    
    Bundle* iter_bundle;
    BundleList::iterator iter;
    ScopeLock lock(blist->lock());
    
    log_info("N[%d] CL: current bundle list:",sc->src()->id());
        
    for (iter = blist->begin(); 
         iter != blist->end(); ++iter) {
        iter_bundle = *iter;
        log_info("\tbundle:%d pending:%d",
                 iter_bundle->bundleid_,iter_bundle->pendingtx());
    }
    // check, if the contact is open. If yes, send one msg from the queue
    if (sc->is_open()) {
        bundle = blist->pop_front();
        if(bundle) {
            log_info("\tsending bundle:%d",bundle->bundleid_);
            Message* msg = SimConvergenceLayer::bundle2msg(bundle);
            sc->chew_message(msg);
        }
    }
}

bool 
SimConvergenceLayer::open_contact(Contact* contact) 
{
//    PANIC("can not explicitly open contact using simulator convergence layer");
//  Do nothing, as by default all simulator contacts are already open
    return true;
}
    
bool 
SimConvergenceLayer::close_contact(Contact* contact) 
{
    //PANIC("can not explicitly close contact using simulator convergence layer");
    //this is a callback from the Contact - some state cleanup might
    //be necessary
    return true;
}


/******************************************************************
 * Static functions follow (for maintaining mappings)
 ***************************************************************** */

SimContact*
SimConvergenceLayer::dtnlink2simlink(Link* link)
{
   // use contact info in contact to find this out
    ASSERT(link != NULL);
    int id = ((SimLinkInfo*)link->link_info())->id();
    SimContact* s =     Topology::contact(id);
    if (s == NULL) {
        PANIC("undefined contact mapping with stored id %d",id);
    }
    return s;
}


std::string
SimConvergenceLayer::id2node(int i) 
{
    StringBuffer buf;
    buf.appendf("bundles://sim/simcl://%d", i);
    std::string ret(buf.c_str());
    return ret;
}

void
SimConvergenceLayer::create_ct(int id) 
{
    ASSERT(id >=0 && id < MAX_LINKS);

    BundleTuple tuple(id2node(id));

    printf("link creating %s \n",tuple.c_str());
    fflush(stdout);

    Link*  link = Link::create_link(tuple.c_str(),Link::ONDEMAND,"simcl",tuple);
    link->set_link_info(new SimLinkInfo(id));
    links_[id] = link;

}

Link*
SimConvergenceLayer::simlink2dtnlink(SimContact* simlink)
{
    ASSERT(simlink != NULL);
    Link* link =  links_[simlink->id()] ; 
    ASSERT(link != NULL);
    return link;

}

Contact*
SimConvergenceLayer::simlink2ct(SimContact* simlink)
{
    return simlink2dtnlink(simlink)->contact();
}

Message*
SimConvergenceLayer::bundle2msg(Bundle* b) 
{
    ASSERT(b != NULL);
    // no fragmentation is supported
    Message* m =  messages_[b->bundleid_];
    ASSERT(m != NULL);
    return m;
}

Bundle*
SimConvergenceLayer::msg2bundle(Message* m) 
{
    ASSERT(m != NULL);
    // no fragmentation is supported, messages/bundles uniquely identified by id's
    
    ASSERT(m->id() >= 0 && m->id() < MAX_BUNDLES);

    Bundle* b = bundles_[m->id()];
    messages_[m->id()] = m;
    
    if (b == NULL) {
        b  = new Bundle(m->id(), BundlePayload::NODATA);
        //b->bundleid_ = m->id();
    
        const std::string src = id2node(m->src());
        const std::string dst = id2node(m->dst());

        b->source_.assign(src);
        b->dest_.assign(dst);
        b->payload_.set_length((int)m->size());
        bundles_[m->id()] = b;
    }
    return  b;

}

int 
SimConvergenceLayer::node2id(BundleTuplePattern src) 
{
    
    std::string admin = src.admin();
    std::string id;
    size_t start  = admin.find(":",0);
    id.assign(admin, start+3, std::string::npos);
    return atoi(id.c_str());

}

Bundle*  SimConvergenceLayer::bundles_[MAX_BUNDLES];
Message* SimConvergenceLayer::messages_[MAX_BUNDLES];
//Contact* SimConvergenceLayer::contacts_[MAX_CONTACTS];
Link* SimConvergenceLayer::links_[MAX_LINKS];
    
