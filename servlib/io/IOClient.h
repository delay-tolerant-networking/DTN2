#ifndef __IOCLIENT_H__
#define __IOCLIENT_H__

#include <sys/uio.h>

/*!
 * Abstract interface for stream type output channels.
 */
class IOClient {
public:
    virtual int read(char* bp, size_t len) = 0;
    virtual int readv(const struct iovec* iov, int iovcnt)  = 0;
    virtual int write(const char* bp, size_t len) = 0;
    virtual int writev(const struct iovec* iov, int iovcnt) = 0;

    /// Read / write out the entire supplied buffer, potentially
    /// requiring multiple calls to read/write().
    virtual int readall(char* bp, size_t len) = 0;
    virtual int writeall(const char* bp, size_t len) = 0;

    /// Similar functions for iovec
    virtual int readvall(const struct iovec* iov, int iovcnt) = 0;
    virtual int writevall(const struct iovec* iov, int iovcnt) = 0;

    // Variants that include a timeout
    virtual int timeout_read(char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms)  = 0;

};

#endif // __IOCLIENT_H__
