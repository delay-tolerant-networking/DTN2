
#include "SQLGlobalStore.h"
#include "SQLImplementation.h"
#include "SQLStore.h"

static const char* TABLENAME = "globals";

SQLGlobalStore::SQLGlobalStore(SQLImplementation* impl)
{
    store_ = new SQLStore(TABLENAME, impl);
}

SQLGlobalStore::~SQLGlobalStore()
{
    NOTREACHED;
}

bool
SQLGlobalStore::load()
{
    log_debug("loading global store");
    
    if (store_->has_table(TABLENAME)) {
        SerializableObjectVector elements;
        elements.push_back(this);
        
        if (store_->elements(&elements) != 1) {
            log_err("error reloading global store table");
            return false;
        }

        log_debug("loaded next bundle id %d next reg id %d",
                  next_bundleid_, next_regid_);
    } else {
        log_warn("globals table does not exist... creating it");
        store_->create_table(this);
        next_bundleid_ = 0;
        next_regid_ = 0;
        update();
    }

    return true;
}

bool
SQLGlobalStore::update()
{
    log_debug("updating global store");
    
    if (store_->put(this) != 0) {
        log_err("error updating global store");
        return false;
    }

    return true;
}
