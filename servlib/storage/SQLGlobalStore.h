#ifndef _SQL_GLOBAL_STORE_H_
#define _SQL_GLOBAL_STORE_H_

#include "GlobalStore.h"

class SQLStore;
class SQLImplementation;

/**
 * Implementation of GlobalStore that uses an underlying SQL database.
 */
class SQLGlobalStore : public GlobalStore {
public:
    /**
     * Constructor
     */
    SQLGlobalStore(SQLImplementation* impl);

    /**
     * Destructor
     */
    virtual ~SQLGlobalStore();

    /**
     * Load in the values from the database.
     */
    virtual bool load();

    /**
     * Update the values in the database.
     */
    virtual bool update();

protected:
    SQLStore* store_;
};

#endif /* _SQL_GLOBAL_STORE_H_ */
