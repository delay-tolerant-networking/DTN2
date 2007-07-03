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

#include "contacts/Link.h"

namespace dtn {

class BlockProcessor;

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
    
    /// Default constructor assigns the owner and optionally the
    /// BlockInfo source (i.e. the block as it arrived off the wire)
    BlockInfo(BlockProcessor* owner, const BlockInfo* source = NULL);

    /// Constructor for unserializing
    BlockInfo(oasys::Builder& builder);

    /// @{ Accessors
    BlockProcessor*   owner()          const { return owner_; }
    const BlockInfo*  source()         const { return source_; }
    const DataBuffer& contents()       const { return contents_; }
    u_int32_t         data_length()    const { return data_length_; }
    u_int32_t         data_offset()    const { return data_offset_; }
    u_int32_t         full_length()    const { return (data_offset_ +
                                                       data_length_); }
    u_char*           data()           const { return (contents_.buf() +
                                                       data_offset_); }
    bool              complete()       const { return complete_; }
    bool              last_block()     const;
    ///@}

    /// @{ Mutating accessors
    void        set_complete(bool t)         { complete_ = t; }
    void        set_data_length(u_int32_t l) { data_length_ = l; }
    void        set_data_offset(u_int32_t o) { data_offset_ = o; }
    DataBuffer* writable_contents()          { return &contents_; }
    /// @}

    /// @{ These accessors need special case processing since the
    /// primary block doesn't have the fields in the same place.
    u_int8_t  type()  const;
    u_int64_t flags() const;
    void      set_flag(u_int64_t flag);
    /// @}

    /// Virtual from SerializableObject
    virtual void serialize(oasys::SerializeAction* action);

protected:
    BlockProcessor*  owner_;       ///< Owner of this block
    u_int16_t        owner_type_;  ///< Extracted from owner
    const BlockInfo* source_;      ///< Owner of this block
    DataBuffer       contents_;    ///< Block contents with length set to
                                   ///  the amount currently in the buffer
    u_int32_t        data_length_; ///< Length of the block data (w/o preamble)
    u_int32_t        data_offset_; ///< Offset of first byte of the block data
    bool             complete_;    ///< Whether or not this block is complete
};

/**
 * Class for a vector of BlockInfo structures.
 *
 * Instead of deriving from oasys::SerializableVector, this class
 * contains one.
 */
class BlockInfoVec : public oasys::SerializableObject {
public:
    /// @{ Typedefs for the underlying types
    typedef oasys::SerializableVector<BlockInfo> Vector;
    typedef Vector::iterator                     iterator;
    typedef Vector::const_iterator               const_iterator;
    /// @}
    
    BlockInfoVec(oasys::Lock* lock) : lock_(lock) {}

    /**
     * Append a block using the given processor and optional source
     * block.
     */
    BlockInfo* append_block(BlockProcessor* owner,
                            const BlockInfo* source = NULL);
    
    /**
     * Find the block for the given type.
     *
     * @return the block or NULL if not found
     */
    const BlockInfo* find_block(u_int8_t type) const;
    
    /**
     * Check if an entry exists in the vector for the given block type.
     */
    bool has_block(u_int8_t type) const { return find_block(type) != NULL; }

    /// @{ Macros for readability    
    #define SCOPELOCK  oasys::ScopeLock l(lock_, __FUNCTION__);
    #define ASSERTLOCK ASSERTF(lock_->is_locked_by_me(),                      \
                               "bundle must be locked to call BlockInfo::%s", \
                               __FUNCTION__);
    /// @}

    /// @{ Wrappers around the vector functions that enforce the
    /// serialized access.
    size_t         size()  const { SCOPELOCK;  return vector_.size();  }
    BlockInfo&     front()       { SCOPELOCK;  return vector_.front(); }
    BlockInfo&     back()        { SCOPELOCK;  return vector_.back();  }
    bool           empty() const { SCOPELOCK;  return vector_.empty(); }
    iterator       begin()       { ASSERTLOCK; return vector_.begin(); }
    iterator       end()         { ASSERTLOCK; return vector_.end();   }
    const_iterator begin() const { ASSERTLOCK; return vector_.begin(); }
    const_iterator end()   const { ASSERTLOCK; return vector_.end();   }
    /// @}

    #undef SCOPELOCK
    #undef ASSERTLOCK

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* action);

protected:    
    /// The actual vector
    Vector vector_;
    
    /// Pointer to the lock protecting the vector
    oasys::Lock* lock_;
};

/**
 * A set of BlockInfoVecs, one for each outgoing link.
 */
class LinkBlockSet {
public:
    LinkBlockSet(oasys::Lock* lock) : lock_(lock) {}
    
    /**
     * Destructor that clears the set.
     */
    virtual ~LinkBlockSet();
    
    /**
     * Create a new BlockInfoVec for the given link.
     *
     * @return Pointer to the new BlockInfoVec
     */
    BlockInfoVec* create_blocks(const LinkRef& link);
    
    /**
     * Find the BlockInfoVec for the given link.
     *
     * @return Pointer to the BlockInfoVec or NULL if not found
     */
    BlockInfoVec* find_blocks(const LinkRef& link);
    
    /**
     * Remove the BlockInfoVec for the given link.
     */
    void delete_blocks(const LinkRef& link);

protected:
    /**
     * Struct to hold a block list and a link pointer. Note that we
     * allow the STL vector to copy the pointers to both the block
     * list and the link pointer. This is safe because the lifetime of
     * the BlockInfoVec object is defined by create_blocks() and
     * delete_blocks().
     */
    struct Entry {
        Entry(const LinkRef& link);
        
        BlockInfoVec* blocks_;
        LinkRef       link_;
    };

    typedef std::vector<Entry> Vector;
    typedef std::vector<Entry>::iterator iterator;
    Vector entries_;
    oasys::Lock* lock_;
};

} // namespace dtn

#endif /* _BUNDLEBLOCKINFO_H_ */
