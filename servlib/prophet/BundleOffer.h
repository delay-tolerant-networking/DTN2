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

#ifndef _PROPHET_BUNDLE_OFFER_H_
#define _PROPHET_BUNDLE_OFFER_H_

#include "Bundle.h"
#include "BundleList.h"
#include "AckList.h"
#include "BundleTLVEntryList.h"
#include "FwdStrategy.h"
#include "Decider.h"

#include <list>

namespace prophet
{

/**
 * BundleOffer is the executor of forwarding strategy. A Decider chooses
 * which Bundles to allow into the list.  A Comparator assists in 
 * sorting in strategy order.
 */
class BundleOffer
{
public:
    /**
     * Constructor. This object's destructor cleans up memory pointed to
     * by comp and decider
     */
    BundleOffer(BundleCore* core,
                const BundleList& bundles,
                FwdStrategyComp* comp,
                Decider* decider);

    /**
     * Destructor
     */
    ~BundleOffer();

    /**
     * Add Bundle to offer. Strategy-based decider will determine
     * whether it gets added, and strategy-based comparator will
     * determine where it gets inserted.
     */
    void add_bundle(const Bundle* b);
 
    /**
     * Return whether Bundle offer is empty
     */
    bool empty() const { return list_.empty(); }

    /**
     * Return number of elements in Bundle offer
     */
    size_t size() const { return list_.size(); }

    /**
     * Convenience method to return const ref to sorted
     * BundleOfferList.
     */
    const BundleOfferList& get_bundle_offer(const Dictionary& ribd,
                                            const AckList* acks);

protected:
    typedef std::list<const Bundle*> List;

    BundleCore* const core_; ///< BundleCore instance
    List list_; ///< bundles to offer to remote peer
    BundleOfferList bolist_; ///< TLV-ready bundle offer list
    FwdStrategyComp* comp_; ///< comparator for ordering Bundle Offer TLV
    Decider* decider_; ///< forwarding decision per Bundle
}; // class BundleOffer

}; // namespace prophet

#endif // _PROPHET_BUNDLE_OFFER_H_
