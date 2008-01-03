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

#ifndef _PROPHET_BUNDLE_LIST_H_
#define _PROPHET_BUNDLE_LIST_H_

#include "prophet/BundleList.h"
#include "prophet/Repository.h"
#include "prophet/BundleCore.h"
#include "ProphetBundle.h"

namespace dtn
{

/**
 * Maintain mapping between dtn::Bundle and prophet::Bundle
 */

class ProphetBundleList
{
public:
    /**
     * Constructor
     */
    ProphetBundleList(prophet::Repository::BundleCoreRep* core);

    /**
     * Destructor
     */
    ~ProphetBundleList();
    
    /**
     * Add mapping for dtn::BundleRef to list
     */
    void add(const BundleRef& b);
    void add(const prophet::Bundle* b);

    /**
     * Remove mapping for dtn::BundleRef from list
     */
    void del(const BundleRef& b);
    void del(const prophet::Bundle* b);

    /**
     * Given destination ID and creation ts, return prophet::Bundle*
     */
    const prophet::Bundle* find(const std::string& dst, 
            u_int creation_ts,
            u_int seqno) const;

    const prophet::Bundle* find(const Bundle* b) const
    {
        return find(b->dest().str(),
                    b->creation_ts().seconds_,
                    b->creation_ts().seqno_);
    }

    /**
     * Given a prophet object, return dtn::BundleRef&
     */
    const BundleRef& find_ref(const prophet::Bundle* b) const;

    /**
     * Return a const reference to BundleList, in no guaranteed order
     */
    const prophet::BundleList& get_bundles() const
    {
        return list_.get_bundles();
    }

    /**
     * Expose pointer to Bundle repository
     */
    prophet::Repository* bundles() { return &list_; }

    /**
     * Drop all prophet::Bundle*'s from list
     */
    void clear();

    ///@{ Accessors
    bool empty() const { return list_.empty(); }
    size_t size() const { return list_.size(); }
    ///@}

protected:
    typedef prophet::BundleList::const_iterator const_iterator;
    /**
     * Utility function for internal find
     */
    bool find(const std::string& dst, u_int creation_ts,
            u_int seqno, const_iterator& i) const;

    prophet::Repository list_; ///< collection of ProphetBundle's
    static BundleRef NULL_BUNDLE; ///< pointer to NULL

}; // class ProphetBundleList

}; // namespace dtn

#endif // _PROPHET_BUNDLE_LIST_H_
