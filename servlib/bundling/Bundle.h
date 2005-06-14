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

#include <map>
#include <sys/time.h>

#include <oasys/debug/Formatter.h>
#include <oasys/debug/Debug.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "BundlePayload.h"
#include "BundleTuple.h"


namespace dtn {

class BundleList;
class BundleMapping;
class BundleStore;
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
    int format(char* buf, size_t sz);
    
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
    bool is_fragment_;		///< Fragmentary Bundle
    u_int32_t orig_length_;	///< Length of original bundle
    u_int32_t frag_offset_;	///< Offset of fragment in the original bundle
    BundlePayload payload_;	///< Reference to the payload
    
    /*
     * Internal fields for managing the bundle.
     */
    
    u_int32_t bundleid_;	///< Local bundle identifier
    oasys::SpinLock lock_;	///< Lock for bundle data that can be
                                ///  updated by multiple threads, e.g.
                                ///  containers_ and refcount_.
    BundleMappings mappings_;	///< The set of BundleLists that
                                ///  contain the Bundle.
    int refcount_;		///< Bundle reference count
    bool is_reactive_fragment_; ///< Reactive fragmentary bundle

private:
    /**
     * Initialization helper function.
     */
    void init(u_int32_t id);
};


} // namespace dtn

#endif /* _BUNDLE_H_ */
