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

#ifdef HAVE_CONFIG_H
#include <dtn-config.h>
#endif

#include "ProphetNode.h"

namespace dtn {

ProphetNode::ProphetNode(const prophet::NodeParams* params)
    : prophet::Node(params)
{
}

ProphetNode::ProphetNode(const ProphetNode& node)
    : prophet::Node(node),
      oasys::SerializableObject(node),
      remote_eid_(node.remote_eid_)
{
}

ProphetNode::ProphetNode(const prophet::Node& node)
    : prophet::Node(node),
      oasys::SerializableObject(),
      remote_eid_(node.dest_id())
{
}

ProphetNode::ProphetNode(const oasys::Builder&)
    : prophet::Node(),
      oasys::SerializableObject()
{
}

void
ProphetNode::serialize(oasys::SerializeAction* a)
{
    a->process("age",&age_);
    a->process("custody",&custody_);
    a->process("relay",&relay_);
    a->process("internet_gw",&internet_gateway_);

    // serialize into 8 bits, much like RIBTLV
    u_int8_t p_value_pass = 0;

    // if reading from disk into memory
    if (a->action_code() == oasys::Serialize::UNMARSHAL)
    {
        a->process("remote_eid",&remote_eid_);
        set_dest_id(remote_eid_.c_str());

        a->process("p_value",&p_value_pass);
        p_value_ = ((p_value_pass & 0xff) + 0.0) / (255.0);
    }
    // else writing to disk from memory
    else
    {
        remote_eid_.assign(dest_id_);
        a->process("remote_eid",&remote_eid_);

        p_value_pass = (u_int8_t) ( (int) (p_value_ * (255.0)) ) & 0xff;
        a->process("p_value",&p_value_pass);
    }
}

}; // dtn
