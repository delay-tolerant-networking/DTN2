#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include "debug/Formatter.h"
#include "storage/Serialize.h"
#include "BundlePayload.h"
#include "BundleTuple.h"
#include "thread/Atomic.h"
#include <sys/time.h>

class SQLBundleStore;

/**
 * Values for the bundle delivery options.
 */
typedef enum {
} bundle_delivery_opts_t;

/**
 * The internal representation of a bundle.
 */
class Bundle : public Formatter, public SerializableObject {
public:
    /**
     * Default constructor to create an empty bundle, initializing all
     * fields to defaults and allocating a new bundle id.
     */
    Bundle();

    /**
     * Minimal constructor for a valid routable bundle.
     */
    Bundle(const std::string& source,
           const std::string& dest,
           const std::string& payload);

    /**
     * Special constructor for a temporary (invalid) bundle, used to
     * create the database table schema for SQL databases.
     */
    Bundle(SQLBundleStore* store) {}

    /**
     * Default initialization.
     */
    void init();

    /**
     * Bundle destructor.
     */
    virtual ~Bundle();
    
    // virtual from Formatter
    int format(char* buf, size_t sz);
    
    /**
     * Virtual from SerializableObject
     */
    void serialize(SerializeAction* a);

    /**
     * Return the bundle's reference count. Incremented whenever the
     * bundle is added to a bundle list for forwarding either to
     * another daemon or to an application.
     */
    int refcount() { return refcount_; }

    /**
     * Bump up the reference count.
     */
    void addref() {
        atomic_incr(&refcount_);
        log_debug("/bundle/refs", "refcount bundleid %d %d -> %d",
                  bundleid_, refcount_ - 1, refcount_);
        ASSERT(refcount_ > 0);
    }

    /**
     * Decrement the reference count.
     */
    void delref() {
        ASSERT(refcount_ > 0);
        atomic_decr(&refcount_);
        log_debug("/bundle/refs", "refcount bundleid %d %d -> %d",
                  bundleid_, refcount_ + 1, refcount_);
    }

    /**
     * Values for the bundle priority field.
     */
    typedef enum {
        COS_BULK      = 0, 		///< lowest priority
        COS_NORMAL    = 1, 		///< regular priority
        COS_EXPEDITED = 2, 		///< important
        COS_RESERVED  = 3  		///< TBD
    } bundle_cos_t;

    /**
     * Use a struct timeval (for now) as the creation timestamp type.
     */
    typedef struct timeval timestamp_t;
    

    // Bundle data fields (all public to avoid the need for accessor functions)
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
    
protected:
    int refcount_;		///< reference count
};

#endif /* _BUNDLE_H_ */
