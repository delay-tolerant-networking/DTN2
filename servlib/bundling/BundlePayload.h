/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include <oasys/serialize/Serialize.h>
#include <oasys/debug/Debug.h>
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
 */
class BundlePayload : public oasys::SerializableObject {
public:
    BundlePayload();
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
    void init(oasys::SpinLock* lock, int bundleid,
              location_t location = UNDETERMINED);
  
    /**
     * Initialization when re-reading the database
     */
    void init(oasys::SpinLock* lock, int bundleid, BundleStore* store);
  
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
     * Copy (or link) the payload to the given path.
     */
    void copy_file(const std::string& copy_path);
    
    /**
     * Get a pointer to the in-memory data buffer.
     */
    const u_char* memory_data()
    {
        ASSERT(location_ == MEMORY);
        return (const u_char*)data_.c_str();
    }
    
    /**
     * Return a pointer to a chunk of payload data. For in-memory
     * bundles, this will just be a pointer to the data buffer.
     * Otherwise, it will call read() into the supplied buffer (which
     * must be >= len).
     */
    const u_char* read_data(size_t offset, size_t len, u_char* buf,
                            bool keep_file_open = false);

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);

    /*
     * Tunable parameters
     */
    static size_t mem_threshold_; ///< maximum bundle size to keep in memory
    static bool test_no_remove_;  ///< test: don't rm payload files

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
    oasys::SpinLock* lock_;		///< the lock for the given bundle
};

} // namespace dtn

#endif /* _BUNDLE_PAYLOAD_H_ */
