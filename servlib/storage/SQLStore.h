#ifndef _SQL_STORE_H_
#define _SQL_STORE_H_

#include <sys/time.h>
#include "BundleStore.h"
#include "PersistentStore.h"
#include "SQLSerialize.h"

class SQLManager;

/**
 * Implementation of a StorageManager with an underlying SQL
 * database.
 */
class SQLStore : public PersistentStore {
public:
    SQLStore(const char* table_name, SerializableObject* obj,
             SQLImplementation *db);
    
    /// @{ Virtual overrides from PersistentStore

    const char* table_name(); 

    int get(SerializableObject* obj, const int key);
    int num_elements();
    void keys(std::vector<int> l);

    // Returns a sql query that can be  used to fetch the object  from database
    
    int put(SerializableObject* obj, const int key);
    int del(const int key);
    void elements(std::vector<SerializableObject*> l);
      

    /// @}

protected:
    const char*  get_sqlquery(SerializableObject* obj, const int key);
    const char* num_elements_sqlquery();
    const char* keys_sqlquery();

    // creates table in the database if it does not exist
    int create_table(SerializableObject* obj);

    int exec_query(const char* query);

    const char* OBJECT_ID_FIELD;
      
    friend class SQLBundleStore;
private:
    const char* table_name_;
    
    //= "oid";
      

    SQLImplementation* data_base_pointer_;
};



/**
 * Implementation of a BundleStore that uses an underlying SQL
 * database.
 */

class SQLBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods.
     */
    SQLBundleStore(SQLStore* store)
        : BundleStore(store) {}

    /**
     * Destructor.
     */
    //    virtual ~SQLBundleStore();

    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    // int next_id();
    
    /**
     * Delete expired bundles
     *
     * (was sweepOldBundles)
     */
    // REMOVE FROM bundles where bundles.expiration > now
    int delete_expired(const time_t now);

    /**
     * Return true if we're the custodian of the given bundle.
     * TODO: is this really needed??
     *
     * (was db_bundle_retain)
     */
    bool is_custodian(int bundle_id);

private:
    int next_bundle_id_; 	/// running serial number for bundles
    SQLStore* store_;	/// abstract persistent storage implementation
};

#endif /* _SQL_STORE_H_ */
