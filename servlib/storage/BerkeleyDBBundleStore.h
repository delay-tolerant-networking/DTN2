#ifndef _BERKELEYDB_BUNDLESTORE_H_
#define _BERKELEYDB_BUNDLESTORE_H_

#include "BundleStore.h"

class BerkeleyDBStore;

/**
 * Implementation of a BundleStore that uses an underlying BerkeleyDB
 * database.
 */

class BerkeleyDBBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods.
     */
    BerkeleyDBBundleStore();

    /**
     * Destructor.
     */
    virtual ~BerkeleyDBBundleStore();

    Bundle* get(int bundle_id);
    int     insert(Bundle* bundle);
    int     update(Bundle* bundle);
    int     del(int bundle_id);

protected:
    BerkeleyDBStore* store_;
};


#endif /* _BERKELEYDB_BUNDLESTORE_H_ */
