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

#ifndef _PROPHET_OFFER_TLV_H_
#define _PROPHET_OFFER_TLV_H_

#include "BaseTLV.h"
#include "BundleTLV.h"
#include "BundleTLVEntryList.h"

namespace prophet
{

class OfferTLV : public BundleTLV
{
public:
    /**
     * Constructor.
     */
    OfferTLV(const BundleOfferList& list);

    /**
     * Destructor.
     */
    virtual ~OfferTLV() {}

    /**
     * Virtual from BaseTLV
     */
    size_t serialize(u_char* bp,size_t len) const;

    ///@{ Accessors
    const BundleOfferList list() const { return list_; }
    ///@}

protected:
    friend class TLVFactory<OfferTLV>;

    /**
     * Constructor.  Protected to force access through TLVFactory.
     */
    OfferTLV();

    /**
     * Virtual from BaseTLV. 
     */
    bool deserialize(const u_char* bp, size_t len);

    BundleOfferList list_; ///< Priority sorted list of Bundles to offer
                           ///  to peer
}; // class OfferTLV

}; // namespace prophet

#endif // _PROPHET_OFFER_TLV_H_
