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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "BundlePayload.h"

namespace dtn {

/*
 * Configurable settings.
 */
std::string BundlePayload::payloaddir_;
size_t BundlePayload::mem_threshold_;
bool BundlePayload::test_no_remove_;

/**
 * Constructor
 */
BundlePayload::BundlePayload()
    : location_(DISK), length_(0), rcvd_length_(0), file_(NULL),
      cur_offset_(0), base_offset_(0), lock_(0)
{
}

/**
 * Actual payload initialization function.
 */
void
BundlePayload::init(oasys::SpinLock* lock, int bundleid, location_t location)
{
    lock_ = lock;
    location_ = location;

    // initialize the file handle for the backing store, but
    // immediately close it
    if (location != NODATA) {
        oasys::StringBuffer path("%s/bundle_%d.dat",
                                 BundlePayload::payloaddir_.c_str(), bundleid);
        file_ = new oasys::FileIOClient();
        file_->logpathf("/bundle/payload/%d", bundleid);
        if (file_->open(path.c_str(),
                        O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR) < 0)
        {
            log_crit("/bundle/payload", "error opening payload file %s: %s",
                     path.c_str(), strerror(errno));
            return;
        }
    }
}

/**
 * Initialization when re-reading the database.
 */
void
BundlePayload::init(oasys::SpinLock* lock, int bundleid, BundleStore* store)
{
    lock_ = lock;
    location_ = DISK;

    oasys::StringBuffer path("%s/bundle_%d.dat",
                             BundlePayload::payloaddir_.c_str(), bundleid);
    file_ = new oasys::FileIOClient();
    file_->logpathf("/bundle/payload/%d", bundleid);
    if (file_->open(path.c_str(),
                    O_RDWR, S_IRUSR | S_IWUSR) < 0)
    {
        log_crit("/bundle/payload", "error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        return;
    }
}

/**
 * Destructor
 */
BundlePayload::~BundlePayload()
{
    if (file_) {
        if (!test_no_remove_)
            file_->unlink();
        delete file_;
        file_ = NULL;
    }
}

/**
 * Virtual from SerializableObject
 */
void
BundlePayload::serialize(oasys::SerializeAction* a)
{
    a->process("filename",    &fname_);
    a->process("length",      (u_int32_t*)&length_);
    a->process("rcvd_length", (u_int32_t*)&rcvd_length_);
    a->process("base_offset", (u_int32_t*)&base_offset_);
}

/**
 * Set the payload length in preparation for filling in with data.
 * Optionally also force-sets the location, or leaves it based on
 * configured parameters.
 */
void
BundlePayload::set_length(size_t length, location_t new_location)
{
    oasys::ScopeLock l(lock_, "BundlePayload::set_length");

    length_ = length;

    if (location_ == UNDETERMINED) {
        if (new_location == UNDETERMINED) {
            location_ = (length_ < mem_threshold_) ? MEMORY : DISK;
        } else {
            location_ = new_location;
        }
    }

    ASSERT(location_ != UNDETERMINED);
}

/**
 * Truncate the payload. Used for reactive fragmentation.
 */
void
BundlePayload::truncate(size_t length)
{
    oasys::ScopeLock l(lock_, "BundlePayload::truncate");
    
    ASSERT(length <= length_);
    ASSERT(length <= rcvd_length_);
    length_ = length;
    rcvd_length_ = length;
    cur_offset_ = length;

    if (location_ == MEMORY) {
        data_.resize(length);
        ASSERT(data_.length() == length);
    }

    reopen_file();
    file_->truncate(length);
    close_file();
}

/**
 * Reopen the payload file.
 */
void
BundlePayload::reopen_file()
{
    if (!file_->is_open()) {
        if (file_->reopen(O_RDWR) < 0) {
            log_err("/payload", "error reopening file %s: %s",
                    file_->path(), strerror(errno));
            return;
        }
        
        cur_offset_ = 0;
    }
}

/**
 * Close the payload file.
 */
void
BundlePayload::close_file()
{
    if (file_->is_open()) {
        file_->close();
    }
}

/**
 * Copy (or link) the payload to the given path.
 */
void
BundlePayload::copy_file(oasys::FileIOClient* dst)
{
    if (! is_file_open()) {
        reopen_file();
    } else {
        file_->lseek(0, SEEK_SET);
    }
    
    file_->copy_contents(length(), dst);

    close_file();
}
    
/**
 * Internal write helper function.
 */
void
BundlePayload::internal_write(const u_char* bp, size_t offset, size_t len)
{
    ASSERT(lock_->is_locked_by_me());
    ASSERT(file_->is_open());
    ASSERT(location_ != NODATA && location_ != UNDETERMINED);

    if (location_ == MEMORY) {

        // case 1: appending new data
        if (offset == data_.length()) {
            data_.append((const char*)bp, len);
        }
        
        // case 2: adding data after an empty space, so need some
        // intermediate padding
        else if (offset > data_.length()) {
            data_.append(offset - data_.length(), '\0');
            data_.append((const char*)bp, len);
        }

        // case 3: fully overwriting data in the buffer
        else if ((offset + len) <= data_.length()) {
            data_.replace(offset, len, (const char*)bp, len);
        }

        // that should cover it all
        else {
            PANIC("unexpected case in internal_write: "
                  "data.length=%u offset=%u len=%u",
                  (u_int)data_.length(), (u_int)offset, (u_int)len);
        }

        // sanity check
        ASSERTF(data_.length() >= offset + len,
                "length=%u offset=%u len=%u",
                (u_int)data_.length(), (u_int)offset, (u_int)len);
    }
    
    // check if we need to seek
    if (cur_offset_ != offset) {
        file_->lseek(offset, SEEK_SET);
        cur_offset_ = offset;
    }

    file_->writeall((char*)bp, len);

    cur_offset_  += len;
    rcvd_length_ += len;
}

/**
 * Set the payload data and length, closing the payload file after
 * it's been written to.
 */
void
BundlePayload::set_data(const u_char* bp, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::set_data");
    
    ASSERT(rcvd_length_ == 0);
    set_length(len);
    
    reopen_file();
    internal_write(bp, base_offset_, len);
    close_file();
}

/**
 * Append a chunk of payload data. Assumes that the length was
 * previously set. Keeps the payload file open.
 */
void
BundlePayload::append_data(const u_char* bp, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::append_data");
    
    ASSERT(length_ > 0);
    ASSERT(file_->is_open());

    // check if we need to seek
    if (cur_offset_ != rcvd_length_) {
        file_->lseek(rcvd_length_, SEEK_SET);
        cur_offset_ = rcvd_length_;
    }
    
    internal_write(bp, base_offset_ + cur_offset_, len);
}

/**
 * Write a chunk of payload data at the specified offset. Keeps
 * the payload file open.
 */
void
BundlePayload::write_data(const u_char* bp, size_t offset, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");
    
    ASSERT(length_ >= (len + offset));
    ASSERT(file_->is_open());
    
    internal_write(bp, base_offset_ + offset, len);
}

/**
 * Writes len bytes of payload data from from another payload at
 * the given src_offset to the given dst_offset. Keeps the payload
 * file open.
 */
void
BundlePayload::write_data(BundlePayload* src, size_t src_offset,
                          size_t len, size_t dst_offset)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");

    log_debug("/bundle/payload",
              "write_data: file=%s length_=%u src_offset=%u dst_offset=%u len %u",
              file_->path(),
              (u_int)length_, (u_int)src_offset, (u_int)dst_offset, (u_int)len);

    ASSERT(length_       >= dst_offset + len);
    ASSERT(src->length() >= src_offset + len);
    ASSERT(file_->is_open());

    // XXX/mho: todo - for cases where we're creating a fragment from
    // an existing bundle, make a hard link for the new fragment and
    // store the offset in base_offset_
    u_char buf[len];
    const u_char* bp = src->read_data(src_offset, len, buf, KEEP_FILE_OPEN);
    internal_write(bp, dst_offset, len);
}

/**
 * Return a pointer to a chunk of payload data. For in-memory
 * bundles, this will just be a pointer to the data buffer (unless
 * the FORCE_COPY flag is set).
 *
 * Otherwise, it will call read() into the supplied buffer (which
 * must be >= len).
 */
const u_char*
BundlePayload::read_data(size_t offset, size_t len, u_char* buf, int flags)
{
    oasys::ScopeLock l(lock_, "BundlePayload::read_data");
    
    ASSERTF(length_ >= (offset + len),
            "length=%u offset=%u len=%u",
            (u_int)length_, (u_int)offset, (u_int)len);

    ASSERTF(rcvd_length_ >= (offset + len),
            "rcvd_length=%u offset=%u len=%u",
            (u_int)rcvd_length_, (u_int)offset, (u_int)len);
    
    if (location_ == MEMORY) {
        if (flags & FORCE_COPY) {
            memcpy(buf, data_.data() + offset, len);
            return buf;
        } else {
            return (u_char*)data_.data() + offset;
        }
        
    } else {
        ASSERT(buf);
        
        reopen_file();
        
        // check if we need to seek first
        if (offset != cur_offset_) {
            file_->lseek(offset, SEEK_SET);
        }
        
        file_->readall((char*)buf, len);
        cur_offset_ = offset + len;

        if (! flags & KEEP_FILE_OPEN)
            close_file();
        
        return buf;
    }
}


} // namespace dtn
