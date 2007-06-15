/*
 *    Copyright 2007 Baylor University
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

#ifndef _BUNDLE_TLV_ENTRY_H_
#define _BUNDLE_TLV_ENTRY_H_

#include <sys/types.h>

#include "PointerList.h"

namespace prophet 
{

/**
 * BundleTLVEntry is the in-memory representation of an element
 * listed within the Bundle Offer and Response TLV, p. 30, 4.4.5
 */
class BundleTLVEntry
{
public:
    /**
     * A BundleTLVEntry can represent either an OFFER or a RESPONSE,
     * depending on what is inferred from the flags
     */
    typedef enum
    {
        UNDEFINED = 0, ///< no valid type has been specified
        OFFER,         ///< Bundle OFFER
        RESPONSE,      ///< Bundle RESPONSE
    } bundle_entry_t;

    /**
     * Convenience function
     */
    static const char* type_to_str(bundle_entry_t type)
    {
        switch(type) {
        case OFFER:    return "OFFER"; 
        case RESPONSE: return "RESPONSE";
        case UNDEFINED:
        default: break;
        }
        return "UNDEFINED";
    }

    /**
     * Factory method for convenience
     */
    static inline BundleTLVEntry* create_entry(u_int32_t cts, u_int32_t seq,
                                               u_int16_t sid, bool custody,
                                               bool accept, bool ack);

protected:
    /**
     * Default constructor, only used by friend classes
     */
    BundleTLVEntry(bundle_entry_t type = UNDEFINED)
        : cts_(0), seq_(0), sid_(0), custody_(false),
          accept_(false), ack_(false), type_(type) {}

    /**
     * Constructor, only to be used by factory methods and friend classes
     */
    BundleTLVEntry(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                bool custody=false, bool accept=false, bool ack=false,
                bundle_entry_t type=UNDEFINED)
        : cts_(cts), seq_(seq), sid_(sid), custody_(custody),
          accept_(accept), ack_(ack), type_(type)
    {
        init_type(type);
    }

public:
    /**
     * Copy constructor
     */
    BundleTLVEntry(const BundleTLVEntry& b)
        : cts_(b.cts_), seq_(b.seq_), sid_(b.sid_), custody_(b.custody_),
          accept_(b.accept_), ack_(b.ack_), type_(b.type_)
    {
        init_type(b.type_);
    }

    /**
     * Assignment operator
     */
    BundleTLVEntry& operator= (const BundleTLVEntry& b) 
    {
        cts_     = b.cts_;
        seq_     = b.seq_;
        sid_     = b.sid_;
        custody_ = b.custody_;
        accept_  = b.accept_;
        ack_     = b.ack_;
        type_    = b.type_;
        init_type(b.type_); // sanity check
        return *this;
    }

    /**
     * Destructor
     */
    virtual ~BundleTLVEntry() {}

    /**
     * Comparison operator to facilitate STL sorting
     */
    bool operator< (const BundleTLVEntry& b) const 
    {
        // if (type_ != b.type_) ... ignore difference
        if (sid_ == b.sid_)
        {
            if (cts_ == b.cts_)
                return seq_ < b.seq_;
            else
                return (cts_ < b.cts_);
        }
        return (sid_ < b.sid_);
    }

    ///@{ Accessors
    u_int32_t creation_ts() const { return cts_; }
    u_int32_t seqno() const { return seq_; }
    u_int16_t sid() const { return sid_; }
    bool custody() const { return custody_; }
    bool accept() const { return accept_; }
    bool ack() const { return ack_; }
    virtual bundle_entry_t type() const { return type_; }
    ///@}

