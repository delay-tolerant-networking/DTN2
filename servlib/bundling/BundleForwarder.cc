
#include "Bundle.h"
#include "BundleAction.h"
#include "BundleList.h"
#include "BundleForwarder.h"
#include "Contact.h"
#include "routing/BundleRouter.h"
#include "storage/BundleStore.h"

BundleForwarder* BundleForwarder::instance_ = NULL;

BundleForwarder::BundleForwarder()
    : Logger("/bundle/fwd")
{
}

/**
 * Queues the given event on the pending list of events.
 */
void
BundleForwarder::post(BundleEvent* event)
{
    __log_debug("/bundle/fwd", "queuing event with type %d", event->type_);
    instance_->eventq_.push(event);
}

/**
 * Routine that actually effects the forwarding operations as
 * returned from the BundleRouter.
 */
void
BundleForwarder::process(BundleAction* action)
{
    Bundle* bundle;
    bundle = action->bundleref_.bundle();
    
    switch (action->action_) {
    case FORWARD_UNIQUE:
    case FORWARD_COPY: {
        BundleForwardAction* fwdaction = (BundleForwardAction*)action;
        log_debug("forward bundle %d", bundle->bundleid_);
        fwdaction->nexthop_->consume_bundle(bundle);
        break;
    }
    case STORE_ADD: {
        bool added = BundleStore::instance()->insert(bundle);
        ASSERT(added);
        break;
    }
    case STORE_DEL: {
        bool removed = BundleStore::instance()->del(bundle->bundleid_);
        ASSERT(removed);
        break;
    }        
    default:
        PANIC("unimplemented action code %s",
              bundle_action_toa(action->action_));
    }
    
    delete action;
}

/**
 * The main run loop.
 */
void
BundleForwarder::run()
{
    BundleEvent* event;
    BundleActionList actions;
    BundleActionList::iterator iter;
    
    while (1) {
        // grab an event off the queue, blocking until we get one
        event = eventq_.pop_blocking();
        ASSERT(event);

        // always clear the action list
        actions.clear();
        
        // dispatch to the router to fill in the actions list
        router_->handle_event(event, &actions);
        
        // process the actions
        for (iter = actions.begin(); iter != actions.end(); ++iter) {
            process(*iter);
        }
    }
}

