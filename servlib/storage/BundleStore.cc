
#include "bundling/Bundle.h"
#include "BundleStore.h"
#include "iostream"

BundleStore* BundleStore::instance_;

BundleStore::BundleStore()
{
    // XXX read the next_bundle_id 
}

BundleStore::~BundleStore()
{
}


/**
 * Get a new bundle id, updating the value in the store
 *
 */
int
BundleStore::next_id()
{
    return next_bundle_id_++;
}
