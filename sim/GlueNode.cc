#include "GlueNode.h"

#include "bundling/Bundle.h"
#include "bundling/BundleAction.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleConsumer.h"
#include "bundling/Contact.h"

#include "SimConvergenceLayer.h"


GlueNode::GlueNode(int id,const char* logpath): Node(id,logpath) 
{
    router_ = BundleRouter::create_router(BundleRouter::type_.c_str());
    log_info("N[%d]: creating router of type:%s",id,BundleRouter::type_.c_str());
    
}


void 
GlueNode::message_received(Message* msg) 
{

    if (msg->dst() == id()) {
        log_info("RCV[%d]: msg id %d, size-rcv %f",
                 id(),msg->id(),msg->size());
    }
    else {
        log_info("FWD[%d]: msg id %d, size-rcv %f",
                 id(),msg->id(),msg->size());
        forward(msg);
    }
}

void forward_event(BundleEvent* event) ;

void 
GlueNode::chewing_complete(SimContact* c, double size, Message* msg) 
{

    bool acked = true;
    Bundle* bundle = SimConvergenceLayer::msg2bundle(msg);
    Contact* consumer = SimConvergenceLayer::simlink2ct(c);
    int tsize = (int)size;
    BundleTransmittedEvent* e = 
        new BundleTransmittedEvent(bundle,consumer,tsize,acked);
    forward_event(e);
    
}

void 
GlueNode::open_contact(SimContact* c) 
{
    Contact* ct = SimConvergenceLayer::simlink2ct(c);
    ContactAvailableEvent* e = new ContactAvailableEvent(ct);
    log_info("CUP[%d]: Contact OPEN",id());
    forward_event(e);
}


void 
GlueNode::close_contact(SimContact* c)
{
    Contact* ct = SimConvergenceLayer::simlink2ct(c);
    ContactBrokenEvent* e = new ContactBrokenEvent(ct);
    log_info("CDOWN[%d]: Contact OPEN",id());
    forward_event(e);
}


void
GlueNode::process(Event* e) {
    
    switch (e->type()) {
    case MESSAGE_RECEIVED:    {
        Event_message_received* e1 = (Event_message_received*)e;
        Message* msg = e1->msg_;
        log_info("GOT[%d]: msg (%d) size %3f",id(),msg->id(),msg->size());
        
        // update the size of the message that is received
        msg->set_size(e1->sizesent_);
        message_received(msg);
        break;
    }
        
    case FOR_BUNDLE_ROUTER: {
        BundleEvent* be = ((Event_for_br* )e)->bundle_event_;
        forward_event(be);
        break;
    }
    default:
        PANIC("unimplemented action code");
    }
}


void
GlueNode::forward(Message* msg) 
{
    // first see if there exists a  bundle in the global hashtable
    Bundle *b = SimConvergenceLayer::msg2bundle(msg); 
    BundleReceivedEvent* bundle_rcv = new BundleReceivedEvent(b, b->payload_.length()) ;
    forward_event(bundle_rcv);
}



/**
 * Routine that actually effects the forwarding operations as
 * returned from the BundleRouter.
 */
void
GlueNode::execute_router_action(BundleAction* action)
{
    Bundle* bundle;
    bundle = action->bundleref_.bundle();
    
    switch (action->action_) {
    case FORWARD_UNIQUE:
    case FORWARD_COPY: {
        BundleForwardAction* fwdaction = (BundleForwardAction*)action;
        
        log_info("N[%d] forward bundle (%d) as told by routercode",
                id(),bundle->bundleid_);
        BundleConsumer* bc = fwdaction->nexthop_ ; 
        bc->consume_bundle(bundle);
        break;
        }
    case STORE_ADD:{ 
        log_info("N[%d] storing ignored %d", id(), bundle->bundleid_);
        break;
        }
    
    case STORE_DEL: {
        log_debug("N[%d] deletion ignored %d", id(), bundle->bundleid_);
        break;
        }
            
    default:
        PANIC("unimplemented action code %s",
              bundle_action_toa(action->action_));
    }
    delete action;
}


void 
GlueNode::forward_event(BundleEvent* event) 
{

    BundleActionList actions;
    BundleActionList::iterator iter;
    
    ASSERT(event);
    
    // always clear the action list
    actions.clear();
    
    // dispatch to the router to fill in the actions list
    router_->handle_event(event, &actions);
    
    // process the actions
    for (iter = actions.begin(); iter != actions.end(); ++iter) {
        execute_router_action(*iter);
    }
    
}

