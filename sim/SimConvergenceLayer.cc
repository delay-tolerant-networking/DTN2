#include "SimConvergenceLayer.h"

#include "Topology.h"
#include "bundling/BundleList.h"
#include "util/StringBuffer.h"

#include <string.h>
#include <iostream>
#include <sstream>

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
 
    SimContact* sc = ct2simlink(contact); 
    
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
    PANIC("can not explicitly open contact using simulator convergence layer");
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
SimConvergenceLayer::ct2simlink(Contact* contact)
{
   // use contact info in contact to find this out
    ASSERT(contact != NULL);
    int id = ((SimContactInfo*)contact->contact_info())->id();
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
    ASSERT(id >=0 && id < MAX_CONTACTS);

    BundleTuple tuple(id2node(id));
    Contact* ct = new Contact(SCHEDULED,tuple);
    ct->set_contact_info(new SimContactInfo(id));
    contacts_[id] = ct;
}

Contact*
SimConvergenceLayer::simlink2ct(SimContact* simlink)
{
    ASSERT(simlink != NULL);
    Contact* ct =  contacts_[simlink->id()] ; 
    ASSERT(ct != NULL);
    return ct;

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
Contact* SimConvergenceLayer::contacts_[MAX_CONTACTS];
    
