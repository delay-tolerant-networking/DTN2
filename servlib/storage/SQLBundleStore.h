#ifndef _SQL_BUNDLE_STORE_H_
#define _SQL_BUNDLE_STORE_H_

#include <sys/time.h>
#include "BundleStore.h"

class SQLImplementation;
class SQLStore;

/**
 * Implementation of a BundleStore that uses an underlying SQL
 * database.
 */

class SQLBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods. The table_name identifies the table in which all bundles
     * will be stored
     */
    SQLBundleStore(SQLImplementation* db, const char* table_name = "bundles");
    
    /**
     * Virtual methods inheritied from BundleStore
     */
    Bundle* get(int bundle_id) ;
    int     put(Bundle* bundle);
    int     del(int bundle_id) ;
    int delete_expired(const time_t now);
    bool is_custodian(int bundle_id);
    
private:
    
    /**
     * The SQLStore instance used to store all the bundles.
     */
    SQLStore* store_;
};

#endif /* _SQL_BUNDLE_STORE_H_ */
