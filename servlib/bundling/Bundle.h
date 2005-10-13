/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include "BundlePayload.h"
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
class Bundle : public oasys::Formatter, public oasys::SerializableObject {
public:
    /**
     * Default constructor to create an empty bundle, initializing all
     * fields to defaults and allocating a new bundle id.
     */
    Bundle();

    /**
     * Constructor when re-reading the database.
     */
    Bundle(const oasys::Builder&);

    /**
     * Constructor that takes an explicit id and location of the
     * payload.
     */
    Bundle(u_int32_t id, BundlePayload::location_t location);

    /**
     * Bundle destructor.
     */
    virtual ~Bundle();

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
     * Return the bundle's reference count, corresponding to the
     * number of entries in the containers_ set, i.e. the number of
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
     * Validate the bundle's fields
     */
    bool validate(oasys::StringBuffer* errbuf);

    /**
     * True if any return receipt fields are set
     */
    bool receipt_requested()
    {
        return (receive_rcpt_ | custody_rcpt_ | forward_rcpt_ |
                delivery_rcpt_ | delivery_rcpt_);
    }
    
    /**
     * Values for the bundle priority field.
     */
    typedef enum {
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

    /**
     * Use a struct timeval (for now) as the creation timestamp type.
     */
    typedef struct timeval timestamp_t;

    /*
     * Bundle data fields that correspond to data transferred between
     * nodes according to the bundle protocol (all public to avoid the
     * need for accessor functions).
     */
    EndpointID source_;		///< Source eid
    EndpointID dest_;		///< Destination eid
    EndpointID custodian_;	///< Current custodian eid
    EndpointID replyto_;	///< Reply-To eid
    bool is_fragment_;		///< Fragmentary Bundle
    bool is_admin_;		///< Administrative record bundle
    bool do_not_fragment_;	///< Bundle shouldn't be fragmented
    bool custody_requested_;	///< Custody requested
    u_int8_t priority_;		///< Bundle priority
    bool receive_rcpt_;		///< Hop by hop reception receipt
    bool custody_rcpt_;		///< Custody xfer reporting
    bool forward_rcpt_;		///< Hop by hop forwarding reporting
    bool delivery_rcpt_;	///< End-to-end delivery reporting
    bool deletion_rcpt_;	///< Bundle deletion reporting
    timestamp_t creation_ts_;	///< Creation timestamp
    u_int32_t expiration_;	///< Bundle expiration time
    u_int32_t frag_offset_;	///< Offset of fragment in the original bundle
    u_int32_t orig_length_;	///< Length of original bundle
    BundlePayload payload_;	///< Reference to the payload

    /*
     * Public internal fields for managing the bundle.
     */
    u_int32_t bundleid_;	///< Local bundle identifier
    oasys::SpinLock lock_;	///< Lock for bundle data that can be
                                ///  updated by multiple threads, e.g.
                                ///  containers_ and refcount_.
    bool is_reactive_fragment_; ///< Reactive fragmentary bundle
    std::string owner_;         ///< Declared owner of this bundle,
                                ///  could be empty
    ExpirationTimer* expiration_timer_;	///< The expiration timer

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
