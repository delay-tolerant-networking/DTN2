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


#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>

#include "BundlePayload.h"

namespace dtn {

/*
 * Configurable settings.
 */
std::string BundlePayload::payloaddir_ = "";
size_t BundlePayload::mem_threshold_ = 16384;
bool BundlePayload::test_no_remove_ = false;

/**
 * Constructor
 */
BundlePayload::BundlePayload(oasys::SpinLock* lock)
    : Logger("BundlePayload", "/dtn/bundle/payload"),
      location_(DISK), length_(0), rcvd_length_(0), file_(NULL),
      cur_offset_(0), base_offset_(0), lock_(lock)
{
}

/**
 * Actual payload initialization function.
 */
void
BundlePayload::init(int bundleid, location_t location)
{
    location_ = location;

    logpathf("/dtn/bundle/payload/%d", bundleid);

    // initialize the file handle for the backing store, but
    // immediately close it
    if (location != NODATA) {
        oasys::StringBuffer path("%s/bundle_%d.dat",
                                 BundlePayload::payloaddir_.c_str(), bundleid);
        file_ = new oasys::FileIOClient();
        file_->logpathf("%s/file", logpath_);
        int open_errno = 0;
        int err = file_->open(path.c_str(), O_EXCL | O_CREAT | O_RDWR,
                              S_IRUSR | S_IWUSR, &open_errno);

        if (err < 0 && open_errno == EEXIST)
        {
            log_err("payload file %s already exists: overwriting and retrying",
                    path.c_str());

            err = file_->open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
        }
        
        if (err < 0)
        {
            log_crit("error opening payload file %s: %s",
                     path.c_str(), strerror(errno));
            return;
        }
    }
}

/**
 * Initialization when re-reading the database.
 */
void
BundlePayload::init_from_store(int bundleid)
{
    location_ = DISK;

    oasys::StringBuffer path("%s/bundle_%d.dat",
                             BundlePayload::payloaddir_.c_str(), bundleid);
    file_ = new oasys::FileIOClient();
    file_->logpathf("/bundle/payload/%d", bundleid);
    if (file_->open(path.c_str(),
                    O_RDWR, S_IRUSR | S_IWUSR) < 0)
    {
        log_crit("error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        return;
    }
    file_->close();
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
            log_err("error reopening file %s: %s",
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
                  "data.length=%zu offset=%zu len=%zu",
                  data_.length(), offset, len);
        }

        // sanity check
        ASSERTF(data_.length() >= offset + len,
                "length=%zu offset=%zu len=%zu",
                data_.length(), offset, len);
    }
    
    // check if we need to seek
    if (cur_offset_ != offset) {
        file_->lseek(offset, SEEK_SET);
        cur_offset_ = offset;
    }

    file_->writeall((char*)bp, len);

    cur_offset_  += len;
    rcvd_length_ = std::max(rcvd_length_, offset + len);

    ASSERT(rcvd_length_ <= length_);
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

    log_debug("write_data: file=%s length_=%zu src_offset=%zu dst_offset=%zu len %zu",
              file_->path(),
              length_, src_offset, dst_offset, len);

    ASSERT(length_       >= dst_offset + len);
    ASSERT(src->length() >= src_offset + len);
    ASSERT(file_->is_open());

    // XXX/mho: todo - for cases where we're creating a fragment from
    // an existing bundle, make a hard link for the new fragment and
    // store the offset in base_offset_

    // XXX/demmer todo -- we should copy the payload in max-length chunks
    
    oasys::ScratchBuffer<u_char*, 1024> buf(len);
    const u_char* bp = src->read_data(src_offset, len, buf.buf(), KEEP_FILE_OPEN);
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
            "length=%zu offset=%zu len=%zu",
            length_, offset, len);

    ASSERTF(rcvd_length_ >= (offset + len),
            "rcvd_length=%zu offset=%zu len=%zu",
            rcvd_length_, offset, len);
    
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

        if (! (flags & KEEP_FILE_OPEN))
            close_file();
        
        return buf;
    }
}


} // namespace dtn
