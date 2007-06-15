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

#ifndef _PROPHET_LINK_FACADE_H_
#define _PROPHET_LINK_FACADE_H_

#include <string.h>
#include <string>

#include "AckList.h"

namespace prophet
{

class Link
{
public:
    /**
     * Destructor
     */
    virtual ~Link() {}

    ///@{ Accessors
    virtual const char* nexthop() const = 0;
    virtual const char* remote_eid() const = 0;
    AckList* acks() { return &acks_; }
    ///@}

    ///@{ Operators
    virtual bool operator==(const Link* l) const
    { return strncmp(nexthop(),l->nexthop(),strlen(nexthop())) == 0; }
    virtual bool operator<(const Link* l) const
    { return strncmp(nexthop(),l->nexthop(),strlen(nexthop())) < 0; }
    virtual bool operator==(const Link& l) const
    { return strncmp(nexthop(),l.nexthop(),strlen(nexthop())) == 0; }
    virtual bool operator<(const Link& l) const
    { return strncmp(nexthop(),l.nexthop(),strlen(nexthop())) < 0; }
    ///@}

protected:
    mutable AckList acks_; ///< track what's been sent, so we don't send more than once

}; // class Link

class LinkImpl : public Link
{
public:
    /**
     * Constructor
     */
    LinkImpl(const std::string& nexthop = "") :
        next_hop_(nexthop) {}

    /**
     * Destructor
     */
    virtual ~LinkImpl() {}

    ///@{ Accessors
    const char* nexthop() const { return next_hop_.c_str(); }
    const char* remote_eid() const { return next_hop_.c_str(); }
    ///@}

    ///@{ Mutators
    void set_nexthop( const std::string& nexthop )
    {
        next_hop_.assign(nexthop);
    }
    ///@}

protected:
    std::string next_hop_;
}; // class LinkImpl

}; // namespace prophet

#endif // _PROPHET_LINK_FACADE_H_
