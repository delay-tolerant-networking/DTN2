#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include <map>
#include <sys/time.h>

#include <oasys/debug/Formatter.h>
#include <oasys/debug/Debug.h>

#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>

#include "BundlePayload.h"
#include "BundleTuple.h"

class BundleList;
class BundleMapping;
class SQLBundleStore;

/**
 * The internal representation of a bundle.
 *
 * Bundles are reference counted, with references generally
 * correlating one-to-one with each BundleList on which the Bundle
 * resides.
 *
 * However, although the push() methods of the BundleList always add a
 * reference and a backpointer to the bundle, the pop() methods do not
 * decremente the reference count. This means that the caller must
 * explicitly remove it when done with the bundle.
 *
 * Note that delref() will delete the Bundle when the reference count
 * reaches zero, so care must be taken to never use the pointer after
 * that point.
 *
 * The Bundle class maintains a set of back-pointers each BundleList
 * it is container on, and list addition/removal methods maintain the
 * invariant that the entiries of this set correlate exactly with the
 * list pointers.
 */
class Bundle : public Formatter, public SerializableObject {
public:
    /**
     * Default constructor to create an empty bundle, initializing all
     * fields to defaults and allocating a new bundle id.
     */
    Bundle();

    /**
     * Constructor that takes an explicit id and location of bundle payload
     * 
     */
    Bundle(u_int32_t id, BundlePayload::location_t location);

    /**
     * Special constructor for a temporary (invalid) bundle, used to
     * create the database table schema for SQL databases.
     */
    Bundle(SQLBundleStore* store) {}
    
    /**
     * Bundle destructor.
     */
    virtual ~Bundle();

    /**
     * Virtual from formatter.
     */
    int format(char* buf, size_t sz);
    
    /**
     * Virtual from SerializableObject
     */
    void serialize(SerializeAction* a);

    /**
     * Return the bundle's reference count, corresponding to the
     * number of entries in the containers_ set, i.e. the number of
     * BundleLists that have a reference to this bundle, as well as
     * any other scopes that are processing the bundle.
     */
    int refcount() { return refcount_; }

    /**
     * Return the number of transmissions pending for this bundle.
     * When this goes to zero, the bundle can be removed from the
     * router's pending bundles list.
     */
    int pendingtx() { return pendingtxcount_; }

    /**
     * Bump up the pending transmissions count.
     */
    int add_pending();

    /**
     * Decrement the pending transmissions count.
     */
    int del_pending();

    /**
     * Bump up the reference count. Parameters are used for logging.
     *
     * @return the new reference count
     */
    int add_ref(const char* what1, const char* what2 = "");

    /**
     * Decrement the reference count. Parameters are used for logging.
     *
     * If the reference count becomes zero, the bundle is deleted.
     *
     * @return the new reference count
     */
    int del_ref(const char* what1, const char* what2 = "");

    /*
     * Types used for the mapping table.
     */
    typedef std::map<BundleList*, BundleMapping*> BundleMappings;
    typedef BundleMappings::const_iterator MappingsIterator;
    
    /**
     * The number of mappings for this bundle.
     */
    int num_mappings() { return mappings_.size(); }
    
    /**
     * Add a BundleList to the set of mappings, copying the mapping
     * information.
     *
     * @return the new mapping if it was added successfully, NULL if
     * the list was already in the set
     */
    BundleMapping* add_mapping(BundleList* blist,
                               const BundleMapping* mapping_info);

    /**
     * Remove a mapping.
     *
     * @return the mapping if it was removed successfully, NULL if
     * wasn't in the set.
     */
    BundleMapping* del_mapping(BundleList* blist);

    /**
     * Get the mapping state for the given list. Returns NULL if the
     * bundle is not currently queued on the list.
     */
    BundleMapping* get_mapping(BundleList* blist);

    /**
     * Return an iterator to scan through the mappings.
     */
    MappingsIterator mappings_begin();
    
    /**
     * Return an iterator to mark the end of the mappings set.
     */
    MappingsIterator mappings_end();
    
    /**
     * Values for the bundle priority field.
     */
    typedef enum {
        COS_BULK      = 0, 		///< lowest priority
        COS_NORMAL    = 1, 		///< regular priority
        COS_EXPEDITED = 2, 		///< important
        COS_RESERVED  = 3  		///< TBD
    } bundle_priority_t;

    /**
     * Pretty printer function for bundle_priority_t.
     */
    static const char* prioritytoa(u_int8_t priority) {
        switch (priority) {
        case COS_BULK: 		return "BULK";
        case COS_NORMAL: 	return "NORMAL";
        case COS_EXPEDITED: 	return "EXPEDITED";
        default:		return "_UNKNOWN_PRIORITY_";
        }
    }

    /**
     * Use a struct timeval (for now) as the creation timestamp type.
     */
    typedef struct timeval timestamp_t;

    /*
     * Bundle data fields (all public to avoid the need for accessor
     * functions).
     */
    
    u_int32_t bundleid_;	///< Local bundle identifier
    BundleTuple source_;	///< Source tuple
    BundleTuple dest_;		///< Destination tuple
    BundleTuple custodian_;	///< Current custodian tuple
    BundleTuple replyto_;	///< Reply-To tuple
    u_int8_t priority_;		///< Bundle priority
    bool custreq_;		///< Custody requested
    bool custody_rcpt_;		///< Custody xfer reporting
    bool recv_rcpt_;		///< Hop by hop reception receipt
    bool fwd_rcpt_;		///< Hop by hop forwarding receipt
    bool return_rcpt_;		///< End-to-end return receipt
    timestamp_t creation_ts_;	///< Creation timestamp
    u_int32_t expiration_;	///< Bundle expiration time
    BundlePayload payload_;	///< Reference to the payload

    bool is_fragment_;		///< Fragmentary Bundle
    bool is_reactive_fragment_; ///< Reactive fragmentary bundle
    u_int32_t orig_length_;	///< Length of original bundle
    u_int32_t frag_offset_;	///< Offset of fragment in the original bundle

    /*
     * Internal fields for managing the bundle.
     */
    
    SpinLock lock_;		///< Lock for bundle data that can be
                                ///  updated by multiple threads, e.g.
                                ///  containers_ and refcount_.
    BundleMappings mappings_;	///< The set of BundleLists that
                                ///  contain the Bundle.
    
    int refcount_;		///< Bundle reference count
    int pendingtxcount_;        ///< Number of pending transmissions left

private:
    /**
     * Initialization helper function.
     */
    void init(u_int32_t id, BundlePayload::location_t location);
};


#endif /* _BUNDLE_H_ */
