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

#ifndef _PROPHET_BUNDLE_LIST_FACADE_H_
#define _PROPHET_BUNDLE_LIST_FACADE_H_

#include "Bundle.h"

#include <string>
#include <vector>

namespace prophet
{

/**
 * List of Prophet's Bundle facade objects
 */
class BundleList : public std::vector<const Bundle*>
{
public:
    /**
     * Destructor
     */
    virtual ~BundleList() {}

    /**
     * Look up and return Bundle* else NULL
     */
    const Bundle* find(const std::string& dest_id,
                       u_int32_t creation_ts,
                       u_int32_t seqno) const
    {
        for (const_iterator i = begin(); i != end(); i++)
        {
            if ((*i)->creation_ts() == creation_ts &&
                (*i)->sequence_num() == seqno &&
                (*i)->destination_id() == dest_id)
                    return (*i);
        }
        return NULL;
    }

}; // class BundleList

}; // namespace prophet

#endif // _PROPHET_BUNDLE_LIST_FACADE_H_
