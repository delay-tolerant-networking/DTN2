#include "FileIOClient.h"
#include "IO.h"

int
FileIOClient::open(const char* path, int flags)
{
    path_.assign(path);
    fd_ = IO::open(path, flags, logpath_);
    return fd_;
}

int
FileIOClient::open(const char* path, int flags, mode_t mode)
{
    path_.assign(path);
    fd_ = IO::open(path, flags, mode, logpath_);
    return fd_;
}

int
FileIOClient::close()
{
    int ret = IO::close(fd_, logpath_);
    fd_ = -1;
    return ret;
}

int
FileIOClient::unlink()
{
    int ret = 0;
    if (fd_ == -1) {
        ASSERT(path_.length() == 0);
        return 0;
    }
    
    ret = IO::unlink(path_.c_str(), logpath_);
    path_.assign("");
    return ret;
}

int
FileIOClient::lseek(off_t offset, int whence)
{
    return IO::lseek(fd_, offset, whence, logpath_);
}
