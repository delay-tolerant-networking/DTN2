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

#ifndef _DTN_PROPHET_NODE_
#define _DTN_PROPHET_NODE_

#include "prophet/Node.h"
#include "naming/EndpointID.h"
#include <oasys/serialize/Serialize.h>
#include <oasys/debug/Formatter.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

/**
 * ProphetNode stores state for a remote node as identified by remote_eid
 */
class ProphetNode : public prophet::Node,
                    public oasys::SerializableObject
{
public:

    /**
     * Default constructor
     */
    ProphetNode(const prophet::NodeParams* params = NULL);

    ///@{ Copy constructor
    ProphetNode(const ProphetNode& node);
    ProphetNode(const prophet::Node& node);
    ///@}

    /**
     * Deserialization and testing constructor
     */
    ProphetNode(const oasys::Builder&);

    /**
     * Destructor
     */
    virtual ~ProphetNode() {}

    /**
     * Accessors
     */
    const EndpointID& remote_eid()
    {
        remote_eid_.assign(dest_id_); return remote_eid_;
    }

    /**
     * Assignment operator
     */
    ProphetNode& operator= (const ProphetNode& p)
    {
        ((prophet::Node)*this) = (prophet::Node) p;
        remote_eid_ = p.remote_eid_;
        return *this;
    }

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    const EndpointID& durable_key() { return remote_eid(); }

    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* a);

protected:
    friend class ProphetNodeList; ///< for access to prophet::Node mutators

    /**
     * Mutator
     */
    void set_eid( const EndpointID& eid )
    {
        set_dest_id(eid.str());
        remote_eid_.assign(eid);
    }

    EndpointID remote_eid_; ///< EID of remote peer represented by this route
}; // ProphetNode

}; // dtn

#endif // _DTN_PROPHET_NODE_
