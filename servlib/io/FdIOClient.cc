#include "FdIOClient.h"
#include "IO.h"
#include "debug/Debug.h"

FdIOClient::FdIOClient(int fd)
    : fd_(fd)
{}

int 
FdIOClient::read(char* bp, size_t len)
{
    return IO::read(fd_, bp, len, "/fdio");
}

int 
FdIOClient::readv(const struct iovec* iov, int iovcnt)
{
    return IO::readv(fd_, iov, iovcnt, "/fdio");
}

int 
FdIOClient::readall(char* bp, size_t len)
{
    return IO::readall(fd_, bp, len, "/fdio");
}

int 
FdIOClient::readvall(const struct iovec* iov, int iovcnt)
{
    return IO::readvall(fd_, iov, iovcnt, "/fdio");
}

int 
FdIOClient::write(const char* bp, size_t len)
{
    return IO::write(fd_, bp, len, "/fdio");
}

int
FdIOClient::writev(const struct iovec* iov, int iovcnt)
{
    return IO::writev(fd_, iov, iovcnt, "/fdio");
}

int 
FdIOClient::writeall(const char* bp, size_t len)
{
    return IO::writeall(fd_, bp, len, "/fdio");
}

int 
FdIOClient::writevall(const struct iovec* iov, int iovcnt)
{
    return IO::writevall(fd_, iov, iovcnt, "/fdio");
}

int 
FdIOClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_read(fd_, bp, len, timeout_ms, "/fdio");
}

int 
FdIOClient::timeout_readv(const struct iovec* iov, int iovcnt,
	          int timeout_ms)
{
    return IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, "/fdio");
}
