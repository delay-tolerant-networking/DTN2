/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _BUNDLE_H_
#define _BUNDLE_H_

#include <set>
#include <sys/time.h>

#include <oasys/debug/Formatter.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "BlockInfo.h"
#include "BundlePayload.h"
#include "BundleTimestamp.h"
#include "CustodyTimer.h"
#include "ForwardingLog.h"
#include "naming/EndpointID.h"


namespace dtn {

class BundleList;
class BundleStore;
class ExpirationTimer;
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
 * The Bundle class maintains a set of back-pointers to each BundleList
 * it is contained on, and list addition/removal methods maintain the
 * invariant that the entiries of this set correlate exactly with the
 * list pointers.
 */
class Bundle : public oasys::Formatter, public oasys::SerializableObject
{
public:
    /**
     * Default constructor to create an empty bundle, initializing all
     * fields to defaults and allocating a new bundle id.
     *
     * For temporary bundles, the location can be set to MEMORY, and
     * to support the simulator, the location can be overridden to be
     * BundlePayload::NODATA.
     */
    Bundle(BundlePayload::location_t location = BundlePayload::DISK);

    /**
     * Constructor when re-reading the database.
     */
    Bundle(const oasys::Builder&);

    /**
     * Bundle destructor.
     */
    virtual ~Bundle();

    /**
     * Copy the metadata from one bundle to another (used in
     * fragmentation).
     */
    void copy_metadata(Bundle* new_bundle);

    /**
     * Virtual from formatter.
     */
    int format(char* buf, size_t sz) const;
    
    /**
     * Virtual from formatter.
     */
    void format_verbose(oasys::StringBuffer* buf);
    
    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    u_int32_t durable_key() { return bundleid_; }

    /**
     * Hook for the bundle store implementation to count the storage
     * impact of this bundle. Currently just returns the payload
     * length but should be extended to include the metadata as well.
     */
    size_t durable_size() { return payload_.length(); }
    
    /**
     * Return the bundle's reference count, corresponding to the
     * number of entries in the mappings set, i.e. the number of
     * BundleLists that have a reference to this bundle, as well as
     * any other scopes that are processing the bundle.
     */
    int refcount() { return refcount_; }

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
    typedef std::set<BundleList*> BundleMappings;
    typedef BundleMappings::const_iterator MappingsIterator;
    
    /**
     * The number of mappings for this bundle.
     */
    int num_mappings() { return mappings_.size(); }

    /**
     * Return an iterator to scan through the mappings.
     */
    MappingsIterator mappings_begin();
    
    /**
     * Return an iterator to mark the end of the mappings set.
     */
    MappingsIterator mappings_end();

    /**
     * Return true if the bundle is on the given list.
     */
    bool is_queued_on(BundleList* l);

    /**
     * Validate the bundle's fields
     */
    bool validate(oasys::StringBuffer* errbuf);

    /**
     * True if any return receipt fields are set
     */
    bool receipt_requested() const
    {
        return (receive_rcpt_ || custody_rcpt_ || forward_rcpt_ ||
                delivery_rcpt_ || deletion_rcpt_);
    }
    
    /**
     * Values for the bundle priority field.
     */
    typedef enum {
        COS_INVALID   = -1, 		///< invalid
        COS_BULK      = 0, 		///< lowest priority
        COS_NORMAL    = 1, 		///< regular priority
        COS_EXPEDITED = 2, 		///< important
        COS_RESERVED  = 3  		///< TBD
    } priority_values_t;

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
    
    typedef enum {
        RETENTION_DISPATCH    = 0x01,
        RETENTION_FORWARD     = 0x02,
        RETENTION_CUSTODY     = 0x04,
        RETENTION_REASSEMBLY  = 0x08,
        RETENTION_ANY         = 0xff
    } retention_t;
    
    static std::string retention_to_string(u_int32_t retention) {
        if (retention == 0)
            return std::string("none");
        
        std::string str;
        if (retention & RETENTION_DISPATCH) str += "DISPATCH ";
        if (retention & RETENTION_FORWARD) str += "FORWARD ";
        if (retention & RETENTION_CUSTODY) str += "CUSTODY ";
        if (retention & RETENTION_REASSEMBLY) str += "REASSEMBLY ";
        
        return str;
    }
    
