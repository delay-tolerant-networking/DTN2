#ifndef __SQL_DISABLED__

#include "SQLGlobalStore.h"
#include "SQLStore.h"
#include "StorageConfig.h"

static const char* TABLENAME = "globals";

SQLGlobalStore::SQLGlobalStore(SQLImplementation* impl)
{
    store_ = new SQLStore(TABLENAME, impl);
    store_->create_table(this);
}

SQLGlobalStore::~SQLGlobalStore()
{
    NOTREACHED;
}

bool
SQLGlobalStore::load()
{
    log_debug("loading global store");

    SerializableObjectVector elements;
    elements.push_back(this);
    
    int cnt = store_->elements(&elements);
    
    if (cnt == 1) {
        log_debug("loaded next bundle id %d next reg id %d",
                  next_bundleid_, next_regid_);

    } else if (cnt == 0 && StorageConfig::instance()->tidy_) {
        log_info("globals table does not exist... initializing it");
        
        next_bundleid_ = 0;
        next_regid_ = 0;
        
        if (store_->insert(this) != 0) {
            log_err("error initializing globals table");
            exit(-1);
        }

    } else {
        log_err("error loading globals table");
        exit(-1);
    }

    return true;
}

bool
SQLGlobalStore::update()
{
    log_debug("updating global store");
    
    if (store_->update(this, -1) != 1) {
        log_err("error updating global store");
        return false;
    }
    
    return true;
}

#endif /* __SQL_DISABLED__ */
