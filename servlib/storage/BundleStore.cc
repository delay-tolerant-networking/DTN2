
#include "bundling/Bundle.h"
#include "BundleStore.h"
#include "PersistentStore.h"

BundleStore::BundleStore(PersistentStore* store)
    : next_bundle_id_(0), store_(store)
{
    // read the next_bundle_id 
}

/**
 * Create a new bundle instance, then make a generic call into the
 * persistent store to look up the bundle and fill in it's members if
 * found.
 */
Bundle*
BundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle();
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

/**
 * Store the given bundle in the persistent store.
 */
int
BundleStore::put(Bundle* bundle)
{
    return store_->put(bundle, bundle->id());
}

/**
 * Delete the given bundle from the persistent store.
 */
int
BundleStore::del(int bundle_id)
{
    return store_->del(bundle_id);
}