    /**
     * Utility function to decipher which Bundle_X_Entry type 
     * based on the combination of flags
     */
    static bundle_entry_t decode_flags(bool custody, bool accept, bool ack) 
    {
        // this combination makes no sense
        if ( accept && ack )
            return UNDEFINED;

        // infer OFFER from !accept and some combos of custody and ack
        if (! accept)
        {
            // It doesn't make any sense to offer custody
            // on ACK
            if (custody && ack)
                return UNDEFINED;

            // The other three combos of custody and ack are legal for OFFER
            return OFFER;
        }

        // infer RESPONSE from this combo
        if ( (custody || accept) && (! ack))
        {
            // It doesn't make any sense to offer custody 
            // on a bundle unless we accept it
            if (custody && !accept)
                return UNDEFINED;

            return RESPONSE;
        }

        // failed to decode
        return UNDEFINED;
    }

protected:
    /**
     * Initialization routine used by constructors and assignment operator
     */
    void init_type(bundle_entry_t type)
    {
        bundle_entry_t df = decode_flags(custody_,accept_,ack_);

        // Three values to compare: The initial value
        // of "type_", the incoming parameter "type",
        // and the inferred value "df". 

        // One value to set: class member "type_"

        // If type_ == UNDEFINED, then df and type must match
        // and type_ is set to their shared value.
        // If type_ == UNDEFINED, and df and type do not match,
        // type_ does not change
        // If type_ != UNDEFINED, then df and type must match type_
        // else type_ gets set to UNDEFINED

        if (type_ == UNDEFINED)
        {
            if (df == type) type_ = type;
            // else type_ = UNDEFINED
            return;
        }
        // else if (type_ != UNDEFINED)
        if ((df == type && df == type_))
            return;

        // logical fault
        type_ = UNDEFINED;
    }

    u_int32_t cts_; ///< Creation timestamp
    u_int32_t seq_; ///< sub-second sequence number
    u_int16_t sid_; ///< string id of bundle destination
    bool      custody_; ///< whether this node accepts custody
    bool      accept_;  ///< whether this Bundle Entry is accepted
    bool      ack_; ///< represents successful Prophet delivery for bundle
    bundle_entry_t type_; ///< indicates whether offer or response TLV

}; // class BundleTLVEntry

/**
 * BundleOfferEntry represents one Bundle, a single entry in a Bundle TLV
 * sent by a Prophet router in the WAIT_RIB or OFFER state.
 */
class BundleOfferEntry : public BundleTLVEntry
{
public:
    /**
     * Constructor
     */
    BundleOfferEntry(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                  bool custody, bool ack)
        : BundleTLVEntry(cts,seq,sid,custody,false,ack,BundleTLVEntry::OFFER)
    {}

    /**
     * Copy constructor.
     */
    BundleOfferEntry(const BundleOfferEntry& b)
        : BundleTLVEntry(b) {}

    /**
     * Destructor
     */
    virtual ~BundleOfferEntry() {}

protected:
    friend class PointerList<BundleOfferEntry>;

    /**
     * Default constructor, only used by friend classes
     */
    BundleOfferEntry()
    {
        BundleTLVEntry::type_ = BundleTLVEntry::UNDEFINED;
    }
}; // class BundleOfferEntry

/**
 * BundleResponseEntry represents one Bundle, a single entry in a Bundle TLV
 * sent by a Prophet router in the REQUEST state.
 */
class BundleResponseEntry : public BundleTLVEntry
{
public:
    /**
     * Constructor
     */
    BundleResponseEntry(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                  bool custody, bool accept)
        : BundleTLVEntry(cts,seq,sid,custody,accept,false,BundleTLVEntry::RESPONSE)
    {}

    /**
     * Copy constructor.
     */
    BundleResponseEntry(const BundleResponseEntry& b)
        : BundleTLVEntry(b) {}

    /**
     * Destructor
     */
    virtual ~BundleResponseEntry() {}

protected:
    friend class PointerList<BundleResponseEntry>;

    /**
     * Default constructor, only used by friend classes
     */
    BundleResponseEntry()
    {
        BundleTLVEntry::type_ = BundleTLVEntry::UNDEFINED;
    }
}; // class BundleResponseEntry

BundleTLVEntry*
BundleTLVEntry::create_entry(u_int32_t cts, u_int32_t seq, u_int16_t sid, 
                             bool custody, bool accept, bool ack) 
{
    switch (decode_flags(custody,accept,ack))
    {
        case OFFER:
            return new BundleOfferEntry(cts,seq,sid,custody,ack);
        case RESPONSE:
            return new BundleResponseEntry(cts,seq,sid,custody,accept);
        case UNDEFINED:
        default:
            return NULL;
    }
    return NULL; // not reached
}

}; // namespace

#endif // _BUNDLE_TLV_ENTRY_H_
