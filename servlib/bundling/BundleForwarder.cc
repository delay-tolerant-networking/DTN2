
#include <oasys/util/StringBuffer.h>

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
    bundles_received_ = 0;
    bundles_sent_local_ = 0;
    bundles_sent_remote_ = 0;
    bundles_expired_ = 0;
}

/**
 * Queues the given event on the pending list of events.
 */
void
BundleForwarder::post(BundleEvent* event)
{
    __log_debug("/bundle/fwd", "queuing event with type %d", event->type_);

    switch(event->type_) {

    case BUNDLE_RECEIVED:
        instance_->bundles_received_++;
        break;

    case BUNDLE_TRANSMITTED:
        if (((BundleTransmittedEvent*)event)->consumer_->is_local()) {
            instance_->bundles_sent_local_++;
        } else {
            instance_->bundles_sent_remote_++;
        }
        break;

    case BUNDLE_EXPIRED:
        instance_->bundles_expired_++;
        break;
        
    default:
        break;
    }
    
    instance_->eventq_.push(event);
}


/**
 * Format the given StringBuffer with the current statistics value.
 */
void
BundleForwarder::get_statistics(StringBuffer* buf)
{
    buf->appendf("%d pending -- %d received -- %d sent_local -- %d sent_remote "
                 "-- %d expired",
                 active_router()->pending_bundles()->size(),
                 bundles_received_,
                 bundles_sent_local_,
                 bundles_sent_remote_,
                 bundles_expired_);
}

/**
 * Format the given StringBuffer with summary information of each
 * bundle on the current pending list.
 */
void
BundleForwarder::get_pending(StringBuffer* buf)
{
    const Bundle* b;
    BundleList::const_iterator iter;
    const BundleList* pending = active_router()->pending_bundles();
    
    ScopeLock l(pending->lock());

    buf->appendf("Currently Pending Bundles (%d): \n", pending->size());
    
    for (iter = pending->begin(); iter != pending->end(); ++iter) {
        b = *iter;
        buf->appendf("\t%-3d: %s -> %s length %d\n",
                     b->bundleid_,
                     b->source_.c_str(),
                     b->dest_.c_str(),
                     b->payload_.length());
    }
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
    case ENQUEUE_BUNDLE:
    {
        BundleEnqueueAction* enqaction = (BundleEnqueueAction*) action;
        log_debug("forward bundle %d", bundle->bundleid_);
        enqaction->nexthop_->enqueue_bundle(bundle, &enqaction->mapping_);
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

