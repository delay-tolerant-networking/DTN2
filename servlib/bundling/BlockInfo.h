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

#ifndef _BUNDLEBLOCKINFO_H_
#define _BUNDLEBLOCKINFO_H_

#include <oasys/serialize/Serialize.h>
#include <oasys/serialize/SerializableVector.h>
#include <oasys/util/ScratchBuffer.h>

namespace dtn {

class BlockProcessor;
class Link;

/**
 * Class used to store unparsed bundle blocks and associated metadata
 * about them.
 */
class BlockInfo : public oasys::SerializableObject {
public:
    /// To store the formatted block data, we use a scratch buffer
    /// with 64 bytes of static buffer space which should be
    /// sufficient to cover most blocks and avoid mallocs.
    typedef oasys::ScratchBuffer<u_char*, 64> DataBuffer;
    
    /// Default constructor assigns the owner
    BlockInfo(BlockProcessor* owner);

    /// Constructor for unserializing
    BlockInfo(oasys::Builder& builder);

    /// @{ Accessors
    BlockProcessor*   owner()       const { return owner_; }
    u_int8_t          type()        const { return contents_.buf()[0]; }
    u_int8_t          flags()       const { return contents_.buf()[1]; }
    const DataBuffer& contents()    const { return contents_; }
    u_int32_t	      data_length() const { return data_length_; }
    u_int32_t	      data_offset() const { return data_offset_; }
    u_int32_t	      full_length() const { return data_offset_ + data_length_; }
    u_char*           data()        const { return contents_.buf() + data_offset_; }
    bool              complete()    const { return complete_; }
    ///@}

    /// @{ Mutating accessors
    void        set_complete(bool t)         { complete_ = t; }
    void        set_data_length(u_int32_t l) { data_length_ = l; }
    void        set_data_offset(u_int32_t o) { data_offset_ = o; }
    DataBuffer* writable_contents()          { return &contents_; }
    /// @}
    
    /// Virtual from SerializableObject
    virtual void serialize(oasys::SerializeAction* action);

protected:
    BlockProcessor* owner_;       ///< Owner of this block
    DataBuffer      contents_;    ///< Copy of the off-the-wire block,
                                  ///  with length set to the amount that's
                                  ///  currently in the buffer
    u_int32_t       data_length_; ///< Length of the block data (w/o preamble)
    u_int32_t       data_offset_; ///< Offset of first byte of the block data
    bool            complete_;    ///< Whether or not this block is complete
};

/**
 * Class for a vector of BlockInfo structures.
 */
class BlockInfoVec : public oasys::SerializableVector<BlockInfo> {
};

/**
 * Simple class to wrap a BlockInfoVec and an outgoing link.
 */
class LinkBlocks {
public:
    LinkBlocks() {}

    LinkBlocks(const BlockInfoVec& info_vec, Link* link)
        : info_vec_(info_vec), link_(link) {}

    BlockInfoVec info_vec_;
    Link*        link_;
};

/**
 * A vector of LinkBlockVec structs, one for each outgoing link for
 * each bundle.
 */
class LinkBlockVec : public std::vector<LinkBlocks> {
public:
    /**
     * Find the BlockInfoVec for the given link.
     *
     * @return Pointer to the BlockInfoVec or NULL if not found
     */
    BlockInfoVec* find_info(Link* link);
};

} // namespace dtn

#endif /* _BUNDLEBLOCKINFO_H_ */
