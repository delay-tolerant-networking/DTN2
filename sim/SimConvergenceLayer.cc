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
    // use contact-info in contact to find this out
    SimContact* sc = ct2simlink(contact); 
    
    Bundle* bundle;
    BundleList* blist = contact->bundle_list();
 
    if (sc->is_open()) {
	bundle = blist->pop_front();
	Message* msg = SimConvergenceLayer::bundle2msg(bundle);
	sc->chew_message(msg);
    }

    //  remove the reference on the bundle (which may delete it)
   //    bundle->del_ref();

}

bool 
SimConvergenceLayer::open_contact(Contact* contact) 
{
    PANIC("can not explicitly open contact using simulator convergence layer");
}
    
bool 
SimConvergenceLayer::close_contact(Contact* contact) 
{
    PANIC("can not explicitly close contact using simulator convergence layer");
}


/******************************************************************
 * Static functions follow
 ***************************************************************** */

SimContact*
SimConvergenceLayer::ct2simlink(Contact* contact)
{
    ASSERT(contact != NULL);
    int id = ((SimContactInfo*)contact->contact_info())->id();
    SimContact* s =     Topology::contact(id);
    if (s == NULL) {
	PANIC("undefined contact mapping with stored id %d",id);
    }
    return s;
}


const char * 
SimConvergenceLayer::id2node(int i) 
{
    std::string retval = "bundles://sim/simcl://";
//    retval.append(i);
    return retval.c_str();
}

void
SimConvergenceLayer::create_ct(int id) 
{
    
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
    Bundle* b = bundles_[m->id()];
    messages_[m->id()] = m;
    
    if (b == NULL) {
	b  = new Bundle(m->id());
//	b->bundleid_ = m->id();
	
	const char* src = id2node(m->src());
	const char* dst = id2node(m->dst());

	b->source_.set_tuple(src);
	b->dest_.set_tuple(dst);
	b->payload_.set_length((int)m->size());
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
    
