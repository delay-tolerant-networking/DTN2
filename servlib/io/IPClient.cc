
#include <stdlib.h>
#include <sys/poll.h>

#include "IPClient.h"

IPClient::IPClient(const char* logbase, int socktype)
    : IPSocket(logbase, socktype)
{
}

IPClient::IPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
                   const char* logbase)
    : IPSocket(fd, remote_addr, remote_port, logbase)
{
}

IPClient::~IPClient()
{
}

int
IPClient::read(char* bp, size_t len)
{
// debugging hookto make sure that callers can handle short reads
// #define TEST_SHORT_READ
#ifdef TEST_SHORT_READ
    if (len > 64) {
        int rnd = rand() % len;
        ::logf("/test/shortread", LOG_DEBUG, "read(%d) -> read(%d)", len, rnd);
        len = rnd;
    }
#endif
    return IO::read(fd_, bp, len, logpath_);
}

int
IPClient::readv(const struct iovec* iov, int iovcnt)
{
    return IO::readv(fd_, iov, iovcnt, logpath_);
}

int
IPClient::write(const char* bp, size_t len)
{
    return IO::write(fd_, bp, len, logpath_);
}

int
IPClient::writev(const struct iovec* iov, int iovcnt)
{
    return IO::writev(fd_, iov, iovcnt, logpath_);
}

int
IPClient::readall(char* bp, size_t len)
{
    return IO::readall(fd_, bp, len, logpath_);
}

int
IPClient::writeall(const char* bp, size_t len)
{
    return IO::writeall(fd_, bp, len, logpath_);
}

int
IPClient::readvall(const struct iovec* iov, int iovcnt)
{
    return IO::readvall(fd_, iov, iovcnt, logpath_);
}

int
IPClient::writevall(const struct iovec* iov, int iovcnt)
{
    return IO::writevall(fd_, iov, iovcnt, logpath_);
}

int
IPClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_read(fd_, bp, len, timeout_ms, logpath_);
}

int
IPClient::timeout_readv(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    return IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, logpath_);
}

int
IPClient::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_readall(fd_, bp, len, timeout_ms, logpath_);
}

int
IPClient::timeout_readvall(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    return IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms, logpath_);
}
