
#include <sys/errno.h>
#include "BundlePayload.h"
#include "debug/Debug.h"
#include "thread/SpinLock.h"
#include "util/StringBuffer.h"

/*
 * Configurable settings.
 */
size_t BundlePayload::mem_threshold_;
std::string BundlePayload::dir_;
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
BundlePayload::init(SpinLock* lock, int bundleid, location_t location)
{
    lock_ = lock;
    location_ = location;

    // initialize the file handle for the backing store, but
    // immediately close it
    if (location != NODATA) {
        StringBuffer path("%s/bundle_%d.dat", dir_.c_str(), bundleid);
        file_ = new FileIOClient();
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
BundlePayload::serialize(SerializeAction* a)
{
    a->process("filename",    &fname_);
    a->process("length",      &length_);
    a->process("base_offset", &base_offset_);
}

/**
 * Set the payload length in preparation for filling in with data.
 * Optionally also force-sets the location, or leaves it based on
 * configured parameters.
 */
void
BundlePayload::set_length(size_t length, location_t location)
{
    ScopeLock l(lock_);

    length_ = length;
    
    if (location == UNDETERMINED) {
        location_ = (length_ < mem_threshold_) ? MEMORY : DISK;
    } else {
        location_ = location;
    }

    ASSERT(location_ != UNDETERMINED);

    if (location_ == MEMORY) {
        data_.reserve(length);
    }
}

/**
 * Truncate the payload. Used for reactive fragmentation.
 */
void
BundlePayload::truncate(size_t length)
{
    ScopeLock l(lock_);
    
    ASSERT(length <= length_);
    ASSERT(length <= rcvd_length_);
    length_ = length;

    // XXX/demmer this should truncate the file as well
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
    file_->close();
}

/**
 * Internal write helper function.
 */
void
BundlePayload::internal_write(const char* bp, size_t offset, size_t len)
{
    ASSERT(lock_->is_locked_by_me());
    ASSERT(file_->is_open());
    ASSERT(location_ != NODATA && location_ != UNDETERMINED);

    if (location_ == MEMORY) {
        ASSERT(data_.capacity() >= offset + len);
        data_.replace(offset, len, bp, len);
    }
    
    // check if we need to seek
    if (cur_offset_ != offset) {
        file_->lseek(offset, SEEK_SET);
        cur_offset_ = offset;
    }

    file_->writeall(bp, len);

    cur_offset_      += len;
    rcvd_length_ += len;
}

/**
 * Set the payload data and length, closing the payload file after
 * it's been written to.
 */
void
BundlePayload::set_data(const char* bp, size_t len)
{
    ScopeLock l(lock_);
    
    ASSERT(length_ == 0 && rcvd_length_ == 0);
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
BundlePayload::append_data(const char* bp, size_t len)
{
    ScopeLock l(lock_);
    
    ASSERT(length_ > 0);
    ASSERT(file_->is_open());
    
    internal_write(bp, base_offset_, len);
}

/**
 * Write a chunk of payload data at the specified offset. Keeps
 * the payload file open.
 */
void
BundlePayload::write_data(const char* bp, size_t offset, size_t len)
{
    ScopeLock l(lock_);
    
    ASSERT(length_ >= (len + offset));
    ASSERT(file_->is_open());
    
    internal_write(bp, offset, len);
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
    ScopeLock l(lock_);

    ASSERT(length_       >= dst_offset + len);
    ASSERT(src->length() >= src_offset + len);
    ASSERT(file_->is_open());

    // XXX/mho: todo - for cases where we're creating a fragment from
    // an existing bundle, make a hard link for the new fragment and
    // store the offset in base_offset_
    char buf[len];
    const char* bp = src->read_data(src_offset, len, buf, true);
    internal_write(bp, dst_offset, len);
}

/**
 * Return a pointer to a chunk of payload data. For in-memory
 * bundles, this will just be a pointer to the data buffer.
 * Otherwise, it will call read() into the supplied buffer (which
 * must be >= len).
 */
const char*
BundlePayload::read_data(size_t offset, size_t len, char* buf,
                         bool keep_file_open)
{
    ScopeLock l(lock_);
    
    ASSERT(length_ >= (len + offset));
    
    if (location_ == MEMORY) {
        return data_.data() + offset;
        
    } else {
        reopen_file();
        
        // check if we need to seek first
        if (offset != cur_offset_) {
            file_->lseek(offset, SEEK_SET);
        }
        
        file_->readall(buf, len);
        cur_offset_ = offset + len;

        if (!keep_file_open)
            close_file();
        
        return buf;
    }
}

