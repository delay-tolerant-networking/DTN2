#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include "debug/Formatter.h"
#include "storage/Serialize.h"
#include "BundlePayload.h"
#include "BundleTuple.h"
#include "thread/Atomic.h"

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
 * Values for the bundle delivery options.
 */
typedef enum {
    COS_NONE 	       = 0,	///< no custody, etc
    COS_CUSTODY        = 1,	///< custody xfer
    COS_RET_RCPT       = 2,	///< return receipt
    COS_DELIV_REC_FORW = 4,	///< request forwarder info
    COS_DELIV_REC_CUST = 8,	///< request custody xfer info
    COS_OVERWRITE      = 16	///< request queue overwrite option
} bundle_delivery_opts_t;

/**
 * The internal representation of a bundle.
 */
class Bundle : public Formatter, public SerializableObject {
public:
    Bundle();
    Bundle(const std::string& source,
           const std::string& dest,
           const std::string& payload);
    virtual ~Bundle();

    // virtual from Formatter
    int format(char* buf, size_t sz);
    
    // basic bundle information
    u_int32_t bundleid_;	///< Local bundle identifier
    u_int32_t expiration_;	///< Expiration time of the bundle
    BundlePayload payload_;	///< Reference to the payload
    BundleTuple source_;	///< Source tuple
    BundleTuple dest_;		///< Destination tuple
    BundleTuple custodian_;	///< Current custodian tuple (may not be present)
    BundleTuple replyto_;	///< Reply-To tuple (may not be present)

    // cos and delivery options
    bool custreq_;		///< True means custody requested
    bool return_rept_;		///< True means return-receipt requested
    u_int16_t priority_;	///< Bundle priority
    u_int16_t delivery_opts_;	///< Bundle delivery options

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
        log_debug("/bundle/refs", "refcount %d %d -> %d",
                  bundleid_, refcount_ - 1, refcount_);
        ASSERT(refcount_ > 0);
    }

    /**
     * Decrement the reference count.
     */
    void delref() {
        ASSERT(refcount_ > 0);
        atomic_decr(&refcount_);
        log_debug("/bundle/refs", "refcount %d %d -> %d",
                  bundleid_, refcount_ + 1, refcount_);
    }

protected:
    int refcount_;		///< reference count
};

#endif /* _BUNDLE_H_ */
