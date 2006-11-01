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
      block_type_(owner->block_type()),
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
void
BlockInfo::serialize(oasys::SerializeAction* a)
{
    a->process("block_type", &block_type_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        // need to re-assign the owner
        owner_ = BundleProtocol::find_processor(block_type_);
    }
    ASSERT(block_type_ == owner_->block_type());
    
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
