
#include "BerkeleyDBStore.h"
#include "BerkeleyDBGlobalStore.h"

BerkeleyDBGlobalStore::BerkeleyDBGlobalStore()
{
    store_ = new BerkeleyDBStore("bundles");
}

BerkeleyDBGlobalStore::~BerkeleyDBGlobalStore()
{
    NOTREACHED;
}

bool
BerkeleyDBGlobalStore::load()
{
    log_crit("%s not implemented", __FUNCTION__);
    return false;
}

bool
BerkeleyDBGlobalStore::update()
{
    log_crit("%s not implemented", __FUNCTION__);
    return false;
}
