
#include <sys/errno.h>
#include <fcntl.h>
#include "BundlePayload.h"
#include "debug/Debug.h"
#include "util/StringBuffer.h"

size_t BundlePayload::mem_threshold_ = 16384;
std::string BundlePayload::dir_ = "/tmp/bundles";

BundlePayload::BundlePayload()
    : location_(DISK), length_(0), file_(NULL)
{
}

void
BundlePayload::init(int bundleid)
{
    StringBuffer path("%s/bundle_%d.dat", dir_.c_str(), bundleid);
    int fd = open(path.c_str(), O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        log_crit("/bundle/payload", "error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        return;
    }
    file_ = new FdIOClient(fd);
}

BundlePayload::~BundlePayload()
{
    // XXX/demmer need to delete the payload file somewhere
}


void
BundlePayload::serialize(SerializeAction* a)
{
    a->process("data", &data_);
    a->process("fname", &data_);
}

void
BundlePayload::set_length(size_t length)
{
    length_ = length;
    location_ = (length_ < mem_threshold_) ? MEMORY : DISK;
}

void
BundlePayload::set_data(const char* bp, size_t len)
{
    ASSERT(length_ == 0); // can only use this once
    length_ = len;
    append_data(bp, len);
}

void
BundlePayload::append_data(const char* bp, size_t len)
{
    ASSERT(length_ > 0);

    if (location_ == MEMORY) {
        data_.append(bp, len);
    }

    file_->writeall(bp, len);
}