    void add_retention_constraint(retention_t constraint) {
        retention_ |= constraint;
        log_debug_p("/dtn/bundle/retention", 
            "added retention constraint %s to bundle %d; current restraints: %s",
            retention_to_string(constraint).c_str(), bundleid_,
            retention_to_string(retention_).c_str());
    }
    
    void remove_retention_constraint(retention_t constraint) {
        retention_ &= ~constraint;
        log_debug_p("/dtn/bundle/retention", 
            "removed retention constraint %s from bundle %d; current restraints: %s",
            retention_to_string(constraint).c_str(), bundleid_,
            retention_to_string(retention_).c_str());
    }
    
    bool has_retention_constraint(retention_t constraint = RETENTION_ANY) {
        return (retention_ & constraint) != 0;
    }

    
    /*
     * Bundle data fields that correspond to data transferred between
     * nodes according to the bundle protocol (all public to avoid the
     * need for accessor functions).
     */
    EndpointID source_;		///< Source eid
    EndpointID dest_;		///< Destination eid
    EndpointID custodian_;	///< Current custodian eid
    EndpointID replyto_;	///< Reply-To eid
    EndpointID prevhop_;	///< Previous hop eid
    bool is_fragment_;		///< Fragmentary Bundle
    bool is_admin_;		///< Administrative record bundle
    bool do_not_fragment_;	///< Bundle shouldn't be fragmented
    bool custody_requested_;	///< Custody requested
    bool singleton_dest_;	///< Destination endpoint is a singleton
    u_int8_t priority_;		///< Bundle priority
    bool receive_rcpt_;		///< Hop by hop reception receipt
    bool custody_rcpt_;		///< Custody xfer reporting
    bool forward_rcpt_;		///< Hop by hop forwarding reporting
    bool delivery_rcpt_;	///< End-to-end delivery reporting
    bool deletion_rcpt_;	///< Bundle deletion reporting
    bool app_acked_rcpt_;	///< Acknowlege by application reporting
    BundleTimestamp creation_ts_; ///< Creation timestamp
    u_int32_t expiration_;	///< Bundle expiration time
    u_int32_t frag_offset_;	///< Offset of fragment in the original bundle
    u_int32_t orig_length_;	///< Length of original bundle
    BundlePayload payload_;	///< Reference to the payload

    /*
     * Internal fields and structures for managing the bundle that are
     * not transmitted over the network.
     */
    u_int32_t bundleid_;	///< Local bundle identifier
    oasys::SpinLock lock_;	///< Lock for bundle data that can be
                                ///  updated by multiple threads
    bool in_datastore_;		///< Is the bundle in the persistent store
    bool local_custody_;	///< Local node has custody
    std::string owner_;         ///< Declared router that "owns" this
                                ///  bundle, which could be empty
    u_int32_t retention_;   ///< Why the bundle is hanging around
    ForwardingLog fwdlog_;	///< Log of bundle forwarding records
    ExpirationTimer* expiration_timer_;	///< The expiration timer
    CustodyTimerVec custody_timers_; ///< Live custody timers for this bundle

    BlockInfoVec recv_blocks_;	///< BP blocks as they arrived off the wire
    BlockInfoVec api_blocks_;	///< BP blocks as they arrived from API
    LinkBlockSet xmit_blocks_;	///< Block vector for each link
protected:
    friend class BundleList;
    
    /*
     * Protected fields.
     */
    BundleMappings mappings_;   ///< The set of BundleLists that
                               	///  contain the Bundle.
    
    int  refcount_;		///< Bundle reference count
    bool freed_;		///< Bit to indicate whether or not a bundle
                                ///  free event has been posted for us

private:
    /**
     * Initialization helper function.
     */
    void init(u_int32_t id);
};


} // namespace dtn

#endif /* _BUNDLE_H_ */
