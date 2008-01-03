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

#ifndef _PROPHET_BUNDLE_H_
#define _PROPHET_BUNDLE_H_

#include "bundling/Bundle.h"
#include "prophet/Bundle.h"

namespace dtn
{

/**
 * Unification class that ties together Prophet's metadata view with 
 * DTN's complete Bundle object.
 */
class ProphetBundle : public prophet::Bundle
{
public:
    /**
     * Constructor
     */
    ProphetBundle(const BundleRef& bundle)
        : ref_("ProphetBundle"), str_("")
    {
        ref_ = bundle.object();
    }

    /**
     * Constructor
     */
    ProphetBundle(const ProphetBundle& other)
        : prophet::Bundle(other), ref_(other.ref_) {}

    /**
     * Destructor
     */
    virtual ~ProphetBundle()
    {
        ref_ = NULL;
    }

    /**
     * Assignment operator
     */
    ProphetBundle& operator= (const ProphetBundle& other)
    {
        ref_ = other.ref_;
        return *this;
    }

    /**
     * Return const ref to BundleRef member
     */
    const BundleRef& ref() const { return ref_; }

    ///@{ Virtual from prophet::Bundle
    virtual const std::string& destination_id() const
    {
        return (ref_ == NULL) ? str_ : ref()->dest().str();
    }
    virtual const std::string& source_id() const
    {
        return (ref_ == NULL) ? str_ : ref()->source().str();
    }
    virtual u_int32_t creation_ts() const
    {
        return (ref_ == NULL) ? 0 : ref()->creation_ts().seconds_;
    }
    virtual u_int32_t sequence_num() const
    {
        return (ref_ == NULL) ? 0 : ref()->creation_ts().seqno_;
    }
    virtual u_int32_t expiration_ts() const
    {
        return (ref_ == NULL) ? 0 : ref()->expiration();
    }
    virtual u_int size() const
    {
        return (ref_ == NULL) ? 0 : ref()->payload().length();
    }
    virtual u_int num_forward() const
    {
        return (ref_ == NULL) ? 0 :
            ref()->fwdlog()->get_count(ForwardingInfo::TRANSMITTED,
                ForwardingInfo::COPY_ACTION);
    }
    virtual bool custody_requested() const
    {
        return (ref_ == NULL) ? false : ref()->custody_requested();
    }
    ///@}

protected:

    BundleRef ref_; ///< DTN bundle object
    std::string str_; ///< return value for NULL condition

}; // class ProphetBundle

}; // namespace dtn

#endif // _PROPHET_BUNDLE_H_
