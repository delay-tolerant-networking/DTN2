
#include "bundling/Bundle.h"
#include "BundleStore.h"
#include "PersistentStore.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleEvent.h"


BundleStore* BundleStore::instance_;

BundleStore::BundleStore(PersistentStore * store)
    : Logger("/storage/bundles")
{
    store_ = store;
}

bool BundleStore::load()
{
    log_debug("Loading existing bundles from database.");

    // load existing stored bundles
    Bundle * bundle;
    std::vector<int> ids;
    std::vector<int>::iterator iter;

    store_->keys(&ids);

    for (iter = ids.begin();
         iter != ids.end();
         ++iter)
    {
        bundle = get(*iter);
        if (bundle)
        {
            BundleForwarder::post(new BundleReceivedEvent(bundle, bundle->payload_.length()));
        }
    }

    return true;
}

BundleStore::~BundleStore()
{
}

Bundle*
BundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle(); // note: this unecessarily increments bundle id
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

bool
BundleStore::add(Bundle* bundle)
{
    return store_->add(bundle, bundle->bundleid_) == 0;
}

bool
BundleStore::update(Bundle* bundle)
{
    return store_->update(bundle, bundle->bundleid_) == 0;
}

bool
BundleStore::del(int bundle_id)
{
    return store_->del(bundle_id) == 0;
}

