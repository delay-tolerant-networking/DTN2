
#include "Bundle.h"
#include "BundleAction.h"
#include "BundleList.h"
#include "BundleForwarder.h"
#include "Contact.h"
#include "reg/RegistrationTable.h"
#include "storage/BundleStore.h"

BundleForwarder* BundleForwarder::instance_ = NULL;

BundleForwarder::BundleForwarder()
    : Logger("/bundlefwd")
{
}

void
BundleForwarder::init()
{
    instance_ = new BundleForwarder();
}

void
BundleForwarder::process(BundleActionList* actions)
{
    Bundle* bundle;
    BundleAction* action;
    BundleActionList::iterator iter;

    for (iter = actions->begin(); iter != actions->end(); ++iter) {
        action = *iter;
        bundle = action->bundleref_.bundle();
        switch (action->action_) {
        case FORWARD_UNIQUE:
        case FORWARD_COPY:
            log_debug("forward bundle %d", bundle->bundleid_);
            action->nexthop_->consume_bundle(bundle);
            break;
        default:
            PANIC("unimplemented action code");
        }

        delete action;
    }

    actions->clear();
}

