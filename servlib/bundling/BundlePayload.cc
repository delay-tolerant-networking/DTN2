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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>

#include "BundlePayload.h"
#include "storage/BundleStore.h"

namespace dtn {

//----------------------------------------------------------------------
bool BundlePayload::test_no_remove_ = false;

//----------------------------------------------------------------------
BundlePayload::BundlePayload(oasys::SpinLock* lock)
    : Logger("BundlePayload", "/dtn/bundle/payload"),
      location_(DISK), length_(0), 
      cur_offset_(0), base_offset_(0), lock_(lock)
{
}

//----------------------------------------------------------------------
void
BundlePayload::init(int bundleid, location_t location)
{
    location_ = location;
    
    logpathf("/dtn/bundle/payload/%d", bundleid);

    // nothing to do if there's no backing file
    if (location == MEMORY || location == NODATA) {
        return;
    }

    // initialize the file handle for the backing store, but
    // immediately close it
    BundleStore* bs = BundleStore::instance();

    // XXX/demmer the simulator can't really deal with files, so this
    // is a hacky way to handle bundles that get created with a DISK
    // location in the simulator... a better fix would have this class
    // use oasys::FileBackedObjectStore and then have an in-memory
    // abstraction of the store that we use in the simulator, akin
    // to the memorydb version of the DurableStore
    if (bs->payload_dir() == "NO_PAYLOAD_FILES") {
        location_ = MEMORY;
        return;
    }

    oasys::StringBuffer path("%s/bundle_%d.dat",
                             bs->payload_dir().c_str(), bundleid);
    

    file_.logpathf("%s/file", logpath_);
    
    int open_errno = 0;
    int err = file_.open(path.c_str(), O_EXCL | O_CREAT | O_RDWR,
                         S_IRUSR | S_IWUSR, &open_errno);
    
    if (err < 0 && open_errno == EEXIST)
    {
        log_err("payload file %s already exists: overwriting and retrying",
                path.c_str());

        err = file_.open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    }
        
    if (err < 0)
    {
        log_crit("error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        return;
    }

    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }

    sync_payload();

    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::sync_payload()
{
    if (location_==MEMORY || location_==NODATA) {
        return;
    }

    pin_file();
    fsync(file_.fd());
    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::init_from_store(int bundleid)
{
    location_ = DISK;

    BundleStore* bs = BundleStore::instance();
    oasys::StringBuffer path("%s/bundle_%d.dat",
                             bs->payload_dir().c_str(), bundleid);

    file_.logpathf("%s/file", logpath_);
    
    if (file_.open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR) < 0)
    {
        // if the payload file open fails when trying to initialize,
        // we're in some amount of trouble, so set the location state
        // to NODATA to signal the upper layers that there's a problem
        // and that the bundle isn't valid
        log_crit("error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        location_ = NODATA;
        return;
    }

    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }
    unpin_file();
}

//----------------------------------------------------------------------
BundlePayload::~BundlePayload()
{
    if (location_ == DISK && file_.is_open()) {
        BundleStore::instance()->payload_fdcache()->close(file_.path());
        file_.set_fd(-1); // avoid duplicate close
        
        if (!test_no_remove_)
        {
            file_.unlink();
        }
    }
}

//----------------------------------------------------------------------
void
BundlePayload::serialize(oasys::SerializeAction* a)
{
    a->process("length",      (u_int32_t*)&length_);
    a->process("base_offset", (u_int32_t*)&base_offset_);
}

//----------------------------------------------------------------------
void
BundlePayload::set_length(size_t length)
{
    oasys::ScopeLock l(lock_, "BundlePayload::set_length");
    length_ = length;
    if (location_ == MEMORY) {
        data_.reserve(length);
        data_.set_len(length);
    }
}


//----------------------------------------------------------------------
void
BundlePayload::pin_file() const
{
    if (location_ != DISK) {
        return;
    }
    
    BundleStore* bs = BundleStore::instance();
    int fd = bs->payload_fdcache()->get_and_pin(file_.path());
    
    if (fd == -1) {
        if (file_.reopen(O_RDWR) < 0) {
            log_err("error reopening file %s: %s",
                    file_.path(), strerror(errno));
            return;
        }
        
        cur_offset_ = 0;

        int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
        if (fd != file_.fd()) {
            PANIC("duplicate entry in open fd cache");
        }
        
    } else {
        ASSERT(fd == file_.fd());
    }
}

//----------------------------------------------------------------------
void
BundlePayload::unpin_file() const
{
    if (location_ != DISK) {
        return;
    }
    
    BundleStore::instance()->payload_fdcache()->unpin(file_.path());
}

//----------------------------------------------------------------------
void
BundlePayload::truncate(size_t length)
{
    oasys::ScopeLock l(lock_, "BundlePayload::truncate");
    
    ASSERT(length <= length_);
    length_     = length;
    cur_offset_ = length; // XXX/demmer is this right?
    
    switch (location_) {
    case MEMORY:
        data_.set_len(length);
        break;
    case DISK:
        pin_file();
        file_.truncate(length);
        unpin_file();
        break;
    case NODATA:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
BundlePayload::copy_file(oasys::FileIOClient* dst) const
{
    ASSERT(location_ == DISK);
    pin_file();
    file_.lseek(0, SEEK_SET);
    file_.copy_contents(dst, length());
    unpin_file();
}

//----------------------------------------------------------------------
bool
BundlePayload::replace_with_file(const char* path)
{
    oasys::ScopeLock l(lock_, "BundlePayload::replace_with_file");
    
    ASSERT(location_ == DISK);
    std::string payload_path = file_.path();

    // first flush the old fd from the cache and unlink the file
    BundleStore* bs = BundleStore::instance();
    bs->payload_fdcache()->close(file_.path());
    file_.unlink();

    // now try to make the hard link
    int err = ::link(path, payload_path.c_str());
    if (err == 0) {
        log_debug("replace_with_file: successfully created link to %s", path);

        // unlink() clobbered path_ in file_, so we have to set it
        // again and re-open the copy
        file_.set_path(payload_path);
        if (file_.reopen(O_RDWR) < 0) {
            log_err("replace_with_file: error reopening file: %s",
                    strerror(errno));
            return false;
        }
        
    } else {
        // ::link failed
        err = errno;
        if (err != EXDEV) {
            log_err("error linking to path '%s': %s", path, strerror(err));
            return false;
        }
        
        // copy the contents if they're on different filesystems
        log_debug("replace_with_file: link failed: %s", strerror(err));
        
        oasys::FileIOClient src;
        int fd = src.open(path, O_RDONLY, &err);
        if (fd < 0) {
            log_err("error opening path '%s' for reading: %s",
                    path, strerror(err));
            return false;
        }
        
        file_.set_path(payload_path);
        if (file_.reopen(O_RDWR | O_CREAT, S_IRUSR | S_IWUSR) < 0) {
            log_err("replace_with_file: error reopening file: %s",
                    strerror(err));
            return false;
        }
        
        src.copy_contents(&file_);
        src.close();
    }

    set_length(oasys::FileUtils::size(file_.path()));

    // now need to re-add the entry to the cache
    ASSERT(file_.fd() != -1);
    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }
    unpin_file();
    return true;
}
    
//----------------------------------------------------------------------
void
BundlePayload::internal_write(const u_char* bp, size_t offset, size_t len)
{
    // the caller should have pinned the fd
    if (location_ == DISK) {
        ASSERT(file_.is_open());
    }
    ASSERT(lock_->is_locked_by_me());
    ASSERT(length_ >= (offset + len));

    switch (location_) {
    case MEMORY:
        memcpy(data_.buf() + offset, bp, len);
        break;
    case DISK:
        // check if we need to seek
        if (cur_offset_ != offset) {
            file_.lseek(offset, SEEK_SET);
            cur_offset_ = offset;
        }
        file_.writeall((char*)bp, len);
        cur_offset_ += len;
        break;
    case NODATA:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
BundlePayload::set_data(const u_char* bp, size_t len)
{
    set_length(len);
    write_data(bp, 0, len);
}

//----------------------------------------------------------------------
void
BundlePayload::set_data(const std::string& data)
{
    set_data((const u_char*)(data.data()), data.length());
}

//----------------------------------------------------------------------
void
BundlePayload::append_data(const u_char* bp, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::append_data");

    size_t old_length = length_;
    set_length(length_ + len);
    
    pin_file();
    internal_write(bp, old_length, len);
    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::write_data(const u_char* bp, size_t offset, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");
    
    ASSERT(length_ >= (len + offset));
    pin_file();
    internal_write(bp, offset, len);
    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::write_data(const BundlePayload& src, size_t src_offset,
                          size_t len, size_t dst_offset)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");

    log_debug("write_data: file=%s length_=%zu src_offset=%zu "
              "dst_offset=%zu len %zu",
              file_.path(),
              length_, src_offset, dst_offset, len);

    ASSERT(length_       >= dst_offset + len);
    ASSERT(src.length() >= src_offset + len);

    // XXX/mho: todo - for cases where we're creating a fragment from
    // an existing bundle, make a hard link for the new fragment and
    // store the offset in base_offset_

    // XXX/demmer todo -- we should copy the payload in max-length chunks
    
    oasys::ScratchBuffer<u_char*, 1024> buf(len);
    const u_char* bp = src.read_data(src_offset, len, buf.buf());

    pin_file();
    internal_write(bp, dst_offset, len);
    unpin_file();
}

//----------------------------------------------------------------------
const u_char*
BundlePayload::read_data(size_t offset, size_t len, u_char* buf)
{
    oasys::ScopeLock l(lock_, "BundlePayload::read_data");
    
    ASSERTF(length_ >= (offset + len),
            "length=%zu offset=%zu len=%zu",
            length_, offset, len);

    ASSERT(buf != NULL);
    
    switch(location_) {
    case MEMORY:
        memcpy(buf, data_.buf() + offset, len);
        break;

    case DISK:
        pin_file();
        
        // check if we need to seek first
        if (offset != cur_offset_) {
            file_.lseek(offset, SEEK_SET);
        }
        
        file_.readall((char*)buf, len);
        cur_offset_ = offset + len;
        
        unpin_file();
        break;

    case NODATA:
        NOTREACHED;
    }

    return buf;
}


} // namespace dtn
