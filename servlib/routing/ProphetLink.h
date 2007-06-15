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

#ifndef _PROPHET_LINK_H_
#define _PROPHET_LINK_H_

#include "prophet/Link.h"
#include "contacts/Link.h"

namespace dtn
{

class ProphetLink : public prophet::Link 
{

public:
    ProphetLink(const LinkRef& link)
        : ref_("ProphetLink")
    {
        ref_ = link;
    }

    ~ProphetLink()
    {
        ref_ = NULL;
    }

    const LinkRef& ref() const { return ref_; }

    ///@{ Virtual from prophet::Link
    const char* nexthop() const
    {
        return (ref().object() == NULL) ? "" : ref()->nexthop();
    }
    const char* remote_eid() const
    {
        return (ref().object() == NULL) ? "" : ref()->remote_eid().c_str();
    }
    ///@}

protected:

    LinkRef ref_; ///< DTN Link object
}; // class ProphetLink

}; // namespace dtn

#endif // _PROPHET_LINK_H_
