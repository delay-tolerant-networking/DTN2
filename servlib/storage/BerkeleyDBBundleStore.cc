#ifndef __DB_DISABLED__

#include "BerkeleyDBStore.h"
#include "BerkeleyDBBundleStore.h"
#include "bundling/Bundle.h"

BerkeleyDBBundleStore::BerkeleyDBBundleStore()
{
    store_ = new BerkeleyDBStore("bundles");
}

BerkeleyDBBundleStore::~BerkeleyDBBundleStore()
{
}

Bundle*
BerkeleyDBBundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle();
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

bool
BerkeleyDBBundleStore::insert(Bundle* bundle)
{
    return store_->put(bundle, bundle->bundleid_) == 0;
}

bool
BerkeleyDBBundleStore::update(Bundle* bundle)
{
    return store_->put(bundle, bundle->bundleid_) == 0;
}

bool
BerkeleyDBBundleStore::del(int bundle_id)
{
    return store_->del(bundle_id) == 0;
}

#endif /* __DB_DISABLED__ */
