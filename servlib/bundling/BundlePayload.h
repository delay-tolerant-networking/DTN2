/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include <oasys/serialize/Serialize.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/io/FileIOClient.h>

namespace dtn {

class BundleStore;

/**
 * The representation of a bundle payload.
 *
 * This is abstracted into a separate class to allow the daemon to
 * separately manage the serialization of header information from the
 * payload.
 *
 * Note that this implementation doesn't support payloads larger than
 * 4GB. XXX/demmer fix this.
 *
 */
class BundlePayload : public oasys::SerializableObject, public oasys::Logger {
public:
    BundlePayload(oasys::SpinLock* lock);
    virtual ~BundlePayload();
    
    /**
     * Options for payload location state.
     */
    typedef enum {
        MEMORY = 1,		/// copy of the payload kept in memory
        DISK = 2,		/// payload only kept on disk
        UNDETERMINED = 3,	/// determine MEMORY or DISK based on threshold
        NODATA = 4,		/// no data storage at all (used for simulator)
    } location_t;
    
    /**
     * Actual payload initialization function.
     */
    void init(int bundleid, location_t location = UNDETERMINED);
  
    /**
     * Initialization when re-reading the database
     */
    void init_from_store(int bundleid);
  
    /**
     * Set the payload length in preparation for filling in with data.
     * Optionally also force-sets the location, or leaves it based on
     * configured parameters.
     */
    void set_length(size_t len, location_t location = UNDETERMINED);

    /**
     * Truncate the payload. Used for reactive fragmentation.
     */
    void truncate(size_t len);
    
    /**
     * The payload length.
     */
    size_t length() const { return length_; }

    /**
     * The payload location.
     */
    location_t location() const { return location_; }
    
    /**
     * Set the payload data and length, closing the payload file after
     * it's been written to.
     */
    void set_data(const u_char* bp, size_t len);

    /**
     * Set the payload data, closing the payload file after it's been
     * written to.
     */
    void set_data(const std::string& data)
    {
        set_data((u_char*)data.data(), data.length());
    }

    /**
     * Append a chunk of payload data. Assumes that the length was
     * previously set. Keeps the payload file open.
     */
    void append_data(const u_char* bp, size_t len);

    /**
     * Write a chunk of payload data at the specified offset. Keeps
     * the payload file open.
     */
    void write_data(const u_char* bp, size_t offset, size_t len);

    /**
     * Writes len bytes of payload data from from another payload at
     * the given src_offset to the given dst_offset. Keeps the payload
     * file open.
     */
    void write_data(BundlePayload* src, size_t src_offset,
                    size_t len, size_t dst_offset);

    /**
     * Reopen the payload file.
     */
    void reopen_file();
    
    /**
     * Close the payload file.
     */
    void close_file();

    /**
     * Return the file state.
     */
    bool is_file_open() { return file_->is_open(); }

    /**
     * Copy (or link) the payload to the given file client object
     */
    void copy_file(oasys::FileIOClient* dst);
    
    /**
     * Get a pointer to the in-memory data buffer.
     */
    const u_char* memory_data()
    {
        ASSERT(location_ == MEMORY);
        return (const u_char*)data_.c_str();
    }

    /**
     * Valid flags to read_data.
     */
    typedef enum {
        KEEP_FILE_OPEN = 0x1,	///< Don't close file after read
        FORCE_COPY     = 0x2,	///< Always copy payload, even for in-memory
                                ///  bundles
    } read_data_flags_t;
    
    /**
     * Return a pointer to a chunk of payload data. For in-memory
     * bundles, this will just be a pointer to the data buffer (unless
     * the FORCE_COPY flag is set).
     *
     * Otherwise, it will call read() into the supplied buffer (which
     * must be >= len).
     */
    const u_char* read_data(size_t offset, size_t len, u_char* buf,
                            int flags = 0);

    /**
     * Since read_data doesn't really change anything of substance in
     * the payload class (just the internal bookkeeping fields), we
     * define a "const" variant that just casts itself away and calls
     * the normal variant.
     */
    const u_char* read_data(size_t offset, size_t len, u_char* buf,
                            int flags = 0) const
    {
        return const_cast<BundlePayload*>(this)->
            read_data(offset, len, buf, flags);
    }
     
    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);

    /*
     * Tunable parameters
     */
    static std::string payloaddir_; ///< directory to store payload files
    static size_t mem_threshold_;   ///< maximum bundle size to keep in memory
    static bool test_no_remove_;    ///< test: don't rm payload files

protected:
    void internal_write(const u_char* bp, size_t offset, size_t len);

    location_t location_;	///< location of the data (disk or memory)
    std::string data_;		///< the actual payload data if in memory
    size_t length_;     	///< the payload length
    size_t rcvd_length_;     	///< the payload length we actually have
    std::string fname_;		///< payload file name
    oasys::FileIOClient* file_;	///< file handle if on disk
    size_t cur_offset_;		///< cache of current fd position
    size_t base_offset_;	///< for fragments, offset into the file (todo)
    oasys::SpinLock* lock_;	///< the lock for the given bundle
};

} // namespace dtn

#endif /* _BUNDLE_PAYLOAD_H_ */
