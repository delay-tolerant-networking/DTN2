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

#ifndef _PROPHET_RESPONSE_TLV_H_
#define _PROPHET_RESPONSE_TLV_H_

#include "BaseTLV.h"
#include "BundleTLV.h"
#include "BundleTLVEntryList.h"

namespace prophet
{

class ResponseTLV : public BundleTLV
{
public:
    /**
     * Constructor.
     */
    ResponseTLV(const BundleResponseList& list);

    /**
     * Destructor.
     */
    virtual ~ResponseTLV() {}

    /**
     * Virtual from BaseTLV
     */
    size_t serialize(u_char* bp,size_t len) const;

    ///@{ Accessors
    const BundleResponseList& list() const { return list_; }
    ///@}

protected:
    friend class TLVFactory<ResponseTLV>;

    /**
     * Constructor.  Protected to force access through TLVFactory.
     */
    ResponseTLV();

    /**
     * Virtual from BaseTLV.
     */
    bool deserialize(const u_char* bp, size_t len);

    BundleResponseList list_; ///< Priority sorted list of Bundles to request
                              ///  from peer
}; // class ResponseTLV

}; // namespace prophet

#endif // _PROPHET_RESPONSE_TLV_H_
