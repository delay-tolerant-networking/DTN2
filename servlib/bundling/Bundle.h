#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include "storage/Serialize.h"
#include "BundlePayload.h"
#include "BundleTuple.h"

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
class Bundle : public SerializableObject {
public:
    Bundle();
    virtual ~Bundle();
    
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
};

#endif /* _BUNDLE_H_ */
