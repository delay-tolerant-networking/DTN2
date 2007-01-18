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

#include <oasys/debug/Log.h>
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "APIBlockProcessor.h"
#include "BundleProtocol.h"

namespace dtn {

//----------------------------------------------------------------------
BlockInfo::BlockInfo(BlockProcessor* owner, const BlockInfo* source)
    : SerializableObject(),
      owner_(owner),
      owner_type_(owner->block_type()),
      source_(source),
      contents_(),
      data_length_(0),
      data_offset_(0),
      complete_(false)
{
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(oasys::Builder& builder)
    : owner_(NULL),
      owner_type_(0),
      source_(NULL),
      contents_(),
      data_length_(0),
      data_offset_(0),
      complete_(false)
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
    if (type() == BundleProtocol::PRIMARY_BLOCK) {
        return 0x0;
    }

    ASSERT(contents_.len() >= 2);
    return contents_.buf()[1];
}

//----------------------------------------------------------------------
void
BlockInfo::set_flag(u_int8_t flag)
{
    ASSERT(contents_.len() >= 2);
    contents_.buf()[1] &= flag;
}

//----------------------------------------------------------------------
bool
BlockInfo::primary_block() const
{
    return (type() == BundleProtocol::PRIMARY_BLOCK);
}

//----------------------------------------------------------------------
bool
BlockInfo::payload_block() const
{
    return (type() == BundleProtocol::PAYLOAD_BLOCK);
}

//----------------------------------------------------------------------
bool
BlockInfo::last_block() const
{
    return (flags() & BundleProtocol::BLOCK_FLAG_LAST_BLOCK);
}

//----------------------------------------------------------------------
void
BlockInfo::serialize(oasys::SerializeAction* a)
{
    a->process("owner_type", &owner_type_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        // need to re-assign the owner
        if (owner_type_ == BundleProtocol::API_EXTENSION_BLOCK) {
            owner_ = APIBlockProcessor::instance();
        } else {
            owner_ = BundleProtocol::find_processor(owner_type_);
        }
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
const BlockInfo*
BlockInfoVec::find_block(u_int8_t type) const
{
    for (const_iterator iter = begin(); iter != end(); ++iter) {
        if (iter->type() == type ||
            iter->owner()->block_type() == type)
        {
            return &*iter;
        }
    }
    return false;
}


//----------------------------------------------------------------------
LinkBlockSet::Entry::Entry(const LinkRef& link)
    : blocks_(NULL), link_(link.object(), "LinkBlockSet::Entry")
{
}

//----------------------------------------------------------------------
LinkBlockSet::~LinkBlockSet()
{
    for (iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        delete iter->blocks_;
        iter->blocks_ = 0;
    }
}

//----------------------------------------------------------------------
BlockInfoVec*
LinkBlockSet::create_blocks(const LinkRef& link)
{
    ASSERT(find_blocks(link) == NULL);
    entries_.push_back(Entry(link));
    entries_.back().blocks_ = new BlockInfoVec();
    return entries_.back().blocks_;
}

//----------------------------------------------------------------------
BlockInfoVec*
LinkBlockSet::find_blocks(const LinkRef& link)
{
    for (iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        if (iter->link_ == link) {
            return iter->blocks_;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
void
LinkBlockSet::delete_blocks(const LinkRef& link)
{
    for (iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        if (iter->link_ == link) {
            delete iter->blocks_;
            entries_.erase(iter);
            return;
        }
    }
    
    PANIC("LinkBlockVec::delete_blocks: "
          "no block vector for link *%p", link.object());
}

} // namespace dtn
