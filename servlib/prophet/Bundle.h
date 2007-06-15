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

#ifndef _PROPHET_BUNDLE_FACADE_H_
#define _PROPHET_BUNDLE_FACADE_H_

#include <sys/types.h>
#include <string>

namespace prophet
{

/**
 * Facade interface between Prophet router and host implmentation's
 * Bundle representation. Rather than duplicate the extensive Bundle API,
 * this facade is only interested in a subset of the metadata.
 */
class Bundle
{
public:
    /**
     * Destructor
     */
    virtual ~Bundle() {};

    ///@{ Accessors
    virtual const std::string& destination_id()    const = 0;
    virtual const std::string& source_id()         const = 0;
    virtual       u_int32_t    creation_ts()       const = 0;
    virtual       u_int32_t    sequence_num()      const = 0;
    virtual       u_int32_t    expiration_ts()     const = 0;
    virtual       u_int        size()              const = 0;
    virtual       u_int        num_forward()       const = 0;
    virtual       bool         custody_requested() const = 0;
    ///@}

    ///@{ Operators
    virtual bool operator< (const Bundle& a) const
    {
        if (sequence_num() == a.sequence_num())
            return destination_id() < a.destination_id();
        return sequence_num() < a.sequence_num();
    }
    virtual bool operator> (const Bundle& a) const
    {
        if (sequence_num() == a.sequence_num())
            return destination_id() > a.destination_id();
        return sequence_num() > a.sequence_num();
    }
    virtual bool operator== (const Bundle& a) const
    {
        if (sequence_num() == a.sequence_num())
            return destination_id() == a.destination_id();
        return false;
    }
    ///@}

}; // class Bundle

}; // namespace prophet

#endif // _PROPHET_BUNDLE_FACADE_H_
