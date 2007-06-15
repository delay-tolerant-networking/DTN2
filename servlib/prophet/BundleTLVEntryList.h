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

#ifndef _PROPHET_BUNDLE_ENTRY_LIST_H_
#define _PROPHET_BUNDLE_ENTRY_LIST_H_

#include <map>
#include <string>

#include "PointerList.h"
#include "BundleTLVEntry.h"
#include "Dictionary.h"
#include "Table.h"

namespace prophet
{

/**
 * BundleEntryList is the in-memory representation of the Bundle offer that
 * is exchanged between peers as Bundle TLV.
 */
template <class BundleEntryType>
class BundleEntryList
{
public:
    typedef PointerList<BundleEntryType> List;
    typedef typename PointerList<BundleEntryType>::iterator
        iterator;
    typedef typename PointerList<BundleEntryType>::const_iterator
        const_iterator;

    /**
     * Default constructor
     */
    BundleEntryList(BundleTLVEntry::bundle_entry_t type =
            BundleTLVEntry::UNDEFINED)
        : type_(type) {}

    /**
     * Copy constructor
     */
    BundleEntryList(const BundleEntryList& list)
        : type_(list.type_), list_(list.list_) {}

    /**
     * Destructor
     */
    virtual ~BundleEntryList() { list_.clear(); }

    /**
     * Return number of Bundle offers in list 
     */
    size_t size() const { return list_.size(); }

    /**
     * Return whether list is empty 
     */
    bool empty() const { return list_.empty(); }

    /**
     * Remove all entries from internal list
     */
    void clear() { list_.clear(); }

    /**
     * Add a Bundle entry to this list; return whether successful
     */
    bool add_entry(const BundleEntryType* entry)
    {
        // weed out the oddball
        if (entry == NULL) return false;

        // pass on the parameters to the "other" add_entry
        return add_entry(entry->type(), entry->creation_ts(),
                         entry->seqno(), entry->sid(),
                         entry->custody(), entry->accept(),
                         entry->ack());
    }

    /**
     * Remove entry from Bundle offer list; returns true if found (and
     * removed) else false if it did not exist
     */
    bool remove_entry(u_int32_t cts, u_int32_t seq, u_int16_t sid)
    {
        // walk the list in search of these unique identifiers
        for (iterator i = list_.begin(); i != list_.end(); i++)
        {
            if ((*i)->creation_ts() == cts &&
                (*i)->seqno() == seq &&
                (*i)->sid() == sid)
            {
                // erase if found
                list_.erase(i);
                return true;
            }
        }
        // not found
        return false;
    }

    /**
     * Return pointer to entry if found, else return NULL
     */
    const BundleEntryType* find(u_int32_t cts,
                                u_int32_t seq,
                                u_int16_t sid) const
    {
        BundleEntryType* bo = NULL;
        for (const_iterator i = list_.begin(); i != list_.end(); i++)
        {
            if ((*i)->creation_ts() == cts &&
                (*i)->seqno() == seq &&
                (*i)->sid() == sid)
            {
                bo = *i;
                break;
            }
        }
        return bo;
    }

    /**
     * Return type of entries hosted by this list
     */
    BundleTLVEntry::bundle_entry_t type() const { return type_; }

    ///@{ These iterators are not thread safe
    iterator begin() { return list_.begin(); }
    iterator end() { return list_.end(); }
    const_iterator begin() const { return (const_iterator) list_.begin(); }
    const_iterator end() const { return (const_iterator) list_.end(); }
    ///@}

    /**
     * Convenience method to access first entry in list, or NULL if empty
     */
    const BundleEntryType* front() const { return list_.front(); }

    /**
     * Convenience method to access last entry in list, or NULL if empty
     */
    const BundleEntryType* back() const { return list_.back(); }

    /**
     * Estimate serialized buffer length
     */
    size_t guess_size(size_t BOEsz) const { return BOEsz * size(); }

