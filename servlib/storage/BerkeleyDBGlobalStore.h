#ifndef _BERKELEY_DB_GLOBAL_STORE_H_
#define _BERKELEY_DB_GLOBAL_STORE_H_

#include "GlobalStore.h"

class BerkeleyDBStore;

class BerkeleyDBGlobalStore : public GlobalStore {
public:
    /**
     * Constructor
     */
    BerkeleyDBGlobalStore();

    /**
     * Destructor
     */
    virtual ~BerkeleyDBGlobalStore();

    /**
     * Load in the values from the database.
     */
    virtual bool load();

    /**
     * Update the values in the database.
     */
    virtual bool update();

protected:
    BerkeleyDBStore* store_;
};

#endif /* _BERKELEY_DB_GLOBAL_STORE_H_ */
