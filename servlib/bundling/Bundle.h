#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include "BundlePayload.h"
#include "BundleTuple.h"
#include "debug/Formatter.h"
#include "storage/Serialize.h"
#include "thread/SpinLock.h"
#include <sys/time.h>
#include <set>

class BundleList;
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
     * Bump up the reference count.
     *
     * @return the new reference count
     */
    int add_ref();

    /**
     * Decrement the reference count.
     *
     * If the reference count becomes zero, the bundle is deleted.
     *
     * @return the new reference count
     */
    int del_ref();

    /**
     * The number of container lists that have a reference to this
     * bundle.
     */
    int num_containers() { return containers_.size(); }
    
    /**
     * Add a BundleList to the set of containers.
     *
     * @return true if the list pointer was added successfully, false
     * if it was already in the set
     */
    bool add_container(BundleList* blist);

    /**
     * Remove a BundleList from the set of containers.
     *
     * @return true if the list pointer was removed successfully,
     * false if it wasn't in the set
     */
    bool del_container(BundleList* blist);
    
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
    u_int32_t orig_length_;	///< Length of original bundle
    u_int32_t frag_offset_;	///< Offset of fragment in the original bundle

    /*
     * Internal fields for managing the bundle.
     */
    // XXX/demmer this should actually store a struct of the list and
    // the iterator representing the position of the bundle in the
    // list. this a) makes removal more efficient, and b) allows the
    // same bundle to be on a list twice (e.g. sent_but_unacked in the
    // udp cl)
    typedef std::set<BundleList*> BundleListSet;
    
    BundleListSet containers_;	///< The set of BundleLists that
                                ///  contain the Bundle.
    int refcount_;		///< Bundle reference count
    SpinLock lock_;		///< Lock for bundle data that can be
                                ///  updated by multiple threads, e.g.
                                ///  containers_ and refcount_.

private:
    /**
     * Initialization helper function.
     */
    void init(u_int32_t id, BundlePayload::location_t location);
};

#endif /* _BUNDLE_H_ */