    /**
     * Assignment operator
     */
    BundleEntryList& operator= (const BundleEntryList& list) 
    {
        list_ = list.list_;
        type_ = list.type_;
        return *this;
    }

protected:
    /**
     * Convenience method for adding new entry to this Bundle offer list,
     * return whether successful
     */
    bool add_entry(BundleTLVEntry::bundle_entry_t type,
                   u_int32_t cts, u_int32_t seq, u_int16_t sid,
                   bool custody=false,
                   bool accept=false,
                   bool ack=false)
    {
        // there can only be one type in this list, and UNDEFINED
        // isn't one of them
        if (type_ == BundleTLVEntry::UNDEFINED)
            return false;
        else if (type_ != type)
            return false;

        // attempt to create new entry
        BundleTLVEntry* b =
            BundleTLVEntry::create_entry(cts,seq,sid,custody,accept,ack);

        // fail out on error
        if (b == NULL)
            return false;
        else
        if (type_ != b->type())
        {
            delete b;
            return false;
        }

        // push back upon success
        return push_back(dynamic_cast<BundleEntryType*>(b));
    }

    /**
     * Add entry to back of list
     */
    virtual bool push_back(BundleEntryType* bo) = 0;

    BundleTLVEntry::bundle_entry_t type_; ///< type of Bundle entry in list
    List list_;
}; // class BundleEntryList

/**
 * In-memory representation of list of bundle offer entries
 * from a Bundle TLV sent by WAIT_RIB or OFFER
 */
class BundleOfferList : public BundleEntryList<BundleOfferEntry>
{
public:
    /**
     * Constructor
     */
    BundleOfferList()
        : BundleEntryList<BundleOfferEntry>(BundleTLVEntry::OFFER) {}

    /**
     * Destructor
     */
    virtual ~BundleOfferList() {}

    /**
     * Convenience method to add Bundle to the list; return
     * whether successful
     */
    bool add_offer(const Bundle* b, u_int16_t sid)
    {
        // weed out the oddball
        if (b == NULL) return false;

        return add_offer(b->creation_ts(),
                         b->sequence_num(),
                         sid,
                         b->custody_requested(),
                         false);
    }

    /**
     * Convenience wrapper around base class's add_entry
     */
    bool add_offer(u_int32_t cts,
                   u_int32_t seq,
                   u_int16_t sid,
                   bool custody,
                   bool ack)
    {
        return
            add_entry(BundleTLVEntry::OFFER,cts,seq,sid,custody,false,ack);
    }
protected:
    virtual bool push_back(BundleOfferEntry* b)
    {
        if (b == NULL) return false;
        if (find(b->creation_ts(),b->seqno(),b->sid()) == NULL)
        {
            list_.push_back(b);
            return true;
        }
        return false;
    }
}; // class BundleOfferList 

/**
 * In-memory representation of list of bundle response entries
 * from a Bundle TLV sent by SEND_DR or REQUEST
 */
class BundleResponseList : public BundleEntryList<BundleResponseEntry>
{
public:
    /**
     * Constructor
     */
    BundleResponseList()
        : BundleEntryList<BundleResponseEntry>(BundleTLVEntry::RESPONSE) {}

    /**
     * Destructor
     */
    virtual ~BundleResponseList() {}

    /**
     * Convenience wrapper around base class's add_entry
     */
    bool add_response(u_int32_t cts,
                      u_int32_t seq,
                      u_int16_t sid,
                      bool custody, 
                      bool accept = true)
    {
        return
            add_entry(BundleTLVEntry::RESPONSE,cts,seq,sid,custody,accept,false);
    }
protected:
    virtual bool push_back(BundleResponseEntry* b)
    {
        if (b == NULL) return false;
        if (find(b->creation_ts(),b->seqno(),b->sid()) == NULL)
        {
            list_.push_back(b);
            return true;
        }
        return false;
    }
}; // class BundleResponseList 

}; // namespace prophet

#endif // _PROPHET_BUNDLE_ENTRY_LIST_H_
