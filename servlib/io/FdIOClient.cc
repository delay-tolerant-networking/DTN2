#include "FdIOClient.h"
#include "IO.h"
#include "debug/Debug.h"

FdIOClient::FdIOClient(int fd)
    : Logger("/fdio"), fd_(fd)
{}

int 
FdIOClient::read(char* bp, size_t len)
{
    return IO::read(fd_, bp, len, logpath_);
}

int 
FdIOClient::readv(const struct iovec* iov, int iovcnt)
{
    return IO::readv(fd_, iov, iovcnt, logpath_);
}

int 
FdIOClient::readall(char* bp, size_t len)
{
    return IO::readall(fd_, bp, len, logpath_);
}

int 
FdIOClient::readvall(const struct iovec* iov, int iovcnt)
{
    return IO::readvall(fd_, iov, iovcnt, logpath_);
}

int 
FdIOClient::write(const char* bp, size_t len)
{
    return IO::write(fd_, bp, len, logpath_);
}

int
FdIOClient::writev(const struct iovec* iov, int iovcnt)
{
    return IO::writev(fd_, iov, iovcnt, logpath_);
}

int 
FdIOClient::writeall(const char* bp, size_t len)
{
    return IO::writeall(fd_, bp, len, logpath_);
}

int 
FdIOClient::writevall(const struct iovec* iov, int iovcnt)
{
    return IO::writevall(fd_, iov, iovcnt, logpath_);
}

int 
FdIOClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_read(fd_, bp, len, timeout_ms, logpath_);
}

int 
FdIOClient::timeout_readv(const struct iovec* iov, int iovcnt,
	          int timeout_ms)
{
    return IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, logpath_);
}

int
FdIOClient::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_readall(fd_, bp, len, timeout_ms, logpath_);
}

int
FdIOClient::timeout_readvall(const struct iovec* iov, int iovcnt,
                             int timeout_ms)
{
    return IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms, logpath_);
}
