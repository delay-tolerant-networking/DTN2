
#include "config.h"

#if __DB_ENABLED__

#include "BerkeleyDBStore.h"
#include "BerkeleyDBGlobalStore.h"

BerkeleyDBGlobalStore::BerkeleyDBGlobalStore()
{
    store_ = new BerkeleyDBStore("globals");
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

#endif /* __DB_ENABLED__ */
