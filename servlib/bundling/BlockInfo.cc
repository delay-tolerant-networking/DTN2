/*
 *    Copyright 2006 Intel Corporation
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

#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "BundleProtocol.h"

namespace dtn {

//----------------------------------------------------------------------
BlockInfo::BlockInfo(BlockProcessor* owner)
    : owner_(owner),
      owner_type_(owner->block_type()),
      contents_(),
      data_length_(0),
      data_offset_(0),
      complete_(false)
{
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(oasys::Builder& builder)
{
    (void)builder;
}

//----------------------------------------------------------------------
u_int8_t
BlockInfo::type() const
{
    if (owner_->block_type() == BundleProtocol::PRIMARY_BLOCK) {
        return BundleProtocol::PRIMARY_BLOCK;
    }

    if (contents_.len() == 0) {
        return 0xff;
    }

    return contents_.buf()[0];
}

//----------------------------------------------------------------------
u_int8_t
BlockInfo::flags() const
{
    if (owner_->block_type() == BundleProtocol::PRIMARY_BLOCK) {
        return 0x0;
    }

    ASSERT(contents_.len() >= 2);
    return contents_.buf()[1];
}

//----------------------------------------------------------------------
void
BlockInfo::serialize(oasys::SerializeAction* a)
{
    a->process("owner_type", &owner_type_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        // need to re-assign the owner
        owner_ = BundleProtocol::find_processor(owner_type_);
    }
    ASSERT(owner_type_ == owner_->block_type());
    
    u_int32_t length = contents_.len();
    a->process("length", &length);
    
    // when we're unserializing, we need to reserve space and set the
    // length of the contents buffer before we write into it
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        contents_.reserve(length);
        contents_.set_len(length);
    }

    a->process("contents", contents_.buf(), length);
    a->process("data_length", &data_length_);
    a->process("data_offset", &data_offset_);
    a->process("complete", &complete_);

}

//----------------------------------------------------------------------
BlockInfoVec*
LinkBlockVec::find_info(Link* link)
{
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (iter->link_ == link) {
            return &(iter->info_vec_);
        }
    }
    return NULL;
}


} // namespace dtn
